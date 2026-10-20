## Why

Jira: **BARTON-424** (epic **BARTON-420** — "SBMD Data Access Layer: safe, consistent driver data access").

**Depends on:** BARTON-421 (`sbmd-dal-core-store`), BARTON-422 (`sbmd-dal-driver-api`), BARTON-423 (`sbmd-dal-optimistic-transactions`). **Prerequisite (outside the epic):** BARTON-412 (exactly-once `onError` on all failure paths). The epic architecture is in `openspec/changes/sbmd-dal-core-store/design.md`.

BARTON-423 makes DAL writes atomic, but device side effects are not: once `VideoStreamAllocate` succeeds on a camera, a later failure (SolicitOffer error, timeout, unmergeable conflict) leaves an allocated stream on the device. Today drivers must remember to undo this in every hop's `onError` (`handleVideoStreamAllocateError`, `handleSolicitOfferError`, ...), which is repetitive and was missed on pre-send failure paths (BARTON-412's camera `streaming` session leak). Drivers need an ergonomic way to say "if this operation fails from here on, do Z", with the runtime guaranteeing Z runs exactly once.

## What Changes

- **Driver API**: new non-terminal result-builder op `Sbmd.result().onFailure(handler, context)` registering a cleanup (compensation) hook on the current operation. Typically returned from the `onResponse` that observed a non-reversible side effect (e.g., after `VideoStreamAllocate` succeeds, register a hook that sends `VideoStreamDeallocate`).
- **Cleanup handler contract**: `handler(args)` receives `args.handlerContext` (the registered `context`), `args.failure` (`{ type: "error" | "timeout" | "conflict" | "aborted", message }`), and fresh `args.supplements` (inheriting the operation's declaration). It returns a result whose data ops commit in a compensation transaction and whose terminal is `success`, `error` (logged), or `sendCommand` (fire-and-forget device command; no parking).
- **Runtime guarantee**: registered hooks run **exactly once** when the operation fails for any reason — `error` terminal, deferred error, per-hop or overall timeout, unmergeable conflict, max deferral depth, pre-send failure, device removal, or driver deactivation while parked. Hooks run after the failing hop's `onError` (if any), in LIFO registration order. On success, hooks are discarded without running.
- **Runtime plumbing**: `CleanupRegistry` in the DAL keyed by transaction id; `ResultOp::OnFailure` parsing; invocation from the single abort point `CompletePendingOperation(id, false)` / synchronous failure path established by BARTON-412 and BARTON-423.

### Non-goals

- Automatic inference of compensations (the driver states intent).
- Retrying failed cleanup hooks or parking within a hook (`requestCommand`/`readAttribute` are rejected in hooks).
- Persisting hooks across service restart (in-flight operations do not survive restart).
- Fixing BARTON-412 itself.
- Migrating drivers — BARTON-425.

## Capabilities

### New Capabilities

- `sbmd-cleanup-hooks`: Driver-registered compensation hooks with exactly-once invocation on any operation failure, LIFO ordering, fresh supplements, and compensation-transaction writes.

### Modified Capabilities

- `sbmd-v4-runtime`: "Result chain execution" adds the `onFailure` op type.

## Impact

- **Affected layers**: SBMD runtime, DAL, SBMD script common library, docs. No core service, subsystem, or public API changes.
- **Affected files**: new `core/deviceDrivers/matter/sbmd/dal/CleanupRegistry.{h,cpp}`; modified `SpecBasedMatterDeviceDriver.{h,cpp}` (`CompletePendingOperation`, `ExecuteTerminal`, `ReleasePendingHandlers`, device removal/deactivation), `mquickjs/SbmdResultExecutor.{h,cpp}`, `mquickjs/SbmdHandlerInvoker.{h,cpp}` (`BuildCleanupArgs`), `mquickjs/SbmdWireContract.h`, `scriptCommon/sbmd-result.js`, `scriptCommon/sbmd-script.d.ts`, `docs/SBMD.md`, `schema/CHANGELOG.md`; tests `core/test/src/SbmdDalCleanupTest.cpp`, `SbmdDriverTest.cpp`.
- **CMake flags**: `BCORE_MATTER`. No new flags.
- **Backward compatibility**: Additive. Drivers that do not use `onFailure` are unaffected.
- **Public API / GObject**: None.
