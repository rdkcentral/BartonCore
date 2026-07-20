## ADDED Requirements

### Requirement: JS heap pool utilization tracking
The SBMD runtime SHALL record mquickjs heap pool utilization as a histogram named `sbmd.js.heap.used_bytes` using a hybrid sampling strategy: (1) in-activity captures from `SbmdHandlerInvoker::InvokeHandler` after each `JS_Call` while the JS mutex is held, and (2) idle background captures from a thread that fires only when no handler activity has occurred for `BARTON_CONFIG_SBMD_METRICS_SAMPLE_PERIOD_MS` milliseconds (set via `BCORE_SBMD_METRICS_SAMPLE_PERIOD_MS` CMake option, default 30000 ms). The runtime SHALL also record the fixed arena size once at initialization as a gauge named `sbmd.js.heap.arena_bytes`, and SHALL maintain a gauge named `sbmd.js.heap.free_bytes` (sourced from `usage.free_size` — the gap between the heap top and the stack bottom) updated with each snapshot.

#### Scenario: Heap utilization captured during handler invocation
- **WHEN** a JS handler is invoked via `InvokeHandler`
- **THEN** `sbmd.js.heap.used_bytes` histogram gains one observation of the current heap utilization

#### Scenario: Heap utilization captured during idle period
- **WHEN** no handler invocations have occurred for `BARTON_CONFIG_SBMD_METRICS_SAMPLE_PERIOD_MS` milliseconds
- **THEN** the idle background thread records one observation to `sbmd.js.heap.used_bytes`

#### Scenario: Arena size recorded at initialization
- **WHEN** `MQuickJsRuntime::Initialize(N)` is called
- **THEN** the `sbmd.js.heap.arena_bytes` gauge records the value `N`

#### Scenario: Free bytes reflects remaining capacity
- **WHEN** a heap snapshot is taken (via `InvokeHandler` or `ForceSnapshot()`)
- **THEN** `sbmd.js.heap.free_bytes` gauge records the current `usage.free_size` value (gap between heap top and stack bottom)

### Requirement: Peak heap watermark gauge
The SBMD runtime SHALL maintain a gauge named `sbmd.js.heap.peak_bytes` recording the highest value of `usage.heap_used` observed since `MQuickJsRuntime::Initialize()`. (`JS_MEMUSAGE_WALK_HEAP` is not set, so `usage.heap_free_blocks` is always 0; `heap_used` is used directly as the high-water mark of heap expansion.) The gauge SHALL be updated with each heap snapshot.

#### Scenario: Peak watermark monotonically increases
- **WHEN** successive JS handler invocations allocate progressively more memory
- **THEN** `sbmd.js.heap.peak_bytes` reflects the highest value observed, never decreasing

### Requirement: Force snapshot API
The system SHALL provide a `MQuickJsRuntime::ForceSnapshot()` public static function that synchronously samples heap stats, adds one observation to the `sbmd.js.heap.used_bytes` histogram, updates `sbmd.js.heap.free_bytes` with the current `usage.free_size`, advances `sbmd.js.heap.peak_bytes` only if the current `usage.heap_used` exceeds the previously recorded peak (maintaining the high-water mark guarantee), and calls `TickleSampler()` to reset the idle thread's timer. This function SHALL be callable without holding the JS mutex (it acquires it internally). `ForceSnapshot()` SHALL silently no-op — without acquiring the JS mutex or calling `JS_GetMemoryUsage` — if called before `MQuickJsRuntime::Initialize()` has succeeded. This check MUST be thread-safe: implementations SHALL use a dedicated `std::atomic<bool> jsContextReady` flag (set to `true` with `memory_order_release` at the end of `Initialize()` and cleared to `false` at the start of `Shutdown()`) rather than calling `GetSharedContext()`, which requires the JS mutex to already be held and therefore cannot be used for a lock-free early-exit guard. The system SHALL also provide `MQuickJsRuntime::TickleSampler()`, which notifies the idle background thread to reset its sleep timer without taking a snapshot. The idle background thread SHALL use `ForceSnapshot()` when its idle timer expires.

#### Scenario: ForceSnapshot produces an immediate observation
- **WHEN** `MQuickJsRuntime::ForceSnapshot()` is called
- **THEN** `sbmd.js.heap.used_bytes` observation count increases by one

### Requirement: Per-invocation heap delta tracking
The SBMD runtime SHALL record the net heap allocation of each JS handler invocation as a histogram named `sbmd.handler.heap_delta_bytes`. The delta SHALL be computed as `heap_used_after - heap_used_before`, measured from immediately before `JS_PushArg` through immediately after `JS_Call` returns in `SbmdHandlerInvoker::InvokeHandler` (the same window as `sbmd.handler.duration_ms`), so that argument-marshalling allocations are included. The histogram SHALL support `"driver"`, `"op_type"`, and `"resource_id"` attributes; `"resource_id"` SHALL be included when `opCtx->resourceId` is non-null and SHALL be omitted for attribute/event handler invocations where it is `nullptr`.

#### Scenario: Heap delta recorded per invocation
- **WHEN** a JS handler is invoked via `InvokeHandler` with a `driverName` and `opType`
- **THEN** `sbmd.handler.heap_delta_bytes` contains one new observation attributed to that driver and op type

#### Scenario: Heap delta may be negative
- **WHEN** a JS handler invocation triggers implicit GC (allocation pressure)
- **THEN** `sbmd.handler.heap_delta_bytes` records a negative value, which is valid

### Requirement: Handler invocation duration tracking
The SBMD runtime SHALL record the wall-clock duration of each JS handler invocation as a histogram named `sbmd.handler.duration_ms`. Duration SHALL be measured from immediately before `JS_PushArg` to immediately after `JS_Call` returns in `SbmdHandlerInvoker::InvokeHandler`. The histogram SHALL support `"driver"`, `"op_type"`, and `"resource_id"` attributes; `"resource_id"` SHALL be included when `opCtx->resourceId` is non-null and SHALL be omitted for attribute/event handler invocations where it is `nullptr`.

#### Scenario: Duration recorded for each invocation
- **WHEN** a JS handler is invoked with a driver name and op type
- **THEN** `sbmd.handler.duration_ms` gains one observation greater than zero

#### Scenario: Timeout invocations are recorded
- **WHEN** a JS handler exceeds the `BARTON_CONFIG_SBMD_SCRIPT_TIMEOUT_MS` execution deadline and is interrupted
- **THEN** `sbmd.handler.duration_ms` still records the elapsed duration for that invocation

### Requirement: Handler invocation outcome tracking
The SBMD runtime SHALL count handler invocation outcomes using a counter named `sbmd.handler.outcome` with attributes `"driver"`, `"op_type"`, `"resource_id"` (omitted for attribute/event handler invocations where `resourceId` is `nullptr`), and `"outcome"`. Valid outcome values are: `"success"` (handler returned a valid result), `"exception"` (JS exception thrown, not a timeout), `"timeout"` (script execution deadline exceeded), `"stack_overflow"` (JS_StackCheck failed before call), and `"error"` (handler returned a ResultTerminal::Error). The counter SHALL be incremented via `SbmdHandlerInvoker::RecordOutcomeError(driver, opType, resourceId, outcome)` — from `InvokeHandler` for the first four outcomes (success, exception, timeout, stack_overflow) and from the `ResultTerminal::Error` handling sites in `ExecuteTerminal` and `ContinueDeferredChain` for the `"error"` outcome. `RecordOutcomeError` SHALL include the `"resource_id"` attribute only when `resourceId` is non-null, allowing attribute/event handler call sites to pass `nullptr` to omit it.

#### Scenario: Success outcome counted
- **WHEN** a JS handler invocation returns a success terminal
- **THEN** `sbmd.handler.outcome` counter for `outcome="success"` increments by one

#### Scenario: Exception outcome counted
- **WHEN** a JS handler throws a JS exception (not a timeout)
- **THEN** `sbmd.handler.outcome` counter for `outcome="exception"` increments by one

#### Scenario: Timeout outcome counted
- **WHEN** a JS handler exceeds the `BARTON_CONFIG_SBMD_SCRIPT_TIMEOUT_MS` deadline and the interrupt handler fires
- **THEN** `sbmd.handler.outcome` counter for `outcome="timeout"` increments by one

#### Scenario: Stack overflow outcome counted
- **WHEN** `JS_StackCheck` returns nonzero before a handler call
- **THEN** `sbmd.handler.outcome` counter for `outcome="stack_overflow"` increments by one before `InvokeHandler` returns `std::nullopt`

#### Scenario: Error outcome counted
- **WHEN** a JS handler returns a `ResultTerminal::Error`
- **THEN** `sbmd.handler.outcome` counter for `outcome="error"` increments by one

### Requirement: JS mutex wait time tracking
The SBMD runtime SHALL record the time a request spends waiting to acquire the JS runtime mutex as a histogram named `sbmd.js.mutex.wait_ms`. This SHALL be measured from the point a call to `std::lock_guard<std::mutex>(MQuickJsRuntime::GetMutex())` begins until the mutex is acquired, in all code paths that acquire the mutex for handler invocation.

#### Scenario: Mutex wait recorded on contention
- **WHEN** two threads attempt to invoke SBMD handlers concurrently
- **THEN** the waiting thread records a non-zero observation in `sbmd.js.mutex.wait_ms`

#### Scenario: Mutex wait near zero when uncontested
- **WHEN** a handler invocation acquires the JS mutex without contention
- **THEN** `sbmd.js.mutex.wait_ms` records an observation close to zero

### Requirement: Driver load cost tracking
The SBMD runtime SHALL record, for each successfully loaded driver: the wall-clock time to evaluate and activate its `.sbmd.js` file as a histogram named `sbmd.driver.load.duration_ms` with attribute `"driver"`, and the net heap bytes consumed during load and activation as a histogram named `sbmd.driver.load.heap_bytes` with attribute `"driver"`. Both SHALL be recorded once per driver at startup in `SbmdFactory::RegisterDriversFromDirectory`.

#### Scenario: Load duration recorded per driver
- **WHEN** a `.sbmd.js` file is successfully loaded and activated
- **THEN** `sbmd.driver.load.duration_ms` contains one observation attributed to that driver's name

#### Scenario: Load heap cost recorded per driver
- **WHEN** a `.sbmd.js` file is successfully loaded and activated
- **THEN** `sbmd.driver.load.heap_bytes` contains one observation reflecting the heap consumed

### Requirement: Registered driver count tracking
The SBMD runtime SHALL record the total number of successfully registered drivers as a gauge named `sbmd.driver.registered.count` once after all drivers have been loaded in `SbmdFactory::RegisterDriversFromDirectory`.

#### Scenario: Driver count reflects successful loads
- **WHEN** N drivers are successfully loaded and one fails
- **THEN** `sbmd.driver.registered.count` gauge records N

### Requirement: Driver load failure tracking
The SBMD runtime SHALL count driver load failures using a counter named `sbmd.driver.load.failure` with attributes `"driver"` (filename stem) and `"reason"`. Valid reason values are: `"file_read"` (file could not be opened or read), `"eval_failed"` (JS evaluation failed), `"activation_failed"` (driver Activate() failed).

#### Scenario: Evaluation failure counted
- **WHEN** `SbmdLoader::LoadDriver` fails due to a JS evaluation error
- **THEN** `sbmd.driver.load.failure` counter for `reason="eval_failed"` increments for that driver

#### Scenario: Activation failure counted
- **WHEN** `SbmdDriver::Activate` fails
- **THEN** `sbmd.driver.load.failure` counter for `reason="activation_failed"` increments for that driver

### Requirement: JS exception event tracking
The SBMD runtime SHALL count JS exception events during lifecycle phases using a counter named `sbmd.js.exception` with attributes `"driver"` and `"phase"`. The `"driver"` attribute SHALL be set to the driver's filename stem when known (loading phase) and SHALL be omitted entirely when not known (init phase); implementations MUST NOT record an empty string or placeholder value. Valid phase values are: `"init"` (exception during runtime/polyfill initialization) and `"loading"` (exception during driver script evaluation). Handler-phase exceptions are captured by `sbmd.handler.outcome{outcome="exception"}` and are not recorded here.

#### Scenario: Loading phase exception counted
- **WHEN** a driver's `.sbmd.js` evaluation throws a JS exception
- **THEN** `sbmd.js.exception` counter for `phase="loading"` increments with the driver's filename stem

### Requirement: Deferred operation in-flight count
The SBMD runtime SHALL maintain a gauge named `sbmd.deferred.in_flight` representing the number of `requestCommand` operations currently parked waiting for a device response. The gauge SHALL be incremented when a `PendingOperation` is added to `pendingOperations` in `ExecuteRequestCommand` and decremented in `CompletePendingOperation`.

#### Scenario: In-flight count reflects active deferred ops
- **WHEN** a deferred operation is started and not yet resolved
- **THEN** `sbmd.deferred.in_flight` gauge is greater than zero

#### Scenario: In-flight count decrements on completion
- **WHEN** a deferred operation completes (success, error, or timeout)
- **THEN** `sbmd.deferred.in_flight` gauge decrements back toward zero

### Requirement: Deferred operation total duration tracking
The SBMD runtime SHALL record the total wall-clock duration of each deferred operation — from initial `ExecuteRequestCommand` to `CompletePendingOperation` — as a histogram named `sbmd.deferred.duration_ms` using `observabilityHistogramRecordWithAttrs` with attributes `"driver"`, `"op_type"` (from `pending.operationCtx.opType`), and `"resource_id"` (from `pending.operationCtx.resourceId`); use an empty string for any nullptr field. Duration includes device round-trip time.

#### Scenario: Total duration includes device round-trip
- **WHEN** a deferred operation completes after a device response
- **THEN** `sbmd.deferred.duration_ms` records a value greater than the JS execution time alone

### Requirement: Deferral depth distribution tracking
The SBMD runtime SHALL record the deferral depth at completion of each deferred operation as a histogram named `sbmd.deferred.depth` using `observabilityHistogramRecordWithAttrs` with attributes `"driver"`, `"op_type"`, and `"resource_id"` (use empty string for any nullptr field). Depth 0 means the operation resolved after one round-trip; depth N means the operation re-armed N times.

#### Scenario: Single round-trip records depth zero
- **WHEN** a deferred operation resolves after one device response
- **THEN** `sbmd.deferred.depth` records 0 for that driver

#### Scenario: Re-armed operation records correct depth
- **WHEN** a deferred operation re-arms twice before resolving
- **THEN** `sbmd.deferred.depth` records 2 for that driver

### Requirement: Deferred operation timeout counting
The SBMD runtime SHALL count deferred operations that exceed their configured `overallDeadline` using a counter named `sbmd.deferred.timeout` with attributes `"driver"`, `"op_type"`, and `"resource_id"`. The deadline is set at operation start from `cmd.timeoutMs` if present, otherwise `driver.defaultTimeoutMs` if set, otherwise `PendingOperation::DEFAULT_OVERALL_TIMEOUT_MS` (30 s).

#### Scenario: Timeout counted when deadline exceeded
- **WHEN** a deferred operation's `overallDeadline` is exceeded before a device response arrives
- **THEN** `sbmd.deferred.timeout` counter increments for that driver

### Requirement: Deferred operation max-depth-exceeded counting
The SBMD runtime SHALL count deferred operations terminated because they reached `MAX_DEFERRAL_DEPTH` (10) using a counter named `sbmd.deferred.max_depth` with attributes `"driver"`, `"op_type"`, and `"resource_id"`.

#### Scenario: Max depth counter increments on depth limit
- **WHEN** a deferred operation re-arms 10 times and `ContinueDeferredChain` rejects the next re-arm
- **THEN** `sbmd.deferred.max_depth` counter increments for that driver

### Requirement: Subsystem metrics initialization
Each instrumented module (`MQuickJsRuntime`, `SbmdHandlerInvoker`, `SbmdFactory`, `SpecBasedMatterDeviceDriver`) SHALL provide `static void InitializeMetrics()` and `static void ShutdownMetrics()` functions. `MQuickJsRuntime::InitializeMetrics()` SHALL be called by `SbmdFactory::RegisterDriversFromDirectory` before `MQuickJsRuntime::Initialize()`, so that the `sbmd.js.exception{phase="init"}` counter is live for exceptions that occur during initialization. The remaining three `InitializeMetrics()` calls (`SbmdHandlerInvoker`, `SbmdFactory`, `SpecBasedMatterDeviceDriver`) SHALL be made after `MQuickJsRuntime::Initialize()` succeeds. The corresponding `ShutdownMetrics()` calls SHALL be made in the subsystem shutdown path.

#### Scenario: Metrics available after initialization
- **WHEN** all four `InitializeMetrics()` calls complete
- **THEN** `MQuickJsRuntime::ForceSnapshot()` and all recording functions operate without error

#### Scenario: Recording before initialization is safe
- **WHEN** a recording function is called before its module's `InitializeMetrics()`
- **THEN** the call is silently ignored without crashing

### Requirement: GC cycle count tracking
The SBMD runtime SHALL count GC cycles using a counter named `sbmd.js.gc.count`. The counter SHALL increment by one each time the mquickjs garbage collector completes a cycle. In production, all cycles are allocator-triggered (implicit); the callback also fires for explicit `JS_GC()` calls, which appear only in tests. The counter is incremented in the `is_end == 1` phase of the `JS_SetGCCallback` callback registered by `MQuickJsRuntime::Initialize()`.

#### Scenario: GC cycle increments counter
- **WHEN** a GC cycle completes (via explicit `JS_GC()` call or allocator trigger)
- **THEN** `sbmd.js.gc.count` increments by one

### Requirement: GC cycle duration tracking
The SBMD runtime SHALL record the duration of each GC cycle as a histogram named `sbmd.js.gc.duration_ms`. Duration is measured from the `is_end == 0` callback (GC starting) to the `is_end == 1` callback (GC finished), in milliseconds. Each completed GC cycle produces exactly one histogram observation.

#### Scenario: GC duration recorded per cycle
- **WHEN** a GC cycle completes
- **THEN** `sbmd.js.gc.duration_ms` gains one observation with a non-negative value

### Requirement: GC root list size tracking
The SBMD runtime SHALL maintain a gauge named `sbmd.js.gc_roots` reflecting the total number of live GC roots currently registered on the JS context via `JS_PushGCRef` (push/pop stack) and `JS_AddGCRef` (add/delete list), as returned by `JS_GetGCRootCount()`. The gauge SHALL be updated with each call to `MQuickJsRuntime::RecordHeapSnapshot()` (both in-activity and idle captures). A monotonically growing gauge value indicates GC root leaks from any source — `SafeJSValue` misuse, direct `JSValue` usage, or other root registrations.

#### Scenario: GC root count reflects live roots
- **WHEN** `MQuickJsRuntime::RecordHeapSnapshot()` is called with live `SafeJSValue` objects held
- **THEN** `sbmd.js.gc_roots` gauge records a value greater than zero

#### Scenario: GC root count is stable after driver teardown
- **WHEN** all `SafeJSValue` objects for a driver are destroyed and a snapshot is taken
- **THEN** `sbmd.js.gc_roots` gauge does not increase relative to its pre-load value
