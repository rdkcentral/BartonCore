## MODIFIED Requirements

### Requirement: C-linkage metrics API
The metrics adapter SHALL expose a C99-compatible API via `extern "C"` functions for creating and recording counters, gauges, and histograms. Instruments SHALL be identified by name and obtained once, then recorded against repeatedly. Counter and gauge recording functions SHALL support optional string key-value attribute pairs via NULL-terminated variadic variants.

#### Scenario: Create a counter
- **WHEN** `observabilityCounterCreate("device.discovery.count", "Number of discovery attempts", "{event}")` is called
- **THEN** a monotonic counter instrument SHALL be registered with the MeterProvider

#### Scenario: Increment a counter
- **WHEN** `observabilityCounterAdd(counter, 1)` is called
- **THEN** the counter value SHALL increase by 1 with no attributes

#### Scenario: Increment a counter with attributes
- **WHEN** `observabilityCounterAddWithAttrs(counter, 1, "device.class", "light", NULL)` is called
- **THEN** the counter value SHALL increase by 1 with attribute `device.class=light`

#### Scenario: Create a gauge
- **WHEN** `observabilityGaugeCreate("device.active.count", "Number of active devices", "{device}")` is called
- **THEN** an observable gauge instrument SHALL be registered with the MeterProvider

#### Scenario: Record a gauge value
- **WHEN** `observabilityGaugeRecord(gauge, 42)` is called
- **THEN** the gauge SHALL report the value 42 at the next export

#### Scenario: Record a gauge value with attributes
- **WHEN** `observabilityGaugeRecordWithAttrs(gauge, 42, "subsystem.name", "matter", NULL)` is called
- **THEN** the gauge SHALL report the value 42 with attribute `subsystem.name=matter` at the next export

#### Scenario: Create a histogram
- **WHEN** `observabilityHistogramCreate("device.discovery.duration", "Duration of discovery", "s")` is called
- **THEN** a histogram instrument SHALL be registered with the MeterProvider

#### Scenario: Record a histogram value
- **WHEN** `observabilityHistogramRecord(histogram, 2.5)` is called
- **THEN** the value 2.5 SHALL be recorded into the histogram distribution

#### Scenario: No-op when disabled
- **WHEN** `BCORE_OBSERVABILITY=OFF` and any metrics API function is called
- **THEN** the call SHALL be a no-op

### Requirement: Metrics unit tests
The metrics adapter module SHALL have C++ unit tests (Google Test) under `core/test/` that verify counter, gauge, and histogram creation, recording, attribute handling, and no-op behavior.

#### Scenario: Unit test counter increment with attributes
- **WHEN** the metrics unit tests are run
- **THEN** they SHALL verify that `observabilityCounterAddWithAttrs` records values with attributes captured in the in-memory metric exporter

#### Scenario: Unit test histogram recording
- **WHEN** the metrics unit tests are run
- **THEN** they SHALL verify that `observabilityHistogramCreate` registers an instrument and `observabilityHistogramRecord` records values into the expected distribution

#### Scenario: Unit test gauge recording
- **WHEN** `observabilityGaugeRecord` is called
- **THEN** the unit test SHALL verify the gauge value and attributes are present in exported metric data

#### Scenario: Unit test no-op behavior
- **WHEN** the C-linkage metrics API is called with `BARTON_CONFIG_OBSERVABILITY` undefined
- **THEN** the unit test SHALL verify all functions are safe no-ops

### Requirement: Metrics integration tests
The observability metrics behavior SHALL be verified by pytest integration tests under `testing/` that exercise the full BartonCore library with `BCORE_OBSERVABILITY=ON`.

#### Scenario: Integration test new metrics emitted during device lifecycle
- **WHEN** a pytest integration test starts a `BCore.Client` with OpenTelemetry enabled, performs discovery and device operations, and collects exported OTLP data
- **THEN** the test SHALL verify that new metric names (`device.discovery.started`, `device.add.success`, `subsystem.init.completed`, `event.produced`, etc.) are present in the collected data
