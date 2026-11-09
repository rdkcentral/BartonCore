## Why

Jira: **BARTON-424** (epic **BARTON-420** — "SBMD Data Access Layer: safe, consistent driver data access").

**Delivery position:** epic delivery order (decision D13) is BARTON-412 fix → 423 → 421 → 430 → 422 → **424** → 425; this change lands after BARTON-422.

**Depends on:** BARTON-423 (`sbmd-transaction-orchestrator` — `SbmdTransaction`, its operation log, the failure hand-off point, `SbmdWorker`, the `SbmdDataAccess` seam), BARTON-421/430 (DAL core and plugins, supplying `DalSbmdDataAccess` with `AllocateSequence`/`StageConditional`), BARTON-422 (`sbmd-dal-driver-api` — fresh supplements and the driver data API used by the handler). **Prerequisite (outside the epic):** BARTON-412 (`sbmd-deferred-failure-callbacks`) — failure types `transport` and `internal`. Epic architecture: `openspec/changes/sbmd-dal-core/design.md`.

**Archive order:** BARTON-422 (`sbmd-dal-driver-api`), BARTON-423 (`sbmd-transaction-orchestrator`), and this change modify `sbmd-v4-runtime` requirements. They MUST be archived in the order **423 → 422 → 424**, and each later delta MUST restate the full requirement text as left by the earlier one.

An SBMD Transaction can perform several device operations before one fails. After BARTON-423, the runtime discards the transaction's staged data writes when it fails, but device side effects that already succeeded (a user created on a lock, a credential added, an attribute written, a camera stream allocated) stay on the device. Per-hop `onError` handlers see only their own hop: they do not know what earlier steps did, they do not run for non-deferred terminals, aborts, or commit failures, and their data writes are discarded with the rest of the failed transaction. Drivers need one place, called exactly once per failed transaction, that sees everything the transaction did and can compensate.

Motivating case — door lock user provisioning: step 1 `SetUser` succeeds, step 2 `SetCredential` succeeds, step 3 `SetWeekDaySchedule` fails. The lock now holds a user and a PIN with no schedule. The driver must remove them, using the user index and credential from steps 1 and 2.

## What Changes

- **Driver-level failure handler**: an optional `onTransactionFailure` entry in the `SbmdDriver({...})` registration object, declared once per driver with the same shape as other handler entries: `onTransactionFailure: { supplements?: {...}, handler: functionRef }`.
- **Exactly once per failed SBMD Transaction**, for every failure type: device error (`commandFailed`, `readFailed`), `timeout` (per-hop, overall, or missed wait deadline), depth limit and pre-send failures (`internal`, `transport`, from BARTON-412), `aborted` (plugin unregistered, device removed, driver deactivated/unloaded, shutdown), and `commitFailed` (commit failed, including reason `conditionFailed` when a conditional write's expected value no longer matches, epic D11). Not invoked for transactions that commit.
- **Recovery rule (epic D12)**: if a step's `onError` returns `success()` or another non-`error` terminal that the runtime honors, the driver has recovered: the SBMD Transaction continues or succeeds and `onTransactionFailure` does not run. It runs only when the SBMD Transaction terminates in failure.
- **Ordering**: runs after the failing step's `onError` (if any) and after the failed transaction's data session (its DAL Transaction) has been aborted (staged writes discarded).
- **Arguments**:
  - `args.operations` — the ordered operation log: `{ step, kind: "command"|"readAttribute"|"writeAttribute", endpoint, cluster, command|attribute, request, status: "succeeded"|"failed"|"notSent", response?, error? }`.
  - `args.failure` — `{ step, type, message, matterCode? }`, `type` ∈ `timeout`, `commandFailed`, `readFailed`, `transport`, `internal`, `aborted`, `commitFailed`.
  - `args.context` — the driver context of the failed transaction (the `context` supplied on its deferred requests), or `null`.
  - Common fields (`deviceUuid`, `endpointId`, `clusterFeatureMaps`), `args.resource` when the transaction originated from a resource operation, and `args.supplements` fetched freshly for the handler.
- **Compensation as a new SBMD Transaction**: the handler runs as the first step of a new SBMD Transaction (origin `Compensation`) with failure handling disabled. Its ops and terminal (for example a compensating `sendCommand` or a `requestCommand` chain) execute in that transaction; if it fails, the failure is logged and counted, and `onTransactionFailure` is not invoked again (no recursion). Compensation runs on `SbmdWorker`; the original caller is unblocked with the original failure once the compensation transaction has been handed to the worker.
- **Loader, schema, typings**: `SbmdRegistration` gains `onTransactionFailure`; `SbmdLoader` extracts it; `SbmdDriver::VisitHandlers` roots/releases it; `sbmd-spec-schema.json` and `sbmd-script.d.ts` accept it.
- **Docs**: `docs/SBMD.md` gains a "Transaction failure handler" section (registration, args, ordering, guarantees, compensation semantics, door-lock example) and updates §4.1, §5.1, §5.2, §6.2.
- **Metrics**: failure-handler invocations, failures, compensation outcomes per driver and failure type.

### Non-goals

- **Per-step cleanup hooks** (`Sbmd.result().onFailure(...)`, `CleanupRegistry`, LIFO hooks). Dropped by epic decision D2; replaced by this driver-level handler.
- **Automatic compensation or undo** of commands or attribute writes. The runtime never guesses inverse operations.
- **Recursive failure handling.** A failing compensation transaction does not invoke `onTransactionFailure`.
- **Reporting compensation outcome to the original caller.** The caller receives the original failure; compensation outcome is logged and metered.
- **Status-only command continuation** (needed to chain after commands such as `SetUser` that have no response command). See design open questions; the door-lock example depends on it for its forward path only.
- **Migrating drivers** to use the handler (camera stream release, door lock): BARTON-425.
- **Public API changes.** None.

## Capabilities

### New Capabilities

- `sbmd-transaction-failure-handler`: declaration, invocation guarantees, arguments, ordering, and compensation semantics of the driver-level `onTransactionFailure` handler.

### Modified Capabilities

- `sbmd-v4-runtime`:
  - "Registration object extraction" — `onTransactionFailure` is extracted and its handler reference preserved.

## Impact

- **Affected layers:**
  - Drivers — SBMD runtime (C++): `core/deviceDrivers/matter/sbmd/SbmdRegistration.h`, `SbmdDriver.h/.cpp` (`VisitHandlers`), `mquickjs/SbmdLoader.cpp` (extraction), `mquickjs/SbmdHandlerInvoker.cpp/.h` (`BuildTransactionFailureArgs`), `SpecBasedMatterDeviceDriver.cpp/.h` (`OnTransactionFailed` body), `txn/SbmdTransactionOrchestrator.cpp` (compensation origin, failure handling disabled).
  - Drivers — SBMD schema/typings: `core/deviceDrivers/matter/sbmd/schema/sbmd-spec-schema.json`, `schema/CHANGELOG.md`, `scriptCommon/sbmd-script.d.ts`.
  - Docs: `docs/SBMD.md`.
  - Tests: `core/test/src/SbmdTransactionFailureHandlerTest.cpp` (new), `SbmdDriverTest.cpp` (loader), `core/test/CMakeLists.txt`; a test-only door-lock fixture driver under `core/test/resources/` (or the existing SBMD test spec location).
  - **Not affected:** public API (`api/c/public/`), GObject types/signals/properties, GIR, core service, Zigbee/Thread, production `.sbmd.js` drivers (BARTON-425).
- **CMake flags:** `BCORE_MATTER`; unit tests under `BUILD_TESTING`. No new flags.
- **Driver compatibility:** fully additive. Drivers that do not declare `onTransactionFailure` behave exactly as after BARTON-423 (failures are logged by the hand-off hook).
