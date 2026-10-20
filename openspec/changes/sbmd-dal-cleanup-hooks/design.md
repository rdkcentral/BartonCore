# Design: SBMD DAL driver cleanup/compensation hooks (BARTON-424, epic BARTON-420)

## Context

Builds on BARTON-421/422/423; foundation architecture in `openspec/changes/sbmd-dal-core-store/design.md`. BARTON-412 guarantees that every failed operation passes through one abort point (`CompletePendingOperation(id, false)` or the synchronous failure path in `ExecuteTerminal`) and invokes the current hop's `onError` exactly once. BARTON-423 attaches a `TxId` to each operation and runs `onError` ops in a compensation transaction.

Today, compensations are hand-written in each hop's `onError` in drivers such as `camera.sbmd.js` (`handleVideoStreamAllocateError`, `handleSolicitOfferError`, `handleProvideOfferError`). A hook registered once, at the point a side effect becomes real, removes that duplication.

No `docs/dev/architecture.md` exists in the project.

## References

- `openspec/changes/sbmd-dal-core-store/design.md` — architecture, thread model, open questions (driver lifecycle / BARTON-419).
- `openspec/changes/sbmd-dal-optimistic-transactions/design.md` — D5 (failure and compensation transactions).
- H. Garcia-Molina, K. Salem, "Sagas", SIGMOD 1987 — compensating actions for non-reversible steps, LIFO compensation order.

## Goals / Non-Goals

**Goals:**

- One-line driver registration of a compensation at the point of side effect.
- Exactly-once invocation on every failure path; never on success.

**Non-Goals:**

- Retrying compensations; parking inside compensations; durable hooks.

## Architecture

```text
onResponse(VideoStreamAllocate ok)
  returns result.onFailure(deallocateStream, {videoStreamID})   --> CleanupRegistry[tx].push(hook)
                .device.requestCommand(SolicitOffer, ...)
...
failure (error | timeout | conflict | depth | pre-send | device removed | driver deactivated)
  -> single abort point (BARTON-412/423)
       1. current hop onError (if any)        -> compensation tx commit
       2. CleanupRegistry::RunAll(tx) LIFO     -> each hook: fresh supplements, compensation tx commit,
                                                  optional fire-and-forget sendCommand
       3. TransactionManager::Abort(tx); CleanupRegistry::Clear(tx)
success
  -> Commit(tx); CleanupRegistry::Clear(tx) (hooks released under JS mutex, never run)
```

## Decisions

### D1: `onFailure(handler, context)` is a non-terminal op on the current operation

**Decision:** Registration is a result-builder op so it composes with existing chains and is naturally tied to "this operation". The hook is attached to the operation's transaction at op-execution time; ops in a result whose terminal is `error` still register (the hook then runs immediately as part of that failure), because the side effect may already have happened in an earlier hop.

```js
// core/deviceDrivers/matter/sbmd/scriptCommon/sbmd-result.js (addition)
onFailure: function (handler, context) {
    if (typeof handler !== 'function') {
        throw new TypeError('onFailure(handler: function, context?: any)');
    }
    self._ops.push({op: 'onFailure', handler: handler, context: context});
    return self;
},
```

```cpp
// core/deviceDrivers/matter/sbmd/mquickjs/SbmdResultExecutor.h (addition)
struct OnFailure
{
    SafeJSValue handler;
    SafeJSValue context;
};
using Data = std::variant<UpdateResource, SetMetadata, SetPersistentData, SetTransientData,
                          MergeTransientData, OnFailure, Log>;
```

**Alternative considered:** An `onFailure` option on `requestCommand`. Rejected — couples registration to a terminal; the right moment is when the previous side effect is confirmed, which is in the response handler.

**Alternative considered:** Rely on per-hop `onError`. Rejected — that is the status quo and is what leaks.

---

### D2: `CleanupRegistry` keyed by transaction, exactly-once by construction

**Decision:** Hooks are stored per `TxId` in a vector; `RunAll` moves the vector out before invoking anything, so re-entrant failure paths find nothing to run. `Clear` on success releases JS references under the JS mutex.

```cpp
// core/deviceDrivers/matter/sbmd/dal/CleanupRegistry.h
#pragma once

#include "Transaction.h"
#include "SafeJSValue.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace barton::sbmd::dal
{
    enum class FailureType : uint8_t
    {
        Error,
        Timeout,
        Conflict,
        Aborted, // device removed, driver deactivated, depth exceeded, pre-send failure
    };

    struct CleanupHook
    {
        SafeJSValue handler;
        SafeJSValue context;
    };

    /** Matter-thread-confined. SafeJSValue members are created/destroyed under the JS mutex. */
    class CleanupRegistry
    {
    public:
        void Register(TxId tx, CleanupHook hook);

        /** Remove and return hooks in LIFO order; subsequent calls for the same tx return empty. */
        std::vector<CleanupHook> Take(TxId tx);

        /** Success path: drop hooks without running. Caller holds the JS mutex. */
        void Clear(TxId tx);

        size_t Pending(TxId tx) const;

    private:
        std::unordered_map<TxId, std::vector<CleanupHook>> hooks;
    };
} // namespace barton::sbmd::dal
```

```cpp
// core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.cpp (new helper)
void SpecBasedMatterDeviceDriver::RunCleanupHooks(PendingOperation &pending,
                                                  dal::FailureType type,
                                                  const std::string &message)
{
    std::vector<dal::CleanupHook> hooks;
    {
        auto lock = MQuickJsRuntime::Instance().AcquireMutex();
        hooks = Cleanup().Take(pending.txId); // reversed (LIFO)
    }

    for (auto &hook : hooks)
    {
        dal::TxId comp = Transactions().Begin(pending.handlerContext.deviceUuid, dal::TxKind::Compensation);
        FetchedSupplements fresh = FetchFreshSupplements(pending); // BARTON-422 path, reads via comp tx

        std::optional<ParsedResult> result;
        ScopedResultRelease release {result};
        {
            auto lock = MQuickJsRuntime::Instance().AcquireMutex();
            auto *ctx = MQuickJsRuntime::Instance().GetSharedContext();
            SafeJSValue args = SbmdHandlerInvoker::BuildCleanupArgs(
                ctx, pending.handlerContext, ToString(type), message.c_str(), hook.context.Get(), &fresh);
            result = SbmdHandlerInvoker::InvokeHandler(ctx, hook.handler.Get(), args, &pending.operationCtx);
            hook = {}; // release references under the mutex
        }

        if (result.has_value())
        {
            SbmdHandlerInvoker::ExecuteOps(pending.handlerContext, result->ops, StageInto(comp));
            ExecuteCleanupTerminal(pending, *result); // success | error(logged) | sendCommand (fire-and-forget)
        }
        else
        {
            icError("Cleanup hook threw for operation %" PRIu64, pending.id);
            metrics.RecordCleanupFailure(pending.operationCtx.driverStem.c_str());
        }

        Transactions().Commit(comp);
    }
}
```

A hook that throws or returns `error` is logged and counted; remaining hooks still run. `requestCommand`/`readAttribute` terminals in a hook are rejected (logged) because the operation is already completing.

**Alternative considered:** FIFO order. Rejected — LIFO matches saga semantics (undo most recent side effect first).

---

### D3: Every failure path funnels to one invocation site

**Decision:** `CompletePendingOperation(id, false)` calls the hop's `onError` (BARTON-412), then `RunCleanupHooks`, then `TransactionManager::Abort`. Synchronous failures in `ExecuteTerminal` (handler `error`, pre-send `FailOperation`) do the same with a stack-local operation context. Additional abort sources call the same path with `FailureType::Aborted`: device removal (iterate `pendingOperations` for that device), driver deactivation/unload (all pending operations; relevant to BARTON-419 runtime loading), and `MAX_DEFERRAL_DEPTH` exceeded. An unmergeable conflict at commit (BARTON-423) uses `FailureType::Conflict`.

## Thread Safety

Registry and invocation are Matter-thread-confined. `SafeJSValue` hook references are created in `SbmdResultExecutor` under the JS mutex and moved into the registry under the mutex (same pattern as `ExecuteRequestCommand` moving `onResponse`/`onError` into `PendingOperation`); they are released under the mutex in `Take`/`Clear`. DAL reads/writes for the compensation transaction happen outside the JS mutex. Device-removal notifications from other threads arrive via the DAL ingress (`DeviceRemoved`) and are processed on the Matter thread before keys are erased, so hooks still see the device's data.

## Interfaces

### Driver JS

| API | Description |
| --- | --- |
| `Sbmd.result().onFailure(handler, context?)` | Register a cleanup hook for the current operation. |
| Hook `args.handlerContext` | The registered `context`. |
| Hook `args.failure` | `{ type: "error" \| "timeout" \| "conflict" \| "aborted", message }` |
| Hook `args.supplements` | Fresh, inheriting the operation's declaration. |
| Hook terminals | `success`, `error` (logged), `sendCommand` (fire-and-forget). |

### Public API / GObject

None. GIR unchanged; backward compatible.

## Migrations

None. Additive; recorded in `schema/CHANGELOG.md`.

## Testing Philosophy

### Registry

`SbmdDalCleanupTest.cpp`: LIFO order, `Take` is idempotent (second call empty), `Clear` drops without running.

### Runtime

`SbmdDriverTest.cpp` with a fake device: hook runs once for each failure type (error terminal, deferred error, per-hop timeout, overall timeout, conflict, depth exceeded, pre-send failure, device removed, driver deactivated); never on success; runs after `onError`; hook writes commit even though the operation's writes are discarded; hook `sendCommand` is sent once without parking; a throwing hook does not prevent later hooks.

### Integration

BARTON-425 covers camera stream deallocation end-to-end in Docker.

## Documentation Plan

### `docs/SBMD.md`

**Audience:** Driver authors

New §7.x "Cleanup hooks — `onFailure`": when to register, args, allowed terminals, ordering, exactly-once guarantee, relationship to `onError`, camera stream deallocation example.

## Risks / Trade-offs

### Device unreachable during cleanup

**Risk:** A fire-and-forget `sendCommand` (e.g., deallocate) fails because the device is offline, leaving the device-side resource allocated.

**Mitigation:** Logged and counted; device-side resources typically expire or are reconciled on reconnect; durable cleanup is out of scope.

### Double compensation with onError

**Risk:** Drivers both register a hook and keep the same undo in `onError`.

**Mitigation:** Documentation; BARTON-425 removes duplicated undo logic from `camera.sbmd.js`.
