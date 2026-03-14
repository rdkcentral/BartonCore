## MODIFIED Requirements

### Requirement: Device discovery tracing
The device discovery flow SHALL be instrumented with spans covering the full discovery lifecycle per subsystem, with parent-child relationships linking discovery to device found operations.

#### Scenario: Discovery start span
- **WHEN** a discovery operation begins for a device class
- **THEN** a root span named `device.discovery` SHALL be created with attributes `device.class` and `discovery.recovery_mode`

#### Scenario: Discovery context set as current
- **WHEN** the `device.discovery` span is created
- **THEN** its context SHALL be set as the thread-local current context so that callbacks invoked during discovery (e.g., `onDeviceFoundCallback`) can create child spans without explicit context passing

#### Scenario: Device found child span
- **WHEN** a device is found during discovery
- **THEN** a child span named `device.found` SHALL be created as a child of the current thread-local context (the `device.discovery` span) rather than as an isolated root span, with attributes `device.class`, `device.uuid`, `device.manufacturer`, and `device.model`

#### Scenario: Discovery context propagated to drivers
- **WHEN** a device is found and driver configuration begins
- **THEN** the `device.discovery` span context SHALL be available via thread-local current context for creating child spans in driver-level callbacks (e.g., Zigbee `deviceDiscoveredCallback`)

#### Scenario: Discovery end
- **WHEN** the discovery operation completes (success or timeout)
- **THEN** the `device.discovery` span SHALL be ended with its status reflecting the outcome

## ADDED Requirements

### Requirement: Span context ref-counting
`ObservabilitySpanContext` SHALL support explicit reference counting for safe sharing across threads and object lifetimes.

#### Scenario: Acquire additional reference
- **WHEN** `observabilitySpanContextRef(ctx)` is called with a valid context
- **THEN** the context's reference count SHALL be atomically incremented

#### Scenario: Safe no-op on null
- **WHEN** `observabilitySpanContextRef(NULL)` is called
- **THEN** the call SHALL be a safe no-op

### Requirement: Thread-local current span context
The tracing API SHALL provide thread-local storage for a current span context, allowing same-thread flows to inherit context implicitly without modifying function signatures.

#### Scenario: Set current context
- **WHEN** `observabilitySpanContextSetCurrent(ctx)` is called with a valid context
- **THEN** the context SHALL be ref-acquired and stored as the calling thread's current context, and any previously-set current context SHALL be released

#### Scenario: Set current to null clears context
- **WHEN** `observabilitySpanContextSetCurrent(NULL)` is called
- **THEN** the calling thread's current context SHALL be cleared and the previously-set context SHALL be released

#### Scenario: Get current context returns borrowed reference
- **WHEN** `observabilitySpanContextGetCurrent()` is called on a thread with a current context set
- **THEN** a pointer to the current context SHALL be returned WITHOUT incrementing the reference count (borrowed reference)

#### Scenario: Get current returns null when unset
- **WHEN** `observabilitySpanContextGetCurrent()` is called on a thread with no current context
- **THEN** NULL SHALL be returned

#### Scenario: Thread-local isolation
- **WHEN** thread A sets a current context
- **THEN** thread B's current context SHALL NOT be affected

#### Scenario: No-op when observability disabled
- **WHEN** `BARTON_CONFIG_OBSERVABILITY` is not defined
- **THEN** `observabilitySpanContextRef()`, `observabilitySpanContextSetCurrent()`, and `observabilitySpanContextGetCurrent()` SHALL be safe no-ops (no-op, no-op, return NULL respectively)

### Requirement: Tracing integration tests
The observability tracing behavior SHALL be verified by pytest integration tests under `testing/` that exercise the full BartonCore library with `BCORE_OBSERVABILITY=ON`.

#### Scenario: Integration test traces emitted during device operations
- **WHEN** a pytest integration test starts a `BCore.Client` with OpenTelemetry enabled, performs a device operation (e.g., discovery or commissioning), and collects exported OTLP data
- **THEN** the test SHALL verify that spans with expected names (`device.discovery`, `subsystem.init`, etc.) and attributes are present in the collected data

#### Scenario: Integration test verifies span parent-child relationships
- **WHEN** an integration test performs an operation that produces a span hierarchy (e.g., commissioning → CASE connect → subscribe)
- **THEN** the test SHALL verify that child spans share the same trace ID as their parent and that each child's `parentSpanId` matches the parent's `spanId`
