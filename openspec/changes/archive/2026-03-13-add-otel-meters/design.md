## Context

BartonCore's OpenTelemetry integration (the `integrate-opentelemetry` change) established a C adapter layer in `core/src/observability/` that wraps the opentelemetry-cpp SDK behind `extern "C"` APIs. The metrics API currently exposes two instrument types — counters (`observabilityCounterCreate`/`observabilityCounterAdd`) and gauges (`observabilityGaugeCreate`/`observabilityGaugeRecord`) — with no attribute (dimension) support and no histogram instrument type.

Three metrics are currently emitted:
- `device.active.count` (gauge) — in `deviceService.c`
- `device.commfail.count` (counter) — in `deviceCommunicationWatchdog.c`
- `device.commrestore.count` (counter) — in `deviceCommunicationWatchdog.c`

The codebase has dozens of observable events, state changes, and lifecycle transitions across device management, protocol subsystems (Matter, Zigbee, Thread), driver management, and internal services that are invisible to operators. This change adds comprehensive meter coverage by:
1. Extending the C adapter API with attribute support and histograms
2. Instrumenting call sites across core/, subsystems, and internal services

All instrumentation targets are C (`.c`) files except the Matter subsystem (`.cpp`). The existing pattern — static `ObservabilityCounter*`/`ObservabilityGauge*` globals, created once at init, recorded at event sites — scales cleanly.

## Goals / Non-Goals

**Goals:**
- Extend the `observabilityMetrics` C API with attribute key-value pairs for counters and gauges
- Add histogram instrument type to the C API for measuring durations and distributions
- Instrument device lifecycle operations (discovery, add, remove, rejection) in `deviceService.c`
- Instrument Matter subsystem (commissioning pipeline, pairing, OTA) in `core/src/subsystems/matter/`
- Instrument Zigbee subsystem (join/announce, health checks, interference, PAN ID attacks) in `core/src/subsystems/zigbee/`
- Instrument subsystem manager (init lifecycle, readiness) in `subsystemManager.c`
- Instrument device driver manager (init, discovery lifecycle) in `deviceDriverManager.c`
- Instrument communication watchdog (monitored device gauge, comm-fail state gauge) in `deviceCommunicationWatchdog.c`
- Instrument storage/persistence (persist counts, database totals) in `database/jsonDatabase.c` and `deviceService.c`
- Instrument event system (event production by type) in `event/deviceEventProducer.c`
- Add unit tests for new API surface (attributes, histograms)
- Update integration tests to verify new metric names

**Non-Goals:**
- Adding new tracing spans (separate concern)
- Modifying the log bridge
- Adding new exporters or changing transport (OTLP HTTP stays)
- Changing the MeterProvider configuration (10s export interval stays)
- Runtime metric enable/disable toggles
- Histogram bucket configuration (using SDK defaults)
- Instrumenting third-party code (Matter SDK, GLib, ZHAL)

## Decisions

### Decision 1: Attribute support via variadic key-value pairs

**Choice:** Extend `observabilityCounterAdd` and `observabilityGaugeRecord` with new `WithAttrs` variants that accept a NULL-terminated variadic list of `const char *key, const char *value` pairs. Keep the original zero-attribute functions unchanged for backward compatibility.

```c
// Existing (unchanged)
void observabilityCounterAdd(ObservabilityCounter *counter, uint64_t value);

// New
void observabilityCounterAddWithAttrs(ObservabilityCounter *counter, uint64_t value,
                                      const char *key1, const char *value1, ...);
// Sentinel: last key must be NULL

// Usage:
observabilityCounterAddWithAttrs(discoveryCounter, 1,
    "device.class", deviceClass,
    "subsystem.name", subsystemName,
    NULL);
```

**Rationale:** Variadic `const char*` pairs are idiomatic C99 and require no heap allocation at the call site. A NULL sentinel is safe and familiar (similar to GLib's `g_object_new` pattern). String-only values are sufficient — OTel attributes in metric contexts are almost always strings or small integers that can be string-serialized.

**Alternatives considered:**
- *Attribute builder struct*: Requires allocate/populate/free lifecycle — too verbose for single-line instrumentation calls that are repeated dozens of times.
- *Macro wrapper*: Hides the sentinel but adds macro complexity; the sentinel is clearer.
- *Passing attribute count*: Error-prone — count mismatches cause undefined behavior. Sentinel is safer.

**Thread safety:** The OTel C++ SDK's `Counter::Add()` and gauge callback are thread-safe. String values are passed by value (copied into OTel attributes) so no lifetime concerns.

### Decision 2: Histogram instrument via duration-oriented API

**Choice:** Add a histogram instrument to the C API optimized for duration measurement:

```c
typedef struct ObservabilityHistogram ObservabilityHistogram;

ObservabilityHistogram *observabilityHistogramCreate(const char *name, const char *description, const char *unit);
void observabilityHistogramRecord(ObservabilityHistogram *histogram, double value);
void observabilityHistogramRecordWithAttrs(ObservabilityHistogram *histogram, double value,
                                           const char *key1, const char *value1, ...);
void observabilityHistogramRelease(ObservabilityHistogram *histogram);
```

**Rationale:** Duration histograms are the primary use case (commissioning time, discovery time, initialization time). Using `double` for the value supports both millisecond and second granularity. The SDK provides default bucket boundaries that work well for most latency distributions.

**Alternatives considered:**
- *Integer-only histograms*: Loses sub-second precision for fast operations.
- *Configurable buckets via C API*: Over-engineering — SDK defaults are well-tuned and can be adjusted via Views if needed post-deployment.

### Decision 3: Phased instrumentation — outward from existing patterns

**Choice:** Instrument in phases, starting from the core device lifecycle (where patterns already exist) and expanding outward to subsystems and internal services:

1. **Phase 1 — API extension**: Attribute and histogram support in `observabilityMetrics.h/.cpp`
2. **Phase 2 — Device lifecycle**: Discovery, add/remove, and rejection in `deviceService.c`
3. **Phase 3 — Protocol subsystems**: Matter commissioning/pairing/OTA, Zigbee join/health/interference
4. **Phase 4 — Internal services**: Subsystem manager, driver manager, comm watchdog, storage, events

**Rationale:** Phase 1 is a prerequisite for everything else (attributes needed for dimensions). Phase 2 builds on the existing gauge pattern in `deviceService.c`. Phases 3–4 can be done in any order since the instrumentation sites are independent.

### Decision 4: Meter naming convention — OpenTelemetry semantic conventions

**Choice:** Follow OTel semantic convention naming patterns: `<namespace>.<object>.<metric>` with dot separators. Use these namespaces:

| Namespace | Scope |
|-----------|-------|
| `device.*` | Device lifecycle, active counts, comm-fail |
| `matter.*` | Matter subsystem operations |
| `zigbee.*` | Zigbee subsystem operations |
| `subsystem.*` | Subsystem manager lifecycle |
| `driver.*` | Device driver manager |
| `storage.*` | Database/persistence |
| `event.*` | Event system |

**Rationale:** Consistent with the existing `device.commfail.count` and `device.active.count` naming. Namespace prefixes allow easy filtering in Grafana/Jaeger and avoid collisions.

### Decision 5: No-op stubs for all new API functions

**Choice:** All new functions (`observabilityCounterAddWithAttrs`, `observabilityHistogramCreate`, etc.) get `static inline` no-op stubs in the header when `BARTON_CONFIG_OBSERVABILITY` is undefined, following the existing pattern.

**Rationale:** Zero overhead when disabled. Callers don't need `#ifdef` guards. This is the established pattern.

### Decision 6: Counter creation at module initialization, not at first use

**Choice:** All counters, gauges, and histograms are created in the module's initialization function (e.g., `deviceCommunicationWatchdogInit()`, `subsystemManagerInitialize()`) and stored in static file-scope variables. They are released during shutdown.

**Rationale:** This is the pattern already used by the existing three meters. It avoids lazy-init races, keeps initialization deterministic, and makes cleanup predictable. The global static pattern is appropriate because each module is a singleton in the process.

**Thread safety:** Creation is single-threaded (service init phase). Recording is thread-safe per OTel SDK guarantees. The `__atomic_*` pattern used for the device active gauge is only needed when a separate C-side counter shadows the OTel state — otherwise `observabilityCounterAdd` is naturally thread-safe.

## Risks / Trade-offs

- **[Metric cardinality]** → Attributes like `device.class` and `subsystem.name` have bounded cardinality (< 20 distinct values). Attributes like `device.node_id` are high-cardinality and should be avoided on metrics (use traces/logs for per-device correlation). Each spec explicitly lists allowed attributes.
- **[Performance overhead from many meters]** → OTel SDK aggregates in-memory and exports periodically (10s). Counter/gauge Add/Record operations are lockless atomic updates. The overhead per-call is ~100ns. With ~50 new meters total, the impact is negligible.
- **[API surface size]** → Adding `WithAttrs` variants and histogram functions roughly doubles the API surface. Mitigated by the no-op stub pattern — callers see the same header regardless of build config.
- **[Subsystem-specific metrics compile conditionally]** → Matter meters only exist when `BCORE_MATTER=ON`. Zigbee meters only when `BCORE_ZIGBEE=ON`. The observability API's no-op stubs handle this naturally — calls in conditionally-compiled subsystem code simply don't exist when the subsystem is off.
- **[Histogram default buckets may not suit all distributions]** → The SDK's default buckets (0, 5, 10, 25, 50, 75, 100, 250, 500, 750, 1000, 2500, 5000, 7500, 10000 ms) work for most latency distributions. Custom Views can be applied at the MeterProvider level post-deployment without code changes.
