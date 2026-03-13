## Why

BartonCore's OpenTelemetry integration currently exports only three metrics: `device.active.count` (gauge), `device.commfail.count` (counter), and `device.commrestore.count` (counter). This provides minimal visibility into system behavior. Operators need deeper observability across the device lifecycle, protocol subsystems, and core service internals to diagnose issues, track trends, and monitor fleet health at scale.

## What Changes

- Add **device lifecycle counters**: discovery attempts/results, device add/remove success/failure, and devices rejected by filters
- Add **Matter subsystem meters**: commissioning pipeline counts (attempt, success, failure), pairing success/failure, and OTA update request flow
- Add **Zigbee subsystem meters**: device join/announce pipeline, network health check counts, interference detection, and PAN ID attack detection
- Add **subsystem manager meters**: initialization success/failure counts and a readiness gauge across all subsystems
- Add **device driver manager meters**: driver registration and initialization success counts
- Add **communication watchdog meters**: a gauge for devices currently in comm-fail state, and counts of watchdog check cycles
- Add **storage/database meters**: device persist count, database restore success/failure, and a gauge for total devices in the database
- Add **event system meters**: event production counts by event type
- Extend the existing `observabilityMetrics` C API with **attribute support** so counters and gauges can carry key-value dimensions (e.g., `device.class`, `subsystem.name`)
- Add **histogram support** to the C API for duration measurements (discovery, commissioning, initialization)

## Non-goals

- No new tracing spans — tracing expansion is a separate concern (the deferred commission span from the prior change is not addressed here)
- No changes to the log bridge or log pipeline
- No new OTel exporters or backends — the existing OTLP HTTP exporter is sufficient
- No gRPC exporter support
- No runtime metric enable/disable toggles — metrics are all-or-nothing via `BCORE_OBSERVABILITY`
- No custom dashboards or alerting rules — this change covers emission only

## Capabilities

### New Capabilities
- `otel-meters-api`: Extensions to the observability C API — attribute support for counters/gauges, histogram instrument type, and attribute builder helpers
- `otel-meters-device-lifecycle`: Meters covering device discovery, add, remove, and filter rejection across all subsystems
- `otel-meters-matter`: Meters for Matter commissioning pipeline stages, pairing, and OTA update flow
- `otel-meters-zigbee`: Meters for Zigbee join/announce pipeline, health checks, interference, and PAN ID attack detection
- `otel-meters-subsystem`: Meters for subsystem manager initialization lifecycle and cross-subsystem readiness
- `otel-meters-internals`: Meters for device driver manager, communication watchdog, storage/database, and event system

### Modified Capabilities
- `otel-metrics`: The existing metrics spec requires updates to reflect the expanded C API (attributes, histograms) and the broader set of emitted metrics

## Impact

- **Core layer** (`core/src/`): Instrumentation call sites added to `deviceService.c`, `deviceCommunicationWatchdog.c`, `subsystemManager.c`, `deviceDriverManager.c`, `deviceStorageMonitor.c`, `event/deviceEventProducer.c`, and database code
- **Subsystems** (`core/src/subsystems/`): Instrumentation added to `matter/` (commissioning orchestrator, OTA provider, subsystem init), `zigbee/` (event handler, health check, defender), and `thread/` (subsystem init)
- **Observability library** (`core/src/observability/`): `observabilityMetrics.h/.cpp` extended with attribute and histogram APIs
- **CMake flag**: `BCORE_OBSERVABILITY` — no new flags needed; all meters compile conditionally on the existing flag
- **Dependencies**: No new dependencies — uses the already-linked OpenTelemetry C++ SDK metrics API
- **Tests**: New unit tests for the extended C API (attributes, histograms); updated integration tests verifying new metric names appear in OTLP exports
