# Design: SBMD DAL optimistic per-operation transactions (BARTON-423, epic BARTON-420)

## Context

Builds on BARTON-421 (store, versions, ingress) and BARTON-422 (fresh deferred supplements, `mergeTransientData`). Foundation architecture, thread model, and open questions: `openspec/changes/sbmd-dal-core-store/design.md`. Requires BARTON-412 (exactly-once `onError` on every failure path) so that every failed operation reaches a single, well-defined abort point.

Today `SbmdHandlerInvoker::ExecuteOps` applies writes immediately for every result, including results whose terminal is `error`. A `PendingOperation` lives in `SpecBasedMatterDeviceDriver::pendingOperations` from `ExecuteRequestCommand` until `CompletePendingOperation(id, success)`. That lifecycle is the natural transaction boundary.

No `docs/dev/architecture.md` exists in the project.

## References

- `openspec/changes/sbmd-dal-core-store/design.md` — D2 (optimistic, no locks), D4 (versions), D5 (store), open questions.
- H. T. Kung, J. T. Robinson, "On Optimistic Methods for Concurrency Control", ACM TODS 6(2), 1981 — read phase / validation / write phase model.
- [RFC 7396](https://www.rfc-editor.org/rfc/rfc7396) — merge semantics used for post-park conflict resolution.

## Goals / Non-Goals

**Goals:**

- Atomic commit of an operation's writes, only on success.
- Automatic conflict detection with zero driver involvement.
- Transparent retry where safe (no device side effects yet); merge or fail where not.

**Non-Goals:**

- Driver-visible transactions; serializable isolation across devices; blocking locks.

## Architecture

```text
HandleResourceOp / DispatchToHandlers / InvokeSeedHandler        (Matter thread)
    |
    | tx = TransactionManager::Begin(device, kind)
    v
FetchSupplements --(DAL Read via tx)--> readSet += (key, version)
handler(args) -> result
ExecuteOps --(stage)--> writeSet += op
    |
    +-- terminal success ------------------------------> Commit(tx)
    +-- terminal error --------------------------------> Abort(tx)
    +-- terminal requestCommand/readAttribute:
          before send: ValidatePrePark(tx)
               conflict -> Abort, retry handler (<= kMaxPreParkRetries)
          tx.sideEffects = true ; send ; park (pending.txId = tx)
          ... other txs commit, reports reconcile, versions bump ...
          onResponse: fresh supplements (reads into same tx), stage writes
          CompletePendingOperation(id, true)  -> Commit(tx): validate
               ok          -> apply writeSet
               conflict    -> per-key merge
                    mergeable   -> apply merged
                    unmergeable -> Abort + failure path (onError type "conflict" / cleanup hooks [424])
          CompletePendingOperation(id, false) -> Abort(tx); onError ops run in compensation tx (always commits)
```

## Decisions

### D1: The runtime owns transaction scope; one transaction per operation

**Decision:** `TransactionManager::Begin` is called by the runtime at the start of each resource operation, dispatch, and seed; the transaction id is stored on `PendingOperation` across parks and re-arms (`ContinueDeferredChain`). Drivers never see it.

```cpp
// core/deviceDrivers/matter/sbmd/dal/Transaction.h
#pragma once

#include "DataKey.h"
#include "VersionedValue.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace barton::sbmd::dal
{
    using TxId = uint64_t;

    enum class TxKind : uint8_t
    {
        ResourceOp,
        Dispatch,
        Seed,
        Compensation, // writes produced by onError / cleanup; always commits
    };

    struct StagedWrite
    {
        enum class Mode : uint8_t
        {
            Replace,   // setTransientData / setPersistentData / updateResource / setMetadata
            MergePatch // mergeTransientData
        };

        DataKey key;
        Mode mode;
        ValueHandle value;          // Replace: new value (nullptr == delete); MergePatch: patch JSON
        std::optional<std::chrono::seconds> ttl;
    };

    struct Transaction
    {
        TxId id = 0;
        std::string deviceUuid;
        TxKind kind = TxKind::ResourceOp;
        bool sideEffects = false; // set before the first device command is sent
        uint32_t preParkRetries = 0;

        std::unordered_map<DataKey, Version, DataKeyHash> readSet; // first-read version per key
        std::vector<StagedWrite> writeSet;                         // in op order
    };

    enum class CommitOutcome : uint8_t
    {
        Committed,
        Merged,
        Conflict,   // unmergeable; nothing applied
        RetryPrePark,
    };

    struct CommitResult
    {
        CommitOutcome outcome;
        std::vector<DataKey> conflictingKeys;
    };
} // namespace barton::sbmd::dal
```

```cpp
// core/deviceDrivers/matter/sbmd/dal/TransactionManager.h
#pragma once

#include "DataAccessLayer.h"
#include "Transaction.h"

namespace barton::sbmd::dal
{
    /** Matter-thread-confined. */
    class TransactionManager
    {
    public:
        static constexpr uint32_t kMaxPreParkRetries = 3;

        explicit TransactionManager(DataAccessLayer &dal) : dal(dal) {}

        TxId Begin(const std::string &deviceUuid, TxKind kind);

        /** Read-your-writes; records first-read version in the read set. */
        VersionedValue Read(TxId tx, const DataKey &key);

        void Stage(TxId tx, StagedWrite write);

        /** Called immediately before sending a device command. */
        CommitResult PrepareSideEffect(TxId tx);

        CommitResult Commit(TxId tx);
        void Abort(TxId tx);

        /** Restart a pre-park transaction for retry: clears sets, keeps id, bumps retry count. */
        bool ResetForRetry(TxId tx);

        bool Exists(TxId tx) const { return active.count(tx) != 0; }

    private:
        std::vector<DataKey> Validate(const Transaction &t);
        bool TryMerge(Transaction &t, const std::vector<DataKey> &conflicts);
        void Apply(Transaction &t);

        DataAccessLayer &dal;
        std::unordered_map<TxId, Transaction> active;
        TxId nextId = 1;
    };
} // namespace barton::sbmd::dal
```

**Alternative considered:** Driver-declared transactions. Rejected — leaks DAL internals into drivers (agreed separation of concerns).

---

### D2: Validation and commit

**Decision:** At commit, for each `(key, v)` in the read set, compare with the current version. Keys whose version is unchanged pass. If none conflict, apply the write set in order via `DataAccessLayer::Write`/`MergeJson`. Because the DAL is Matter-thread-confined, validate+apply is atomic with respect to other transactions without any lock.

```cpp
// core/deviceDrivers/matter/sbmd/dal/TransactionManager.cpp (excerpt)
std::vector<DataKey> TransactionManager::Validate(const Transaction &t)
{
    std::vector<DataKey> conflicts;
    for (const auto &[key, readVersion] : t.readSet)
    {
        const Entry *e = dal.Store().Find(key);
        Version now = e ? e->version : kAbsentVersion;
        if (now != readVersion)
        {
            conflicts.push_back(key);
        }
    }
    return conflicts;
}

CommitResult TransactionManager::Commit(TxId id)
{
    auto it = active.find(id);
    if (it == active.end())
    {
        return {CommitOutcome::Conflict, {}};
    }
    Transaction &t = it->second;

    if (t.kind == TxKind::Compensation)
    {
        Apply(t);
        active.erase(it);
        return {CommitOutcome::Committed, {}};
    }

    auto conflicts = Validate(t);
    if (conflicts.empty())
    {
        Apply(t);
        active.erase(it);
        return {CommitOutcome::Committed, {}};
    }

    if (!t.sideEffects)
    {
        return {CommitOutcome::RetryPrePark, conflicts}; // caller re-runs handler
    }

    if (TryMerge(t, conflicts))
    {
        Apply(t);
        active.erase(it);
        return {CommitOutcome::Merged, conflicts};
    }

    active.erase(it); // discard staged writes
    return {CommitOutcome::Conflict, conflicts};
}
```

**Alternative considered:** Validate the write set only (write-write conflicts). Rejected — misses the BARTON-411 shape where a value is read, used to compute a write, and the read is stale.

---

### D3: Pre-park conflicts retry; post-park conflicts never retry

**Decision:** Immediately before `MatterDevice::SendCommandWithCallbacks` (and before `writeAttribute`/`sendCommand` terminals), the runtime calls `PrepareSideEffect`, which validates the read set. On conflict it returns `RetryPrePark`; the runtime aborts staged writes, calls `ResetForRetry`, re-fetches supplements, and re-invokes the same handler, up to `kMaxPreParkRetries`, after which the operation fails with `type: "conflict"`. On success it sets `sideEffects = true`. A synchronous handler ending in `success` with a conflict is also retried (no side effects). After `sideEffects` is set, a conflict is never retried, because a command has been sent to the device and cannot be replayed.

**Alternative considered:** Retry post-park by replaying the whole chain. Rejected — repeats non-idempotent device commands (e.g., a second `VideoStreamAllocate`).

---

### D4: Post-park per-key merge

**Decision:** For each conflicting key, the transaction is mergeable if every conflicting key satisfies one of:

1. It is only in the read set (not written by this transaction) — accept; the driver's decision stands (fresh supplements at callback time already showed the latest value to the final hop).
2. All staged writes to it are `MergePatch` — re-apply the patches to the current value at commit.
3. The staged `Replace` value equals the current value — no-op.

Any `Replace` write to a key whose version changed since this transaction read it is **unmergeable**. A `Replace` write to a key never read by the transaction is a blind write and is not a conflict (last-writer-wins, matching today).

```cpp
// core/deviceDrivers/matter/sbmd/dal/TransactionManager.cpp (excerpt)
bool TransactionManager::TryMerge(Transaction &t, const std::vector<DataKey> &conflicts)
{
    for (const DataKey &k : conflicts)
    {
        for (const StagedWrite &w : t.writeSet)
        {
            if (!(w.key == k) || w.mode == StagedWrite::Mode::MergePatch)
            {
                continue;
            }
            VersionedValue cur = dal.Store().Get(k, std::chrono::steady_clock::now());
            bool same = (cur.value == nullptr && w.value == nullptr) ||
                        (cur.value && w.value && cur.value->text == w.value->text);
            if (!same)
            {
                return false;
            }
        }
    }
    return true;
}
```

**Alternative considered:** Field-level three-way merge of whole-value JSON writes (diff park-time base vs staged, apply to current). Deferred — it would rescue unmigrated drivers but needs the base value retained and has ambiguous semantics for deletes; noted as a possible follow-up.

---

### D5: Failure and compensation

**Decision:** `CompletePendingOperation(id, false)` and any synchronous `error` terminal call `Abort`. Ops returned by `onError` run in a new `Compensation` transaction that commits unconditionally (merge-patch ops still apply against current values). An unmergeable conflict at commit is a failure: the runtime invokes the driver's failure path exactly once — `onError` with `args.error.type = "conflict"` (relying on BARTON-412's exactly-once guarantee) — and, once BARTON-424 lands, registered cleanup hooks. The parked Barton operation fails.

Seed and device-initiated dispatch transactions have no side effects; on conflict they retry up to `kMaxPreParkRetries`, then commit the last attempt with a warning (device reports must not be dropped).

## Thread Safety

All transaction state is Matter-thread-confined (foundation design). Validation and apply occur within one Matter-loop turn, so no other transaction can interleave; no locks are needed. The JS mutex is never held during `Commit`. Cross-thread writes arrive via ingress and are ordered relative to commits by the Matter loop.

## Interfaces

### Driver JS

No new API. Semantics change: writes are applied only on operation success; `onError` gains `type: "conflict"`.

### Public API / GObject

None. GIR unchanged; backward compatible.

## Migrations

Behavior change for drivers that rely on writes preceding an `error` terminal being applied. BARTON-425 audits all drivers in `core/deviceDrivers/matter/sbmd/specs/`.

## Testing Philosophy

### Transaction unit tests

`SbmdDalTransactionTest.cpp`: read-your-writes, first-read version recorded, commit applies in order, abort discards, validation detects changed/deleted/created keys, blind writes do not conflict, compensation always commits, merge-patch rebase, unmergeable `Replace`, pre-park retry bound.

### Runtime tests

`SbmdDriverTest.cpp` with a fake device: (a) conflict before send → handler re-invoked, command sent once; (b) conflict after send with merge writes → merged commit, single command; (c) conflict after send with whole-value write → `onError` invoked once with `conflict`, staged writes discarded, Barton op fails; (d) `error` terminal discards writes but executes `log`.

### Integration

BARTON-425 provides Docker-based pytest reproductions.

## Documentation Plan

### `docs/SBMD.md`

**Audience:** Driver authors

New section "Consistency guarantees": writes commit only on success; automatic conflict detection; retry before side effects; merge vs conflict after; prefer `mergeTransientData`; `conflict` error type. Update §5 "Operations execute even on error terminal" language.

## Risks / Trade-offs

### Semantic change on error terminals

**Risk:** Drivers that write diagnostics before `error()` lose them.

**Mitigation:** `log` still executes; BARTON-425 audit; diagnostic writes belong in `onError`.

### Retry storms

**Risk:** Hot keys cause repeated pre-park retries.

**Mitigation:** Bounded retries; metrics; the sequencing-queue fallback remains an open question.

### Unmigrated drivers fail more

**Risk:** Whole-value writes after a park now fail on conflict instead of silently clobbering.

**Mitigation:** Intentional (lost updates become visible failures); BARTON-425 migrates `camera.sbmd.js`.
