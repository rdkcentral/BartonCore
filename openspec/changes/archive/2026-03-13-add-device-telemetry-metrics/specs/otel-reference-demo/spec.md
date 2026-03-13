## MODIFIED Requirements

### Requirement: Observability documentation
The observability documentation SHALL describe the full metrics stack including Prometheus, Jaeger SPM, system/process metrics, and device telemetry metrics.

#### Scenario: Dedicated metrics reference
- **WHEN** a developer seeks metrics information
- **THEN** they SHALL find `docs/OBSERVABILITY_METRICS.md` containing all metric tables including device telemetry metrics and Prometheus query reference examples

#### Scenario: Device telemetry section
- **WHEN** a developer reads `docs/OBSERVABILITY_METRICS.md`
- **THEN** they SHALL find a "Device Telemetry Metrics" section covering numeric resource gauges, state transition counters, update rate counters, Zigbee link quality gauges, and Matter subscription lifecycle counters

#### Scenario: Device telemetry queries
- **WHEN** a developer reads the Prometheus Query Reference in `docs/OBSERVABILITY_METRICS.md`
- **THEN** they SHALL find a "Device Telemetry" query group with PromQL examples for fleet-level monitoring (temperature across sensors, device update rates, RSSI distribution, state transitions)
