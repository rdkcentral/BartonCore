## ADDED Requirements

### Requirement: Observability initialization and shutdown
The observability layer SHALL be initialized during `deviceServiceInitialize()` and shut down (with flush) during `deviceServiceShutdown()`. Initialization SHALL configure a `TracerProvider` with an OTLP HTTP span exporter. Shutdown SHALL flush all pending spans before releasing resources.

#### Scenario: Successful initialization
- **WHEN** `deviceServiceInitialize()` is called with `BCORE_OBSERVABILITY=ON`
- **THEN** a `TracerProvider` SHALL be created with an OTLP HTTP exporter configured from environment variables (`OTEL_EXPORTER_OTLP_ENDPOINT`, `OTEL_SERVICE_NAME`)

#### Scenario: Initialization with default config
- **WHEN** no `OTEL_EXPORTER_OTLP_ENDPOINT` environment variable is set
- **THEN** the exporter SHALL default to `http://localhost:4318`

#### Scenario: Clean shutdown
- **WHEN** `deviceServiceShutdown()` is called
- **THEN** the observability layer SHALL flush all pending spans and shut down the `TracerProvider` before the service stops

#### Scenario: Feature disabled
- **WHEN** `BCORE_OBSERVABILITY=OFF` (default)
- **THEN** all tracing API calls SHALL be no-ops with zero runtime overhead

### Requirement: C-linkage tracing API
The tracing adapter SHALL expose a C99-compatible API via `extern "C"` functions for creating, annotating, and finishing spans. The API SHALL use opaque pointer types (`ObservabilitySpan*`, `ObservabilitySpanContext*`) to hide C++ implementation details.

#### Scenario: Create and finish a span
- **WHEN** C code calls `observabilitySpanStart("operationName")`
- **THEN** a new OpenTelemetry span SHALL be created with the given name and a valid `ObservabilitySpan*` pointer SHALL be returned

#### Scenario: Release a span
- **WHEN** `observabilitySpanRelease(span)` is called with a valid span pointer
- **THEN** the span SHALL be ended (recording its duration) and the memory SHALL be freed when the last reference is dropped

#### Scenario: Add span attributes
- **WHEN** `observabilitySpanSetAttribute(span, "key", "value")` is called
- **THEN** the span SHALL have the attribute `key=value` attached

#### Scenario: Record span error
- **WHEN** `observabilitySpanSetError(span, "error description")` is called
- **THEN** the span's status SHALL be set to ERROR with the given description

#### Scenario: No-op when disabled
- **WHEN** `BCORE_OBSERVABILITY=OFF` and C code calls `observabilitySpanStart("op")`
- **THEN** `NULL` SHALL be returned and all subsequent calls with `NULL` SHALL be safe no-ops

### Requirement: Explicit span context propagation
Trace context SHALL be propagated across function call chains via explicit `ObservabilitySpanContext*` parameters. A span created with a parent context SHALL be recorded as a child of that parent's trace.

#### Scenario: Create child span
- **WHEN** `observabilitySpanStartWithParent("child-op", parentContext)` is called with a valid parent context
- **THEN** the new span SHALL share the same trace ID as the parent and be linked as a child span

#### Scenario: Extract context from span
- **WHEN** `observabilitySpanGetContext(span)` is called with an active span
- **THEN** a `ObservabilitySpanContext*` SHALL be returned that can be passed to other functions or threads

#### Scenario: Null parent creates root span
- **WHEN** `observabilitySpanStartWithParent("op", NULL)` is called
- **THEN** a new root span with a new trace ID SHALL be created

### Requirement: Device discovery tracing
The device discovery flow SHALL be instrumented with spans covering the full discovery lifecycle per subsystem.

#### Scenario: Discovery start span
- **WHEN** a discovery operation begins for a device class
- **THEN** a root span named `device.discovery` SHALL be created with attributes `device.class` and `discovery.recovery_mode`

#### Scenario: Device found child span
- **WHEN** a device is found during discovery
- **THEN** a child span named `device.found` SHALL be created under the discovery span with attributes `device.class`, `device.uuid`, `device.manufacturer`, and `device.model`

#### Scenario: Discovery end
- **WHEN** the discovery operation completes (success or timeout)
- **THEN** the `device.discovery` span SHALL be ended with its status reflecting the outcome

### Requirement: Resource operation tracing
Resource read and write operations SHALL be instrumented with spans.

#### Scenario: Resource read span
- **WHEN** a resource read operation is performed
- **THEN** a span named `resource.read` SHALL be created with attribute `resource.uri`

#### Scenario: Resource write span
- **WHEN** a resource write operation is performed
- **THEN** a span named `resource.write` SHALL be created with attribute `resource.uri`

### Requirement: Subsystem lifecycle tracing
Subsystem initialization and shutdown SHALL be instrumented with spans.

#### Scenario: Subsystem init span
- **WHEN** a subsystem's `initialize` callback is invoked
- **THEN** a span named `subsystem.init` SHALL be created

#### Scenario: Subsystem shutdown span
- **WHEN** a subsystem's `shutdown` callback is invoked
- **THEN** a span named `subsystem.shutdown` SHALL be created

### Requirement: Tracing unit tests
The tracing adapter module SHALL have C++ unit tests (Google Test) under `core/test/` that verify span lifecycle, context propagation, and attribute handling using in-memory exporters.

#### Scenario: Unit test span creation and attributes
- **WHEN** the tracing unit tests are run
- **THEN** they SHALL verify that `observabilitySpanStart` creates a span, `observabilitySpanSetAttribute` attaches attributes, and `observabilitySpanRelease` finalizes the span — all captured via an InMemorySpanExporter

#### Scenario: Unit test parent-child context
- **WHEN** a span is created with `observabilitySpanStartWithParent`
- **THEN** the unit test SHALL verify the child span shares the parent's trace ID

#### Scenario: Unit test no-op behavior
- **WHEN** the C-linkage tracing API is called with `BARTON_CONFIG_OBSERVABILITY` undefined
- **THEN** the unit test SHALL verify all functions are safe no-ops (NULL returns, no crashes)

### Requirement: Tracing integration tests
The observability tracing behavior SHALL be verified by pytest integration tests under `testing/` that exercise the full BartonCore library with `BCORE_OBSERVABILITY=ON`.

#### Scenario: Integration test traces emitted during device operations
- **WHEN** a pytest integration test starts a `BCore.Client` with OpenTelemetry enabled, performs a device operation (e.g., discovery or commissioning), and collects exported OTLP data
- **THEN** the test SHALL verify that spans with expected names (`device.discovery`, `subsystem.init`, etc.) and attributes are present in the collected data
