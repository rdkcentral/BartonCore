## Why

Jira: **BARTON-423** (epic **BARTON-420** — "SBMD Data Access Layer: safe, consistent driver data access").

**Depends on:** BARTON-421 (`sbmd-dal-core-store`) and BARTON-422 (`sbmd-dal-driver-api`). **Prerequisite (outside the epic):** BARTON-412 — exactly-once `onError` on every failure path, including pre-send failures (`FailOperation` sites and `CompletePendingOperation(pendingId, false)`). The epic architecture is in `openspec/changes/sbmd-dal-core-store/design.md`.

BARTON-422 gives drivers fresh reads and merge writes, but nothing stops a driver from deriving a whole-value write from data that changed while the operation was parked, and nothing prevents partial writes from a failed operation from being visible (today `ExecuteOps` applies writes immediately, even when the terminal is `error` or the deferred chain later fails). The DAL needs operation-level atomicity and conflict detection without exposing transactions to drivers and without blocking locks (which deadlock on the single Matter thread).

## What Changes

- **Implicit per-operation transactions**: every resource operation (`HandleResourceOp`: read/write/execute), every device-initiated dispatch (`DispatchToHandlers`), and every seed invocation runs inside a DAL transaction created and finished by the runtime. Drivers do not declare transaction scope.
- **Automatic read/write-set tracking**: every DAL read through supplements records `(key, version)` in the read set; every write op is staged in the write set (in order) instead of being applied.
- **Commit on success, discard on failure**: staged writes are committed only when the operation completes successfully (`CompletePendingOperation(id, true)` or a synchronous `success` terminal); discarded on `error`, timeout, failure, or abort. Writes staged inside an `onError` handler form a separate compensation transaction that always commits (it is the driver's failure-path intent).
- **Commit-time validation**: the read set is validated against current versions. Reads of the transaction's own staged writes are served from the write set (read-your-writes).
- **Pre-park conflicts are retried transparently**: if validation fails for an operation that has not yet sent anything to the device, the runtime discards staged writes and re-runs the handler (bounded retries).
- **Post-park conflicts are never retried**: once a device command has been sent, the operation is not replayable. Conflicting keys are resolved by per-key merge (merge-patch writes re-applied to the current value; keys only read but not written are accepted; whole-value writes to a changed key are unmergeable). Unmergeable conflicts fail the operation and invoke the cleanup path (BARTON-424 hooks; before BARTON-424 lands, the driver's `onError` with `type: "conflict"`).
- **Metrics**: commits, aborts, retries, merges, unmergeable conflicts per driver.

### Non-goals

- Any driver-facing transaction API (`begin`/`commit`). Explicitly rejected.
- Blocking locks or per-device serialization (see foundation design D2). A fallback per-resource sequencing queue is an open question.
- Cleanup hook API — BARTON-424.
- Fixing BARTON-412 — prerequisite, fixed separately.
- Transactions spanning multiple devices.

## Capabilities

### New Capabilities

- `sbmd-dal-transactions`: Implicit per-operation optimistic transactions with automatic read/write-set tracking, staged writes, commit-time validation, pre-park transparent retry, post-park per-key merge, and conflict failure.

### Modified Capabilities

- `sbmd-v4-runtime`: "Result chain execution" — data write ops are staged and committed only on operation success.

## Impact

- **Affected layers**: SBMD runtime and DAL only. No core service, subsystem, or public API changes.
- **Affected files**: new `core/deviceDrivers/matter/sbmd/dal/Transaction.{h,cpp}`, `TransactionManager.{h,cpp}`; modified `DataAccessLayer.{h,cpp}`, `SpecBasedMatterDeviceDriver.{h,cpp}` (`HandleResourceOp`, `ExecuteTerminal`, `ExecuteRequestCommand`, `ExecuteReadAttribute`, `ContinueDeferredChain`, `CompletePendingOperation`, `DispatchToHandlers`, `InvokeSeedHandler`, `PendingOperation`), `mquickjs/SbmdHandlerInvoker.cpp` (`ExecuteOps` stages instead of applying); tests in `core/test/src/SbmdDalTransactionTest.cpp`, `SbmdDriverTest.cpp`.
- **CMake flags**: `BCORE_MATTER`. No new flags.
- **Behavior change**: Writes from an operation that ends in `error` are no longer applied (today ops run "even on error terminal"). `log` ops still execute immediately. `setMetadata` is staged like other writes. This is a deliberate semantic change; drivers that relied on partial writes before an error terminal must move that intent to `onError` or cleanup hooks (audit in BARTON-425).
- **Public API / GObject**: None.
