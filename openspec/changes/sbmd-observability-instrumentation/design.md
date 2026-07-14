## Context

SBMDv4 runs all device driver JavaScript through a single shared mquickjs context — a pre-allocated, fixed-size arena (default 1 MB) protected by a single mutex. Every resource read/write/execute, attribute report, and event goes through `SbmdHandlerInvoker::InvokeHandler`, which acquires this mutex and calls `JS_Call`. Because all drivers compete for the same runtime, the JS context is the primary resource bottleneck of the SBMD subsystem.

The existing observability infrastructure (`core/src/observability/`) provides counter, gauge, and histogram instruments with a JSON dump API. It is fully implemented but has zero call sites in production code today.

This design covers how to wire the observability API into the SBMD runtime without modifying the observability infrastructure itself.

## Goals / Non-Goals

**Goals:**
- Instrument all meaningful SBMD runtime events using the existing observability API
- Centralize all SBMD metric handles in a single `SbmdMetrics` class
- Provide a synchronous `ForceSnapshot()` API for the periodic heap sampler (usable in tests and on-demand telemetry)
- Attribute per-driver and per-operation-type metrics using the `"driver"` and `"op_type"` attribute keys
- Test all metric wiring using the real mquickjs runtime (no mocking required)

**Non-Goals:**
- Changes to `core/src/observability/` infrastructure
- Implicit or explicit GC tracking (follow-on)
- `deviceId` attribute (follow-on)
- Production/fleet identifier attributes

## Decisions

### Decision 1: Centralized `SbmdMetrics` class

All metric handles (histograms, gauges, counters) live as static members of a new `SbmdMetrics` class in `core/deviceDrivers/matter/sbmd/SbmdMetrics.h/.cpp`. Recording functions (`RecordHandlerInvocation`, `RecordDriverLoad`, `RecordDeferredOpComplete`, etc.) are called from the existing code paths.

**Why not create instruments inline at each call site?** Instrument creation has overhead and the observability spec requires a handle to persist for the lifetime of measurements. Centralizing in `SbmdMetrics` mirrors the `MQuickJsRuntime` singleton pattern already established in this subsystem.

### Decision 2: `SbmdHandlerInvoker::InvokeHandler` signature extension

`InvokeHandler` is extended with two optional parameters: `const char *driverName = nullptr, const char *opType = nullptr`. All existing callers compile unchanged (default nulls suppress attribution). Callers that know both values (`HandleResourceOp`, `HandleDeferredCommandResponse`, `HandleDeferredCommandError`, `ContinueDeferredChain`) pass them through.

**Why not wrap `InvokeHandler` in a separate metered function?** The parameters are optional with safe defaults — this avoids a parallel API surface and keeps the call graph simple.

**What is measured around `JS_Call`:**
- `steady_clock::now()` before and after → invocation duration histogram
- `JS_GetMemoryUsage` before and after → heap delta histogram
- Exception / timeout / stack overflow distinction → outcome counter

### Decision 3: Periodic heap sampler as a background thread in `MQuickJsRuntime`

A `std::thread` is started at the end of `MQuickJsRuntime::Initialize()` and stopped in `Shutdown()`. The thread calls `SbmdMetrics::ForceSnapshot()` in a loop, sleeping for `BCORE_SBMD_METRICS_SAMPLE_PERIOD_MS` (default 30000 ms, configurable at CMake time) between calls. `ForceSnapshot()` handles all mutex management internally.

**Why a background thread rather than sampling only on invocations?** Post-invocation sampling misses idle-state heap trends and slow leaks between bursts of activity. Pool utilization between invocations is a meaningful signal.

**`SbmdMetrics::ForceSnapshot()`** is a public synchronous function that performs the same snapshot operation without sleeping. Used in unit tests and available for on-demand telemetry refresh. The thread simply calls this function in its loop.

```
MQuickJsRuntime::Initialize()
  │
  ├── JS_NewContext(memBuffer, memSize)
  ├── SbmdBundleLoader + SbmdLoader injection
  └── starts periodicSamplerThread
         │
         └── loop:
               SbmdMetrics::ForceSnapshot()  ← acquires mutex internally
               sleep(SAMPLE_PERIOD_MS)
```

**Thread safety:** `ForceSnapshot()` acquires `MQuickJsRuntime::GetMutex()` before calling `JS_GetMemoryUsage`, the same mutex held during all JS execution. This prevents concurrent access to the JSContext.

### Decision 4: `PendingOperation` extended with `startTime`

A `std::chrono::steady_clock::time_point startTime` field is added to `PendingOperation`. It is set alongside `overallDeadline` in `ExecuteRequestCommand`. `CompletePendingOperation` — the single convergence point for all deferred op exits — reads it to compute total duration.

**Why `CompletePendingOperation` rather than each individual exit site?** There are ~15 `CompletePendingOperation` call sites. Recording in the single callee avoids duplicating metric logic and ensures no exit path is missed.

`origOpType` (e.g., `"write"` or `"execute"`) is also added to `PendingOperation` so that the deferred metrics carry the originating operation type through the full chain. It is used as the `"op_type"` attribute on `sbmd.deferred.duration_ms`, `sbmd.deferred.depth`, `sbmd.deferred.timeout`, and `sbmd.deferred.max_depth`.

### Decision 5: Metric attributes

| Attribute key | Value type | Used on |
|---|---|---|
| `"driver"` | driver name string (e.g., `"door-lock"`) | all per-driver metrics |
| `"op_type"` | `"read"` / `"write"` / `"execute"` / `"attribute_report"` / `"event"` / `"deferred_response"` / `"deferred_error"` | invocation, outcome, deferred |
| `"outcome"` | `"success"` / `"exception"` / `"timeout"` / `"stack_overflow"` / `"error"` | outcome counter |
| `"reason"` | `"file_read"` / `"eval_failed"` / `"activation_failed"` | driver load failure counter |
| `"phase"` | `"init"` / `"loading"` | JS exception event counter |

**Note on `"op_type"` for deferred lifecycle metrics:** For handler callback invocations going through `InvokeHandler` (op_type `"deferred_response"` / `"deferred_error"`), `"op_type"` reflects the callback type. For deferred lifecycle metrics (`sbmd.deferred.duration_ms`, `sbmd.deferred.depth`, `sbmd.deferred.timeout`, `sbmd.deferred.max_depth`), `"op_type"` uses `pending.origOpType` — the originating operation type (e.g., `"write"` or `"execute"`) captured when the `PendingOperation` was created.

`deviceId` is explicitly deferred (see Non-Goals). The team consensus: include it in a follow-on when attribute indexing strategy for the backend is clearer.

### Decision 6: Testing strategy

**Unit tests** (`core/test/src/SbmdMetricsTest.cpp`): The existing SBMD GTest suite already uses a real mquickjs runtime (`MQuickJsRuntime::Initialize(512 * 1024)` in `SetUpTestSuite`). The new test file follows the same pattern, adding `SbmdMetrics::Initialize()` to the setup. Tests invoke handlers via `SbmdHandlerInvoker::InvokeHandler`, then call `observabilityDumpJson()` and parse the JSON to assert:
- Observation count increased (wiring is correct)
- Duration > 0 (timing is working)
- Heap delta is within a plausible range
- Outcome counter incremented for the expected outcome key

**Integration tests** (`testing/test/`): Deferred operation metrics require real Matter command round-trips. A pytest test commissions a simulated device, triggers a write operation that exercises the deferred path, then calls `b_core_client_get_telemetry()` and asserts the expected deferred metrics appear.

**Periodic sampler in tests**: Tests call `SbmdMetrics::ForceSnapshot()` directly rather than waiting for the background thread. This avoids time-dependent test flakiness entirely.

## Component diagram

```
core/deviceDrivers/matter/sbmd/
│
├── SbmdMetrics.h / SbmdMetrics.cpp          ← NEW: owns all metric handles
│   ├── Initialize() / Shutdown()
│   ├── ForceSnapshot()                       ← synchronous heap snapshot
│   ├── RecordHandlerInvocation(...)
│   ├── RecordDriverLoad(...)
│   ├── RecordMemorySnapshot(...)
│   └── RecordDeferredOpComplete(...)
│
├── mquickjs/
│   ├── MQuickJsRuntime.cpp                  ← adds periodicSamplerThread
│   └── SbmdHandlerInvoker.cpp               ← InvokeHandler extended
│
├── SbmdFactory.cpp                          ← driver load cost metrics
└── SpecBasedMatterDeviceDriver.cpp          ← deferred op metrics; PendingOperation.startTime + origOpType
                                                mutex wait timing
```

## Risks / Trade-offs

- **Mutex contention from sampler thread** → The periodic sampler acquires the JS mutex briefly to call `JS_GetMemoryUsage`. At 30 s intervals this is negligible. If sample period is lowered significantly (e.g., for tests), brief extra mutex contention is introduced. `ForceSnapshot()` mitigates this in tests by making the thread unnecessary.

- **`JS_GetMemoryUsage` overhead inside `InvokeHandler`** → Called twice per invocation (before + after `JS_Call`). The mquickjs implementation is an O(1) struct read (no heap walk; the `JS_MEMUSAGE_WALK_HEAP` flag is not set). Overhead is negligible.

- **Heap delta sign** → If implicit GC fires during an invocation, `heap_used` may drop. The delta will be negative, which is valid and informative. Histograms accept negative values (the OTel SDK default buckets go below zero). This is expected behavior, not a bug.

- **mquickjs changes conflict risk** → Another developer is actively modifying mquickjs. The `InvokeHandler` changes and `MQuickJsRuntime` sampler thread are in BartonCore C++, not mquickjs itself, so conflicts are unlikely. Coordination is only needed if the other dev changes `JSMemoryUsage` struct layout.

## Open Questions

- **Sample period default**: 30 s is a reasonable default but untested in production. Should `BCORE_SBMD_METRICS_SAMPLE_PERIOD_MS` be a runtime-configurable property instead of a compile-time CMake option? (Deferred — compile-time is fine for now.)
- **Integration test simulated device**: does the existing matter.js virtual device infrastructure support triggering a deferred command path deterministically? Needs verification during implementation.
