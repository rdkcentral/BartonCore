## 1. Startup and shutdown wiring

- [x] 1.1 Add CMake option `BCORE_SBMD_METRICS_SAMPLE_PERIOD_MS` (default 30000, compiled definition `BARTON_CONFIG_SBMD_METRICS_SAMPLE_PERIOD_MS`) to `config/cmake/options.cmake`
- [x] 1.2 In `SbmdFactory::RegisterDriversFromDirectory`, call `MQuickJsRuntime::InitializeMetrics()` **before** `MQuickJsRuntime::Initialize()` so the `sbmd.js.exception` handle is live for init-phase exceptions; after `MQuickJsRuntime::Initialize()` succeeds, call `SbmdHandlerInvoker::InitializeMetrics()`, `SbmdFactory::InitializeMetrics()`, and `SpecBasedMatterDeviceDriver::InitializeMetrics()` (these functions are added in tasks 3.0, 4.0, and 5.0 respectively)
- [x] 1.3 Wire the corresponding `ShutdownMetrics()` calls into the subsystem shutdown path that already calls `MQuickJsRuntime::Shutdown()`; identify the correct call site during implementation

## 2. MQuickJsRuntime — hybrid heap sampler

- [x] 2.0 Add heap metric handles (`sbmd.js.heap.used_bytes`, `sbmd.js.heap.arena_bytes`, `sbmd.js.heap.free_bytes`, `sbmd.js.heap.peak_bytes`), `sbmd.js.mutex.wait_ms`, and `sbmd.js.exception` as private static members of `MQuickJsRuntime`; add public static functions `static void InitializeMetrics()`, `static void ShutdownMetrics()`, `static void ForceSnapshot()`, `static void TickleSampler()`, `static void RecordHeapSnapshot(const JSMemoryUsage &usage, size_t gcRootCount)`, `static void RecordMutexWait(double ms)`, and `static void RecordJsException(const char *phase, const char *driver)` — when `driver == nullptr`, `RecordJsException` MUST omit the `"driver"` attribute entirely (not record an empty string); `ForceSnapshot` MUST check the `jsContextReady` atomic flag (in addition to the metric handle null-check) and silently no-op without acquiring the JS mutex if `!jsContextReady`; `GetSharedContext()` MUST NOT be used for this guard: calling it without the mutex held is a data race, and calling it with the mutex held defeats the purpose of a lock-free early-exit; `jsContextReady` is set to `true` at the end of `Initialize()` (task 2.4) and cleared to `false` at the start of `Shutdown()` (task 2.3); the remaining recording functions (`RecordHeapSnapshot`, `RecordMutexWait`, `RecordJsException`) must null-check their handles and return silently if called before `InitializeMetrics()`; `RecordHeapSnapshot` takes an explicit `gcRootCount` parameter — the caller must call `JS_GetGCRootCount(ctx)` while holding the JS mutex and pass the result, avoiding a TOCTOU race with `Shutdown()`
- [x] 2.1 Add `static std::thread periodicSamplerThread`, `static std::atomic<bool> samplerShouldStop`, `static std::atomic<bool> jsContextReady`, `static std::atomic<uint64_t> tickleSeq`, `static std::condition_variable samplerCv`, and `static std::mutex samplerCvMutex` to `MQuickJsRuntime`
- [x] 2.2 Start idle sampler thread at end of `MQuickJsRuntime::Initialize()`: outer loop checks `samplerShouldStop` and exits if set; otherwise computes an absolute `deadline = steady_clock::now() + std::chrono::milliseconds(BARTON_CONFIG_SBMD_METRICS_SAMPLE_PERIOD_MS)` and records the current `tickleSeq`; inner loop calls `samplerCv.wait_until(lock, deadline)` — on `cv_status::timeout` calls `ForceSnapshot()` and breaks to outer loop; on early return compares current `tickleSeq` to the recorded value: if changed (real tickle) breaks to outer loop to re-check `samplerShouldStop` and compute a fresh deadline; if unchanged (spurious) loops back to `wait_until` with the same `deadline`, consuming only the remaining time rather than restarting a full `BARTON_CONFIG_SBMD_METRICS_SAMPLE_PERIOD_MS` wait
- [x] 2.3 In `MQuickJsRuntime::Shutdown()`, set `jsContextReady = false` first (so any concurrent `ForceSnapshot()` caller sees the flag cleared before the JS mutex is released), then set `samplerShouldStop = true`, call `TickleSampler()` to wake the thread immediately, then join `periodicSamplerThread` before `JS_FreeContext`; guard the join with `periodicSamplerThread.joinable()` to avoid `std::terminate` if `Initialize()` failed before the thread was started
- [x] 2.4 After context is created at end of `MQuickJsRuntime::Initialize()`: record `sbmd.js.heap.arena_bytes` gauge once, then set `jsContextReady = true` (using `std::memory_order_release` so the flag is visible to any thread that reads it after the JS context is fully set up)

## 3. SbmdHandlerInvoker — per-invocation instrumentation

- [x] 3.0 Add `sbmd.handler.duration_ms`, `sbmd.handler.heap_delta_bytes`, and `sbmd.handler.outcome` metric handles as private static members of `SbmdHandlerInvoker`; add `static void InitializeMetrics()`, `static void ShutdownMetrics()`, and `static void RecordOutcomeError(const char *driver, const char *opType, const char *resourceId, const char *outcome)`; `RecordOutcomeError` must null-check its handle and return silently if called before `InitializeMetrics()`; when `resourceId` is non-null emit a `"resource_id"` attribute, otherwise omit it entirely
- [x] 3.1 Define `OperationContext` struct in `SbmdHandlerInvoker.h` with fields `const char *driverName = nullptr`, `const char *opType = nullptr`, `const char *resourceId = nullptr`, and `std::chrono::steady_clock::time_point startTime`; extend `SbmdHandlerInvoker::InvokeHandler` with `const OperationContext *opCtx = nullptr` (backwards-compatible)
- [x] 3.2 Capture `heap_used` from `JS_GetMemoryUsage` and `steady_clock::now()` immediately before `JS_PushArg` / `JS_Call`
- [x] 3.3 Capture end time and `heap_used` after `JS_Call` returns; record `durationMs` and `heapDelta` to `SbmdHandlerInvoker`'s histogram handles — include `"resource_id"` attribute (from `opCtx->resourceId`) when non-null, omit it for attribute/event invocations where it is `nullptr`; increment the `sbmd.handler.outcome` counter via `RecordOutcomeError(driver, opType, resourceId, outcome)` with the resolved outcome value; call `MQuickJsRuntime::RecordHeapSnapshot(usageAfter, JS_GetGCRootCount(ctx))` — passing the already-captured post-`JS_Call` `JSMemoryUsage` struct and the GC root count read while the JS mutex is still held — to update pool health metrics (`sbmd.js.heap.used_bytes` histogram, `sbmd.js.heap.free_bytes`/`peak_bytes` gauges, and `sbmd.js.gc_roots` gauge); then call `MQuickJsRuntime::TickleSampler()` to notify the idle thread
- [x] 3.4 Distinguish timeout from exception in outcome: check if `steady_clock::now() > MQuickJsRuntime::GetDeadline()` (or use a flag set before `MQuickJsRuntime::ClearDeadline()`) to emit `"timeout"` vs `"exception"`; additionally, emit `"stack_overflow"` if `JS_StackCheck` fails before the call (increment the counter before returning `std::nullopt`)
- [x] 3.5 Update `InvokeHandler` call sites in `SpecBasedMatterDeviceDriver`: synchronous paths (`HandleResourceOp`) stack-allocate `OperationContext` with `driverName`, `opType`, `resourceId` (from `resource->id`), and `startTime = steady_clock::now()`, then pass `&opCtx`; deferred callback paths (`HandleDeferredCommandResponse`, `HandleDeferredCommandError`, `ContinueDeferredChain`) pass `&pending.operationCtx` (the persistent context initialized in task 5.2)
- [x] 3.6 Add mutex wait timing to `HandleResourceOp`, the attribute/event handler paths, and all deferred callback paths (`HandleDeferredCommandResponse`, `HandleDeferredCommandError`, `ContinueDeferredChain`): `auto t0 = steady_clock::now()` before `std::lock_guard<std::mutex>`, call `MQuickJsRuntime::RecordMutexWait(elapsed(t0))` after lock acquired
- [x] 3.7 At each `ResultTerminal::Error` handling site in `SpecBasedMatterDeviceDriver::ExecuteTerminal` and `ContinueDeferredChain`, call `SbmdHandlerInvoker::RecordOutcomeError(driverName, opType, resourceId, "error")` to emit `sbmd.handler.outcome{outcome="error"}`; `opType` and `resourceId` are available as parameters in `ExecuteTerminal` and via `pending.operationCtx` in `ContinueDeferredChain`

## 4. SbmdFactory — driver load instrumentation

- [x] 4.0 Add `sbmd.driver.load.duration_ms`, `sbmd.driver.load.heap_bytes`, `sbmd.driver.load.failure`, and `sbmd.driver.registered.count` metric handles as private static members of `SbmdFactory`; add `static void InitializeMetrics()` and non-static `void ShutdownMetrics()` — the shutdown function is an instance method (called via `SbmdFactory::Instance().ShutdownMetrics()`) so that it can reset the `runtimeReady` flag alongside releasing the metric handles, allowing re-initialization on subsystem restart
- [x] 4.1 In `RegisterDriversFromDirectory`, capture `steady_clock::now()` as `loadStart` immediately before acquiring the JS mutex (so mutex-wait time is included in the load cost metric); capture `heap_used` via `JS_GetMemoryUsage` inside the `lock_guard` before calling `SbmdLoader::LoadDriver` — file I/O happens earlier and is excluded from the metric
- [x] 4.2 Capture end time and `heap_used` after `sbmdDriver->Activate(ctx)` succeeds; record `durationMs` and `heapDelta` to `SbmdFactory`'s `sbmd.driver.load.duration_ms` and `sbmd.driver.load.heap_bytes` handles with the `"driver"` attribute
- [x] 4.3 `SbmdFactory::InitializeMetrics()` is called as part of the startup sequence in task 1.2; ensure it runs before the driver load loop begins
- [x] 4.4 At each `allRegistered = false; continue` branch, increment `SbmdFactory`'s `sbmd.driver.load.failure` counter with `stem` as the `"driver"` attribute and the appropriate `"reason"` string
- [x] 4.5 After the loading loop, record `count` to `SbmdFactory`'s `sbmd.driver.registered.count` gauge

## 5. SpecBasedMatterDeviceDriver — deferred operation instrumentation

- [x] 5.0 Add `sbmd.deferred.in_flight`, `sbmd.deferred.duration_ms`, `sbmd.deferred.depth`, `sbmd.deferred.timeout`, and `sbmd.deferred.max_depth` metric handles as private static members of `SpecBasedMatterDeviceDriver`; add `static void InitializeMetrics()` and `static void ShutdownMetrics()`
- [x] 5.1 Add `OperationContext operationCtx` field to `PendingOperation` struct
- [x] 5.2 Initialize `pending.operationCtx` in `ExecuteRequestCommand` alongside `overallDeadline`: set `operationCtx.driverName`, `operationCtx.opType` (the originating op, e.g., `"write"`), `operationCtx.resourceId` (from `resource->id`), and `operationCtx.startTime = steady_clock::now()`
- [x] 5.3 Increment `sbmd.deferred.in_flight` gauge in `ExecuteRequestCommand` after `pendingOperations.emplace` succeeds
- [x] 5.4 In `CompletePendingOperation`, compute `elapsed(pending.operationCtx.startTime)` and record `durationMs` and `depth` to `SpecBasedMatterDeviceDriver`'s deferred histogram handles using `observabilityHistogramRecordWithAttrs` with `pending.operationCtx.driverName`, `pending.operationCtx.opType`, and `pending.operationCtx.resourceId` attributes (use empty string for any nullptr field); decrement `sbmd.deferred.in_flight` — all before erasing from map
- [x] 5.5 In `HandleDeferredCommandResponse` at the `now() > overallDeadline` branch, increment `SpecBasedMatterDeviceDriver`'s `sbmd.deferred.timeout` counter with `pending.operationCtx.driverName`, `pending.operationCtx.opType`, and `pending.operationCtx.resourceId` attributes before calling `CompletePendingOperation`
- [x] 5.6 In `ContinueDeferredChain` at the `deferralDepth >= MAX_DEFERRAL_DEPTH` branch, increment `SpecBasedMatterDeviceDriver`'s `sbmd.deferred.max_depth` counter with `pending.operationCtx.driverName`, `pending.operationCtx.opType`, and `pending.operationCtx.resourceId` attributes before calling `CompletePendingOperation`

## 6. JS exception event tracking

- [x] 6.1 Call `MQuickJsRuntime::RecordJsException("loading", driverStem)` at the JS evaluation failure site in `SbmdLoader::LoadDriver`
- [x] 6.2 Call `MQuickJsRuntime::RecordJsException("init", nullptr)` at the polyfill/bundle failure sites in `MQuickJsRuntime::Initialize` and `SbmdBundleLoader`
- [x] 6.3 No handler-phase recording here — handler exceptions are fully captured by `sbmd.handler.outcome{outcome="exception"}` (task 3.3); `sbmd.js.exception` covers only `"init"` and `"loading"` phases

## 7. Unit tests — SbmdObservabilityTest.cpp

- [x] 7.1 Create `core/test/src/SbmdObservabilityTest.cpp` with `SetUpTestSuite` calling `MQuickJsRuntime::InitializeMetrics()` first, then `MQuickJsRuntime::Initialize(512*1024)`, then `SbmdHandlerInvoker::InitializeMetrics()` (per task 1.2 ordering); `SbmdFactory::InitializeMetrics()` and `SpecBasedMatterDeviceDriver::InitializeMetrics()` are NOT called in this unit test because those modules' source files are not linked (they pull in Matter SDK symbols unavailable in unit-test builds); the startup ordering requirement from task 1.2 is validated by production code and integration tests instead
- [x] 7.2 Write a minimal test driver JS string (inline, no file) and load it via `SbmdLoader::LoadDriver` for use across tests
- [x] 7.3 Test: `ForceSnapshot` produces pool health histogram observation — call `MQuickJsRuntime::ForceSnapshot()`, parse `observabilityDumpJson()`, assert `sbmd.js.heap.used_bytes` count >= 1
- [x] 7.4 Test: arena size gauge recorded at init — assert `sbmd.js.heap.arena_bytes` value equals 512*1024
- [x] 7.5 Test: handler duration histogram populated — invoke a handler, assert `sbmd.handler.duration_ms` count increased and sum > 0
- [x] 7.6 Test: heap delta histogram populated — invoke a handler, assert `sbmd.handler.heap_delta_bytes` count increased
- [x] 7.7 Test: success outcome counter increments on happy path
- [x] 7.8 Test: exception outcome counter increments when handler throws
- [x] 7.9 Test: driver load cost metrics populated — call `SbmdFactory::RegisterDriversFromDirectory` with a temp directory containing a minimal valid `.sbmd.js` test file; assert `sbmd.driver.load.duration_ms` and `sbmd.driver.load.heap_bytes` each have at least one observation (direct calls to `SbmdLoader::LoadDriver` bypass the factory instrumentation at tasks 4.1–4.2)
- [x] 7.10 Test: driver load failure counter increments — call `SbmdFactory::RegisterDriversFromDirectory` with a directory containing a `.sbmd.js` file whose content is syntactically invalid JS; assert `sbmd.driver.load.failure{reason="eval_failed"}` increments by one (task 4.4 records the counter inside `RegisterDriversFromDirectory`, not inside `SbmdLoader::LoadDriver`)
- [x] 7.11 Add `SbmdObservabilityTest.cpp` to `core/CMakeLists.txt` test target
- [x] 7.12 Test: mutex wait histogram populated — have a second thread acquire and hold `MQuickJsRuntime::GetMutex()`, then on the main thread trigger a handler invocation through the normal path that takes `std::lock_guard<std::mutex>(MQuickJsRuntime::GetMutex())` before calling `InvokeHandler` (e.g., drive it through `SpecBasedMatterDeviceDriver`'s handler path or a test helper that mirrors it); once the second thread releases the lock the main thread acquires it, calls `InvokeHandler`, and `RecordMutexWait` records the wait; assert `sbmd.js.mutex.wait_ms` has at least one non-zero observation
- [x] 7.13 Test: loading-phase exception counter increments — call `SbmdLoader::LoadDriver` with syntactically invalid JS; assert `sbmd.js.exception{phase="loading"}` increments by one
- [x] 7.14 Test: registered driver count gauge records correct value — call `SbmdFactory::RegisterDriversFromDirectory` with a temp directory containing N valid `.sbmd.js` test files; assert `sbmd.driver.registered.count` gauge equals N (task 4.5 records this gauge inside `RegisterDriversFromDirectory`, not inside `SbmdLoader::LoadDriver`)

## 8. GC instrumentation — mquickjs patch + BartonCore wiring

- [x] 8.1 Create `docker/patches/mquickjs/0003-add-gc-callback-and-root-count.patch`: adds `JSGCCallback *gc_callback` and `void *gc_opaque` fields to `JSContext`; adds `JS_SetGCCallback()` that stores the callback; modifies `JS_GC()` to call the callback before and after `JS_GC2(ctx, TRUE)`; adds `JS_GetGCRootCount()` that walks both `top_gc_ref` (push/pop stack) and `last_gc_ref` (add/delete list); adds `JSGCCallback` typedef and declarations to `mquickjs.h`
- [x] 8.2 Rebuild and reinstall mquickjs in dev container using the patched source so that `JS_SetGCCallback` and `JS_GetGCRootCount` are available for linking
- [x] 8.3 Add static metric handles to `MQuickJsRuntime`: `gcCountCounter` (`ObservabilityCounter*`), `gcDurationHisto` (`ObservabilityHistogram*`), `gcRootsGauge` (`ObservabilityGauge*`), and `gcCallback` (static `void GCCallback(JSContext*, int, void*)`)
- [x] 8.4 In `MQuickJsRuntime::InitializeMetrics()`: create `sbmd.js.gc.count` counter, `sbmd.js.gc.duration_ms` histogram, and `sbmd.js.gc_roots` gauge; in `ShutdownMetrics()` release all three handles and call `JS_SetGCCallback(ctx, nullptr, nullptr)`
- [x] 8.5 In `MQuickJsRuntime::Initialize()`: after context creation, call `JS_SetGCCallback(ctx, &MQuickJsRuntime::GCCallback, nullptr)`
- [x] 8.6 Implement `MQuickJsRuntime::GCCallback(ctx, is_end, opaque)`: on `is_end == 0` save `steady_clock::now()` to a static `time_point`; on `is_end == 1` compute elapsed ms, call `observabilityCounterAdd(gcCountCounter, 1)` and `observabilityHistogramRecord(gcDurationHisto, elapsed_ms)`
- [x] 8.7 In `MQuickJsRuntime::RecordHeapSnapshot(const JSMemoryUsage &usage, size_t gcRootCount)`: call `observabilityGaugeRecord(gcRootsGauge, static_cast<int64_t>(gcRootCount))` using the value passed by the caller (callers read `JS_GetGCRootCount(ctx)` while holding the JS mutex before calling `RecordHeapSnapshot`, avoiding a TOCTOU race with `Shutdown()`)
- [x] 8.8 Test: `GcCountIncrements` — acquire JS mutex, call `JS_GC(ctx)`, release; assert `sbmd.js.gc.count` increased by one
- [x] 8.9 Test: `GcRootsGaugeHasValue` — after driver load (SafeJSValue objects live), call `ForceSnapshot()`; assert `sbmd.js.gc_roots` gauge value > 0

## 9. Integration tests — deferred operation metrics

> **Implementation note:** These tests use a test-only SBMD driver
> (`testing/resources/sbmd-specs/deferred-command-test.sbmd.js`) that targets
> the Dimmable Plug-in Unit device type (`0x010B`), which is not claimed by any
> production driver.  The driver's `toggle` execute resource uses
> `requestCommand`, exercising the deferred code path. A dedicated test-only
> driver was chosen instead of modifying existing drivers because changing existing
> calls to `sendCommand` to `requestCommand` would introduce new synchronization on
> the Matter event loop thread and complete a different timeout relationship.
>
> TODO: A future production SBMD driver that naturally uses `requestCommand` can
> replace this test-only driver for the deferred-metrics integration tests.

- [x] 9.1 Verify the matter.js virtual device framework supports triggering a deferred command path (a write or execute that returns `requestCommand`) deterministically with a simulated device
  - Implemented via `DeferredCmdTestDevice.js` (Dimmable Plug-in Unit, `0x010B`) with `deferred-command-test.sbmd.js` using `requestCommand` for the `toggle` execute resource
- [x] 9.2 Write pytest test: commission simulated device, trigger one deferred operation, call `b_core_client_get_telemetry()`, parse JSON, assert `sbmd.deferred.in_flight` was nonzero during op and `sbmd.deferred.duration_ms` has one observation after completion
  - `test_deferred_duration_and_in_flight_metrics` in `testing/test/sbmd_deferred_metrics_test.py`
- [x] 9.3 Write pytest test: assert `sbmd.deferred.depth` records 0 for a single-round-trip operation
  - `test_deferred_depth_zero_for_single_round_trip` in `testing/test/sbmd_deferred_metrics_test.py`
