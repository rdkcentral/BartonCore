## ADDED Requirements

### Requirement: Counter metric instrument
The system SHALL provide an `ObservabilityCounter` opaque type that tracks a monotonically increasing uint64 value. The API SHALL support `observabilityCounterCreate(name)`, `observabilityCounterAdd(counter, value)`, and `observabilityCounterAddWithAttrs(counter, value, ...)` with NULL-terminated key-value attribute pairs.

#### Scenario: Counter increments
- **WHEN** `observabilityCounterAdd(counter, 5)` is called twice
- **THEN** the counter's value is 10

#### Scenario: Counter with attributes
- **WHEN** `observabilityCounterAddWithAttrs(counter, 1, "driver", "light", NULL)` is called
- **THEN** the counter tracks the value 1 associated with the attribute `driver=light`

### Requirement: Gauge metric instrument
The system SHALL provide an `ObservabilityGauge` opaque type that records a current int64 value. The API SHALL support `observabilityGaugeCreate(name)`, `observabilityGaugeRecord(gauge, value)`, and `observabilityGaugeRecordWithAttrs(gauge, value, ...)`.

#### Scenario: Gauge records latest value
- **WHEN** `observabilityGaugeRecord(gauge, 100)` then `observabilityGaugeRecord(gauge, 50)` are called
- **THEN** the gauge's current value is 50

### Requirement: Histogram metric instrument
The system SHALL provide an `ObservabilityHistogram` opaque type that records double values into a distribution. The API SHALL support `observabilityHistogramCreate(name)`, `observabilityHistogramRecord(histogram, value)`, and `observabilityHistogramRecordWithAttrs(histogram, value, ...)`.

#### Scenario: Histogram records distribution
- **WHEN** values 1.0, 2.0, 3.0 are recorded to a histogram
- **THEN** the histogram reports count=3, sum=6.0, and appropriate bucket distributions

### Requirement: Telemetry JSON dump command
The reference app SHALL support a `gettelemetry` (or `gt`) command that dumps all registered metrics as JSON to stdout. The output SHALL include all counters, gauges, and histograms with their current values, organized by metric name.

#### Scenario: gettelemetry returns JSON
- **WHEN** the user issues the `gt` command in the reference app
- **THEN** a JSON object is printed containing all registered metrics with their names and current values

#### Scenario: Metrics include SBMD driver stats
- **WHEN** SBMD drivers are loaded and handling device operations
- **THEN** the telemetry dump includes handler invocation time histograms and JS heap usage gauges

### Requirement: Conditional compilation
The observability API SHALL compile to no-op inline stubs when the `BARTON_CONFIG_OBSERVABILITY` CMake flag is disabled. Call sites SHALL not require conditional compilation guards.

#### Scenario: Disabled at build time
- **WHEN** `BARTON_CONFIG_OBSERVABILITY` is OFF
- **THEN** all `observabilityCounter*`, `observabilityGauge*`, `observabilityHistogram*` calls compile to no-ops with zero runtime cost
