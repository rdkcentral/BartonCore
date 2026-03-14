## ADDED Requirements

### Requirement: Matter attribute change span
The observability layer SHALL trace resource updates triggered by Matter attribute changes at the driver layer.

#### Scenario: Attribute change span created
- **WHEN** `MatterDevice::CacheCallback::OnAttributeChanged()` triggers a call to `updateResource()`
- **THEN** a span named `resource.update` SHALL be created with attributes `device.uuid`, `resource.name`, and `endpoint.profile`

#### Scenario: Attribute change span covers update pipeline
- **WHEN** the resource update span is active
- **THEN** it SHALL cover the full pipeline from driver callback through `updateResource()` including database save and event production

#### Scenario: Attribute change span records success
- **WHEN** the resource update completes
- **THEN** the `resource.update` span SHALL be ended with OK status

### Requirement: Zigbee resource write update span
The observability layer SHALL trace resource updates triggered by Zigbee driver write callbacks.

#### Scenario: Zigbee write update span created
- **WHEN** `writeResource()` in `zigbeeDriverCommon.c` calls `updateResource()` after a successful write
- **THEN** a span named `resource.update` SHALL be created as a child of the `zigbee.cluster.write` span with attributes `device.uuid` and `resource.uri`

#### Scenario: Zigbee write update span records outcome
- **WHEN** the resource update completes
- **THEN** the span SHALL be ended with OK status
