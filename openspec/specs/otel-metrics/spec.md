## Requirements

### Requirement: Metrics provider initialization
The observability layer SHALL configure a `MeterProvider` with an OTLP HTTP metric exporter during initialization. The `MeterProvider` SHALL use a periodic exporting metric reader with a configurable export interval.

#### Scenario: Metrics initialization
- **WHEN** `deviceServiceInitialize()` is called with `BCORE_OBSERVABILITY=ON`
- **THEN** a `MeterProvider` SHALL be created with a periodic metric reader exporting via OTLP HTTP

#### Scenario: Default export interval
- **WHEN** no custom export interval is configured
- **THEN** the metric reader SHALL export at a 10-second interval

#### Scenario: Metrics disabled
- **WHEN** `BCORE_OBSERVABILITY=OFF`
- **THEN** all metrics API calls SHALL be no-ops with zero runtime overhead

### Requirement: C-linkage metrics API
The metrics adapter SHALL expose a C99-compatible API via `extern "C"` functions for creating and recording counters and gauges. Instruments SHALL be identified by name and obtained once, then recorded against repeatedly.

#### Scenario: Create a counter
- **WHEN** `observabilityCounterCreate("device.discovery.count", "Number of discovery attempts")` is called
- **THEN** a monotonic counter instrument SHALL be registered with the MeterProvider

#### Scenario: Increment a counter
- **WHEN** `observabilityCounterAdd(counter, 1, attributes)` is called
- **THEN** the counter value SHALL increase by 1 with the given attribute set

#### Scenario: Create a gauge
- **WHEN** `observabilityGaugeCreate("device.active.count", "Number of active devices")` is called
- **THEN** an observable gauge instrument SHALL be registered with the MeterProvider

#### Scenario: Record a gauge value
- **WHEN** `observabilityGaugeRecord(gauge, 42, attributes)` is called
- **THEN** the gauge SHALL report the value 42 with the given attributes at the next export

#### Scenario: No-op when disabled
- **WHEN** `BCORE_OBSERVABILITY=OFF` and `observabilityCounterAdd(counter, 1, NULL)` is called
- **THEN** the call SHALL be a no-op

### Requirement: Device count metrics
The system SHALL maintain a gauge tracking the number of active devices, broken down by device class.

#### Scenario: Device added
- **WHEN** a device is successfully added to the device database
- **THEN** the `device.active.count` gauge SHALL be updated with the current count for that device class

#### Scenario: Device removed
- **WHEN** a device is removed from the device database
- **THEN** the `device.active.count` gauge SHALL be decremented for that device class

#### Scenario: Device class dimension
- **WHEN** active device count is exported
- **THEN** the metric SHALL include an attribute `device.class` with values like `light`, `sensor`, `doorLock`

### Requirement: Communication failure metrics
The system SHALL maintain a counter tracking device communication failures and restores.

#### Scenario: Comm fail counted
- **WHEN** the communication watchdog detects a device comm failure
- **THEN** a counter `device.commfail.count` SHALL be incremented with attributes `device.class` and `subsystem.name`

#### Scenario: Comm restore counted
- **WHEN** a device communication is restored
- **THEN** a counter `device.commrestore.count` SHALL be incremented with attributes `device.class` and `subsystem.name`

### Requirement: Metrics unit tests
The metrics adapter module SHALL have C++ unit tests (Google Test) under `core/test/` that verify counter and gauge creation, recording, and attribute handling.

#### Scenario: Unit test counter increment
- **WHEN** the metrics unit tests are run
- **THEN** they SHALL verify that `observabilityCounterCreate` registers an instrument, `observabilityCounterAdd` records values, and the data is captured via an in-memory metric exporter

#### Scenario: Unit test gauge recording
- **WHEN** `observabilityGaugeRecord` is called
- **THEN** the unit test SHALL verify the gauge value and attributes are present in exported metric data

#### Scenario: Unit test no-op behavior
- **WHEN** the C-linkage metrics API is called with `BARTON_CONFIG_OBSERVABILITY` undefined
- **THEN** the unit test SHALL verify all functions are safe no-ops

### Requirement: Metrics integration tests
The observability metrics behavior SHALL be verified by pytest integration tests under `testing/` that exercise the full BartonCore library with `BCORE_OBSERVABILITY=ON`.

#### Scenario: Integration test metrics emitted during device lifecycle
- **WHEN** a pytest integration test starts a `BCore.Client` with OpenTelemetry enabled, adds a device, and collects exported OTLP data
- **THEN** the test SHALL verify that metrics with expected names (`device.active.count`, `device.commfail.count`, etc.) are present in the collected data
