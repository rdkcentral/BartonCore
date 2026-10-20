## Why

Jira: **BARTON-422** (epic **BARTON-420** — "SBMD Data Access Layer: safe, consistent driver data access").

**Depends on:** BARTON-421 (`openspec/changes/sbmd-dal-core-store/`), which provides the DAL this change exposes to drivers. The full epic architecture lives in `openspec/changes/sbmd-dal-core-store/design.md`; it is not repeated here.

BARTON-411 has two driver-visible root causes that no amount of driver discipline can fix today:

1. **Deferred callbacks receive no fresh data.** `HandleDeferredCommandResponse` and `HandleDeferredCommandError` in `SpecBasedMatterDeviceDriver.cpp` build args with `SbmdHandlerInvoker::BuildCommandResponseArgs` / `BuildDeferredErrorArgs` and never call `FetchSupplements`. The only state a deferred callback sees is the `context` object captured at park time. `camera.sbmd.js` therefore passes `{sessionId, sessions}` through `context` and `handleAllocateForSolicit` writes the stale `ctx.sessions` back.
2. **Transient writes are whole-value only.** `storage.setTransientData(name, value, ttlSecs)` (`ResultOp::SetTransientData`) replaces the entire value. Even with a fresh read, a driver storing a JSON map of sessions must read-modify-write the whole map, so two interleaved operations editing different sessions still clobber each other.

This change routes the driver JS data API through the DAL so drivers can express intent ("give me the current sessions", "set member `3.videoStreamID`") and the DAL applies it against the current view.

## What Changes

- **Fresh supplements in deferred callbacks**: `onResponse` and `onError` callbacks of `requestCommand` and `readAttribute` receive `args.supplements`, read from the DAL at callback time (not park time). By default the callback inherits the originating handler's `supplements` declaration; a new optional `supplements` field in `requestCommand`/`readAttribute` options overrides it.
- **Per-member transient writes**: new non-terminal op `storage.mergeTransientData(name, patch, ttlSecs)` applying an RFC 7396 JSON Merge Patch to the current DAL value at execution time (a `null` member deletes it). Absent or non-object current values are treated as `{}`.
- **Write ops resolved against the current view**: `setTransientData`, `mergeTransientData`, `setPersistentData`, and `updateResource` from any handler (including deferred callbacks) are applied through `DataAccessLayer::Write` against the then-current value.
- **Runtime plumbing**: `ResultOp::MergeTransientData` in `SbmdResultExecutor`; `PendingOperation` stores the effective supplement declaration; `SbmdHandlerInvoker::BuildCommandResponseArgs` / `BuildDeferredErrorArgs` gain a supplements parameter; `sbmd-result.js` and `sbmd-script.d.ts` gain the builder method and option.
- **Schema/contract**: `docs/SBMD.md` §4.12, §5.1, §7.2, §7.3 updated; `core/deviceDrivers/matter/sbmd/schema/CHANGELOG.md` notes the additive contract change.

### Non-goals

- Transactions, read-set validation, conflict merge across a park — BARTON-423. In this change, a whole-value `setTransientData` in a deferred callback still overwrites concurrent changes if the driver writes back a value it derived from stale data; the fix is that drivers now have the tools (fresh supplements, merge writes) to avoid that.
- Cleanup hooks — BARTON-424.
- Migrating `camera.sbmd.js` — BARTON-425.
- Member-level operations on persistent data or resources (resources are scalar strings; persistent data stays whole-value).
- Exposing versions, transaction IDs, or any DAL internals to JS.

## Capabilities

### New Capabilities

- `sbmd-driver-data-api`: Driver-facing data access semantics backed by the DAL — fresh supplements on deferred callbacks, supplement override on deferred terminals, and `storage.mergeTransientData` with RFC 7396 semantics.

### Modified Capabilities

- `sbmd-v4-runtime`: "Deferred operations" now delivers fresh supplements to `onResponse`/`onError`; "Result chain execution" adds `mergeTransientData` to the supported op types.

## Impact

- **Affected layers**: SBMD runtime (`core/deviceDrivers/matter/sbmd/`), SBMD script common library, SBMD documentation. No core service, subsystem, or public API changes.
- **Affected files**: `SpecBasedMatterDeviceDriver.{h,cpp}` (`PendingOperation`, `HandleDeferredCommandResponse`, `HandleDeferredCommandError`, `ExecuteReadAttribute`, `ContinueDeferredChain`), `mquickjs/SbmdHandlerInvoker.{h,cpp}`, `mquickjs/SbmdResultExecutor.{h,cpp}`, `mquickjs/SbmdWireContract.h`, `scriptCommon/sbmd-result.js`, `scriptCommon/sbmd-script.d.ts`, `schema/CHANGELOG.md`, `docs/SBMD.md`; tests in `core/test/src/SbmdHandlerInvokerTest.cpp`, `SbmdResultExecutorTest.cpp`, `SbmdDriverTest.cpp`.
- **CMake flags**: `BCORE_MATTER`. No new flags.
- **Backward compatibility**: Additive. Existing drivers that ignore `args.supplements` in deferred callbacks are unaffected; `context` continues to be delivered unchanged. Drivers that never declared supplements receive no `args.supplements` in callbacks (same as today).
- **Public API / GObject**: None.
