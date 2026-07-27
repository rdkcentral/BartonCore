## Context

SBMDv4 runs all device driver JavaScript through a single shared mquickjs context — a pre-allocated, fixed-size arena (configurable via `BARTON_CONFIG_MQUICKJS_MEMSIZE_BYTES`, default 1 MB) protected by a single mutex. Every resource read/write/execute, attribute report, and event goes through `SbmdHandlerInvoker::InvokeHandler`, which is called with this mutex already held by the caller. Because all drivers compete for the same runtime, the JS context is the primary resource bottleneck of the SBMD subsystem.

The existing observability infrastructure (`core/src/observability/`) provides counter, gauge, and histogram instruments with a JSON dump API. The dump API (`observabilityDumpJson()`) is already called in production (e.g., `b_core_client_get_telemetry()`), but the metric-recording call sites — `observabilityCounterAdd`, `observabilityGaugeRecord`, `observabilityHistogramRecord` — have zero production callers today.

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

### Decision 1: Instance-based per-subsystem metrics classes owned by their monitored classes

Each SBMD subsystem's metrics live in a dedicated class inside `core/deviceDrivers/matter/sbmd/metrics/`. Each class owns its own handles and recording methods; instruments are initialized lazily on first use via a private `InitInstruments()` helper:

| Class | Metrics covered | Ownership |
|---|---|---|
| `MQuickJsRuntimeMetrics` | `sbmd.js.heap.*`, `sbmd.js.mutex.wait_ms`, `sbmd.js.exception`, `sbmd.js.gc.*`, `sbmd.js.gc_roots` | Non-static member of `MQuickJsRuntime` singleton |
| `SbmdHandlerInvokerMetrics` | `sbmd.handler.duration_ms`, `sbmd.handler.heap_delta_bytes`, `sbmd.handler.outcome` | Static member of `SbmdHandlerInvoker` |
| `SbmdFactoryMetrics` | `sbmd.driver.load.*`, `sbmd.driver.registered.count` | Non-static member of `SbmdFactory` singleton |
| `SpecBasedMatterDeviceDriverMetrics` | `sbmd.deferred.*` | Static member of `SpecBasedMatterDeviceDriver` (class-wide aggregates) |

All metrics classes are **instance-based** (no static methods or static handles). This aligns with the existing patterns in the codebase — static-only utility classes are an anti-pattern that was not to be continued with new code.

**Access control:** All methods and data members of each metrics class live in the default-private section. A single `friend class [OwnerClass]` declaration grants the owning class exclusive compile-time access, making unauthorized access a compile error rather than a documentation convention.

**Lifecycle:** Metric instruments are initialized lazily — the first recording call in each class triggers `InitInstruments()`, a private idempotent helper that creates all handles for that class and is a no-op thereafter. No explicit initialization call is needed from any owner class. Handles persist for the process lifetime. `RegisterDrivers()` is idempotent: if drivers are already loaded it returns `true` immediately, so a second call is always safe.

**`MQuickJsRuntime` singleton pattern:** `MQuickJsRuntime` is converted from a static-only class to a Meyer's singleton (`static MQuickJsRuntime &GetInstance()`). All existing public static methods are preserved as static wrappers that call `GetInstance()` internally, so all callers are unchanged. The `metrics` member is a real non-static `MQuickJsRuntimeMetrics` instance owned by the singleton.

**Cross-module recording:** Calls from modules outside the owning class go through `MQuickJsRuntime` static wrapper methods: `MQuickJsRuntime::RecordJsException(...)`, `MQuickJsRuntime::RecordMutexWait(...)`, `MQuickJsRuntime::RecordHeapSnapshot(...)`, `MQuickJsRuntime::TickleSampler()`, and `MQuickJsRuntime::ForceSnapshot()` — each delegates to `GetInstance().metrics.*`. `SbmdHandlerInvoker` calls these wrappers for runtime metrics and `metrics.RecordInvocation(...)` / `metrics.RecordOutcome(...)` for its own static `metrics` member. `SpecBasedMatterDeviceDriver` calls `MQuickJsRuntime::RecordMutexWait(...)` and `metrics.RecordDeferred*(...)` on its own static `metrics` member. `SbmdFactory` calls `metrics.RecordDriverLoad*(...)` on its own instance member.

All five `sbmd.handler.outcome` outcome values — `"success"`, `"exception"`, `"timeout"`, `"stack_overflow"`, and `"error"` — are recorded exclusively within `SbmdHandlerInvoker::InvokeHandler`. `"error"` is emitted after `SbmdResultExecutor::Parse()` returns when the terminal is `ResultTerminal::Error`; `"success"` is emitted for all other parseable terminals. If `Parse()` returns `std::nullopt` (malformed driver result), no outcome is recorded. `SpecBasedMatterDeviceDriver::ExecuteTerminal` and `ContinueDeferredChain` do NOT record outcomes.

**`SbmdFactory::RegisterDrivers()` is idempotent:** if `drivers` is non-empty it returns `true` immediately. `SpecBasedMatterDeviceDriver` wrappers released to `deviceDriverManager` hold raw `SbmdDriver*` pointers for the process lifetime and cannot be safely unregistered, so re-registration is prevented rather than attempted.

**`InitInstruments()` is idempotent in each class.** Each implementation checks its sentinel handle for non-null and returns immediately if instruments are already created, preventing handle leaks.

**Why a private `InitInstruments()` rather than inline creation at each recording call site?** Multiple recording methods share the same set of handles; centralizing creation in a single shared helper avoids duplicating the creation calls. The instruments persist for the process lifetime.

### Decision 2: `SbmdHandlerInvoker::InvokeHandler` invocation context parameter

`InvokeHandler` is extended with a single optional parameter: `const OperationContext *opCtx = nullptr`. `OperationContext` is a plain struct defined in `SbmdHandlerInvoker.h`:

```cpp
struct OperationContext {
    std::string driverName;  // computed from filePath at assignment time
    std::string opType;      // originating op type (e.g., "write", "execute")
    std::string resourceId;  // resource ID for the operation (empty when not applicable)
    std::chrono::steady_clock::time_point startTime;
    // extensible: add fields here without touching InvokeHandler's signature again
};
```

All existing callers compile unchanged (null default). **Two distinct calling patterns:**

- **Synchronous ops** (`HandleResourceOp`): caller stack-allocates `OperationContext` with `driverName`, `opType`, `resourceId` (from `resource->id`), and `startTime = steady_clock::now()`, and passes `&opCtx`. Lifetime is that single call.
- **Deferred ops** (`HandleDeferredCommandResponse`, `HandleDeferredCommandError`, `ContinueDeferredChain`): caller passes `&pending.operationCtx` — the same object initialized when the `PendingOperation` was created and alive for the entire deferred chain.

**Why a struct rather than individual parameters?** A single stable context slot avoids an ever-growing signature as requirements evolve. New fields are added to the struct without touching `InvokeHandler`'s signature or any call site. The context also carries natural operation-scope lifetime semantics: for deferred operations it is the same object from `ExecuteRequestCommand` through `CompletePendingOperation`, enabling `CompletePendingOperation` to read `startTime` from the same context that was active during every `InvokeHandler` call in the chain.

**What is measured (window: immediately before `JS_PushArg` through immediately after `JS_Call` returns):**
- `steady_clock::now()` before `JS_PushArg` and after `JS_Call` → invocation duration histogram
- `JS_GetMemoryUsage` before `JS_PushArg` and after `JS_Call` → heap delta histogram (includes argument-marshalling allocations)
- Exception / timeout / stack overflow distinction → outcome counter

### Decision 3: Hybrid sampling — in-activity captures + tickleable idle thread

Pool health metrics (`sbmd.js.heap.*`) are recorded from two complementary paths:

**In-activity path:** At the end of every `InvokeHandler` call, while the JS mutex is already held by the caller, `MQuickJsRuntimeMetrics::RecordHeapSnapshot(const JSMemoryUsage &usage, size_t gcRootCount)` updates the pool health metrics (`sbmd.js.heap.used_bytes` histogram and `free_bytes`/`peak_bytes`/`gc_roots` gauges) using the `JSMemoryUsage` struct already captured by the post-`JS_Call` `JS_GetMemoryUsage` call and the `gcRootCount` from `JS_GetGCRootCount(ctx)` — both computed by the caller while the mutex is held. `InvokeHandler` then calls `MQuickJsRuntimeMetrics::TickleSampler()` to notify the idle thread to reset its timer.

**Idle background path:** A `std::thread` in `MQuickJsRuntime` waits on a condition variable with a `BARTON_CONFIG_SBMD_METRICS_SAMPLE_PERIOD_MS` timeout (set via `BCORE_SBMD_METRICS_SAMPLE_PERIOD_MS` CMake option). At the top of each loop iteration it checks `samplerShouldStop` and exits if set. It calls `ForceSnapshot()` only when the wait times out naturally — meaning no `InvokeHandler` activity has occurred for the full idle period. When `wait_until` returns before the deadline, it compares the current `tickleSeq` to the value recorded at the start of the wait. If they differ, a real tickle occurred: reset the timer and re-check `samplerShouldStop` without taking a snapshot. If they are equal, the wakeup was spurious: call `wait_until` again with the same absolute deadline (not a fresh `wait_for`) so that only the remaining time is consumed, not a full new period. This ensures spurious wakeups cannot indefinitely delay an idle-period snapshot.

**Why two paths rather than either alone?**
- Periodic-only: on low-traffic systems (e.g., 20% active, 80% idle), most captures reflect idle-state heap; behavior during activity is sampled at most once per interval.
- In-activity-only: misses the idle baseline, which reveals slow leaks and steady-state overhead between bursts.
- Hybrid: busy systems capture pool health on every invocation and the background thread rarely fires; idle systems rely on the background timer for baseline captures. The tickle mechanism prevents the background thread from taking a redundant snapshot right after a burst of activity.

**`MQuickJsRuntimeMetrics::RecordHeapSnapshot(const JSMemoryUsage &usage, size_t gcRootCount)`** records the provided `JSMemoryUsage` data and GC root count to the pool health instruments. Requires the caller to hold the JS mutex. The caller is responsible for reading `JS_GetGCRootCount(ctx)` while holding the mutex and passing the result as `gcRootCount` — this avoids a TOCTOU race with `Shutdown()` that would arise from calling `JS_GetGCRootCount` inside `RecordHeapSnapshot` after the caller releases the mutex. Called from `InvokeHandler` and from `ForceSnapshot()` with a freshly read struct and root count.

**`MQuickJsRuntimeMetrics::ForceSnapshot()`** is a public synchronous function that acquires the JS mutex, calls `RecordHeapSnapshot` (which advances `sbmd.js.heap.peak_bytes` only if the current value exceeds the recorded peak), releases the mutex, and calls `TickleSampler()`. It SHALL silently no-op — without acquiring the JS mutex — if `!jsContextReady`. `GetSharedContext()` MUST NOT be used for this check: calling it without the mutex held is a data race, and calling it with the mutex held defeats the purpose of a lock-free early-exit; instead, `jsContextReady` is a dedicated `std::atomic<bool>` set to `true` (with `memory_order_release`) at the end of `Initialize()` and cleared to `false` at the start of `Shutdown()`, providing a safe lock-free early-exit path. Used in unit tests and available for on-demand telemetry. Any snapshot from any path resets the idle timer.

**`MQuickJsRuntimeMetrics::TickleSampler()`** notifies the background thread's condition variable (`samplerCv`) to reset its idle timer. Atomically increments `tickleSeq` then calls `samplerCv.notify_one()` — both *without* acquiring `samplerCvMutex`. The atomic increment is the mechanism by which the idle thread distinguishes a real tickle from a spurious wakeup (see below); `notify_one()` without the lock is the standard C++ idiom since only the waiter needs it. Avoiding the lock in `TickleSampler()` also prevents a lock-ordering deadlock: the idle thread can hold `samplerCvMutex` while calling `ForceSnapshot()` (which acquires the JS mutex), so `TickleSampler()` — called while holding the JS mutex — must never acquire `samplerCvMutex`. Safe to call from any context including while holding the JS mutex.

```
InvokeHandler (JS mutex held by caller):     Idle background thread:
  → JS_Call                                    loop:
  → JS_GetMemoryUsage                            if samplerShouldStop: exit
  → record sbmd.handler.*                        deadline = now() + std::chrono::milliseconds(BARTON_CONFIG_SBMD_METRICS_SAMPLE_PERIOD_MS)
  → RecordHeapSnapshot(usage_after, gcRoots) ← pool health  seq = tickleSeq
  → TickleSampler()  ← increments tickleSeq      cv.wait_until(lock, deadline)
                                               if timed_out: ForceSnapshot()
                                               //   real tickle (tickleSeq != seq):
                                               //     no snapshot, reset timer
                                               //   spurious (tickleSeq == seq):
                                               //     no snapshot, keep waiting
```

**Thread startup/shutdown:** The thread is started at the end of `MQuickJsRuntime::Initialize()`. In `Shutdown()`, `jsContextReady` is cleared to `false` first (so any concurrent `ForceSnapshot()` caller sees the flag cleared before the context is torn down), then `samplerShouldStop` is set to `true`, `TickleSampler()` is called to wake the thread immediately, and `periodicSamplerThread` is joined — guarded with `joinable()` to avoid `std::terminate` if `Initialize()` failed before the thread was started — before `JS_FreeContext` is called.

### Decision 4: `PendingOperation` extended with `OperationContext`

An `OperationContext operationCtx` field is added to `PendingOperation`. It is initialized in `ExecuteRequestCommand` by copying from the `const OperationContext *opCtx` threaded in from `HandleResourceOp` via `ExecuteTerminal` (with `driverName` falling back to `driver->GetDriverStem()` when `opCtx` is null); `startTime` is always set fresh to `steady_clock::now()` at the moment the deferred op is registered. `CompletePendingOperation` — the single convergence point for all deferred op exits — reads `pending.operationCtx.startTime` to compute total duration and uses `pending.operationCtx.driverName`, `pending.operationCtx.opType`, and `pending.operationCtx.resourceId` as attributes on all deferred lifecycle metrics (`sbmd.deferred.duration_ms`, `sbmd.deferred.depth`, `sbmd.deferred.timeout`, `sbmd.deferred.max_depth`).

**Why `CompletePendingOperation` rather than each individual exit site?** There are ~15 `CompletePendingOperation` call sites. Recording in the single callee avoids duplicating metric logic and ensures no exit path is missed.

### Decision 5: Metric attributes

| Attribute key | Value type | Used on |
|---|---|---|
| `"driver"` | driver name string (e.g., `"door-lock"`) | all per-driver metrics |
| `"op_type"` | `"read"` / `"write"` / `"execute"` / `"attribute_report"` / `"event"` / `"command"` / `"seed"` | invocation, outcome, per-operation deferred metrics |
| `"resource_id"` | resource ID string (e.g., `"isOn"`); omitted for attribute/event handlers | `sbmd.handler.*`, `sbmd.deferred.duration_ms`, `sbmd.deferred.depth`, `sbmd.deferred.timeout`, `sbmd.deferred.max_depth` |
| `"outcome"` | `"success"` / `"exception"` / `"timeout"` / `"stack_overflow"` / `"error"` | outcome counter |
| `"reason"` | `"file_read"` / `"eval_failed"` / `"activation_failed"` | driver load failure counter |
| `"phase"` | `"init"` / `"loading"` | JS exception event counter |

**Note on `"driver"` for `sbmd.js.exception`:** The attribute is omitted entirely for `"init"`-phase exceptions (no driver context); it is set to the filename stem for `"loading"`-phase exceptions. Empty string and placeholder values are not used.

**Note on `"op_type"` for deferred invocations:** All `InvokeHandler` calls in a deferred chain — initial invocation and all callbacks — carry the same `opCtx->opType` from `pending.operationCtx`, which holds the originating operation type (e.g., `"write"` or `"execute"`). There are no `"deferred_response"` or `"deferred_error"` op type values; the originating type is used throughout, making it straightforward to aggregate total handler cost for a given operation type without joining separate deferred buckets. `ExecuteReadAttribute` (the inline synchronous path for `readAttribute` terminals) propagates the same `opCtx` pointer from its caller through its own `InvokeHandler` calls and any recursive `ExecuteTerminal` invocation, ensuring consistent attribution even when a `readAttribute` terminal is chained before a `requestCommand`.

`deviceId` is explicitly deferred (see Non-Goals). The team consensus: include it in a follow-on when attribute indexing strategy for the backend is clearer.

### Decision 6: Testing strategy

**Unit tests** (`core/test/src/SbmdObservabilityTest.cpp`): The existing SBMD GTest suite already uses a real mquickjs runtime (`MQuickJsRuntime::Initialize(512 * 1024)` in `SetUpTestSuite`). `MQuickJsRuntimeMetrics` and `SbmdHandlerInvokerMetrics` are compiled into the test binary. `SbmdFactoryMetrics` and `SpecBasedMatterDeviceDriverMetrics` are omitted because those monitored classes pull in Matter SDK symbols unavailable in unit-test builds. No explicit metrics initialization is needed in setup — instruments are created lazily on first use. Tests invoke handlers via `SbmdHandlerInvoker::InvokeHandler`, then call `observabilityDumpJson()` and parse the JSON to assert:
- Observation count increased (wiring is correct)
- Duration > 0 (timing is working)
- Heap delta is within a plausible range
- Outcome counter incremented for the expected outcome key

**Integration tests** (`testing/test/`): Deferred operation metrics require real Matter command round-trips. A pytest test commissions a simulated device, triggers an execute operation that exercises the deferred path, then calls `b_core_client_get_telemetry()` and asserts the expected deferred metrics appear. The `sbmd.deferred.duration_ms`, `sbmd.deferred.depth`, and `sbmd.deferred.in_flight` metrics are covered by integration tests in `testing/test/sbmd_deferred_metrics_test.py`. Coverage for `sbmd.deferred.timeout` and `sbmd.deferred.max_depth` is **planned as future work** (tasks 9.6 and 9.7 respectively): each requires a new test-only driver targeting an unused Matter device type, and the max-depth scenario additionally requires a virtual device that responds at least 11 times.

**Test-only driver for deferred metrics**: Rather than modifying the production door-lock driver to use `requestCommand`, the integration tests use a dedicated test-only SBMD driver (`testing/resources/sbmd-specs/deferred-command-test.sbmd.js`) that targets the Dimmable Plug-in Unit device type (`0x010B`), which is not claimed by any production driver. The corresponding matter.js virtual device is `DeferredCmdTestDevice.js`. The driver's `toggle` execute resource uses `requestCommand` for the OnOff Toggle command, producing `sbmd.deferred.*` metrics.

Using `requestCommand` in the production door-lock driver was considered but deferred: it would introduce new synchronization on the Matter event loop thread per lock/unlock response and creates a competing timeout relationship between the outer 15-second `MATTER_ASYNC_DEVICE_TIMEOUT_SECS` and the inner 30-second `PendingOperation::DEFAULT_OVERALL_TIMEOUT_MS`. This path had not been validated in field conditions.

**TODO**: A future production SBMD driver that naturally uses `requestCommand` can replace the test-only driver for the deferred-metrics integration tests.

**Heap sampler in tests**: Tests call `MQuickJsRuntime::ForceSnapshot()` directly rather than waiting for the background thread. This avoids time-dependent test flakiness entirely.

### Decision 7: Compile-time opt-out via `BCORE_SBMD_METRICS`

A `BCORE_SBMD_METRICS` ON/OFF CMake option (default ON, compiled definition `BARTON_CONFIG_SBMD_METRICS`) allows completely disabling all SBMD metrics overhead at compile time. When OFF:

- Each metrics header wraps its real class declaration in `#ifdef BARTON_CONFIG_SBMD_METRICS` and provides an empty inline stub class in the `#else` branch; all callers require zero source changes
- Each metrics `.cpp` wraps its entire body in `#ifdef BARTON_CONFIG_SBMD_METRICS`; the files compile to empty translation units and the inline stubs resolve all call sites
- The sampler thread is never started (`StartSampler()` / `StopSampler()` are empty stubs)
- The timing measurements in `AcquireJsMutex()` are guarded with `#ifdef BARTON_CONFIG_SBMD_METRICS` so clock reads are eliminated even in unoptimized builds
- All measurement overhead in `SbmdHandlerInvoker::InvokeHandler` — pre/post `JS_GetMemoryUsage`, `steady_clock::now()` timing, `JS_GetGCRootCount()`, and `WasInterrupted()` — is guarded with `#ifdef BARTON_CONFIG_SBMD_METRICS`; with metrics disabled the function pays only for `JS_Call` itself and does not reference the patched `JS_GetGCRootCount` symbol
- All measurement overhead in `SbmdFactory::RegisterDriversFromDirectory` — `loadStart`/`loadEnd` (`steady_clock::now()`), `usageBefore`/`usageAfter` (`JS_GetMemoryUsage`), duration/delta math, and all `metrics.Record*` calls — is guarded with `#ifdef BARTON_CONFIG_SBMD_METRICS`; driver loading itself is unaffected
- `InitInstruments()` in each metrics class is a no-op stub; all recording methods are empty stubs; no initialization or shutdown calls are needed

When `BCORE_OBSERVABILITY_BACKEND=none`, the observability backend API calls are already no-ops (via `observabilityNoop.c`), but the SBMD metrics infrastructure — sampler thread, timing measurements — still runs. Setting `BCORE_SBMD_METRICS=OFF` eliminates that remaining overhead for a true zero-cost build.

## Component diagram

```
core/deviceDrivers/matter/sbmd/
│
├── metrics/                                 ← all metric classes (new subdirectory)
│   ├── MQuickJsRuntimeMetrics.h/.cpp        ← instance-based; owned by MQuickJsRuntime singleton;
│   │                                        handles + recording for sbmd.js.*;
│   │                                        ForceSnapshot, TickleSampler, RecordHeapSnapshot,
│   │                                        RecordArenaSize, RecordMutexWait, RecordJsException,
│   │                                        GCCallback (static, opaque = &metrics); background sampler thread
│   ├── SbmdHandlerInvokerMetrics.h/.cpp     ← instance-based; static member of SbmdHandlerInvoker;
│   │                                        handles + recording for sbmd.handler.*;
│   │                                        RecordInvocation, RecordOutcome
│   ├── SbmdFactoryMetrics.h/.cpp            ← instance-based; non-static member of SbmdFactory;
│   │                                        handles + recording for sbmd.driver.*;
│   │                                        RecordDriverLoadFailure, RecordDriverLoadSuccess,
│   │                                        RecordRegisteredDriverCount
│   └── SpecBasedMatterDeviceDriverMetrics.h/.cpp
│                                            ← instance-based; static member of SpecBasedMatterDeviceDriver;
│                                            handles + recording for sbmd.deferred.*;
│                                            RecordDeferredStart, RecordDeferredTimeout,
│                                            RecordDeferredMaxDepth, RecordDeferredComplete
│
├── mquickjs/
│   ├── MQuickJsRuntime.h/.cpp               ← Meyer's singleton (GetInstance()); MQuickJsRuntimeMetrics metrics;
│   │                                        all public methods remain static wrappers calling GetInstance();
│   │                                        static wrappers: RecordJsException, RecordMutexWait,
│   │                                        RecordHeapSnapshot, TickleSampler, ForceSnapshot;
│   │                                        Initialize() calls metrics.StartSampler();
│   │                                        Shutdown() calls metrics.StopSampler()
│   ├── SbmdHandlerInvoker.h/.cpp            ← static SbmdHandlerInvokerMetrics metrics;
│   │                                        InvokeHandler calls MQuickJsRuntime wrappers + metrics.Record*;
│   │                                        InvokeHandler extended with OperationContext
│   └── SbmdBundleLoader.cpp                 ← calls MQuickJsRuntime::RecordJsException(...)
│
├── SbmdFactory.h/.cpp                       ← SbmdFactoryMetrics metrics (instance member);
│                                            RegisterDrivers() loads drivers (instruments lazily initialized on first use);
│                                            RegisterDrivers() is idempotent (no-op if drivers loaded)
└── SpecBasedMatterDeviceDriver.h/.cpp       ← static SpecBasedMatterDeviceDriverMetrics metrics;
                                             calls MQuickJsRuntime::RecordMutexWait(...);
                                             calls metrics.RecordDeferred*(...);
                                             PendingOperation.operationCtx (OperationContext)
```

## Risks / Trade-offs

- **In-activity heap snapshot overhead** → `RecordHeapSnapshot` is called at the end of every `InvokeHandler` invocation while the mutex is held. It reads the same `JS_GetMemoryUsage` struct already captured for the heap delta calculation — no additional JS engine work. Overhead is negligible.

- **`JS_GetMemoryUsage` overhead inside `InvokeHandler`** → Called twice per invocation (before `JS_PushArg` and after `JS_Call` returns). `JS_MEMUSAGE_WALK_HEAP` is not set — the call is an O(1) struct read with no heap walk. As a consequence, `usage.heap_free_blocks` is always 0 (per the mquickjs patch implementation); `sbmd.js.heap.free_bytes` therefore maps to `usage.free_size` (the gap between heap top and stack bottom), not to `heap_free_blocks`. Overhead is negligible.

- **Non-uniform sampling in `sbmd.js.heap.used_bytes`** → Observation rate is proportional to handler call rate plus one sample per idle period, so percentiles are activity-weighted rather than time-weighted. On a busy system, high percentiles reflect heap under load; on an idle system, most observations reflect the settled idle baseline. `sbmd.js.heap.peak_bytes` and the histogram's `min`/`max` fields are not skewed by sampling frequency the way percentiles are, but they still only reflect what was actually observed — heap spikes that occur between snapshots can be missed. Percentiles should be interpreted with the deployment's activity level in mind.

- **Heap delta sign** → If implicit GC fires during an invocation, `heap_used` may drop. The delta will be negative, which is valid and informative. Negative values satisfy `value <= 0` on the first bucket comparison and are counted in the `le=0` bucket (the lowest bound in the histogram). This is expected behavior, not a bug.

- **mquickjs changes conflict risk** → Another developer is actively modifying mquickjs. The `InvokeHandler` changes and `MQuickJsRuntime` sampler thread are in BartonCore C++, not mquickjs itself, so conflicts are unlikely. Coordination is only needed if the other dev changes `JSMemoryUsage` struct layout.

## Open Questions

- **Sample period default**: 30 s is a reasonable default but untested in production. Should `BCORE_SBMD_METRICS_SAMPLE_PERIOD_MS` be a runtime-configurable property instead of a compile-time CMake option? (Deferred — compile-time is fine for now.)
- **Integration test simulated device**: does the existing matter.js virtual device infrastructure support triggering a deferred command path deterministically? Needs verification during implementation.
- **`JS_MEMUSAGE_WALK_HEAP` in dev/Docker builds**: enabling this flag in the mquickjs dev build would give real `heap_free_blocks` data (currently always 0 without it). Walk overhead is unknown — a temporary `sbmd.js.memusage_walk_ms` histogram could be added in dev builds to measure it before deciding whether to enable it in production builds. Deferred pending investigation.
