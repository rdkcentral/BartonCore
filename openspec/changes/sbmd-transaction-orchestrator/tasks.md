## 1. Foundations (internal C++ API)

- [ ] 1.1 Add `core/deviceDrivers/matter/sbmd/txn/ThreadHop.{h,cpp}` with `ThreadHop`, `DefaultThreadHop` (`g_main_context_invoke_full` on the default context, inline when `g_main_context_is_owner`; `PlatformMgr().ScheduleWork` for Matter), deadline-bounded `OnMainLoop`/`OnMatter`, abandoned-flag late-closure suppression, and debug assertion that the Matter thread never calls `OnMainLoop` (≤4 h)
- [ ] 1.2 Unit tests `core/test/src/ThreadHopTest.cpp`: inline-on-owner, deadline miss returns empty, late closure has no side effects, assertion on Matter → main loop (≤3 h)
- [ ] 1.3 Add `sbmd/txn/SbmdOperationLog.{h,cpp}` (record shape: step, kind, endpoint, cluster, command|attribute, request, status, response, error) (≤2 h)
- [ ] 1.4 Add `sbmd/txn/SbmdTransaction.{h,cpp}`: `SbmdFailureType` (incl. `aborted`, `commitFailed`), hop arm/take/generation, exactly-once `TryFinish` latch absorbing BARTON-412 `FailPendingOperation`/`CompletePendingOperation` semantics, thread-safe `RequestAbort` (≤4 h)
- [ ] 1.5 Unit tests `core/test/src/SbmdTransactionTest.cpp`: concurrent `TryFinish`, stale-generation rejection, abort visibility, log status transitions (≤3 h)

## 1A. Data-access seam (internal API, then implementation)

- [ ] 1A.1 Add `sbmd/txn/SbmdDataAccess.h`: `SbmdDataKey`, `SbmdDataValue`, `SbmdWrite`, `SbmdConditionalWrite`, `SbmdFetchRequest` (reads + sequences), `SbmdFetchResult`, `SbmdDataError` (reason incl. `conditionFailed`), `SbmdDataSession` (`Fetch`, `Stage`, `StageConditional`, `RecordAttributeWrite`, `Commit`, `Abort`, `SetAbortListener`), `SbmdDataAccess::Begin` (≤2 h)
- [ ] 1A.2 Implement `sbmd/txn/LegacySbmdDataAccess.{h,cpp}`: move `MakeResFetcher`/`MakePersistFetcher`/`MakeAttrFetcher`/`MakeTransientFetcher`/`MakeTransientSetter` and `transientStore` behind it; device-service reads/writes on the orchestrator; transient + attribute-cache reads, transient sequences, and transient writes/CAS via one `ThreadHop::OnMatter` closure each; staged list held by the session owned by `SbmdTransaction`; overlay map; ordered commit with first-failure stop (no rollback); `Unsupported` for non-Transient sequence/conditional (≤4 h)
- [ ] 1A.3 Add the reusable contract suite `core/test/src/SbmdDataAccessContractTest.h` (GoogleTest typed-parameterized test over an implementation trait; declared unsupported capabilities must return `Unsupported`); instantiate it for `LegacySbmdDataAccess`. BARTON-421/426 instantiate it for `DalSbmdDataAccess` (≤4 h)
- [ ] 1A.4 Legacy-specific unit tests `core/test/src/LegacySbmdDataAccessTest.cpp`: thread affinity, staged writes invisible until commit, ordered apply, abort discards, overlay, concurrent transient sequence allocation unique and not rolled back, CAS mismatch → `conditionFailed` with no transient writes applied, `Unsupported` for other namespaces (≤4 h)

## 2. Matter device outcome APIs

- [ ] 2.1 Add `MatterDevice::InvokeCommand` and `MatterDevice::WriteAttribute` with `DeviceOpOutcomeCallback` (exactly once, on the Matter thread, never runs JS); keep existing promise-based APIs for non-SBMD drivers (≤4 h)
- [ ] 2.2 Unit tests for outcome delivery on success, IM status failure, transport error, and send rejection (≤3 h)

## 3. Orchestrator

- [ ] 3.1 Add `sbmd/txn/SbmdTransactionOrchestrator.{h,cpp}` step loop: one `SbmdDataSession::Fetch` → JS on Matter → `log` + stage → terminal → commit / fail (≤4 h)
- [ ] 3.2 Add `SbmdHandlerInvoker::PlanFetch`, `ToFetchedSupplements`, `ToSbmdWrites`, `RunLogOps`; remove `ExecuteOps` callers (≤4 h)
- [ ] 3.3 Unit tests `core/test/src/SbmdTransactionOrchestratorTest.cpp` with fake `ThreadHop` and fake `SbmdDataAccess`: success commit order, `error` discards writes and runs logs, one fetch per step, fresh supplements after park, commit failure → `commitFailed` (≤4 h)
- [ ] 3.4 Implement `ExecuteDeviceTerminal` (log append, endpoint/TLV/session validation on Matter thread, outcome wait bounded by `min(hop timeoutMs, overall deadline)`, `RecordAttributeWrite` on successful `writeAttribute`) (≤4 h)
- [ ] 3.5 Unit tests: every terminal kind logs correctly; `notSent` on pre-send failure; chained `sendCommand` after `onResponse` waits and fails on device rejection; attribute overlay visible to next step (≤4 h)
- [ ] 3.6 Implement deferred hops: arm/re-arm, response matching in `HandleCommand` before unsolicited dispatch, `onResponse`/`onError` as steps, BARTON-412 recovery rules (honor for `internal`/`transport`/`commandFailed`/`readFailed`, ignore for `timeout`/depth), epic D12 recovery (honored `success()` commits with no failure hand-off; honored continuation continues), depth limit (≤4 h)
- [ ] 3.6a Unit tests for D12: `onError` `success()` after `transport` commits without `OnTransactionFailed`; continuation that later succeeds commits; `success()` after `timeout` ignored → failure hand-off; handler `error` terminal type mapping (hop error type from `onError`, else `internal`) (≤3 h)
- [ ] 3.7 Port `core/test/src/SbmdDeferredFailureTest.cpp` (BARTON-412) to the orchestrator; update the rollback-visibility scenario to the discard-on-failure contract (≤4 h)
- [ ] 3.8 Implement `Fail`: latch, `SbmdDataSession::Abort`, metrics, `SpecBasedMatterDeviceDriver::OnTransactionFailed` hook (logs failure + operation log; BARTON-424 fills in), then unblock caller (≤3 h)
- [ ] 3.9 Unit tests for failure ordering: failing hop's `onError` → session abort → hand-off → caller unblocked (≤2 h)

## 4. Resource operations and seeds

- [ ] 4.1 Add `MatterDeviceDriver::ConnectAndOrchestrate` (same connection establishment as `ConnectAndExecute`, then orchestrator on the calling thread; deadline = min(caller timeout, transaction deadline)); route SBMD `readResource`/`writeResource`/`executeResource` through it (≤4 h)
- [ ] 4.2 Rewrite `SpecBasedMatterDeviceDriver::HandleResourceOp` to build an `SbmdTransaction`; delete `PendingOperation`, `pendingOperations`, `nextPendingId`, `ExecuteRequestCommand`, `ExecuteReadAttribute`, `HandleDeferredCommand*`, `ContinueDeferredChain`, `CompletePendingOperation`, `ReleasePendingHandlers`, BARTON-412 `FailPendingOperation` (≤4 h)
- [ ] 4.3 Run seeds as `Seed` transactions from `DoSynchronizeDevice` and, after `RunOnMatterSync` returns, from resource registration (≤3 h)
- [ ] 4.4 Update `SbmdDriverTest.cpp`, `SbmdCameraWebrtcTest.cpp`, `SbmdObservabilityTest.cpp` for the orchestrated path (≤4 h)

## 5. SbmdWorker

- [ ] 5.1 Add `sbmd/txn/SbmdWorker.{h,cpp}` (single thread, FIFO, `Start`/`Stop`, non-blocking `Post`); start with the Matter subsystem, stop before Matter shutdown (≤3 h)
- [ ] 5.2 Route `HandleAttributeReport`/`HandleEvent`/unsolicited `HandleCommand` → `DispatchToHandlers` through the worker, one transaction per matching handler (≤3 h)
- [ ] 5.3 Unit tests `core/test/src/SbmdWorkerTest.cpp`: per-device FIFO order, ops applied at commit, `error` terminal discards writes, `Stop()` aborts queued and in-flight work (≤3 h)

## 6. Aborts

- [ ] 6.1 Per-driver live-transaction registry; abort on `DeviceRemoved`, driver deactivation, `Shutdown`; wire `SbmdDataSession::SetAbortListener` to `RequestAbort` (fired later by `DalSbmdDataAccess`) (≤4 h)
- [ ] 6.2 Unit tests: device removal while parked wakes the orchestrator and fails with `aborted` without `onError`; plugin unregister aborts bound transactions (≤3 h)

## 7. Metrics

- [ ] 7.1 Add `sbmd.txn.*` and `sbmd.worker.queue_depth` metrics; map existing deferred metrics (≤3 h)
- [ ] 7.2 Extend `SbmdObservabilityTest.cpp` for the new metrics (≤2 h)

## 8. Stress and integration

- [ ] 8.1 Concurrency stress test: 1,000 transactions across caller threads, worker, fake main loop and fake Matter thread with randomized delays; assert no deadlock and every transaction finishes within its deadline (≤4 h)
- [ ] 8.2 Run Matter pytest suites (door lock, light, contact sensor, water leak, camera) in Docker; fix regressions (≤4 h, requires Docker)

## 9. Documentation

- [ ] 9.1 Update `docs/SBMD.md` §2.3, §5.1, §6.2, §7: SBMD Transaction, step model, fresh supplements on deferred handlers, staged writes committed only on success, chained `sendCommand`/`writeAttribute` now awaited (≤3 h)
- [ ] 9.2 Add "SBMD execution model" subsection with the thread diagram and wait-direction rule to `docs/barton-device-service-architecture.md` (≤2 h)
