## Context

SBMD mapper scripts (read, write, execute, event, seedFrom, commandResponse) each return a JSON object from a JavaScript runtime. Barton's C++ layer must interpret these objects to decide what action to take: update a Barton resource, invoke a Matter command, write a Matter attribute, or do nothing (suppress).

Currently, two parallel engine implementations — `quickjs/SbmdScriptImpl.cpp` and `mquickjs/SbmdScriptImpl.cpp` — each contain their own field extraction and validation logic using engine-specific JS C APIs (`JS_GetPropertyStr`, `JS_ToUint32`, etc.). The observable outcomes a result can express (error, suppress, or operation) have no shared C++ type and no formal JSON contract. Script authors must infer the contract from source code comments, and any change to validation rules requires updating both engine files.

The `SbmdScript` virtual interface currently expresses results via `bool` return + output parameters (`std::string& outValue`, `ScriptWriteResult& result`), mixing error and suppress signals into the same `bool false` return.

## Goals / Non-Goals

**Goals:**
- Introduce a `ScriptResult` C++ class as the single authoritative representation of a script result
- Centralize all JSON parsing and validation in `ScriptResult::FromJsonValue()`, shared by both engine implementations
- Replace `bool + out-param` returns on the `SbmdScript` virtual interface with `ScriptResult` returns
- Formalize the script JSON schema (rename `output` → `value`, introduce `error` key) and bump schema version to 3.0
- Add `Sbmd.Response.value()` and `Sbmd.Response.error()` JavaScript helpers for symmetry

**Non-Goals:**
- Changes to SBMD YAML schema structure, device type mappings, alias definitions, or reporting config
- Changes to the `invoke` or `write` JSON shapes returned by write/execute scripts
- Scriptlet feedback (telling scripts what Barton did with their result) — future work
- New operation types beyond the three this change establishes (ResourceUpdate, Invoke, and Write)
- Public GObject API changes — `ScriptResult` is internal to the Matter driver layer

## Decisions

### D1: ScriptResult models observable outcomes via two optional fields

**Decision:** `ScriptResult` holds `optional<string> error` and `optional<variant<ResourceUpdate, ScriptWriteResult>> operation`. No-op is not a named state; it is derived as `!error && !operation`.

**Rationale:** Skip-resource-update is not independently set — it follows from the absence of both error and operation. A dedicated `SkipsResourceUpdate()` convenience method exposes this without adding a third field that could be independently set (creating an invalid state: error=set AND no-op=true).

**Alternative considered:** Three separate named states (Error, NoOp, Operation) as an enum. Rejected because it would require callers to handle a four-state combination matrix (the enum plus whether the operation payload is present).

---

### D2: Operation is `std::variant<ResourceUpdate, ScriptWriteResult>`

**Decision:** The operation payload is a C++17 `std::variant` with two arms: `ResourceUpdate { string value }` (for read/event/seedFrom/commandResponse mappers) and the existing `ScriptWriteResult` (for write/execute mappers).

**Rationale:** This gives `ScriptResult` a self-describing, type-safe payload. Adding future operation types (e.g., `EmitBartonEvent`) means adding a new variant arm — extensible without structural redesign. The existing `ScriptWriteResult` struct is reused as-is to minimize churn at `MatterDevice.cpp` call sites.

**Alternative considered:** Separate `value: optional<string>` and `operation: optional<ScriptWriteResult>` as top-level fields. Rejected because Value and Operation would be implicitly coupled (the type of Value depends on which Operation is set), with no way to express or enforce this coupling in the type system.

**Alternative considered:** Splitting `ScriptWriteResult` into `Invoke` and `Write` variant arms. Rejected as unnecessary — `ScriptWriteResult` already has its own `OperationType::{Invoke, Write}` discriminator; the `variant` handles the higher-level "is this a Matter op or a resource update" discrimination cleanly.

---

### D3: `OperationType::None` removed from `ScriptWriteResult`

**Decision:** Remove `OperationType::None` from `ScriptWriteResult`. The "no operation" state is represented by the outer `optional<variant>` being absent.

**Rationale:** `None` was previously used to represent "no result yet" or "unset". The new model makes this redundant and eliminates a nominally valid but semantically illegal state (`ScriptWriteResult` with `type=None` inside an operation variant).

---

### D4: Shared parsing via `ScriptResult::FromJsonValue(const Json::Value&)`

**Decision:** Both engine implementations extract the script result's JS object into a `Json::Value` (JsonCpp) and pass it to `ScriptResult::FromJsonValue()`. All field validation, type checking, base64 TLV decoding, and struct population live in this single method.

**Rationale:** JsonCpp (`<json/json.h>`) is already used in `quickjs/SbmdScriptImpl.cpp` for building input arg JSON and is linked in all Matter-enabled builds. No new dependency. A `Json::Value`-based intermediate avoids the need for `JS_JSONStringify` (whose availability in the mquickjs engine requires verification), since both engines already use `JS_GetPropertyStr` to extract individual fields into C++ types that map directly to `Json::Value` construction.

**Alternative considered:** Serialize the script result to a JSON string inside the engine, then call `ScriptResult::FromJson(string)`. Requires `JS_JSONStringify` on both engines. Rejected pending confirmation that mquickjs exposes this API.

**Alternative considered:** A builder/fluent API (`ScriptResult::Builder`) populated by engines. More decoupled but adds indirection without clear benefit over the `Json::Value` intermediate.

---

### D5: Ambiguous JSON is rejected as a malformed error

**Decision:** If `ScriptResult::FromJsonValue()` sees more than one of `value`, `invoke`, `write`, or `error` present simultaneously, it returns an error `ScriptResult` with a descriptive message rather than applying a priority ordering.

**Rationale:** Silent priority masking hides script author bugs. An explicit error surfaces the problem immediately during development and testing.

---

### D6: JS exceptions produce synthesized error ScriptResults

**Decision:** Engine implementations catch JS runtime exceptions (from `ExecuteScript`) and convert them to error `ScriptResult` objects before returning. Callers above `SbmdScript` never see raw JS exceptions; they always receive a `ScriptResult`.

**Rationale:** Unifies the two error paths (intentional `return { error: "..." }` vs. unhandled JS throw) at the `ScriptResult` boundary. Callers have one error check, not two.

---

### D7: `SbmdScript` virtual interface returns `ScriptResult` by value

**Decision:** All five mapper methods on `SbmdScript` are changed to return `ScriptResult` by value:

```
MapAttributeRead(...)         → ScriptResult
MapCommandExecuteResponse(...)→ ScriptResult
MapWrite(...)                 → ScriptResult
MapExecute(...)               → ScriptResult
MapEvent(...)                 → ScriptResult
```

**Rationale:** `ScriptResult` captures all three outcomes (error, suppress, operation) in a single return value, eliminating the current convention where `bool false` conflates error and suppress, and `bool true` with an empty `outValue` means suppress. The out-parameter pattern is replaced by typed accessors. `ScriptResult` is move-only (due to `ScopedMemoryBuffer` in `ScriptWriteResult`) — C++17 move semantics make return-by-value efficient.

---

### D8: JSON schema 2.0 → 3.0, `output` renamed to `value`

**Decision:** Bump `schemaVersion` to `"3.0"` across all `.sbmd` files. Rename the `output` key to `value` in all script return objects and all `.d.ts` interface definitions. No compatibility shim; all `.sbmd` files are in-repo and updated atomically.

**Rationale:** The rename aligns the JSON schema with the C++ `ResourceUpdate::value` field and eliminates the semantic ambiguity of calling the result an "output" when it's conceptually the resource's new value. A clean version break avoids a dual-key parser that would mask author errors.

## Architecture

```
                        .sbmd spec file
                        (schemaVersion 3.0)
                               │
                        SBMD Parser / Driver Setup
                               │
                   ┌───────────┴───────────┐
                   │                       │
          quickjs engine           mquickjs engine
          (SbmdScriptImpl)        (SbmdScriptImpl)
                   │                       │
          1. Run script            1. Run script
          2. Catch JS exception    2. Catch JS exception
          3. Extract fields        3. Extract fields
             → Json::Value            → Json::Value
                   │                       │
                   └───────────┬───────────┘
                               │
                  ScriptResult::FromJsonValue()
                    (shared, engine-agnostic)
                               │
                  ┌────────────┴────────────┐
                  │                         │
            error (optional)          operation (optional)
            ─────────────────          ──────────────────
            if set → IsError()         if set → HasOperation()
                                       if absent (and !error)
                                         → SkipsResourceUpdate()
                                       ┌─────────────────┐
                                       │                 │
                                 ResourceUpdate    ScriptWriteResult
                                 {value: string}   {Invoke | Write}
                               │
                    SbmdScript interface
                    (returns ScriptResult)
                               │
                    MatterDevice.cpp callers
                    (use ScriptResult accessors)
```

## Risks / Trade-offs

- **[Risk] mquickjs Json::Value extraction may not be straightforward** → A spike is required before the engine-thinning task. The mquickjs engine already uses `JS_GetPropertyStr` for output field access; building a `Json::Value` from those same calls is likely feasible but unverified. If blocked, fall back to JS-level `JSON.stringify` evaluation.

- **[Risk] MatterDevice.cpp call site churn** → Every mapper call in `MatterDevice.cpp` changes signature (no more out params). This is mechanical but broad. Mitigation: do it as one focused commit after `ScriptResult` is fully tested.

- **[Trade-off] ScriptResult is move-only** → `ScopedMemoryBuffer` in `ScriptWriteResult` makes `ScriptResult` non-copyable. Any code that needs to copy a result (e.g., caching) must be reconsidered. Current usage does not require copying.

- **[Trade-off] `ScriptResult::FromJsonValue()` takes a `Json::Value` not a raw JS value** → The engine-specific extraction step (JS API → `Json::Value`) is still duplicated across the two engine files. This is unavoidable given the two different JS C APIs, but the business logic (what the fields mean, what's valid) is no longer duplicated.

## Migration Plan

1. Introduce `ScriptResult.h/.cpp` with no callers (builds, no behavior change)
2. Add `ScriptResultTest.cpp` unit tests for `ScriptResult::FromJsonValue()` — validates the class before the interface is touched; no JS engine required
3. Update `SbmdScript` virtual interface, both engine impls (using `ScriptResult::FromJsonValue()`), and `MatterDevice.cpp` call sites — tasks 4–7.5 must be applied as a single compilation unit; no intermediate state compiles without all of these
4. Remove `ScriptWriteResult` `OperationType::None` — safe only after all engine and call-site references have been removed in step 3
5. Update JSON schema: rename `output` → `value` in all `.sbmd` files + `.d.ts` types + `sbmd-utils.js` helpers + `SbmdScriptTest.cpp` fixtures

Rollback: revert the `SbmdScript` interface change — `ScriptResult` can be removed without affecting the public API layer since it is entirely internal to the Matter driver subsystem.

## Open Questions

- **OQ1 (spike required):** Can the mquickjs engine build a `Json::Value` from a script result's JSValue using its existing field-extraction API? If not, is JS-level `JSON.stringify` evaluation available? This must be resolved before engine-thinning tasks begin.
- **OQ2:** Should `ScriptResult::FromJsonValue()` log a warning-level message on suppress (no keys present) to help authors debug accidental empty returns? Or is that too noisy? Tentative: log at debug level only.
