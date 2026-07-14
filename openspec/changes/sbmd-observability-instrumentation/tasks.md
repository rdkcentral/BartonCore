## 1. Startup and shutdown wiring

- [ ] 1.1 Add CMake option `BCORE_SBMD_METRICS_SAMPLE_PERIOD_MS` (default 30000) to `config/cmake/options.cmake`
- [ ] 1.2 In `SbmdFactory::RegisterDriversFromDirectory`, after `MQuickJsRuntime::Initialize()` succeeds, call `MQuickJsRuntime::InitializeMetrics()`, `SbmdHandlerInvoker::InitializeMetrics()`, `SbmdFactory::InitializeMetrics()`, and `SpecBasedMatterDeviceDriver::InitializeMetrics()` (these functions are added in tasks 2.0, 3.0, 4.0, and 5.0 respectively)
- [ ] 1.3 Wire the corresponding `ShutdownMetrics()` calls into the subsystem shutdown path that already calls `MQuickJsRuntime::Shutdown()`; identify the correct call site during implementation

## 2. MQuickJsRuntime — hybrid heap sampler

- [ ] 2.0 Add heap metric handles (`sbmd.js.heap.used_bytes`, `sbmd.js.heap.arena_bytes`, `sbmd.js.heap.free_bytes`, `sbmd.js.heap.free_blocks`, `sbmd.js.heap.peak_bytes`), `sbmd.js.mutex.wait_ms`, and `sbmd.js.exception` as private static members of `MQuickJsRuntime`; add `static void InitializeMetrics()`, `ShutdownMetrics()`, `ForceSnapshot()`, `RecordHeapSnapshot(JSContext *ctx)`, `TickleSampler()`, `RecordMutexWait(double ms)`, and `RecordJsException(const char *phase, const char *driver)` static functions
- [ ] 2.1 Add `static std::thread periodicSamplerThread`, `static std::atomic<bool> samplerShouldStop`, `static std::condition_variable samplerCv`, and `static std::mutex samplerCvMutex` to `MQuickJsRuntime`
- [ ] 2.2 Start idle sampler thread at end of `MQuickJsRuntime::Initialize()`: loop waits on `samplerCv` for up to `BCORE_SBMD_METRICS_SAMPLE_PERIOD_MS` ms; on natural timeout calls `ForceSnapshot()`; on early wakeup (tickled by activity) resets the timer without taking a snapshot
- [ ] 2.3 In `MQuickJsRuntime::Shutdown()`, set `samplerShouldStop = true`, call `TickleSampler()` to wake the thread immediately, then join `periodicSamplerThread` before `JS_FreeContext`
- [ ] 2.4 Record `sbmd.js.heap.arena_bytes` gauge once at end of `MQuickJsRuntime::Initialize()` after context is created

## 3. SbmdHandlerInvoker — per-invocation instrumentation

- [ ] 3.0 Add `sbmd.handler.duration_ms`, `sbmd.handler.heap_delta_bytes`, and `sbmd.handler.outcome` metric handles as private static members of `SbmdHandlerInvoker`; add `static void InitializeMetrics()`, `ShutdownMetrics()`, and `RecordOutcomeError(const char *driver, const char *opType)` static functions
- [ ] 3.1 Define `SbmdOperationContext` struct in `SbmdHandlerInvoker.h` with fields `const char *driverName = nullptr`, `const char *opType = nullptr`, and `std::chrono::steady_clock::time_point startTime`; extend `SbmdHandlerInvoker::InvokeHandler` with `const SbmdOperationContext *opCtx = nullptr` (backwards-compatible)
- [ ] 3.2 Capture `heap_used` from `JS_GetMemoryUsage` and `steady_clock::now()` immediately before `JS_PushArg` / `JS_Call`
- [ ] 3.3 Capture end time and `heap_used` after `JS_Call` returns; record `durationMs` and `heapDelta` to `SbmdHandlerInvoker`'s histogram handles; increment the `sbmd.handler.outcome` counter with the resolved outcome value; call `MQuickJsRuntime::RecordHeapSnapshot(ctx)` to update pool health gauges while the JS mutex is still held; then call `MQuickJsRuntime::TickleSampler()` to notify the idle thread
- [ ] 3.4 Distinguish timeout from exception in outcome: check if `steady_clock::now() > GetDeadline()` (or use a flag set before `ClearDeadline()`) to emit `"timeout"` vs `"exception"`
- [ ] 3.5 Update `InvokeHandler` call sites in `SpecBasedMatterDeviceDriver`: synchronous paths (`HandleResourceOp`) stack-allocate `SbmdOperationContext{driver, opType, steady_clock::now()}` and pass `&opCtx`; deferred callback paths (`HandleDeferredCommandResponse`, `HandleDeferredCommandError`, `ContinueDeferredChain`) pass `&pending.operationCtx` (the persistent context initialized in task 5.2)
- [ ] 3.6 Add mutex wait timing to `HandleResourceOp` and the attribute/event handler paths: `auto t0 = steady_clock::now()` before `lock_guard`, call `MQuickJsRuntime::RecordMutexWait(elapsed(t0))` after lock acquired
- [ ] 3.7 At each `ResultTerminal::Error` handling site in `SpecBasedMatterDeviceDriver::ExecuteTerminal` and `ContinueDeferredChain`, call `SbmdHandlerInvoker::RecordOutcomeError(driverName, opType)` to emit `sbmd.handler.outcome{outcome="error"}`

## 4. SbmdFactory — driver load instrumentation

- [ ] 4.0 Add `sbmd.driver.load.duration_ms`, `sbmd.driver.load.heap_bytes`, `sbmd.driver.load.failure`, and `sbmd.driver.registered.count` metric handles as private static members of `SbmdFactory`; add `static void InitializeMetrics()` and `ShutdownMetrics()` static functions
- [ ] 4.1 In `RegisterDriversFromDirectory`, capture `heap_used` and `steady_clock::now()` before `SbmdLoader::LoadDriver` call (inside existing `lock_guard`)
- [ ] 4.2 Capture end time and `heap_used` after `sbmdDriver->Activate(ctx)` succeeds; record `durationMs` and `heapDelta` to `SbmdFactory`'s `sbmd.driver.load.duration_ms` and `sbmd.driver.load.heap_bytes` handles with the `"driver"` attribute
- [ ] 4.3 `SbmdFactory::InitializeMetrics()` is called as part of the startup sequence in task 1.2; ensure it runs before the driver load loop begins
- [ ] 4.4 At each `allRegistered = false; continue` branch, increment `SbmdFactory`'s `sbmd.driver.load.failure` counter with `stem` as the `"driver"` attribute and the appropriate `"reason"` string
- [ ] 4.5 After the loading loop, record `count` to `SbmdFactory`'s `sbmd.driver.registered.count` gauge

## 5. SpecBasedMatterDeviceDriver — deferred operation instrumentation

- [ ] 5.0 Add `sbmd.deferred.in_flight`, `sbmd.deferred.duration_ms`, `sbmd.deferred.depth`, `sbmd.deferred.timeout`, and `sbmd.deferred.max_depth` metric handles as private static members of `SpecBasedMatterDeviceDriver`; add `static void InitializeMetrics()` and `ShutdownMetrics()` static functions
- [ ] 5.1 Add `SbmdOperationContext operationCtx` field to `PendingOperation` struct
- [ ] 5.2 Initialize `pending.operationCtx` in `ExecuteRequestCommand` alongside `overallDeadline`: set `operationCtx.driverName`, `operationCtx.opType` (the originating op, e.g., `"write"`), and `operationCtx.startTime = steady_clock::now()`
- [ ] 5.3 Increment `sbmd.deferred.in_flight` gauge in `ExecuteRequestCommand` after `pendingOperations.emplace` succeeds
- [ ] 5.4 In `CompletePendingOperation`, compute `elapsed(pending.operationCtx.startTime)` and record `durationMs` and `depth` to `SpecBasedMatterDeviceDriver`'s deferred histogram handles with `pending.operationCtx.driverName` and `pending.operationCtx.opType` attributes; decrement `sbmd.deferred.in_flight` — all before erasing from map
- [ ] 5.5 In `HandleDeferredCommandResponse` at the `now() > overallDeadline` branch, increment `SpecBasedMatterDeviceDriver`'s `sbmd.deferred.timeout` counter with `pending.operationCtx.driverName` and `pending.operationCtx.opType` attributes before calling `CompletePendingOperation`
- [ ] 5.6 In `ContinueDeferredChain` at the `deferralDepth >= MAX_DEFERRAL_DEPTH` branch, increment `SpecBasedMatterDeviceDriver`'s `sbmd.deferred.max_depth` counter with `pending.operationCtx.driverName` and `pending.operationCtx.opType` attributes before calling `CompletePendingOperation`

## 6. JS exception event tracking

- [ ] 6.1 Call `MQuickJsRuntime::RecordJsException("loading", driverStem)` at the JS evaluation failure site in `SbmdLoader::LoadDriver`
- [ ] 6.2 Call `MQuickJsRuntime::RecordJsException("init", nullptr)` at the polyfill/bundle failure sites in `MQuickJsRuntime::Initialize` and `SbmdBundleLoader`
- [ ] 6.3 No handler-phase recording here — handler exceptions are fully captured by `sbmd.handler.outcome{outcome="exception"}` (task 3.3); `sbmd.js.exception` covers only `"init"` and `"loading"` phases

## 7. Unit tests — SbmdObservabilityTest.cpp

- [ ] 7.1 Create `core/test/src/SbmdObservabilityTest.cpp` with `SetUpTestSuite` calling `MQuickJsRuntime::Initialize(512*1024)`, then all four `InitializeMetrics()` calls per task 1.2, then `SbmdBundleLoader::LoadBundle` and `SbmdLoader::InjectCaptureFunction`
- [ ] 7.2 Write a minimal test driver JS string (inline, no file) and load it via `SbmdLoader::LoadDriver` for use across tests
- [ ] 7.3 Test: `ForceSnapshot` produces pool health histogram observation — call `MQuickJsRuntime::ForceSnapshot()`, parse `observabilityDumpJson()`, assert `sbmd.js.heap.used_bytes` count >= 1
- [ ] 7.4 Test: arena size gauge recorded at init — assert `sbmd.js.heap.arena_bytes` value equals 512*1024
- [ ] 7.5 Test: handler duration histogram populated — invoke a handler, assert `sbmd.handler.duration_ms` count increased and value > 0
- [ ] 7.6 Test: heap delta histogram populated — invoke a handler, assert `sbmd.handler.heap_delta_bytes` count increased
- [ ] 7.7 Test: success outcome counter increments on happy path
- [ ] 7.8 Test: exception outcome counter increments when handler throws
- [ ] 7.9 Test: driver load cost metrics populated — assert `sbmd.driver.load.duration_ms` and `sbmd.driver.load.heap_bytes` have observations after `SbmdLoader::LoadDriver`
- [ ] 7.10 Test: driver load failure counter increments when `LoadDriver` is called with invalid JS
- [ ] 7.11 Add `SbmdObservabilityTest.cpp` to `core/CMakeLists.txt` test target
- [ ] 7.12 Test: mutex wait histogram populated — hold the JS mutex from a second thread while calling `InvokeHandler` from the main thread to create contention; assert `sbmd.js.mutex.wait_ms` records at least one non-zero observation
- [ ] 7.13 Test: loading-phase exception counter increments — call `SbmdLoader::LoadDriver` with syntactically invalid JS; assert `sbmd.js.exception{phase="loading"}` increments by one
- [ ] 7.14 Test: registered driver count gauge records correct value — load N test drivers; assert `sbmd.driver.registered.count` gauge equals N

## 8. Integration tests — deferred operation metrics

- [ ] 8.1 Verify the matter.js virtual device framework supports triggering a deferred command path (a write or execute that returns `requestCommand`) deterministically with a simulated device
- [ ] 8.2 Write pytest test: commission simulated device, trigger one deferred operation, call `b_core_client_get_telemetry()`, parse JSON, assert `sbmd.deferred.in_flight` was nonzero during op and `sbmd.deferred.duration_ms` has one observation after completion
- [ ] 8.3 Write pytest test: assert `sbmd.deferred.depth` records 0 for a single-round-trip operation
