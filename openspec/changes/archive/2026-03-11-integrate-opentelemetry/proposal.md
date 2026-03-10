## Why

BartonCore currently has no structured observability: logging goes to stdout via `icLogMsg` with plain-text formatting, telemetry markers compile as no-ops (unless on RDK with `CONFIG_TELEMETRY2`), there are no metrics, no distributed tracing, and no correlation IDs across subsystems. Operators deploying BartonCore have no standardized way to instrument, collect, or export telemetry data. OpenTelemetry is the industry-standard, vendor-neutral framework for observability — integrating it gives BartonCore structured logs, request-scoped tracing, and operational metrics with zero lock-in to any specific backend.

## What Changes

- Add the **opentelemetry-cpp** SDK as an optional third-party dependency, fetched and built via CMake (`FetchContent` or `find_package`).
- Introduce a new CMake feature flag **`BCORE_OBSERVABILITY`** (default OFF) to gate all OpenTelemetry functionality — when disabled, zero overhead and no new dependencies.
- Create a **C-compatible observability adapter layer** (`core/src/observability/`) that wraps the C++ OpenTelemetry SDK behind a C99 API, matching BartonCore's internal conventions. This layer provides:
  - **Tracing**: Span creation/management for key operations (device discovery, commissioning, resource read/write, firmware upgrade, subsystem init/shutdown).
  - **Metrics**: Counters and gauges for operational signals (active devices, discovery attempts, comm failures, event throughput).
  - **Log bridge**: An `icLog` backend that emits structured log records through the OpenTelemetry Logs SDK, preserving existing `icLogDebug`/`icLogError` call sites.
- Wire the adapter into **core service lifecycle** — initialize exporters/providers at `deviceServiceInitialize` and flush/shutdown at `deviceServiceShutdown`.
- Provide **OTLP exporter configuration** via BartonCore's existing property/configuration system (endpoint URL, protocol, export interval).
- Extend the **reference application** (`reference/`) to demonstrate a complete observability solution — when built with `BCORE_OBSERVABILITY=ON`, the reference app serves as a working example of how a client uses BartonCore's observability. A **Docker Compose overlay** adds an OpenTelemetry Collector and a local visualizer (e.g., Jaeger) so a developer can run the reference app and immediately see traces, metrics, and logs in a browser.

## Non-goals

- **Replacing `icLog` call sites** — existing `icLogDebug(...)` calls remain unchanged; the integration is at the backend level, not call-site refactoring.
- **Auto-instrumenting third-party code** — the Matter SDK, GLib, and other dependencies are not instrumented; only BartonCore's own operations get spans/metrics.
- **Providing a production backend** — the reference app's local visualizer is for development/demo purposes only; production collection infrastructure is the deployer's responsibility.
- **Zigbee packet capture** — the existing `zigbeeTelemetry` system (network captures) is unrelated and not affected.
- **Legacy telemetry markers** — the existing `TELEMETRY_COUNTER`/`TELEMETRY_VALUE_*` macro system (BartonCommon) is not modified. It will coexist alongside OpenTelemetry until a future migration.
- **Public API (GObject) changes** — no new `BCoreClient` properties, signals, or GIR-visible types are introduced in this change; observability is an internal concern.

## Capabilities

### New Capabilities

- `otel-tracing`: Distributed tracing integration — C adapter for span lifecycle, context propagation across subsystem boundaries, and OTLP span export.
- `otel-metrics`: Metrics collection integration — C adapter for counters/gauges and OTLP metrics export for new operational metrics.
- `otel-log-bridge`: Log bridge connecting the `icLog` facade to the OpenTelemetry Logs SDK for structured, exportable log records.
- `otel-reference-demo`: Reference application observability integration and Docker Compose overlay providing a local OTel Collector + visualizer for a complete end-to-end demo.

### Modified Capabilities

- `build-system`: New `BCORE_OBSERVABILITY` CMake option, opentelemetry-cpp dependency management, and conditional compilation of the observability layer.

## Impact

- **Reference app** (`reference/`): Gains optional observability when `BCORE_OBSERVABILITY=ON`; demonstrates how a client initializes and uses BartonCore observability.
- **Docker** (`docker/`): New Compose overlay adds OTel Collector and Jaeger services for local development visualization.
- **Core service** (`core/src/`): `deviceService.c` gains init/shutdown hooks for the observability layer. Key operations in `deviceDriverManager`, `subsystemManager`, and `communicationWatchdog` gain tracing spans.
- **Build system** (`config/cmake/`, `core/CMakeLists.txt`): New CMake option, dependency fetch for opentelemetry-cpp, conditional source inclusion for `core/src/observability/`.
- **BartonCommon logging** (`icLog`): A new logging backend is added (alongside existing debug/syslog/zlog backends) that routes through OpenTelemetry — selected at compile time.
- **Dependencies**: opentelemetry-cpp SDK (Apache-2.0), libcurl (for OTLP/HTTP export — already present). Adds ~2–5 MB to the shared library when enabled.
- **Threading**: The OpenTelemetry SDK manages its own export threads; care needed around the existing GLib main loop and Matter event loop to avoid contention. Span context propagation across `pthread`-based thread pool boundaries requires explicit passing.
- **CMake flags**: `BCORE_OBSERVABILITY` — a new `bcore_option` alongside existing `BCORE_ZIGBEE`, `BCORE_MATTER`, etc.
