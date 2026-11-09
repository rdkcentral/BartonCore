# Design: SBMD DAL driver API — fresh supplements, atomic operations, merge writes (BARTON-422, epic BARTON-420)

## Context

Epic delivery order is 412 fix → 423 → 421 → 426 → **422** → 424 → 425 (epic D13). When this change lands:

- BARTON-423 has delivered `SbmdTransaction`, `SbmdTransactionOrchestrator`, `SbmdWorker`, and the internal data seam `SbmdDataAccess` / `SbmdDataSession` (`Fetch`, `Stage`, `StageConditional`, `RecordAttributeWrite`, `Commit`, `Abort`, `SetAbortListener`). The step loop is: (1) one batched `Fetch` for the step's supplements, including sequence entries; (2) JS on the Matter thread; (3) `log` ops immediately, data ops converted by `SbmdHandlerInvoker::ToSbmdWrites` and passed to `Stage`/`StageConditional`; (4) terminal; (5) `Commit` on success, `Abort` and failure hand-off on failure. Writes from a failed SBMD Transaction are already discarded. Recovery (epic D12): if a hop's `onError` returns a non-`error` terminal and BARTON-412 honors it (not for `timeout` or depth-limit), the transaction continues and the failure handler does not run.
- BARTON-421 has delivered the DAL core including `DalTransaction::AllocateSequence(key)` and `DalTransaction::StageConditional(key, expected, value)`; a mismatched condition fails commit with `CommitFailed` reason `ConditionFailed`, surfaced to SBMD as `commitFailed` / `conditionFailed`.
- BARTON-426 has delivered the source plugins and `DalSbmdDataAccess`, which replaced `LegacySbmdDataAccess` and deleted `transientStore`. The transient plugin declares the `Atomic` capability.

The seam already carries sequences (`SbmdFetchRequest::sequences`, `SbmdFetchResult::sequences`) and conditional writes (`SbmdConditionalWrite`), but no JS API reaches them, there is no merge write, and the supplement override and inheritance rules are not part of the driver contract. This change adds the driver-facing API.

Terminology (epic D1): an **SBMD Transaction** is the driver-level execution of one request, from the initial handler through every parked device operation and deferred callback, to the terminal result and, on failure, the failure handler. A **DAL Transaction** is the data-access unit owned by the `DalSbmdDataAccess` session. Drivers see neither directly.

No `docs/dev/architecture.md` exists in the project.

## References

- `openspec/changes/sbmd-dal-core/design.md` — Epic architecture, glossary, D9 (fresh reads, per-key writes), D11 (atomic operations), D12 (recovery), D13 (delivery order).
- `openspec/changes/sbmd-transaction-orchestrator/design.md` — Step loop, `SbmdDataAccess` seam (D11 there), `ToSbmdWrites`, `PlanFetch`, D12 recovery application.
- `openspec/changes/sbmd-dal-source-plugins/design.md` — Transient data plugin, `Atomic` capability, commit path that resolves merges.
- `openspec/changes/sbmd-transaction-failure-handler/design.md` — `onTransactionFailure` registration (`function` or `{ supplements, handler }`), `commitFailed` hand-off.
- [RFC 7396 — JSON Merge Patch](https://www.rfc-editor.org/rfc/rfc7396) — Semantics adopted for `mergeTransientData`; §2 gives the reference algorithm implemented in-tree.
- [cJSON](https://github.com/DaveGamble/cJSON) — JSON library linked by BartonCore (system `libcjson-dev`); its optional `cJSON_Utils` is not used by this repository.
- `docs/SBMD.md` §4.12, §5.1, §6.2, §7.2, §7.3 — Current driver contract.

## Goals / Non-Goals

**Goals:**

- Every step sees supplements fetched at that step's time, with inheritance and an explicit override for deferred callbacks.
- Drivers can allocate unique counters without races (sequence supplements).
- Drivers can make a write conditional on the value they read (`setTransientDataIf`).
- Drivers can update one member of a transient JSON object without rewriting the whole object.
- Clear authoring guidance on what remains unsafe.
- Intent-only API: no transaction IDs or DAL internals in JS.

**Non-Goals:**

- Orchestrator, seam, and step loop (BARTON-423); DAL primitives (BARTON-421); plugins (BARTON-426); failure handler (BARTON-424); driver migration (BARTON-425).
- Sequences or conditional writes on non-transient namespaces.
- Automatic retry on `conditionFailed`.

## Architecture

```text
 driver JS (Matter thread)          SbmdTransactionOrchestrator (calling thread)       SbmdDataSession (DalSbmdDataAccess)
 -------------------------          --------------------------------------------       -----------------------------------
                                    step 0: decl = handler.supplements
                                    PlanFetch(decl) -> {reads, sequences}
                                    Fetch(request) ------------------------------------> main loop: DalTransaction.Read
                                                                                           + AllocateSequence (immediate,
                                                                                             never rolled back)
 handler(args{supplements:{transientData, sequence}}) <-- run on Matter thread
   returns setTransientDataIf / mergeTransientData / ... + requestCommand{opts.supplements?}
                                    log ops now; ToSbmdWrites(ops)
                                    Stage / StageConditional --------------------------> staged in DAL Transaction
                                    hop.decl = opts.supplements ?? step decl
                                    device op on Matter thread ... park ...
                                    outcome handed back
                                    step 1: Fetch(PlanFetch(hop.decl)) ----------------> fresh read (own staged writes visible)
 onResponse(args{supplements@T1}) <-- run on Matter thread
   returns mergeTransientData(k, patch) + success
                                    Stage(merge) --------------------------------------> staged merge
                                    Commit -------------------------------------------> transient plugin: check conditions,
                                                                                          apply merges to current value,
                                                                                          apply writes; mismatch ->
                                                                                          CommitFailed/conditionFailed
                                    commit failed -> Fail(commitFailed) -> Abort -> onTransactionFailure (BARTON-424)
```

## Decisions

### D1: Every step receives supplements fetched at that step's time

**Decision:** The effective supplement declaration per step is:

| Step | Effective declaration |
| --- | --- |
| Initial handler (resource read/write/execute, seed, event, attribute report, command handler) | The handler's declared `supplements` |
| `onResponse` / `onError` of `requestCommand` or `readAttribute` (any outcome) | The terminal's `supplements` option if present, else the declaration effective for the step that returned the terminal |
| `onTransactionFailure` (BARTON-424) | The failure handler's own declaration (`{ supplements, handler }` form), fetched as step 1 of the compensation SBMD Transaction |

BARTON-423 already calls `Fetch` once per step with `driver.SupplementsForStep(txn, trigger)`. This change defines inheritance by storing the effective declaration on the armed hop.

```cpp
// core/deviceDrivers/matter/sbmd/txn/SbmdTransaction.h (field added to the armed-hop record from BARTON-423)
        /** Supplements fetched before this hop's onResponse/onError. Inherited unless the terminal overrides. */
        SbmdSupplements supplementDecl;
```

```cpp
// core/deviceDrivers/matter/sbmd/txn/SbmdTransactionOrchestrator.cpp (when arming a hop from a deferred terminal)
hop.supplementDecl = terminal.supplements.has_value() ? *terminal.supplements : step.effectiveSupplements;
```

```cpp
// core/deviceDrivers/matter/sbmd/SbmdDriver.cpp (SupplementsForStep, deferred triggers)
const SbmdSupplements &SbmdDriver::SupplementsForStep(const SbmdTransaction &txn, const SbmdStepTrigger &trigger) const
{
    switch (trigger.kind)
    {
        case SbmdStepTrigger::Kind::HopResponse:
        case SbmdStepTrigger::Kind::HopError:
            return txn.ArmedHop().supplementDecl;
        default:
            return trigger.handler->supplements;
    }
}
```

Reads go through the transaction's data session, so a step sees values committed by other SBMD Transactions plus its own staged writes (read-your-own-staged-writes, including resolved merges).

**Alternative considered:** Require every deferred terminal to re-declare supplements. Rejected as the only option — verbose and easy to forget; kept as the override.

**Alternative considered:** Deliver failure-handler supplements from the failing step's declaration. Rejected — BARTON-424 gives the failure handler its own declaration because it serves every SBMD Transaction of the driver.

**Trade-offs:** One batched `Fetch` per step that declares supplements; steps with none pay nothing.

---

### D2: Sequence supplements

**Decision:** A handler may declare `sequence` in its supplements: a transient key name or an array of names. `PlanFetch` puts them in `SbmdFetchRequest::sequences`; the session allocates each atomically during `Fetch` (`DalTransaction::AllocateSequence`) and the value is delivered as a number in `args.supplements.sequence[name]`. Allocation is immediate, bypasses staging, and is never rolled back (gaps are acceptable). The counter key is a normal transient key: a later read returns the last allocated value; drivers SHOULD NOT also write it. A sequence key inherited by a deferred step is allocated again for that step; drivers that need one value per transaction declare the sequence only on the initial handler and pass the value in `context`, or override deferred supplements.

```js
// Driver usage
resources: {
    createSession: {
        execute: {
            supplements: { transientData: [TD_SESSIONS], sequence: TD_NEXT_SESSION_ID },
            handler: executeCreateSession
        }
    }
}

function executeCreateSession(args) {
    var sessionId = String(args.supplements.sequence[TD_NEXT_SESSION_ID]);
    // ...
}
```

```cpp
// core/deviceDrivers/matter/sbmd/SbmdSupplements.h (addition)
        /** Transient counter keys allocated atomically during the step's fetch (epic D11). */
        std::vector<std::string> sequences;
```

```cpp
// core/deviceDrivers/matter/sbmd/mquickjs/SbmdHandlerInvoker.cpp (PlanFetch, addition)
for (const std::string &name : declared.sequences)
{
    request.sequences.push_back(SbmdDataKey {SbmdDataNamespace::Transient, name});
}
```

```cpp
// core/deviceDrivers/matter/sbmd/mquickjs/SbmdHandlerInvoker.cpp (ToFetchedSupplements, addition)
for (const auto &[key, value] : result.sequences)
{
    fetched.sequences[key.path] = value; // delivered as args.supplements.sequence[name] (JS number)
}
```

`SbmdLoader` accepts `sequence` as a string or array of strings; the JSON schema `$defs/supplements` gains `"sequence": {"oneOf": [{"type": "string"}, {"type": "array", "items": {"type": "string"}}]}`. Values above 2^53 are not expected; the plugin counter is `uint64_t` and the builder throws if it exceeds `Number.MAX_SAFE_INTEGER`.

**Alternative considered:** A result op `storage.nextSequence(name)` resolved at commit. Rejected — the value is needed inside the handler (to build the session and return it), and JS never calls the DAL synchronously (epic D6).

**Alternative considered:** Roll sequences back on abort. Rejected by epic D11 — would reintroduce the race.

**Trade-offs:** Gaps on failed transactions; acceptable for identifiers.

---

### D3: `storage.setTransientDataIf(name, expected, value, ttlSecs)`

**Decision:** New non-terminal op. `expected` is the string value the driver read (typically from supplements) or `null` for "must be absent". It is staged with `SbmdDataSession::StageConditional`. At commit the transient plugin checks all conditions before applying any write in its batch; a mismatch fails the commit, and the orchestrator fails the SBMD Transaction with `commitFailed` and reason `conditionFailed`. Other plugins already committed are rolled back per epic D8; all staged writes are discarded; `onTransactionFailure` runs with `args.failure.type === "commitFailed"` and a message beginning `conditionFailed`. The resource operation fails. `ttlSecs == 0` deletes the key if the condition holds.

```js
// core/deviceDrivers/matter/sbmd/scriptCommon/sbmd-result.js (storage namespace addition)
setTransientDataIf: function (name, expected, value, ttlSecs) {
    if (typeof name !== 'string' || (expected !== null && typeof expected !== 'string') || typeof value !== 'string') {
        throw new TypeError('setTransientDataIf(name: string, expected: string|null, value: string, ttlSecs: number)');
    }
    self._ops.push({op: 'setTransientDataIf', key: name, expected: expected, value: value, ttlSecs: ttlSecs | 0});
    return self;
},
```

```cpp
// core/deviceDrivers/matter/sbmd/mquickjs/SbmdResultExecutor.h
struct SetTransientDataIf
{
    std::string key;
    std::optional<std::string> expected; // nullopt = must be absent
    std::string value;
    uint32_t ttlSecs;
};
```

```cpp
// core/deviceDrivers/matter/sbmd/mquickjs/SbmdHandlerInvoker.cpp (ToSbmdWrites, addition)
else if (const auto *c = std::get_if<ResultOp::SetTransientDataIf>(&op.data))
{
    SbmdWrite w {SbmdDataKey {SbmdDataNamespace::Transient, c->key}, std::nullopt, std::nullopt};
    if (c->ttlSecs != 0)
    {
        w.value = SbmdDataValue {c->value, std::chrono::seconds(c->ttlSecs), std::nullopt};
    }
    out.conditional.push_back(SbmdConditionalWrite {std::move(w), c->expected});
}
```

The orchestrator stages `out.writes` and `out.conditional` in op order (BARTON-423 step 3).

**Alternative considered:** Automatic retry of the transaction on `conditionFailed`. Rejected by epic D3/D11 — device side effects may already have happened; the driver decides via `onTransactionFailure` or the client retries.

**Trade-offs:** A conditional mismatch fails the whole SBMD Transaction, including device operations that already succeeded; compensation is the failure handler's job.

---

### D4: `storage.mergeTransientData(name, patch, ttlSecs)` with RFC 7396 semantics

**Decision:** New non-terminal op. The builder serializes `patch` (a non-array object). It becomes an `SbmdWrite` with `mergePatchJson` set, mapped by `DalSbmdDataAccess` to a DAL `Write` with `mergePatchJson` set. The transient plugin resolves it at commit (after condition checks) against the value current at commit: absent or non-object → `{}`, apply the patch, store with the given TTL. `null` members delete; arrays are replaced. `ttlSecs == 0` deletes the key. Multiple staged writes to a key in one transaction apply in staging order. Reads within the transaction return the resolved value.

Interface extensions (additive fields on types owned by BARTON-423 and BARTON-421):

```cpp
// core/deviceDrivers/matter/sbmd/txn/SbmdDataAccess.h (SbmdWrite addition)
        /** When set, value is ignored and this RFC 7396 merge patch is applied at commit (Transient only). */
        std::optional<std::string> mergePatchJson;
```

```cpp
// core/deviceDrivers/matter/sbmd/dal/DalTypes.h (Write addition)
        /** RFC 7396 merge patch applied to the current value at commit. Only plugins with merge support accept it. */
        std::optional<std::string> mergePatchJson;
```

```js
// core/deviceDrivers/matter/sbmd/scriptCommon/sbmd-result.js (storage namespace addition)
mergeTransientData: function (name, patch, ttlSecs) {
    if (typeof name !== 'string' || patch === null || typeof patch !== 'object' || Array.isArray(patch)) {
        throw new TypeError('mergeTransientData(name: string, patch: object, ttlSecs: number)');
    }
    self._ops.push({op: 'mergeTransientData', key: name, patch: JSON.stringify(patch), ttlSecs: ttlSecs | 0});
    return self;
},
```

**cJSON finding:** BartonCore does not bundle cJSON; it uses the system `libcjson-dev` (`docker/Dockerfile`) and links only `cjson` (`core/CMakeLists.txt`, `core/test/CMakeLists.txt`). No file includes `cJSON_Utils.h` or links `cjson_utils`, and availability of `cJSON_Utils` on the Docker image and Yocto targets is unverified. The design therefore implements RFC 7396 §2 in-tree on top of plain cJSON, with no new link dependency:

```cpp
// core/deviceDrivers/matter/sbmd/dal/plugins/JsonMergePatch.cpp
#include "JsonMergePatch.h"

#include <cjson/cJSON.h>

namespace barton
{
    namespace
    {
        /** RFC 7396 §2 MergePatch(Target, Patch). Takes ownership of target; does not take patch. */
        cJSON *MergePatch(cJSON *target, const cJSON *patch)
        {
            if (!cJSON_IsObject(patch))
            {
                cJSON_Delete(target);
                return cJSON_Duplicate(patch, true);
            }
            if (!cJSON_IsObject(target))
            {
                cJSON_Delete(target);
                target = cJSON_CreateObject();
            }
            for (const cJSON *member = patch->child; member != nullptr; member = member->next)
            {
                if (cJSON_IsNull(member))
                {
                    cJSON_DeleteItemFromObjectCaseSensitive(target, member->string);
                    continue;
                }
                cJSON *existing = cJSON_DetachItemFromObjectCaseSensitive(target, member->string);
                cJSON_AddItemToObject(target, member->string, MergePatch(existing, member));
            }
            return target;
        }
    } // namespace

    std::optional<std::string> ApplyJsonMergePatch(const std::optional<std::string> &current, const std::string &patchJson)
    {
        cJSON *patch = cJSON_Parse(patchJson.c_str());
        if (patch == nullptr || !cJSON_IsObject(patch))
        {
            cJSON_Delete(patch);
            return std::nullopt; // rejected earlier by SbmdResultExecutor; defensive
        }

        cJSON *target = current ? cJSON_Parse(current->c_str()) : nullptr;
        target = MergePatch(target, patch); // non-object target becomes {}

        char *out = cJSON_PrintUnformatted(target);
        std::string merged = out ? out : "{}";
        cJSON_free(out);
        cJSON_Delete(target);
        cJSON_Delete(patch);
        return merged;
    }
} // namespace barton
```

If a later audit confirms `cJSON_Utils` on every target, `cJSONUtils_MergePatch` may replace `MergePatch`; the tests are identical.

**Alternative considered:** Path-based member ops. Rejected — more API surface; merge patch covers set and delete.

**Alternative considered:** Resolve the merge at staging time against the step's read. Rejected — turns it back into a stale whole-value write.

**Trade-offs:** Parse/serialize per merge at commit; values are small.

---

### D5: Authoring guidance and the remaining limitation

**Decision:** `docs/SBMD.md` states that fresh supplements do not make an unconditional read-compute-write safe: two SBMD Transactions can read the same value, and the later commit wins. Drivers must use one of:

| Need | Use |
| --- | --- |
| Unique counter / identifier | `supplements: { sequence: KEY }` |
| Update one member of a JSON object | `storage.mergeTransientData` |
| Replace a whole value derived from what was read | `storage.setTransientDataIf(name, readValue, newValue, ttl)` |
| State only needed for one transaction | `context` (IDs only) or staged writes read back in later steps |

**Alternative considered:** Per-device serialization of SBMD Transactions. Rejected by the epic (D11).

## Thread Safety

Unchanged from BARTON-423 and BARTON-421: the orchestrator calls the data session; `DalSbmdDataAccess` hops to the main loop (inline if the orchestrator owns it); sequence allocation, condition checks, and merge resolution run inside the transient plugin on the main loop and are atomic by confinement. JS runs on the Matter thread and never calls the data session. The Matter thread never waits on the main loop.

## Interfaces

### Driver JS

| API | Change |
| --- | --- |
| `args.supplements` in `onResponse`/`onError` | Fetched at that step; inherited declaration or override. |
| `requestCommand(..., { supplements })`, `readAttribute(..., { supplements })` | New optional override. |
| `supplements: { sequence: string \| string[] }` | New; `args.supplements.sequence[name]` is a number. |
| `storage.setTransientDataIf(name, expected, value, ttlSecs)` | New non-terminal op; mismatch → `commitFailed` / `conditionFailed`. |
| `storage.mergeTransientData(name, patch, ttlSecs)` | New non-terminal op (RFC 7396), resolved at commit. |

### Failure reporting

| Condition | `args.failure.type` | Message prefix |
| --- | --- | --- |
| `setTransientDataIf` mismatch at commit | `commitFailed` | `conditionFailed` |
| Sequence allocation failed (plugin error) | `internal` | data error description |

### Public API / GObject

None. GIR unchanged.

## Migrations

None for stored data. Contract change recorded in `schema/CHANGELOG.md`.

## Testing Philosophy

### Unit

`SbmdResultExecutorTest.cpp`: parse `mergeTransientData` and `setTransientDataIf`; reject malformed, null, or array patches and bad argument types; parse the `supplements` override; loader accepts `sequence` string and array. `JsonMergePatchTest.cpp`: the RFC 7396 Appendix A test vectors plus absent/non-object targets. Transient plugin tests: merge-after-set, merge-after-merge, conditional match/mismatch/absent, sequence increments and survives abort.

### Runtime

`SbmdDriverTest.cpp` with the fake data session: a change committed during a park is visible in `onResponse`, `onError`, and timeout; override replaces inherited; two concurrent transactions declaring the same sequence receive distinct values; a `setTransientDataIf` whose expected value was changed by another transaction fails with `commitFailed`/`conditionFailed`, discards its other writes, and invokes the failure handler; merge results visible to a later step of the same transaction.

### Integration

No new pytest tests here; BARTON-425 adds the camera reproductions (Docker).

## Documentation Plan

### `docs/SBMD.md`

**Audience:** Driver authors

Introduce SBMD Transaction. Update §4.12/§5.1 (fresh supplements per step, inheritance, override, `sequence`), §7.2 (`supplements` option), §7.3 (`mergeTransientData`, `setTransientDataIf`, staged writes, discard on failure). Add an "Interleaving safety" subsection with the D5 table and the limitation.

### `core/deviceDrivers/matter/sbmd/schema/CHANGELOG.md`

**Audience:** Driver authors

Record the additive API.

## Risks / Trade-offs

### Drivers keep doing unconditional read-compute-write

**Risk:** Authors ignore the guidance and lose updates.

**Mitigation:** Documentation; BARTON-425 migrates `camera.sbmd.js` and audits other drivers.

### Conditional failures after device side effects

**Risk:** A `conditionFailed` commit fails a transaction whose device operations already succeeded.

**Mitigation:** The failure handler receives the operation log and can compensate; drivers place conditional writes in steps before device operations where possible.

### Sequence re-allocation on inherited declarations

**Risk:** A deferred step that inherits a `sequence` declaration allocates a new value each step.

**Mitigation:** Documented; recommend declaring sequences only on the initial handler or overriding deferred supplements.

### In-tree merge patch correctness

**Risk:** A hand-written RFC 7396 implementation diverges from the spec.

**Mitigation:** RFC 7396 Appendix A vectors as unit tests.
