## ADDED Requirements

### Requirement: Zigbee device discovery span
The observability layer SHALL trace Zigbee device discovery handling in the driver layer.

#### Scenario: Zigbee discovery span created
- **WHEN** `deviceDiscoveredCallback()` fires in `zigbeeDriverCommon.c`
- **THEN** a child span named `zigbee.device.discovered` SHALL be created under the `device.discovery` span context (if available) with attributes `device.eui64` and `device.manufacturer`

#### Scenario: Zigbee discovery span records outcome
- **WHEN** the device discovered callback completes
- **THEN** the span SHALL be ended with status reflecting whether the device was accepted or rejected

### Requirement: Zigbee cluster read span
The observability layer SHALL trace Zigbee cluster attribute read operations at the driver layer.

#### Scenario: Cluster read span created
- **WHEN** a Zigbee device driver's `readResource()` is invoked via `zigbeeDriverCommon.c`
- **THEN** a span named `zigbee.cluster.read` SHALL be created with attributes `device.uuid`, `resource.uri`, and `endpoint.id`

#### Scenario: Cluster read span records success
- **WHEN** the read operation returns a value successfully
- **THEN** the `zigbee.cluster.read` span SHALL be ended with OK status

#### Scenario: Cluster read span records failure
- **WHEN** the read operation fails
- **THEN** the `zigbee.cluster.read` span SHALL be ended with ERROR status and an error description

### Requirement: Zigbee cluster write span
The observability layer SHALL trace Zigbee cluster attribute write operations at the driver layer.

#### Scenario: Cluster write span created
- **WHEN** a Zigbee device driver's `writeResource()` is invoked via `zigbeeDriverCommon.c`
- **THEN** a span named `zigbee.cluster.write` SHALL be created with attributes `device.uuid`, `resource.uri`, and `endpoint.id`

#### Scenario: Cluster write span records success
- **WHEN** the write operation succeeds
- **THEN** the `zigbee.cluster.write` span SHALL be ended with OK status

#### Scenario: Cluster write span records failure
- **WHEN** the write operation fails
- **THEN** the `zigbee.cluster.write` span SHALL be ended with ERROR status
