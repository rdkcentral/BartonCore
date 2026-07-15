## Why

The SBMDv4 JavaScript runtime (mquickjs) runs inside a fixed 1 MB arena with a single shared JS context, making it a natural resource bottleneck. Today, nothing is measured: memory pressure, handler latency, GC activity, deferred operation health, and driver load cost are all invisible. Instrumenting the SBMD runtime with the existing observability API gives operators and developers the data needed to diagnose performance issues, detect failures, and understand system health.

## What Changes

- **Distributed metric instrumentation**: metric handles are owned by the module that records them. `MQuickJsRuntime` gains heap metrics and `ForceSnapshot()`; `SbmdHandlerInvoker` gains invocation and outcome metrics; `SbmdFactory` gains driver load metrics; `SpecBasedMatterDeviceDriver` gains deferred operation metrics.
- **Hybrid heap sampler** in `MQuickJsRuntime`: pool health metrics are captured in-activity (after every `InvokeHandler` call) and by a background thread that fires only after a configurable idle period with no handler activity.
- **Per-invocation instrumentation** in `SbmdHandlerInvoker::InvokeHandler`: measures duration, heap delta, and outcome for every JS handler call. Signature extended with a single optional `const SbmdOperationContext *opCtx = nullptr` parameter; the struct is extensible without further signature changes and carries natural operation-scope lifetime for deferred chains.
- **Driver load instrumentation** in `SbmdFactory::RegisterDriversFromDirectory`: measures memory cost and wall-clock time for loading and activating each `.sbmd.js` file.
- **Deferred operation instrumentation** in `SpecBasedMatterDeviceDriver`: tracks in-flight count, total duration (attributed by driver and originating op type), deferral depth, timeout events, and max-depth-exceeded events. Adds `SbmdOperationContext operationCtx` field to `PendingOperation` struct.
- **JS exception event counter** capturing exceptions during driver loading and runtime initialization phases.
- **Driver load failure counter** capturing all failure modes in the driver registration loop.
- **Unit tests** (GTest): exercise all metric wiring against the real mquickjs runtime using `observabilityDumpJson()` to assert observations.
- **Integration tests** (pytest): verify deferred operation metrics end-to-end via `b_core_client_get_telemetry()`.

## Capabilities

### New Capabilities

- `sbmd-runtime-observability`: Defines the complete set of SBMD runtime metrics — their names, instrument types, attributes, sampling strategy, and testing requirements. Covers JS heap health, handler invocation timing and outcomes, driver load cost, deferred operation lifecycle, driver load failures, and JS exception events.

### Modified Capabilities

<!-- No existing spec-level requirements are changing. The existing observability-metrics spec anticipated SBMD metrics in a scenario but left them unspecified; this change defines them without modifying the infrastructure spec's requirements. -->

## Impact

- **Affected code**: `core/deviceDrivers/matter/sbmd/` (SbmdHandlerInvoker, SbmdFactory, SpecBasedMatterDeviceDriver, MQuickJsRuntime)
- **New files**: `core/test/src/SbmdObservabilityTest.cpp` (GTest unit tests); new pytest test in `testing/test/`. Metric handles and initialization are added to existing production modules — no new production source files.
- **Struct change**: `PendingOperation` gains `SbmdOperationContext operationCtx` field (carries driver name, originating op type, and start time)
- **API change**: `SbmdHandlerInvoker::InvokeHandler` gains a single optional `const SbmdOperationContext *opCtx = nullptr` parameter; new `SbmdOperationContext` struct defined in `SbmdHandlerInvoker.h` (backwards-compatible)
- **CMake flag**: `BCORE_OBSERVABILITY_BACKEND` must be `memory` (default) for metrics to be recorded; `none` backend silently no-ops all calls — no conditional compilation required at call sites
- **CMake option**: new `BCORE_SBMD_METRICS_SAMPLE_PERIOD_MS` option (default 30000, compiled as `BARTON_CONFIG_SBMD_METRICS_SAMPLE_PERIOD_MS`) controlling the hybrid heap sampler idle period
- **Dependencies**: existing `observabilityMetrics.h` API; no new external dependencies
- **Tests**: new GTest file `core/test/src/SbmdObservabilityTest.cpp`; new pytest test in `testing/test/`

## Non-Goals

- **Implicit GC count + duration**: requires an additional mquickjs patch (GC callback mechanism). Deferred to follow-on change pending coordination with ongoing mquickjs work.
- **Explicit GC tracking** (`gc()` call sites): no call sites currently exist in production code. Deferred to follow-on.
- **`deviceId` attribute on metrics**: useful for isolating per-device failures but adds unbounded cardinality. Deferred to follow-on when attribute indexing strategy is clearer.
- **Per-device-instance heap cost**: not tractable — the JS arena has no per-device partitioning. Deferred to follow-on for further investigation.
- **Intra-invocation peak heap**: `JS_GetMemoryUsage` provides only a global watermark, not a per-call peak. Approximation is unreliable. Deferred to follow-on.
- **Production/fleet attributes** (e.g., `accountId`): out of scope until a backend storage strategy is defined.
- **Observability infrastructure changes**: the counter/gauge/histogram implementation in `core/src/observability/` is not modified by this change.
