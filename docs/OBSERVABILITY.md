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

| Metric Name | Type | Location | Description |
|---|---|---|---|
| `device.active.count` | Gauge | `deviceService.c` | Number of currently paired devices |
| `device.commfail.count` | Counter | `deviceCommunicationWatchdog.c` | Communication failure events |
| `device.commrestore.count` | Counter | `deviceCommunicationWatchdog.c` | Communication restored events |

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
