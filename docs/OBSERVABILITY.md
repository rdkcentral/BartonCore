# OpenTelemetry Observability

BartonCore includes optional OpenTelemetry observability (tracing, metrics,
and log bridging) gated by the `BCORE_OBSERVABILITY` CMake flag. When
disabled (the default), all observability APIs compile to zero-cost no-op
inline stubs — no extra binary size, no runtime overhead.

## Building with OpenTelemetry

```bash
cmake -B build -DBCORE_OBSERVABILITY=ON
cmake --build build -j$(nproc)
```

The build fetches **opentelemetry-cpp v1.16.1** via CMake FetchContent. The
only additional host dependency is **libcurl** (for the OTLP/HTTP exporter),
which is typically already present.

To verify the flag is off (default):

```bash
cmake -B build
cmake --build build
```

## Configuration

The OTel SDK is configured through standard OpenTelemetry environment
variables. No code changes are needed — just set the variables before
starting the application.

| Variable | Default | Description |
|---|---|---|
| `OTEL_EXPORTER_OTLP_ENDPOINT` | `http://localhost:4318` | OTLP/HTTP collector endpoint |
| `OTEL_SERVICE_NAME` | `barton-core` | Service name in exported data |
| `OTEL_BSP_SCHEDULE_DELAY_MS` | `5000` | Batch span processor export delay (ms) |
| `OTEL_BSP_MAX_EXPORT_BATCH_SIZE` | `512` | Batch span processor max batch size |
| `OTEL_BLRP_SCHEDULE_DELAY_MS` | `5000` | Batch log record processor export delay (ms) |
| `OTEL_BLRP_MAX_EXPORT_BATCH_SIZE` | `512` | Batch log record processor max batch size |
| `OTEL_METRIC_EXPORT_INTERVAL_MS` | `10000` | Periodic metric reader export interval (ms) |

## What is Instrumented

### Traces (Spans)

Spans are organized into parent-child hierarchies that trace operations
end-to-end through the stack. Root spans are created at operation entry
points; child spans are created for internal steps and inherit the parent's
trace context automatically via thread-local storage (TLS) or explicit
context propagation across thread boundaries.

#### Span Hierarchy

```
subsystem.init                          (root)
matter.init                             (root, linked to subsystem.init)
├── matter.init.stack                   (child)
├── matter.init.server                  (child)
└── matter.init.commissioner            (child)
    ├── matter.fabric.create            (child, conditional)
    └── matter.init.advertise           (child)
subsystem.shutdown                      (root)
device.discovery                        (root)
└── device.found                        (child)
    ├── device.configure                (child)
    └── device.persist                  (child)
device.commission                       (root)
├── device.found                        (child)
│   ├── device.configure                (child)
│   └── device.persist                  (child)
├── matter.case.connect                 (child)
├── matter.subscribe                    (child)
└── matter.report                       (child)
    └── resource.update                 (child, Matter)
device.pair                             (root)
├── matter.case.connect                 (child)
├── matter.subscribe                    (child)
└── matter.report                       (child)
    └── resource.update                 (child, Matter)
device.remove                           (root)
├── device.driver.remove                (child)
└── matter.fabric.remove                (child, Matter)
device.commission_window.open           (root)
resource.read                           (root)
└── zigbee.cluster.read                 (child, Zigbee)
resource.write                          (root)
└── zigbee.cluster.write                (child, Zigbee)
    └── resource.update                 (child, Zigbee)
zigbee.device.discovered                (child of device.found)
```

#### Span Inventory

See [OBSERVABILITY_SPANS.md](OBSERVABILITY_SPANS.md) for the full spans
reference including detailed per-span documentation, context propagation
mechanisms, error conditions, and attribute reference.

The summary table below lists every instrumented span:

| Span Name | Parent | Location | Attributes |
|---|---|---|---|
| `subsystem.init` | — | `subsystemManager.c` | — |
| `matter.init` | — (linked to `subsystem.init`) | `matterSubsystem.cpp` | `retry.attempt` |
| `matter.init.stack` | TLS (`matter.init`) | `matterSubsystem.cpp` | — |
| `matter.init.server` | TLS (`matter.init`) | `Matter.cpp` | — |
| `matter.init.commissioner` | TLS (`matter.init`) | `Matter.cpp` | — |
| `matter.fabric.create` | TLS (`matter.init.commissioner`) | `Matter.cpp` | — |
| `matter.init.advertise` | TLS (`matter.init.commissioner`) | `Matter.cpp` | — |
| `subsystem.shutdown` | — | `subsystemManager.c` | — |
| `device.discovery` | — | `deviceService.c` | `device.class`, `discovery.recovery_mode` |
| `device.found` | TLS (discovery/commission) | `deviceService.c` | `device.class`, `device.uuid`, `device.manufacturer`, `device.model` |
| `device.configure` | TLS | `deviceService.c` | `device.class` |
| `device.persist` | TLS | `deviceService.c` | `device.uuid` |
| `device.commission` | — | `deviceService.c` | `commissioning.payload`, `commissioning.timeout_seconds` |
| `device.pair` | — | `deviceService.c` | `device.node_id`, `pairing.timeout_seconds` |
| `device.remove` | — | `deviceService.c` | `device.uuid` |
| `device.driver.remove` | `device.remove` | `deviceService.c` | — |
| `device.commission_window.open` | — | `deviceService.c` | `device.node_id`, `window.timeout_seconds` |
| `resource.read` | — | `deviceService.c` | `resource.uri` |
| `resource.write` | — | `deviceService.c` | `resource.uri` |
| `resource.update` | TLS (report/write) | `MatterDevice.cpp`, `zigbeeDriverCommon.c` | `device.uuid`, `resource.name`, `endpoint.profile` |
| `matter.case.connect` | stored context | `DeviceDataCache.cpp` | — |
| `matter.subscribe` | stored context | `DeviceDataCache.cpp` | — |
| `matter.report` | stored context | `DeviceDataCache.cpp` | `device.id` |
| `matter.fabric.remove` | TLS | `MatterDeviceDriver.cpp` | `device.uuid` |
| `zigbee.device.discovered` | TLS (found) | `zigbeeDriverCommon.c` | `device.eui64`, `device.manufacturer` |
| `zigbee.cluster.read` | TLS (resource.read) | `zigbeeDriverCommon.c` | `device.uuid`, `resource.uri` |
| `zigbee.cluster.write` | TLS (resource.write) | `zigbeeDriverCommon.c` | `device.uuid`, `resource.uri` |

### Metrics

See [OBSERVABILITY_METRICS.md](OBSERVABILITY_METRICS.md) for the full
metrics reference including application metrics, device telemetry,
span-derived RED metrics, system/process metrics, and Prometheus query
examples.

Device telemetry metrics emit per-device resource values, state transitions,
and update counts with rich metadata attributes (manufacturer, model,
firmware version, etc.). Zigbee RSSI/LQI gauges and Matter subscription
lifecycle counters are also included. All device telemetry flows through the
central `updateResource()` path and is gated by
`BARTON_CONFIG_OBSERVABILITY`.

### Log Bridge

When enabled, all `icLog` messages are automatically forwarded to the
OpenTelemetry Logs SDK via a linker-wrap shim (`--wrap=icLogMsg`). This
provides:

- **Severity mapping**: icLog priority → OTel severity (TRACE/DEBUG/INFO/WARN/ERROR)
- **Source location**: `code.filepath`, `code.function`, `code.lineno` attributes
- **Trace-log correlation**: If a span is active when a log is emitted, the
  log record automatically carries the span's trace ID and span ID

The original `icLog` output (stdout/syslog) is preserved — the bridge adds
OTel export as a second output channel.

## Running with an OTel Collector

### Devcontainer (default)

The devcontainer includes the OTel Collector and Jaeger by default via the
`compose.observability.yaml` overlay in `.devcontainer/devcontainer.json`.
No extra configuration is needed — just open the project in the devcontainer
and the observability services start automatically.

Jaeger UI is available at http://localhost:16686.

### dockerw

Use the `-o` flag to include the observability services:

```bash
./dockerw -o ./build.sh
```

### Docker Compose (manual)

A Docker Compose overlay is provided that adds an OTel Collector and Jaeger:

```bash
docker compose -f docker/compose.yaml -f docker/compose.observability.yaml up
```

This starts:
- **OTel Collector** on port 4318 (OTLP HTTP receiver → Jaeger exporter)
- **Jaeger** UI on port 16686
- **Prometheus** UI on port 9090

The Barton container's `OTEL_EXPORTER_OTLP_ENDPOINT` is automatically set
to the collector.

### Manual setup

Point the application at any OTLP/HTTP-compatible collector:

```bash
export OTEL_EXPORTER_OTLP_ENDPOINT=http://my-collector:4318
export OTEL_SERVICE_NAME=barton-core
./build/reference/barton-core-reference
```

## Prometheus & Jaeger Monitor Tab

The observability stack includes **Prometheus** as a metrics backend and
wires it into Jaeger's **Monitor** tab (Service Performance Monitoring).
The OTel Collector's `spanmetrics` connector derives RED metrics from traced
spans, and Prometheus scrapes all metrics from the collector's exporter
endpoint. Jaeger queries Prometheus to populate the Monitor tab.

| UI | URL | Description |
|---|---|---|
| Jaeger | http://localhost:16686 | Traces and Monitor (SPM) tab |
| Prometheus | http://localhost:9090 | Raw metric queries and exploration |

See [OBSERVABILITY_METRICS.md](OBSERVABILITY_METRICS.md) for the full list
of available metrics, including span-derived RED metrics and example
Prometheus queries.

## System & Process Metrics

The OTel Collector's `hostmetrics` receiver scrapes system-level and
barton-core process-level metrics automatically when running with the
Docker Compose observability overlay. The collector shares barton's PID
namespace so it can read `/proc` entries directly.

See [OBSERVABILITY_METRICS.md](OBSERVABILITY_METRICS.md) for the full
list of system and process metrics and their descriptions.

## Running Tests

The dev linux cmake initial cache (`config/cmake/platforms/dev/linux.cmake`)
enables `BCORE_OBSERVABILITY` by default, so a standard `./build.sh` build
includes the observability tests automatically.

### Unit tests

```bash
./build.sh
ctest --test-dir build --output-on-failure
```

The observability unit tests include:

- `testObservabilityNoop` — verifies no-op stubs when compiled without the flag
- `testObservabilityTracing` — span creation, attributes, error status, parent context
- `testObservabilityMetrics` — counter/gauge creation and null-safety
- `testObservabilityInit` — init/shutdown lifecycle
- `testObservabilityLogBridge` — log emission, severity mapping, trace-log correlation

### Integration tests

```bash
./scripts/ci/run_integration_tests.sh
```

Observability integration tests auto-skip when `BCORE_OBSERVABILITY` is OFF.

## Architecture

```
icLog messages ─┐
                │ --wrap=icLogMsg
                ▼
        observabilityLogBridgeShim.cpp
        ├── __real_icLogMsg()  → original icLog output
        └── observabilityLogBridgeEmit() → OTel Logs SDK
                                     │
 C adapter layer                          │
 ┌─────────────────────────────────┐      │
 │ observability.h                 │◄─────┘
 │ observabilityTracing.h          │
 │ observabilityMetrics.h          │── OTLP/HTTP ──→ OTel Collector → Jaeger/etc.
 │ observabilityLogBridge.h        │
 └─────────────────────────────────┘
   #ifdef BARTON_CONFIG_OBSERVABILITY
     → opentelemetry-cpp SDK
   #else
     → zero-cost no-op stubs
```
