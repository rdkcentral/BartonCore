**Commit strategy:** Each numbered task group below represents a self-contained, independently committable unit. Commit after completing each group (all tests passing). Do not accumulate the entire feature into a single commit.

## 1. Build System Foundation

- [x] 1.1 Add `BCORE_OBSERVABILITY` CMake option to `config/cmake/options.cmake` (default OFF, definition `BARTON_CONFIG_OBSERVABILITY`)
- [x] 1.2 Add opentelemetry-cpp `FetchContent_Declare` in a new `config/cmake/OpenTelemetry.cmake` module, pinned to a release tag, with minimal component build (SDK, OTLP HTTP exporter, Logs SDK — no gRPC, no Prometheus)
- [x] 1.3 Add conditional `include(OpenTelemetry)` and source compilation block in `core/CMakeLists.txt` — glob `core/src/observability/*.cpp`, add to SOURCES, link opentelemetry-cpp targets, gated by `BCORE_OBSERVABILITY`
- [x] 1.4 Verify the build compiles cleanly with `BCORE_OBSERVABILITY=OFF` (no regressions) and with `BCORE_OBSERVABILITY=ON` (empty observability dir, just linkage)

## 2. C Adapter API Headers (No-Op Layer)

- [x] 2.1 Create `core/src/observability/observability.h` — `extern "C"` init/shutdown functions (`observabilityInit`, `observabilityShutdown`) with no-op inline stubs when `BARTON_CONFIG_OBSERVABILITY` is not defined
- [x] 2.2 Create `core/src/observability/observabilityTracing.h` — `extern "C"` span API (`observabilitySpanStart`, `observabilitySpanStartWithParent`, `observabilitySpanRelease`, `observabilitySpanSetAttribute`, `observabilitySpanSetError`, `observabilitySpanGetContext`, `observabilitySpanContextRelease`) with opaque types, no-op stubs, and `G_DEFINE_AUTOPTR_CLEANUP_FUNC` support
- [x] 2.3 Create `core/src/observability/observabilityMetrics.h` — `extern "C"` metrics API (`observabilityCounterCreate`, `observabilityCounterAdd`, `observabilityGaugeCreate`, `observabilityGaugeRecord`) with no-op stubs
- [x] 2.4 Create `core/src/observability/observabilityLogBridge.h` — `extern "C"` log bridge API (`observabilityLogBridgeInit`, `observabilityLogBridgeEmit`, `observabilityLogBridgeShutdown`) with no-op stubs
- [x] 2.5 Unit test: Write a C test (CMocka) in `core/test/` that includes all observability headers with `BARTON_CONFIG_OBSERVABILITY` undefined and verifies all calls compile and are safe no-ops

## 3. Tracing Implementation

- [x] 3.1 Implement `core/src/observability/observability.cpp` — `TracerProvider` + `MeterProvider` + `LoggerProvider` initialization with OTLP HTTP exporters, reading `OTEL_EXPORTER_OTLP_ENDPOINT` and `OTEL_SERVICE_NAME` env vars; shutdown with flush
- [x] 3.2 Implement `core/src/observability/observabilityTracing.cpp` — span lifecycle wrapping `opentelemetry::trace::Tracer`, context extraction/propagation, attribute setting, error status
- [x] 3.3 Unit test: Write a C++ test (Google Test) in `core/test/` for `observabilityTracing` — create spans, set attributes, create child spans with parent context; use InMemorySpanExporter to assert span names, attributes, and parent-child trace IDs
- [x] 3.4 Unit test: Write a C++ test (Google Test) in `core/test/` for `observability` init/shutdown — verify providers are created and shutdown flushes without errors

## 4. Metrics Implementation

- [x] 4.1 Implement `core/src/observability/observabilityMetrics.cpp` — counter and gauge creation/recording wrapping `opentelemetry::metrics::Meter`, attribute handling
- [x] 4.2 Unit test: Write a C++ test (Google Test) in `core/test/` for `observabilityMetrics` — create counters and gauges, record values with attributes; use InMemoryMetricExporter to assert metric names, values, and dimensions

## 5. Log Bridge Implementation

- [x] 5.1 Implement `core/src/observability/observabilityLogBridge.cpp` — log record creation from icLog parameters (category → scope, priority → severity, file/func/line → attributes, formatted message → body), trace-log correlation from active span context
- [x] 5.2 Integrate log bridge with `icLog` — either add a callback/hook in BartonCommon's `icLogMsg` that the bridge registers with, or create a wrapping backend that chains to the existing backend and also forwards to OTel
- [x] 5.3 Unit test: Write a C++ test (Google Test) in `core/test/` for `observabilityLogBridge` — emit log records with various severities, verify OTel log records have correct severity, scope, source location attributes, and body
- [x] 5.4 Unit test: Verify dual-output behavior — log messages appear in stdout AND are captured by the OTel log exporter

## 6. Core Service Integration

- [x] 6.1 Wire `observabilityInit()` into `deviceServiceInitialize()` and `observabilityShutdown()` into `deviceServiceShutdown()`, gated by `#ifdef BARTON_CONFIG_OBSERVABILITY`
- [x] 6.2 Add tracing spans to device discovery flow in `deviceService.c` and driver manager — `device.discovery` root span, `device.found` child spans
- [ ] 6.3 *(deferred)* Add tracing spans to commissioning operations — `device.commission` span with device attributes
- [x] 6.4 Add tracing spans to resource read/write operations — `resource.read` and `resource.write` spans with URI attributes
- [x] 6.5 Add tracing spans to subsystem init/shutdown in `subsystemManager` — `subsystem.init` and `subsystem.shutdown` spans
- [x] 6.6 Add metrics instrumentation: `device.active.count` gauge updates in device add/remove, `device.commfail.count` and `device.commrestore.count` counters in communication watchdog

## 7. Integration Tests (pytest)

- [x] 7.1 Set up OTLP test infrastructure in `testing/` — a lightweight OTLP receiver (e.g., a local OTel Collector with file exporter, or a Python mock OTLP server) that captures exported spans, metrics, and logs for assertion
- [x] 7.2 Add a `conftest.py` fixture or marker in `testing/` that detects `BCORE_OBSERVABILITY` from `build/CMakeCache.txt` and skips observability tests when the feature is not built
- [x] 7.3 Write a pytest integration test in `testing/test/` that starts a `BCore.Client` with `OTEL_EXPORTER_OTLP_ENDPOINT` pointing to the test receiver, performs a device operation, and asserts that expected spans (`subsystem.init`, `device.discovery`) are present in the captured data
- [x] 7.4 Write a pytest integration test that verifies metrics (`device.active.count`, `device.commfail.count`) are exported after device add/remove and comm-fail events
- [x] 7.5 Write a pytest integration test that verifies OTel log records are exported with correct severity and scope when `icLog` messages are emitted during service startup

## 8. Reference App Observability Demo

- [x] 8.1 Create `docker/otel-collector-config.yaml` — OTel Collector config with OTLP HTTP receiver (port 4318) and Jaeger exporter
- [x] 8.2 Create `docker/compose.observability.yaml` — Docker Compose overlay adding `otel-collector` and `jaeger` services, with the collector config mounted; designed to layer on top of `docker/compose.yaml`
- [x] 8.3 Verify the reference app (`barton-core-reference`) builds and runs with `BCORE_OBSERVABILITY=ON`, exporting observability data to the collector
- [x] 8.4 Test: Start the observability Docker stack, run the reference app, perform device operations, and verify traces appear in Jaeger UI at `http://localhost:16686`

## 9. Documentation and CI

- [x] 9.1 Create `docs/OBSERVABILITY.md` documenting: the `BCORE_OBSERVABILITY` flag, environment variables, how to connect a collector, and a step-by-step walkthrough of running the reference app with the observability Docker stack (build → start stack → run app → view in Jaeger)
- [x] 9.2 Enable `BCORE_OBSERVABILITY` in the dev linux cmake initial cache (`config/cmake/platforms/dev/linux.cmake`) so the default build and CI run observability tests automatically
- [x] 9.3 Verify the full test suite passes with both `BCORE_OBSERVABILITY=OFF` and `BCORE_OBSERVABILITY=ON`
