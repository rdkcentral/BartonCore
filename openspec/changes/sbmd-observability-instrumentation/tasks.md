## 1. SbmdMetrics class scaffold

- [ ] 1.1 Create `SbmdMetrics.h` declaring all metric handles as private static members and all recording functions as public static functions
- [ ] 1.2 Create `SbmdMetrics.cpp` with `Initialize()` creating all instruments via `observabilityHistogramCreate` / `observabilityGaugeCreate` / `observabilityCounterCreate`, and `Shutdown()` releasing handles
- [ ] 1.3 Implement `SbmdMetrics::ForceSnapshot()`: acquires `MQuickJsRuntime::GetMutex()`, calls `JS_GetMemoryUsage`, records to pool health metrics, releases lock
- [ ] 1.4 Add `SbmdMetrics.cpp` to `core/CMakeLists.txt` SBMD source list
- [ ] 1.5 Add CMake option `BCORE_SBMD_METRICS_SAMPLE_PERIOD_MS` (default 30000) to `config/cmake/options.cmake`
- [ ] 1.6 Wire `SbmdMetrics::Shutdown()` into `SbmdFactory` or the subsystem shutdown path that already calls `MQuickJsRuntime::Shutdown()`; identify the correct call site during implementation

## 2. MQuickJsRuntime — periodic sampler

- [ ] 2.1 Add `static std::thread periodicSamplerThread` and `static std::atomic<bool> periodicSamplerRunning` to `MQuickJsRuntime`
- [ ] 2.2 Start sampler thread at end of `MQuickJsRuntime::Initialize()`: loop calls `SbmdMetrics::ForceSnapshot()`, sleeps `BCORE_SBMD_METRICS_SAMPLE_PERIOD_MS` ms, exits when `periodicSamplerRunning` is false
- [ ] 2.3 Stop and join thread in `MQuickJsRuntime::Shutdown()` before `JS_FreeContext`
- [ ] 2.4 Record `sbmd.js.heap.arena_bytes` gauge once at end of `MQuickJsRuntime::Initialize()` after context is created

## 3. SbmdHandlerInvoker — per-invocation instrumentation

- [ ] 3.1 Extend `SbmdHandlerInvoker::InvokeHandler` signature with `const char *driverName = nullptr, const char *opType = nullptr` (backwards-compatible)
- [ ] 3.2 Capture `heap_used` from `JS_GetMemoryUsage` and `steady_clock::now()` immediately before `JS_PushArg` / `JS_Call`
- [ ] 3.3 Capture end time and `heap_used` after `JS_Call` returns; call `SbmdMetrics::RecordHandlerInvocation(driverName, opType, durationMs, heapDelta, outcome)`
- [ ] 3.4 Distinguish timeout from exception in outcome: check if `steady_clock::now() > GetDeadline()` (or use a flag set before `ClearDeadline()`) to emit `"timeout"` vs `"exception"`
- [ ] 3.5 Update all `InvokeHandler` call sites in `SpecBasedMatterDeviceDriver` to pass `driver->GetRegistration().name.c_str()` and the appropriate `opType` string
- [ ] 3.6 Add mutex wait timing to `HandleResourceOp` and the attribute/event handler paths: `auto t0 = steady_clock::now()` before `lock_guard`, record `elapsed(t0)` to `SbmdMetrics::RecordMutexWait` after lock acquired
- [ ] 3.7 At each `ResultTerminal::Error` handling site in `SpecBasedMatterDeviceDriver::ExecuteTerminal` and `ContinueDeferredChain`, call `SbmdMetrics::RecordHandlerOutcomeError(driverName, opType)` to emit `sbmd.handler.outcome{outcome="error"}`

## 4. SbmdFactory — driver load instrumentation

- [ ] 4.1 In `RegisterDriversFromDirectory`, capture `heap_used` and `steady_clock::now()` before `SbmdLoader::LoadDriver` call (inside existing `lock_guard`)
- [ ] 4.2 Capture end time and `heap_used` after `sbmdDriver->Activate(ctx)` succeeds; call `SbmdMetrics::RecordDriverLoad(name, durationMs, heapDelta)`
- [ ] 4.3 Add `SbmdMetrics::Initialize()` call immediately after `MQuickJsRuntime::Initialize()` succeeds in `RegisterDriversFromDirectory`
- [ ] 4.4 At each `allRegistered = false; continue` branch, call `SbmdMetrics::RecordDriverLoadFailure(stem, reason)` with appropriate reason string
- [ ] 4.5 After the loading loop, call `SbmdMetrics::RecordRegisteredDriverCount(count)` with the total successful count

## 5. SpecBasedMatterDeviceDriver — deferred operation instrumentation

- [ ] 5.1 Add `std::chrono::steady_clock::time_point startTime` and `std::string origOpType` fields to `PendingOperation` struct
- [ ] 5.2 Set `pending.startTime = steady_clock::now()` and `pending.origOpType = opType` in `ExecuteRequestCommand` alongside `overallDeadline`
- [ ] 5.3 Increment `sbmd.deferred.in_flight` gauge in `ExecuteRequestCommand` after `pendingOperations.emplace` succeeds
- [ ] 5.4 In `CompletePendingOperation`, compute `elapsed(pending.startTime)` and call `SbmdMetrics::RecordDeferredOpComplete(driverName, pending.origOpType, durationMs, depth, success)` before erasing from map; decrement `sbmd.deferred.in_flight`
- [ ] 5.5 In `HandleDeferredCommandResponse` at the `now() > overallDeadline` branch, call `SbmdMetrics::RecordDeferredTimeout(driverName, pending.origOpType)` before calling `CompletePendingOperation`
- [ ] 5.6 In `ContinueDeferredChain` at the `deferralDepth >= MAX_DEFERRAL_DEPTH` branch, call `SbmdMetrics::RecordDeferredMaxDepth(driverName, pending.origOpType)` before calling `CompletePendingOperation`

## 6. JS exception event tracking

- [ ] 6.1 Call `SbmdMetrics::RecordJsException("loading", driverStem)` at the JS evaluation failure site in `SbmdLoader::LoadDriver`
- [ ] 6.2 Call `SbmdMetrics::RecordJsException("init", nullptr)` at the polyfill/bundle failure sites in `MQuickJsRuntime::Initialize` and `SbmdBundleLoader`
- [ ] 6.3 No handler-phase recording here — handler exceptions are fully captured by `sbmd.handler.outcome{outcome="exception"}` (task 3.3); `sbmd.js.exception` covers only `"init"` and `"loading"` phases

## 7. Unit tests — SbmdMetricsTest.cpp

- [ ] 7.1 Create `core/test/src/SbmdMetricsTest.cpp` with `SetUpTestSuite` calling `MQuickJsRuntime::Initialize(512*1024)`, `SbmdBundleLoader::LoadBundle`, `SbmdLoader::InjectCaptureFunction`, and `SbmdMetrics::Initialize()`
- [ ] 7.2 Write a minimal test driver JS string (inline, no file) and load it via `SbmdLoader::LoadDriver` for use across tests
- [ ] 7.3 Test: `ForceSnapshot` produces pool health histogram observation — call `ForceSnapshot()`, parse `observabilityDumpJson()`, assert `sbmd.js.heap.used_bytes` count >= 1
- [ ] 7.4 Test: arena size gauge recorded at init — assert `sbmd.js.heap.arena_bytes` value equals 512*1024
- [ ] 7.5 Test: handler duration histogram populated — invoke a handler, assert `sbmd.handler.duration_ms` count increased and value > 0
- [ ] 7.6 Test: heap delta histogram populated — invoke a handler, assert `sbmd.handler.heap_delta_bytes` count increased
- [ ] 7.7 Test: success outcome counter increments on happy path
- [ ] 7.8 Test: exception outcome counter increments when handler throws
- [ ] 7.9 Test: driver load cost metrics populated — assert `sbmd.driver.load.duration_ms` and `sbmd.driver.load.heap_bytes` have observations after `SbmdLoader::LoadDriver`
- [ ] 7.10 Test: driver load failure counter increments when `LoadDriver` is called with invalid JS
- [ ] 7.11 Add `SbmdMetricsTest.cpp` to `core/CMakeLists.txt` test target
- [ ] 7.12 Test: mutex wait histogram populated — hold the JS mutex from a second thread while calling `InvokeHandler` from the main thread to create contention; assert `sbmd.js.mutex.wait_ms` records at least one non-zero observation
- [ ] 7.13 Test: loading-phase exception counter increments — call `SbmdLoader::LoadDriver` with syntactically invalid JS; assert `sbmd.js.exception{phase="loading"}` increments by one
- [ ] 7.14 Test: registered driver count gauge records correct value — load N test drivers; assert `sbmd.driver.registered.count` gauge equals N

## 8. Integration tests — deferred operation metrics

- [ ] 8.1 Verify the matter.js virtual device framework supports triggering a deferred command path (a write or execute that returns `requestCommand`) deterministically with a simulated device
- [ ] 8.2 Write pytest test: commission simulated device, trigger one deferred operation, call `b_core_client_get_telemetry()`, parse JSON, assert `sbmd.deferred.in_flight` was nonzero during op and `sbmd.deferred.duration_ms` has one observation after completion
- [ ] 8.3 Write pytest test: assert `sbmd.deferred.depth` records 0 for a single-round-trip operation
