## Why

Jira: **BARTON-423** (epic **BARTON-420** — "SBMD Data Access Layer: safe, consistent driver data access").

**Delivery position:** first change in the epic. Epic delivery order (decision D13): BARTON-412 fix → **BARTON-423** → 421 → 430 → 422 → 424 → 425.

**Depends on:** only the BARTON-412 fix (`sbmd-deferred-failure-callbacks`, branch `BARTON-412-deferred-failure-callbacks`) — exactly-once `onError` on every failure path. Its `FailPendingOperation` choke point, hop generations, and exactly-once delivery are absorbed into `SbmdTransaction`. This change does **not** depend on BARTON-421 (`sbmd-dal-core`) or BARTON-430 (`sbmd-dal-source-plugins`): it introduces an internal data-access seam, `SbmdDataAccess`, with a `LegacySbmdDataAccess` implementation over today's data paths. BARTON-421 + 430 later supply `DalSbmdDataAccess` and delete `transientStore`. The epic-wide architecture, glossary, and decision record live in `openspec/changes/sbmd-dal-core/design.md`.

**Archive order:** BARTON-422 (`sbmd-dal-driver-api`), BARTON-423 (this change), and BARTON-424 (`sbmd-transaction-failure-handler`) modify the same `sbmd-v4-runtime` requirements. They MUST be archived in the order **423 → 422 → 424**, and each later delta MUST restate the full requirement text as left by the earlier one.

Today an SBMD request is not a first-class object. A device-service API call enters `MatterDeviceDriver::ConnectAndExecute` on the calling thread, which blocks on a list of promises while the work lambda runs on the Matter thread. On the Matter thread the runtime fetches supplements (calling into device service), runs the JS handler, applies data writes immediately (`SbmdHandlerInvoker::ExecuteOps` → `updateResource`, `deviceServiceSetMetadata`, `transientStore`), sends the device operation, and — when the handler parked on `requestCommand`/`readAttribute` — later runs `onResponse`/`onError` inline on the Matter thread from a loosely linked `PendingOperation` record keyed by a numeric id. The consequences:

- There is no place to stage writes and commit or abort them as a unit; writes from failed operations are applied.
- There is no record of which device operations a request already performed, so a driver cannot compensate after a partial failure (BARTON-424 needs this log).
- Supplements are captured once and passed through closures and `context`, so deferred callbacks act on stale data (BARTON-411).
- The Matter thread calls into device service synchronously from inside handlers; once data access moves to the main loop (BARTON-421) this would make the Matter thread wait on the main loop, which can deadlock against main-loop code that already waits on the Matter thread.
- Device-initiated handlers (attribute reports, events, unsolicited commands) run inline on the Matter thread with no caller to own a transaction.

## What Changes

- **`SbmdTransaction`** (new, internal C++): the code object for an **SBMD Transaction** — the driver-level execution of one request from the initial handler invocation through every parked device operation and deferred callback to the terminal result. It replaces `PendingOperation` and the `pendingOperations` map. It owns exactly one data session (`SbmdDataSession`; after BARTON-421 a session wraps exactly one `DalTransaction`), the **operation log** (every `command`, `readAttribute`, and `writeAttribute` the transaction requested, with request, status, response or error), the current hop, deferral depth, deadline, and the exactly-once completion latch absorbed from BARTON-412.
- **`SbmdDataAccess` seam** (new, internal): batched supplement fetch including sequence-allocation entries (epic D11), stage, stage-conditional (compare-and-set at commit), commit, abort, attribute-write overlay hook, and abort listener. **`LegacySbmdDataAccess`** wraps today's fetchers, setters, and `transientStore`: staged writes are held in the `SbmdTransaction` and applied at commit; `transientStore` and attribute-cache access are scheduled onto the Matter thread by the orchestrator (legal because only the orchestrator waits); device-service calls run on the orchestrator. Sequence allocation and conditional writes are implemented for the Transient namespace only; no JS API exposes them until BARTON-422.
- **`SbmdTransactionOrchestrator`** (new): runs an SBMD Transaction on the thread that today blocks in `ConnectAndExecute`. Per step: (1) one batched supplement fetch through the seam, (2) run the JS handler on the Matter thread with those supplements, (3) stage returned data writes, (4) send any device operation on the Matter thread and receive its outcome back on the orchestrator, (5) repeat with fresh supplements for the next step, (6) commit on success, or abort and hand off to the failure handler on failure.
- **Deferred callbacks are no longer run inline on the Matter thread.** Device responses, device errors, and timeouts are posted back to the orchestrator, which runs `onResponse`/`onError` as the next step.
- **Recovery rule (epic D12):** when `onError` returns a non-`error` terminal that BARTON-412 honors, the transaction has recovered — it continues or succeeds, and the failure hand-off (BARTON-424's `onTransactionFailure`) does not run.
- **`SbmdWorker`** (new): a dedicated thread that acts as orchestrator for SBMD work with no waiting caller — attribute-report handlers, event handlers, and unsolicited command handlers.
- **Wait-direction rule:** orchestrator → main loop → Matter thread waits are allowed; the Matter thread never waits on the main loop or an orchestrator. When the orchestrator is the main loop itself (`g_main_context_is_owner`), main-loop work runs inline.
- **Deadlines on every wait:** a missed deadline becomes a logged SBMD Transaction failure (`timeout`), never a hang. The overall transaction deadline is enforced by the orchestrator.
- **Commit / abort:** on a successful terminal the orchestrator commits the session; a commit failure (including reason `conditionFailed`) is a `commitFailed` transaction failure. On any failure (including an `error` terminal) staged writes are discarded, then the failure hand-off runs (BARTON-424 supplies the driver-level handler; until then the hand-off logs).
- **Attribute writes:** a successful `writeAttribute` is recorded in the operation log and through the seam's overlay hook, so later steps read the written value. No automatic undo.
- **Aborts:** device removal, driver deactivation/unload, shutdown, or the session's abort listener (fired by `DalSbmdDataAccess` on plugin unregistration, later) fail the SBMD Transaction with type `aborted`.
- **Every device operation outcome is awaited and logged**, including `sendCommand`/`writeAttribute` returned from a deferred callback.
- **Metrics:** transactions started/committed/failed by failure type, steps per transaction, cross-thread hop latency, deadline misses, worker queue depth.

### Non-goals

- **The DAL itself and its plugins** (`DalPluginRegistry`, `DalTransaction`, cross-plugin rollback, plugins, `DalSbmdDataAccess`). Owned by BARTON-421 and BARTON-430, delivered after this change.
- **Driver-facing data API** (`storage.mergeTransientData`, `storage.setTransientDataIf`, sequence supplements, the `supplements` override option on deferred requests). Owned by BARTON-422. The seam already carries what those features need.
- **Driver-level `onTransactionFailure`.** Owned by BARTON-424. This change defines the hand-off point and the operation log it consumes.
- **Optimistic concurrency control or per-device serialization.** No read/write-set validation, no versions, no automatic retries, no `conflict` error type (epic D3 as restated by D11, D4). Interleaving safety comes from fresh reads plus atomic primitives (D11).
- **Cross-store rollback in the legacy commit.** Arrives with `DalSbmdDataAccess` (epic D8).
- **Automatic undo of attribute writes or commands.** Device side effects are compensated only by the driver (BARTON-424).
- **New result-builder terminals** (for example a continuation after a status-only command). See design open questions.
- **Public API changes.** None.

## Capabilities

### New Capabilities

- `sbmd-transaction-orchestration`: the SBMD Transaction object and operation log, the `SbmdDataAccess` seam and its legacy implementation, the orchestrator step loop and its threading/wait rules, the recovery rule, `SbmdWorker`, deadlines, commit/abort, aborts, and the attribute-write overlay.

### Modified Capabilities

- `sbmd-v4-runtime`:
  - "Result chain execution" — data write operations are staged in the SBMD Transaction's data session and applied only if it commits; `log` runs immediately.
  - "Deferred operations" (as modified by BARTON-412) — deferred callbacks run as orchestrator steps with fresh supplements; device outcomes are handed back to the orchestrator.
  - "Deferred failure callback delivery" (added by BARTON-412) — `onError` runs as an orchestrator step; an honored recovery terminal recovers the transaction (D12); its writes are discarded if the transaction ultimately fails.

## Impact

- **Affected layers:**
  - Drivers — Matter base (C++, internal): `core/deviceDrivers/matter/MatterDeviceDriver.cpp/.h` (`ConnectAndOrchestrate`; SBMD read/write/execute route through it), `core/deviceDrivers/matter/MatterDevice.cpp/.h` (outcome-callback `InvokeCommand`/`WriteAttribute`).
  - Drivers — SBMD runtime (C++): `core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.cpp/.h` (remove `PendingOperation`/`pendingOperations`/`nextPendingId`, `ContinueDeferredChain`, `HandleDeferredCommand*`, inline `ExecuteOps` calls; fetchers/setters and `transientStore` move behind `LegacySbmdDataAccess`), `core/deviceDrivers/matter/sbmd/mquickjs/SbmdHandlerInvoker.cpp/.h` (`ExecuteOps` becomes `ToSbmdWrites` + `RunLogOps`; supplement prefetch becomes `PlanFetch`).
  - New files under `core/deviceDrivers/matter/sbmd/txn/`: `SbmdTransaction`, `SbmdOperationLog`, `SbmdDataAccess.h`, `LegacySbmdDataAccess`, `SbmdTransactionOrchestrator`, `SbmdWorker`, `ThreadHop`.
  - Tests: `SbmdTransactionTest.cpp`, `LegacySbmdDataAccessTest.cpp`, `SbmdTransactionOrchestratorTest.cpp`, `SbmdWorkerTest.cpp`, `ThreadHopTest.cpp` (new); updates to `SbmdDriverTest.cpp`, `SbmdCameraWebrtcTest.cpp`, `SbmdObservabilityTest.cpp`, BARTON-412's `SbmdDeferredFailureTest.cpp`; `core/test/CMakeLists.txt`.
  - Docs: `docs/SBMD.md` (§2.3, §5.1, §6.2, §7), `docs/barton-device-service-architecture.md`.
  - **Not affected:** public API (`api/c/public/`), GObject types/signals/properties, GIR, core service, Zigbee/Thread subsystems, SBMD spec files (driver JS) — driver migration is BARTON-425.
- **CMake flags:** `BCORE_MATTER` (all touched code). Unit tests under `BUILD_TESTING`. No new flags.
- **Behavior changes visible to drivers:**
  - Data writes in a result chain whose transaction ultimately fails (any failure, including an `error` terminal or a later deferred step failing) are no longer applied. `log` ops are unaffected.
  - Deferred callbacks receive supplements read at callback time.
  - A `sendCommand`/`writeAttribute` returned from a deferred callback now fails the transaction if the device rejects it (previously reported success once dispatched).
  - Attribute, event, and command handlers run on `SbmdWorker`, so their ops are applied after the Matter callback returns. Per-device ordering is preserved.
