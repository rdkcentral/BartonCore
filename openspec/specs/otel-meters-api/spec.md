## ADDED Requirements

### Requirement: Counter attribute support
The metrics adapter SHALL provide a function `observabilityCounterAddWithAttrs()` that records a counter increment with a set of string key-value attribute pairs. The attribute list SHALL be NULL-terminated.

#### Scenario: Counter with attributes
- **WHEN** `observabilityCounterAddWithAttrs(counter, 1, "device.class", "light", "subsystem.name", "zigbee", NULL)` is called
- **THEN** the counter SHALL increment by 1 and the exported metric data point SHALL include attributes `device.class=light` and `subsystem.name=zigbee`

#### Scenario: Counter with no attributes via sentinel
- **WHEN** `observabilityCounterAddWithAttrs(counter, 1, NULL)` is called
- **THEN** the counter SHALL increment by 1 with no attributes, equivalent to `observabilityCounterAdd(counter, 1)`

#### Scenario: Counter with NULL handle
- **WHEN** `observabilityCounterAddWithAttrs(NULL, 1, "key", "val", NULL)` is called
- **THEN** the call SHALL be a safe no-op

#### Scenario: No-op when disabled
- **WHEN** `BCORE_OBSERVABILITY=OFF` and `observabilityCounterAddWithAttrs(counter, 1, "key", "val", NULL)` is called
- **THEN** the call SHALL be a no-op with zero overhead

### Requirement: Gauge attribute support
The metrics adapter SHALL provide a function `observabilityGaugeRecordWithAttrs()` that records a gauge value with a set of string key-value attribute pairs. The attribute list SHALL be NULL-terminated.

#### Scenario: Gauge with attributes
- **WHEN** `observabilityGaugeRecordWithAttrs(gauge, 5, "subsystem.name", "matter", NULL)` is called
- **THEN** the gauge SHALL report value 5 with attribute `subsystem.name=matter` at the next export

#### Scenario: No-op when disabled
- **WHEN** `BCORE_OBSERVABILITY=OFF` and `observabilityGaugeRecordWithAttrs(gauge, 5, "key", "val", NULL)` is called
- **THEN** the call SHALL be a no-op with zero overhead

### Requirement: Histogram instrument type
The metrics adapter SHALL provide functions to create and record histogram instruments for measuring distributions (e.g., durations). Histogram values SHALL be `double` to support sub-second precision.

#### Scenario: Create a histogram
- **WHEN** `observabilityHistogramCreate("device.discovery.duration", "Duration of discovery operations", "s")` is called
- **THEN** a histogram instrument SHALL be registered with the MeterProvider

#### Scenario: Record a histogram value
- **WHEN** `observabilityHistogramRecord(histogram, 2.5)` is called
- **THEN** the value 2.5 SHALL be recorded into the histogram's bucket distribution

#### Scenario: Record histogram with attributes
- **WHEN** `observabilityHistogramRecordWithAttrs(histogram, 1.2, "subsystem.name", "matter", NULL)` is called
- **THEN** the value 1.2 SHALL be recorded with attribute `subsystem.name=matter`

#### Scenario: Release histogram
- **WHEN** `observabilityHistogramRelease(histogram)` is called
- **THEN** the histogram handle SHALL be freed when the last reference is dropped

#### Scenario: No-op histogram when disabled
- **WHEN** `BCORE_OBSERVABILITY=OFF` and `observabilityHistogramCreate(...)` is called
- **THEN** it SHALL return NULL and all subsequent histogram calls SHALL be safe no-ops

### Requirement: Attribute support unit tests
The extended metrics API SHALL have unit tests verifying attribute handling for counters, gauges, and histograms.

#### Scenario: Counter attributes in exported data
- **WHEN** `observabilityCounterAddWithAttrs` is called with known attributes
- **THEN** the unit test SHALL verify the attributes appear in the in-memory metric export

#### Scenario: Histogram bucket distribution
- **WHEN** multiple values are recorded to a histogram
- **THEN** the unit test SHALL verify the exported histogram contains the expected sum and count

#### Scenario: No-op stubs compile and link
- **WHEN** the unit test is compiled with `BARTON_CONFIG_OBSERVABILITY` undefined
- **THEN** all new API functions SHALL compile as no-ops without linker errors
