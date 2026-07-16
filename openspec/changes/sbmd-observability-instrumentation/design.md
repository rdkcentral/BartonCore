## Context

SBMDv4 runs all device driver JavaScript through a single shared mquickjs context — a pre-allocated, fixed-size arena (configurable via `BARTON_CONFIG_MQUICKJS_MEMSIZE_BYTES`, default 1 MB) protected by a single mutex. Every resource read/write/execute, attribute report, and event goes through `SbmdHandlerInvoker::InvokeHandler`, which is called with this mutex already held by the caller. Because all drivers compete for the same runtime, the JS context is the primary resource bottleneck of the SBMD subsystem.

The existing observability infrastructure (`core/src/observability/`) provides counter, gauge, and histogram instruments with a JSON dump API. It is fully implemented but has zero call sites in production code today.

This design covers how to wire the observability API into the SBMD runtime without modifying the observability infrastructure itself.

## Goals / Non-Goals

**Goals:**
- Instrument all meaningful SBMD runtime events using the existing observability API
- Each module instruments itself; metric handles live in the module that records them
- Provide a synchronous `ForceSnapshot()` API for on-demand telemetry and tests (usable without holding the JS mutex)
- Attribute per-driver and per-operation-type metrics using the `"driver"`, `"op_type"`, and `"resource_id"` attribute keys
- Test all metric wiring using the real mquickjs runtime (no mocking required)

**Non-Goals:**
- Changes to `core/src/observability/` infrastructure
- Implicit or explicit GC tracking (follow-on)
- `deviceId` attribute (follow-on)
- Production/fleet identifier attributes

## Decisions

### Decision 1: Distributed metric ownership

Metric handles (histograms, gauges, counters) are private static members of the module that records them. Each module provides `static void InitializeMetrics()` and `static void ShutdownMetrics()` functions. `SbmdFactory::RegisterDriversFromDirectory` calls all four `InitializeMetrics()` functions after `MQuickJsRuntime::Initialize()` succeeds.

| Module | Metric handles owned |
|---|---|
| `MQuickJsRuntime` | `sbmd.js.heap.*`, `sbmd.js.mutex.wait_ms`, `sbmd.js.exception` |
| `SbmdHandlerInvoker` | `sbmd.handler.duration_ms`, `sbmd.handler.heap_delta_bytes`, `sbmd.handler.outcome` |
| `SbmdFactory` | `sbmd.driver.load.*`, `sbmd.driver.registered.count` |
| `SpecBasedMatterDeviceDriver` | `sbmd.deferred.*` |

**Cross-module recording:** Two cases require cross-module calls:
- `sbmd.handler.outcome{outcome="error"}` is detected in `SpecBasedMatterDeviceDriver` after `InvokeHandler` returns, but the handle lives in `SbmdHandlerInvoker`. `SbmdHandlerInvoker` exposes `static void RecordOutcomeError(const char *driver, const char *opType, const char *resourceId)`.
- `sbmd.js.mutex.wait_ms` is measured at every mutex acquisition site in `SpecBasedMatterDeviceDriver` — `HandleResourceOp`, the attribute/event handler paths, and the deferred callback paths (`HandleDeferredCommandResponse`, `HandleDeferredCommandError`, `ContinueDeferredChain`). `sbmd.js.heap.*` pool health metrics are recorded from `SbmdHandlerInvoker::InvokeHandler`, and `sbmd.js.exception` is recorded from `SbmdLoader` and `SbmdBundleLoader`. All three sets of handles live in `MQuickJsRuntime`, which exposes `static void RecordMutexWait(double ms)`, `static void RecordJsException(const char *phase, const char *driver)`, and `static void RecordHeapSnapshot(JSContext *ctx)` (requires caller to hold the JS mutex).

**Why not create instruments inline at each call site?** Instrument creation has overhead and the observability spec requires a handle to persist for the lifetime of measurements. Handles are created once in `InitializeMetrics()` and persist until `ShutdownMetrics()`.

### Decision 2: `SbmdHandlerInvoker::InvokeHandler` invocation context parameter

`InvokeHandler` is extended with a single optional parameter: `const OperationContext *opCtx = nullptr`. `OperationContext` is a plain struct defined in `SbmdHandlerInvoker.h`:

```cpp
struct OperationContext {
    const char *driverName = nullptr;
    const char *opType = nullptr;     // originating op type (e.g., "write", "execute")
    const char *resourceId = nullptr; // resource ID for resource ops; null for attribute/event handlers
    std::chrono::steady_clock::time_point startTime;
    // extensible: add fields here without touching InvokeHandler's signature again
};
```

All existing callers compile unchanged (null default). **Two distinct calling patterns:**

- **Synchronous ops** (`HandleResourceOp`): caller stack-allocates `OperationContext` with `driverName`, `opType`, `resourceId` (from `resource->id`), and `startTime = steady_clock::now()`, and passes `&opCtx`. Lifetime is that single call.
- **Deferred ops** (`HandleDeferredCommandResponse`, `HandleDeferredCommandError`, `ContinueDeferredChain`): caller passes `&pending.operationCtx` — the same object initialized when the `PendingOperation` was created and alive for the entire deferred chain.

**Why a struct rather than individual parameters?** A single stable context slot avoids an ever-growing signature as requirements evolve. New fields are added to the struct without touching `InvokeHandler`'s signature or any call site. The context also carries natural operation-scope lifetime semantics: for deferred operations it is the same object from `ExecuteRequestCommand` through `CompletePendingOperation`, enabling `CompletePendingOperation` to read `startTime` from the same context that was active during every `InvokeHandler` call in the chain.

**What is measured around `JS_Call`:**
- `steady_clock::now()` before and after → invocation duration histogram
- `JS_GetMemoryUsage` before and after → heap delta histogram
- Exception / timeout / stack overflow distinction → outcome counter

### Decision 3: Hybrid sampling — in-activity captures + tickleable idle thread

Pool health metrics (`sbmd.js.heap.*`) are recorded from two complementary paths:

**In-activity path:** At the end of every `InvokeHandler` call, while the JS mutex is already held by the caller, `MQuickJsRuntime::RecordHeapSnapshot(ctx)` updates the pool health metrics (`sbmd.js.heap.used_bytes` histogram and `free_bytes`/`peak_bytes` gauges) using the same `JS_GetMemoryUsage` data already captured for the heap delta histogram. `InvokeHandler` then calls `MQuickJsRuntime::TickleSampler()` to notify the idle thread to reset its timer.

**Idle background path:** A `std::thread` in `MQuickJsRuntime` waits on a condition variable with a `BARTON_CONFIG_SBMD_METRICS_SAMPLE_PERIOD_MS` timeout (set via `BCORE_SBMD_METRICS_SAMPLE_PERIOD_MS` CMake option). At the top of each loop iteration it checks `samplerShouldStop` and exits if set. It calls `ForceSnapshot()` only when the wait times out naturally — meaning no `InvokeHandler` activity has occurred for the full idle period. When `wait_until` returns before the deadline, it compares the current `tickleSeq` to the value recorded at the start of the wait. If they differ, a real tickle occurred: reset the timer and re-check `samplerShouldStop` without taking a snapshot. If they are equal, the wakeup was spurious: call `wait_until` again with the same absolute deadline (not a fresh `wait_for`) so that only the remaining time is consumed, not a full new period. This ensures spurious wakeups cannot indefinitely delay an idle-period snapshot.

**Why two paths rather than either alone?**
- Periodic-only: on low-traffic systems (e.g., 20% active, 80% idle), most captures reflect idle-state heap; behavior during activity is sampled at most once per interval.
- In-activity-only: misses the idle baseline, which reveals slow leaks and steady-state overhead between bursts.
- Hybrid: busy systems capture pool health on every invocation and the background thread rarely fires; idle systems rely on the background timer for baseline captures. The tickle mechanism prevents the background thread from taking a redundant snapshot right after a burst of activity.

**`MQuickJsRuntime::RecordHeapSnapshot(JSContext *ctx)`** records the current `JS_GetMemoryUsage` output to the pool health instruments. Requires the caller to hold the JS mutex. Called from `InvokeHandler`.

**`MQuickJsRuntime::ForceSnapshot()`** is a public synchronous function that acquires the JS mutex, calls `RecordHeapSnapshot`, releases the mutex, and calls `TickleSampler()`. Used in unit tests and available for on-demand telemetry. Any snapshot from any path resets the idle timer.

**`MQuickJsRuntime::TickleSampler()`** notifies the background thread's condition variable (`samplerCv`) to reset its idle timer. Atomically increments `tickleSeq` then calls `samplerCv.notify_one()` — both *without* acquiring `samplerCvMutex`. The atomic increment is the mechanism by which the idle thread distinguishes a real tickle from a spurious wakeup (see below); `notify_one()` without the lock is the standard C++ idiom since only the waiter needs it. Avoiding the lock in `TickleSampler()` also prevents a lock-ordering deadlock: the idle thread can hold `samplerCvMutex` while calling `ForceSnapshot()` (which acquires the JS mutex), so `TickleSampler()` — called while holding the JS mutex — must never acquire `samplerCvMutex`. Safe to call from any context including while holding the JS mutex.

```
InvokeHandler (JS mutex held by caller):     Idle background thread:
  → JS_Call                                    loop:
  → JS_GetMemoryUsage                            if samplerShouldStop: exit
  → record sbmd.handler.*                        deadline = now() + SAMPLE_PERIOD_MS
  → RecordHeapSnapshot(ctx) ← pool health        seq = tickleSeq
  → TickleSampler()  ← increments tickleSeq      cv.wait_until(deadline)
                                               if timed_out: ForceSnapshot()
                                               //   real tickle (tickleSeq != seq):
                                               //     no snapshot, reset timer
                                               //   spurious (tickleSeq == seq):
                                               //     no snapshot, keep waiting
```

**Thread startup/shutdown:** The thread is started at the end of `MQuickJsRuntime::Initialize()`. In `Shutdown()`, `samplerShouldStop` is set to `true` and `TickleSampler()` is called to wake the thread immediately, then `periodicSamplerThread` is joined — guarded with `joinable()` to avoid `std::terminate` if `Initialize()` failed before the thread was started.

### Decision 4: `PendingOperation` extended with `OperationContext`

An `OperationContext operationCtx` field is added to `PendingOperation`. It is initialized in `ExecuteRequestCommand` alongside `overallDeadline` with the driver name, originating op type, resource ID, and `startTime = steady_clock::now()`. `CompletePendingOperation` — the single convergence point for all deferred op exits — reads `pending.operationCtx.startTime` to compute total duration and uses `pending.operationCtx.driverName`, `pending.operationCtx.opType`, and `pending.operationCtx.resourceId` as attributes on all deferred lifecycle metrics (`sbmd.deferred.duration_ms`, `sbmd.deferred.depth`, `sbmd.deferred.timeout`, `sbmd.deferred.max_depth`).

**Why `CompletePendingOperation` rather than each individual exit site?** There are ~15 `CompletePendingOperation` call sites. Recording in the single callee avoids duplicating metric logic and ensures no exit path is missed.

### Decision 5: Metric attributes

| Attribute key | Value type | Used on |
|---|---|---|
| `"driver"` | driver name string (e.g., `"door-lock"`) | all per-driver metrics |
| `"op_type"` | `"read"` / `"write"` / `"execute"` / `"attribute_report"` / `"event"` | invocation, outcome, per-operation deferred metrics |
| `"resource_id"` | resource ID string (e.g., `"isOn"`); omitted for attribute/event handlers | `sbmd.handler.*`, `sbmd.deferred.duration_ms`, `sbmd.deferred.depth`, `sbmd.deferred.timeout`, `sbmd.deferred.max_depth` |
| `"outcome"` | `"success"` / `"exception"` / `"timeout"` / `"stack_overflow"` / `"error"` | outcome counter |
| `"reason"` | `"file_read"` / `"eval_failed"` / `"activation_failed"` | driver load failure counter |
| `"phase"` | `"init"` / `"loading"` | JS exception event counter |

**Note on `"op_type"` for deferred invocations:** All `InvokeHandler` calls in a deferred chain — initial invocation and all callbacks — carry the same `opCtx->opType` from `pending.operationCtx`, which holds the originating operation type (e.g., `"write"` or `"execute"`). There are no `"deferred_response"` or `"deferred_error"` op type values; the originating type is used throughout, making it straightforward to aggregate total handler cost for a given operation type without joining separate deferred buckets.

`deviceId` is explicitly deferred (see Non-Goals). The team consensus: include it in a follow-on when attribute indexing strategy for the backend is clearer.

### Decision 6: Testing strategy

**Unit tests** (`core/test/src/SbmdObservabilityTest.cpp`): The existing SBMD GTest suite already uses a real mquickjs runtime (`MQuickJsRuntime::Initialize(512 * 1024)` in `SetUpTestSuite`). The new test file follows the same pattern, calling all four `InitializeMetrics()` functions in `SetUpTestSuite`. Tests invoke handlers via `SbmdHandlerInvoker::InvokeHandler`, then call `observabilityDumpJson()` and parse the JSON to assert:
- Observation count increased (wiring is correct)
- Duration > 0 (timing is working)
- Heap delta is within a plausible range
- Outcome counter incremented for the expected outcome key

**Integration tests** (`testing/test/`): Deferred operation metrics require real Matter command round-trips. A pytest test commissions a simulated device, triggers a write operation that exercises the deferred path, then calls `b_core_client_get_telemetry()` and asserts the expected deferred metrics appear.

**Heap sampler in tests**: Tests call `MQuickJsRuntime::ForceSnapshot()` directly rather than waiting for the background thread. This avoids time-dependent test flakiness entirely.

## Component diagram

```
core/deviceDrivers/matter/sbmd/
│
├── mquickjs/
│   ├── MQuickJsRuntime.cpp                  ← heap metrics, ForceSnapshot(), RecordHeapSnapshot(),
│   │                                        RecordMutexWait(), RecordJsException(), TickleSampler();
│   │                                        in-activity + idle sampling
│   └── SbmdHandlerInvoker.cpp               ← invocation/outcome metrics, RecordOutcomeError();
│                                            InvokeHandler extended
│
├── SbmdFactory.cpp                          ← driver load metrics
└── SpecBasedMatterDeviceDriver.cpp          ← deferred op metrics; PendingOperation.operationCtx (OperationContext)
```

## Risks / Trade-offs

- **In-activity heap snapshot overhead** → `RecordHeapSnapshot` is called at the end of every `InvokeHandler` invocation while the mutex is held. It reads the same `JS_GetMemoryUsage` struct already captured for the heap delta calculation — no additional JS engine work. Overhead is negligible.

- **`JS_GetMemoryUsage` overhead inside `InvokeHandler`** → Called twice per invocation (before + after `JS_Call`). `JS_MEMUSAGE_WALK_HEAP` is not set — the call is an O(1) struct read with no heap walk. As a consequence, `usage.heap_free_blocks` is always 0 (per the mquickjs patch implementation); `sbmd.js.heap.free_bytes` therefore maps to `usage.free_size` (the gap between heap top and stack bottom), not to `heap_free_blocks`. Overhead is negligible.

- **Non-uniform sampling in `sbmd.js.heap.used_bytes`** → Observation rate is proportional to handler call rate plus one sample per idle period, so percentiles are activity-weighted rather than time-weighted. On a busy system, high percentiles reflect heap under load; on an idle system, most observations reflect the settled idle baseline. `sbmd.js.heap.peak_bytes` and the histogram's `min`/`max` fields are not skewed by sampling frequency the way percentiles are, but they still only reflect what was actually observed — heap spikes that occur between snapshots can be missed. Percentiles should be interpreted with the deployment's activity level in mind.

- **Heap delta sign** → If implicit GC fires during an invocation, `heap_used` may drop. The delta will be negative, which is valid and informative. Negative values satisfy `value <= 0` on the first bucket comparison and are counted in the `le=0` bucket (the lowest bound in the histogram). This is expected behavior, not a bug.

- **mquickjs changes conflict risk** → Another developer is actively modifying mquickjs. The `InvokeHandler` changes and `MQuickJsRuntime` sampler thread are in BartonCore C++, not mquickjs itself, so conflicts are unlikely. Coordination is only needed if the other dev changes `JSMemoryUsage` struct layout.

## Open Questions

- **Sample period default**: 30 s is a reasonable default but untested in production. Should `BCORE_SBMD_METRICS_SAMPLE_PERIOD_MS` be a runtime-configurable property instead of a compile-time CMake option? (Deferred — compile-time is fine for now.)
- **Integration test simulated device**: does the existing matter.js virtual device infrastructure support triggering a deferred command path deterministically? Needs verification during implementation.
- **`JS_MEMUSAGE_WALK_HEAP` in dev/Docker builds**: enabling this flag in the mquickjs dev build would give real `heap_free_blocks` data (currently always 0 without it). Walk overhead is unknown — a temporary `sbmd.js.memusage_walk_ms` histogram could be added in dev builds to measure it before deciding whether to enable it in production builds. Deferred pending investigation.
