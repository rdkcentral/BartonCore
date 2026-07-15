## ADDED Requirements

### Requirement: JS heap pool utilization tracking
The SBMD runtime SHALL record mquickjs heap pool utilization as a histogram named `sbmd.js.heap.used_bytes` using a hybrid sampling strategy: (1) in-activity captures from `SbmdHandlerInvoker::InvokeHandler` after each `JS_Call` while the JS mutex is held, and (2) idle background captures from a thread that fires only when no handler activity has occurred for `BCORE_SBMD_METRICS_SAMPLE_PERIOD_MS` milliseconds (configurable at CMake time, default 30000 ms). The runtime SHALL also record the fixed arena size once at initialization as a gauge named `sbmd.js.heap.arena_bytes`, and SHALL maintain a gauge named `sbmd.js.heap.free_bytes` (sourced from `usage.free_size` — the gap between the heap top and the stack bottom) updated with each snapshot.

#### Scenario: Heap utilization captured during handler invocation
- **WHEN** a JS handler is invoked via `InvokeHandler`
- **THEN** `sbmd.js.heap.used_bytes` histogram gains one observation attributed to that invocation

#### Scenario: Heap utilization captured during idle period
- **WHEN** no handler invocations have occurred for `BCORE_SBMD_METRICS_SAMPLE_PERIOD_MS` milliseconds
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
The system SHALL provide a `MQuickJsRuntime::ForceSnapshot()` public static function that synchronously samples heap stats, records one observation to each pool health metric, and calls `TickleSampler()` to reset the idle thread's timer. This function SHALL be callable without holding the JS mutex (it acquires it internally). The system SHALL also provide `MQuickJsRuntime::TickleSampler()`, which notifies the idle background thread to reset its sleep timer without taking a snapshot. The idle background thread SHALL use `ForceSnapshot()` when its idle timer expires.

#### Scenario: ForceSnapshot produces an immediate observation
- **WHEN** `MQuickJsRuntime::ForceSnapshot()` is called
- **THEN** `sbmd.js.heap.used_bytes` observation count increases by one

### Requirement: Per-invocation heap delta tracking
The SBMD runtime SHALL record the net heap allocation of each JS handler invocation as a histogram named `sbmd.handler.heap_delta_bytes`. The delta SHALL be computed as `heap_used_after - heap_used_before` around the `JS_Call` in `SbmdHandlerInvoker::InvokeHandler`. The histogram SHALL support `"driver"` and `"op_type"` attributes.

#### Scenario: Heap delta recorded per invocation
- **WHEN** a JS handler is invoked via `InvokeHandler` with a `driverName` and `opType`
- **THEN** `sbmd.handler.heap_delta_bytes` contains one new observation attributed to that driver and op type

#### Scenario: Heap delta may be negative
- **WHEN** a JS handler invocation triggers implicit GC (allocation pressure)
- **THEN** `sbmd.handler.heap_delta_bytes` records a negative value, which is valid

### Requirement: Handler invocation duration tracking
The SBMD runtime SHALL record the wall-clock duration of each JS handler invocation as a histogram named `sbmd.handler.duration_ms`. Duration SHALL be measured from immediately before `JS_PushArg` to immediately after `JS_Call` returns in `SbmdHandlerInvoker::InvokeHandler`. The histogram SHALL support `"driver"` and `"op_type"` attributes.

#### Scenario: Duration recorded for each invocation
- **WHEN** a JS handler is invoked with a driver name and op type
- **THEN** `sbmd.handler.duration_ms` gains one observation greater than zero

#### Scenario: Timeout invocations are recorded
- **WHEN** a JS handler exceeds the `BARTON_CONFIG_SBMD_SCRIPT_TIMEOUT_MS` execution deadline and is interrupted
- **THEN** `sbmd.handler.duration_ms` still records the elapsed duration for that invocation

### Requirement: Handler invocation outcome tracking
The SBMD runtime SHALL count handler invocation outcomes using a counter named `sbmd.handler.outcome` with attributes `"driver"`, `"op_type"`, and `"outcome"`. Valid outcome values are: `"success"` (handler returned a valid result), `"exception"` (JS exception thrown, not a timeout), `"timeout"` (script execution deadline exceeded), `"stack_overflow"` (JS_StackCheck failed before call), and `"error"` (handler returned a ResultTerminal::Error). The counter SHALL be incremented in `SbmdHandlerInvoker::InvokeHandler` for the first four outcomes; `"error"` SHALL be incremented at the `ResultTerminal::Error` handling site.

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
- **THEN** `sbmd.handler.outcome` counter for `outcome="stack_overflow"` increments by one

#### Scenario: Error outcome counted
- **WHEN** a JS handler returns a `ResultTerminal::Error`
- **THEN** `sbmd.handler.outcome` counter for `outcome="error"` increments by one

### Requirement: JS mutex wait time tracking
The SBMD runtime SHALL record the time a request spends waiting to acquire the JS runtime mutex as a histogram named `sbmd.js.mutex.wait_ms`. This SHALL be measured from the point a call to `lock_guard<mutex>(MQuickJsRuntime::GetMutex())` begins until the mutex is acquired, in all code paths that acquire the mutex for handler invocation.

#### Scenario: Mutex wait recorded on contention
- **WHEN** two threads attempt to invoke SBMD handlers concurrently
- **THEN** the waiting thread records a non-zero observation in `sbmd.js.mutex.wait_ms`

#### Scenario: Mutex wait near zero when uncontested
- **WHEN** a handler invocation acquires the JS mutex without contention
- **THEN** `sbmd.js.mutex.wait_ms` records an observation close to zero

### Requirement: Driver load cost tracking
The SBMD runtime SHALL record, for each successfully loaded driver: the wall-clock time to evaluate and activate its `.sbmd.js` file as a gauge named `sbmd.driver.load.duration_ms` with attribute `"driver"`, and the net heap bytes consumed during load and activation as a gauge named `sbmd.driver.load.heap_bytes` with attribute `"driver"`. Both SHALL be recorded once per driver at startup in `SbmdFactory::RegisterDriversFromDirectory`.

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
The SBMD runtime SHALL count JS exception events during lifecycle phases using a counter named `sbmd.js.exception` with attributes `"driver"` (if known) and `"phase"`. Valid phase values are: `"init"` (exception during runtime/polyfill initialization) and `"loading"` (exception during driver script evaluation). Handler-phase exceptions are captured by `sbmd.handler.outcome{outcome="exception"}` and are not recorded here.

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
The SBMD runtime SHALL record the total wall-clock duration of each deferred operation — from initial `ExecuteRequestCommand` to `CompletePendingOperation` — as a histogram named `sbmd.deferred.duration_ms` with attributes `"driver"` and `"op_type"` (the originating operation type from `pending.operationCtx.opType`). Duration includes device round-trip time.

#### Scenario: Total duration includes device round-trip
- **WHEN** a deferred operation completes after a device response
- **THEN** `sbmd.deferred.duration_ms` records a value greater than the JS execution time alone

### Requirement: Deferral depth distribution tracking
The SBMD runtime SHALL record the deferral depth at completion of each deferred operation as a histogram named `sbmd.deferred.depth` with attributes `"driver"` and `"op_type"`. Depth 0 means the operation resolved after one round-trip; depth N means the operation re-armed N times.

#### Scenario: Single round-trip records depth zero
- **WHEN** a deferred operation resolves after one device response
- **THEN** `sbmd.deferred.depth` records 0 for that driver

#### Scenario: Re-armed operation records correct depth
- **WHEN** a deferred operation re-arms twice before resolving
- **THEN** `sbmd.deferred.depth` records 2 for that driver

### Requirement: Deferred operation timeout counting
The SBMD runtime SHALL count deferred operations that exceed the 30 s overall deadline using a counter named `sbmd.deferred.timeout` with attributes `"driver"` and `"op_type"`.

#### Scenario: Timeout counted when deadline exceeded
- **WHEN** a deferred operation's `overallDeadline` is exceeded before a device response arrives
- **THEN** `sbmd.deferred.timeout` counter increments for that driver

### Requirement: Deferred operation max-depth-exceeded counting
The SBMD runtime SHALL count deferred operations terminated because they reached `MAX_DEFERRAL_DEPTH` (10) using a counter named `sbmd.deferred.max_depth` with attributes `"driver"` and `"op_type"`.

#### Scenario: Max depth counter increments on depth limit
- **WHEN** a deferred operation re-arms 10 times and `ContinueDeferredChain` rejects the next re-arm
- **THEN** `sbmd.deferred.max_depth` counter increments for that driver

### Requirement: Subsystem metrics initialization
Each instrumented module (`MQuickJsRuntime`, `SbmdHandlerInvoker`, `SbmdFactory`, `SpecBasedMatterDeviceDriver`) SHALL provide `static void InitializeMetrics()` and `static void ShutdownMetrics()` functions. All four `InitializeMetrics()` calls SHALL be made by `SbmdFactory::RegisterDriversFromDirectory` after `MQuickJsRuntime::Initialize()` succeeds. The corresponding `ShutdownMetrics()` calls SHALL be made in the subsystem shutdown path.

#### Scenario: Metrics available after initialization
- **WHEN** all four `InitializeMetrics()` calls complete
- **THEN** `MQuickJsRuntime::ForceSnapshot()` and all recording functions operate without error

#### Scenario: Recording before initialization is safe
- **WHEN** a recording function is called before its module's `InitializeMetrics()`
- **THEN** the call is silently ignored without crashing
