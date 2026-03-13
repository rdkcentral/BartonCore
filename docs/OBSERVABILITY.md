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

| Span Name | Location | Attributes |
|---|---|---|
| `subsystem.init` | `subsystemManager.c` | — |
| `subsystem.shutdown` | `subsystemManager.c` | — |
| `device.discovery` | `deviceService.c` | `device.class`, `discovery.recovery_mode` |
| `device.found` | `deviceService.c` | `device.class`, `device.uuid`, `device.manufacturer`, `device.model` |
| `resource.read` | `deviceService.c` | `resource.uri` |
| `resource.write` | `deviceService.c` | `resource.uri` |

### Metrics

All metrics are created via the C adapter layer in `observabilityMetrics.h`.
When `BARTON_CONFIG_OBSERVABILITY` is undefined every call compiles to a
zero-cost no-op.

#### Device Lifecycle

| Metric Name | Type | Unit | Attributes | Description |
|---|---|---|---|---|
| `device.active.count` | Gauge | {device} | — | Number of currently paired devices |
| `device.discovery.started` | Counter | {request} | — | Discovery requests started |
| `device.discovery.completed` | Counter | {request} | — | Discovery requests completed |
| `device.discovered.count` | Counter | {device} | `device.class` | Devices discovered |
| `device.add.success` | Counter | {device} | `device.class` | Devices successfully added |
| `device.add.failed` | Counter | {device} | `device.class` | Device add failures |
| `device.remove.success` | Counter | {device} | `device.class` | Devices successfully removed |
| `device.rejected.count` | Counter | {device} | `device.class` | Devices rejected by discovery filters |
| `device.discovery.duration` | Histogram | s | `device.class` | Discovery duration |

#### Communication Watchdog

| Metric Name | Type | Unit | Attributes | Description |
|---|---|---|---|---|
| `device.commfail.count` | Counter | {event} | — | Communication failure events |
| `device.commrestore.count` | Counter | {event} | — | Communication restored events |
| `device.commfail.current` | Gauge | {device} | — | Devices currently in comm-fail |
| `device.communication.check.performed` | Counter | {cycle} | — | Communication check cycles performed |

#### Storage & Restore

| Metric Name | Type | Unit | Attributes | Description |
|---|---|---|---|---|
| `storage.device.persist` | Counter | {operation} | — | Device persistence operations |
| `storage.device.count` | Gauge | {device} | — | Number of devices in storage |
| `storage.restore.attempt` | Counter | {attempt} | — | Storage restore attempts |
| `storage.restore.success` | Counter | {attempt} | — | Storage restore successes |
| `storage.restore.failed` | Counter | {attempt} | — | Storage restore failures |

#### Driver Manager

| Metric Name | Type | Unit | Attributes | Description |
|---|---|---|---|---|
| `driver.registered.count` | Counter | {driver} | `driver.name` | Device drivers registered |
| `driver.init.success` | Counter | {driver} | `driver.name` | Device driver startups |

#### Events

| Metric Name | Type | Unit | Attributes | Description |
|---|---|---|---|---|
| `event.produced` | Counter | {event} | `event.type` | Events produced by device service |

#### Subsystem Manager

| Metric Name | Type | Unit | Attributes | Description |
|---|---|---|---|---|
| `subsystem.init.started` | Counter | {event} | `subsystem.name` | Subsystem initialization started |
| `subsystem.init.completed` | Counter | {event} | `subsystem.name` | Subsystem initialization completed |
| `subsystem.init.failed` | Counter | {event} | — | Subsystem initialization failed |
| `subsystem.initialized.count` | Gauge | {subsystem} | — | Number of initialized subsystems |
| `subsystem.ready_for_devices` | Gauge | {state} | — | All subsystems ready for devices |
| `subsystem.init.duration` | Histogram | s | `subsystem.name` | Subsystem initialization duration |

#### Matter Subsystem

| Metric Name | Type | Unit | Attributes | Description |
|---|---|---|---|---|
| `matter.initializing` | Gauge | 1 | — | Whether Matter is initializing |
| `matter.commissioning.attempt` | Counter | {attempt} | — | Commissioning attempts |
| `matter.commissioning.success` | Counter | {attempt} | — | Commissioning successes |
| `matter.commissioning.failed` | Counter | {attempt} | — | Commissioning failures |
| `matter.commissioning.duration` | Histogram | s | — | Commissioning duration |
| `matter.pairing.attempt` | Counter | {attempt} | — | Pairing attempts |
| `matter.pairing.success` | Counter | {attempt} | — | Pairing successes |
| `matter.pairing.failed` | Counter | {attempt} | — | Pairing failures |
| `matter.ota.query_image.received` | Counter | {request} | — | OTA query image requests received |
| `matter.ota.query_image.available` | Counter | {request} | — | OTA images available |
| `matter.ota.query_image.not_available` | Counter | {request} | — | OTA images not available |
| `matter.ota.apply_update.received` | Counter | {request} | — | OTA apply update requests received |

#### Zigbee Subsystem

| Metric Name | Type | Unit | Attributes | Description |
|---|---|---|---|---|
| `zigbee.network.startup.completed` | Counter | {event} | — | Zigbee network startup completions |
| `zigbee.device.join.count` | Counter | {event} | — | Zigbee device join events |
| `zigbee.device.announce.count` | Counter | {event} | — | Zigbee device announce events |
| `zigbee.device.processed.count` | Counter | {device} | — | Zigbee devices processed |
| `zigbee.interference.detected` | Counter | {event} | — | Zigbee interference detected events |
| `zigbee.interference.resolved` | Counter | {event} | — | Zigbee interference resolved events |
| `zigbee.interference.active` | Gauge | {state} | — | Zigbee interference currently active |
| `zigbee.health_check.enabled` | Gauge | {state} | — | Zigbee health check enabled |
| `zigbee.pan_id_attack.detected` | Counter | {event} | — | Zigbee PAN ID attack detected |
| `zigbee.pan_id_attack.active` | Gauge | {state} | — | Zigbee PAN ID attack currently active |

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

The Barton container's `OTEL_EXPORTER_OTLP_ENDPOINT` is automatically set
to the collector.

### Manual setup

Point the application at any OTLP/HTTP-compatible collector:

```bash
export OTEL_EXPORTER_OTLP_ENDPOINT=http://my-collector:4318
export OTEL_SERVICE_NAME=barton-core
./build/reference/barton-core-reference
```

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
