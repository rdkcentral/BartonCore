## ADDED Requirements

### Requirement: Numeric resource value gauge
The observability layer SHALL emit an OTel gauge recording the current numeric value whenever a device resource of numeric type is updated.

#### Scenario: Integer resource emits gauge
- **WHEN** a device resource with type `com.icontrol.integer` is updated via `updateResource()`
- **THEN** the system SHALL record the parsed integer value to the `device.resource.value` gauge with attributes `device.id`, `endpoint.profile`, and `resource.name`

#### Scenario: Percentage resource emits gauge
- **WHEN** a device resource with type `com.icontrol.percentage` is updated via `updateResource()`
- **THEN** the system SHALL record the parsed percentage value to the `device.resource.value` gauge with the same attributes

#### Scenario: Non-numeric resource does not emit gauge
- **WHEN** a device resource with type `com.icontrol.boolean` or `com.icontrol.string` is updated
- **THEN** the system SHALL NOT record a value to the `device.resource.value` gauge

#### Scenario: Metric attributes include rich device metadata
- **WHEN** any resource update emits a metric (gauge or counter)
- **THEN** the metric SHALL include attributes: `device.id`, `device.class`, `endpoint.profile`, `resource.name`, `device.manufacturer`, `device.model`, `device.hardware_version`, `device.firmware_version`, and `driver.type`

#### Scenario: Metric attributes include endpoint profile
- **WHEN** any numeric resource update emits a gauge
- **THEN** the `endpoint.profile` attribute SHALL contain the endpoint's profile name (e.g., "light", "sensor", "thermostat", "doorLock")

#### Scenario: Device metadata sourced from device resources
- **WHEN** device metadata attributes are populated
- **THEN** `device.manufacturer` SHALL come from the device's "manufacturer" resource, `device.model` from "model", `device.hardware_version` from "hardwareVersion", and `device.firmware_version` from "firmwareVersion"

#### Scenario: Device metadata is cached per device
- **WHEN** a device's resource is updated for the first time
- **THEN** the system SHALL look up and cache the device metadata attributes for subsequent updates from the same device

### Requirement: Boolean state transition counter
The observability layer SHALL emit an OTel counter increment whenever a device resource of boolean type changes state.

#### Scenario: Boolean resource state change increments counter
- **WHEN** a device resource with type `com.icontrol.boolean` is updated and the value has changed
- **THEN** the system SHALL increment the `device.resource.state_change` counter with the full device metadata attribute set (`device.id`, `device.class`, `endpoint.profile`, `resource.name`, `device.manufacturer`, `device.model`, `device.hardware_version`, `device.firmware_version`, `driver.type`) plus `state` (the new value, "true" or "false")

#### Scenario: Unchanged boolean value does not increment counter
- **WHEN** a device resource with type `com.icontrol.boolean` is updated but the value is the same as the previous value
- **THEN** the system SHALL NOT increment the `device.resource.state_change` counter

### Requirement: Resource update rate counter
The observability layer SHALL emit an OTel counter increment for every resource update that results in a value change, regardless of resource type.

#### Scenario: Any resource value change increments update counter
- **WHEN** any device resource is updated via `updateResource()` and the value has changed
- **THEN** the system SHALL increment the `device.resource.update` counter with the full device metadata attribute set (`device.id`, `device.class`, `endpoint.profile`, `resource.name`, `device.manufacturer`, `device.model`, `device.hardware_version`, `device.firmware_version`, `driver.type`)

#### Scenario: No-op update does not increment counter
- **WHEN** a device resource is updated via `updateResource()` but the value is identical to the current value
- **THEN** the system SHALL NOT increment the `device.resource.update` counter

### Requirement: Zigbee link quality gauges
The observability layer SHALL emit OTel gauges for Zigbee device RSSI and LQI values when diagnostics are collected.

#### Scenario: RSSI gauge emitted on diagnostics update
- **WHEN** the Zigbee diagnostics cluster reports an RSSI value for a device via `handleRssiLqiUpdated()`
- **THEN** the system SHALL record the RSSI value (dBm) to the `zigbee.device.rssi` gauge with attribute `device.id`

#### Scenario: LQI gauge emitted on diagnostics update
- **WHEN** the Zigbee diagnostics cluster reports an LQI value for a device via `handleRssiLqiUpdated()`
- **THEN** the system SHALL record the LQI value (0-255) to the `zigbee.device.lqi` gauge with attribute `device.id`

### Requirement: Matter subscription lifecycle counters
The observability layer SHALL emit OTel counters for Matter device subscription lifecycle events.

#### Scenario: Subscription established counter
- **WHEN** a Matter device subscription is established or re-established via `OnSubscriptionEstablished()` in `DeviceDataCache`
- **THEN** the system SHALL increment the `matter.subscription.established` counter with attribute `device.id`

#### Scenario: Subscription report counter
- **WHEN** a Matter device subscription report cycle completes via `OnReportEnd()` in `DeviceDataCache`
- **THEN** the system SHALL increment the `matter.subscription.report` counter with attribute `device.id`

#### Scenario: Subscription error counter
- **WHEN** a Matter device subscription encounters an error
- **THEN** the system SHALL increment the `matter.subscription.error` counter with attribute `device.id`

### Requirement: Matter CASE session metrics
The observability layer SHALL emit OTel metrics for Matter CASE session establishment using the existing `DeviceDataCache` connection callbacks.

#### Scenario: CASE session duration histogram
- **WHEN** a Matter device CASE session is established via `OnDeviceConnected()` in `DeviceDataCache`
- **THEN** the system SHALL record the elapsed time (seconds) from the `GetConnectedDevice()` call to the `matter.case.session.duration` histogram with attribute `device.id`

#### Scenario: CASE session error counter
- **WHEN** a Matter device CASE session fails via `OnDeviceConnectionFailure()` in `DeviceDataCache`
- **THEN** the system SHALL increment the `matter.case.session.error` counter with attributes `device.id` and `error` (the CHIP_ERROR code)

### Requirement: Compile-time observability guard
All device telemetry metrics SHALL be guarded by the existing `BARTON_CONFIG_OBSERVABILITY` compile flag.

#### Scenario: Metrics disabled at compile time
- **WHEN** BartonCore is built with `BCORE_OBSERVABILITY=OFF`
- **THEN** no device telemetry metric code SHALL be compiled and zero runtime overhead SHALL be incurred

#### Scenario: Metrics enabled at compile time
- **WHEN** BartonCore is built with `BCORE_OBSERVABILITY=ON`
- **THEN** all device telemetry metrics SHALL be active and emitting to the configured OTel exporter

### Requirement: Lazy meter initialization
Device telemetry OTel meters SHALL be created lazily on first use, not eagerly at startup.

#### Scenario: Meters created on first resource update
- **WHEN** the first device resource update occurs after startup with observability enabled
- **THEN** the device telemetry meters SHALL be created at that point and cached for subsequent use

#### Scenario: Meters not created if no devices update
- **WHEN** the observability layer is enabled but no device resource updates occur
- **THEN** no device telemetry meters SHALL be created

### Requirement: Device telemetry documentation
The device telemetry metrics SHALL be documented in `docs/OBSERVABILITY_METRICS.md`.

#### Scenario: Metrics tables in documentation
- **WHEN** a developer reads `docs/OBSERVABILITY_METRICS.md`
- **THEN** they SHALL find a "Device Telemetry Metrics" section listing all device telemetry meters with their names, types, units, attributes, and descriptions

#### Scenario: Prometheus query examples
- **WHEN** a developer reads the Prometheus Query Reference in `docs/OBSERVABILITY_METRICS.md`
- **THEN** they SHALL find example PromQL queries for common device telemetry use cases (fleet temperature overview, device update rates, link quality distribution, state transition counts)
