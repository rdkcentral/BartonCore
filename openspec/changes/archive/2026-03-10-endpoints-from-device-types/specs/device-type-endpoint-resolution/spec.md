## ADDED Requirements

### Requirement: Endpoint resolution by device type matching
The system SHALL resolve Matter endpoint IDs for SBMD endpoints by matching the endpoint's Descriptor device type list against the driver's `matterMeta.deviceTypes` list, rather than searching for the first endpoint that hosts a specific cluster.

#### Scenario: Single-endpoint device with matching device type
- **WHEN** a Matter device has one functional endpoint (e.g., endpoint 1) whose Descriptor device type list contains a device type ID present in the SBMD spec's `matterMeta.deviceTypes`
- **THEN** SBMD endpoint index 0 SHALL resolve to that Matter endpoint ID

#### Scenario: Multi-endpoint device with multiple matching endpoints
- **WHEN** a Matter device has multiple functional endpoints (e.g., endpoints 1, 2, 3) whose Descriptor device type lists each contain a device type ID present in `matterMeta.deviceTypes`
- **THEN** the matching endpoints SHALL be collected in PartsList order and assigned to SBMD endpoints by positional index (SBMD endpoint 0 → first match, SBMD endpoint 1 → second match, etc.)

#### Scenario: Endpoint with non-matching device type is skipped
- **WHEN** a Matter device has an endpoint whose Descriptor device type list does not contain any device type ID from `matterMeta.deviceTypes`
- **THEN** that endpoint SHALL NOT be included in the resolved endpoint map

#### Scenario: Root endpoint (endpoint 0) is excluded
- **WHEN** iterating Matter endpoints for device type matching
- **THEN** endpoint 0 (the root/aggregator endpoint) SHALL be skipped

### Requirement: Endpoint map built during device initialization
The system SHALL build the SBMD-endpoint-to-Matter-endpoint mapping once during `AddDevice()`, before resource binding and subscription setup.

#### Scenario: Map built before resource binding
- **WHEN** `AddDevice()` is called for an SBMD-driven device
- **THEN** the endpoint map SHALL be fully resolved before any `BindResourceReadInfo()`, `BindWriteInfo()`, `BindExecuteInfo()`, or `BindResourceEventInfo()` calls

#### Scenario: Map is read-only after construction
- **WHEN** the endpoint map has been built during initialization
- **THEN** the map SHALL NOT be modified during subscription callbacks or attribute/event processing

### Requirement: Nth endpoint selection for SBMD endpoints
The system SHALL support selecting the Nth matching Matter endpoint based on the SBMD endpoint's positional index in the spec's `endpoints` array.

#### Scenario: First SBMD endpoint maps to first matching Matter endpoint
- **WHEN** an SBMD spec defines `endpoints[0]` and the device has matching Matter endpoints at endpoints 1 and 3
- **THEN** `endpoints[0]` resources SHALL be bound to Matter endpoint 1

#### Scenario: Second SBMD endpoint maps to second matching Matter endpoint
- **WHEN** an SBMD spec defines `endpoints[1]` and the device has matching Matter endpoints at endpoints 1 and 3
- **THEN** `endpoints[1]` resources SHALL be bound to Matter endpoint 3

#### Scenario: SBMD endpoint with no corresponding Nth match
- **WHEN** an SBMD spec defines more endpoints than there are matching Matter endpoints on the device
- **THEN** resources on the unmatched SBMD endpoints SHALL NOT be bound and the system SHALL log a warning

### Requirement: Resource binding uses resolved endpoint map
All SBMD resource binding methods SHALL use the pre-resolved endpoint map instead of `GetEndpointForCluster()` to determine the Matter endpoint ID.

#### Scenario: Attribute read binding uses endpoint map
- **WHEN** a resource mapper specifies `read.attribute.clusterId`
- **THEN** the system SHALL look up the Matter endpoint from the endpoint map using the SBMD endpoint index, then bind the attribute path with that endpoint and the specified cluster/attribute IDs

#### Scenario: Command write binding uses endpoint map
- **WHEN** a resource mapper specifies a write command with a cluster ID
- **THEN** the system SHALL use the endpoint map to resolve the target Matter endpoint, not `GetEndpointForCluster()`

#### Scenario: Event binding uses endpoint map
- **WHEN** a resource mapper specifies `event.event.clusterId` and `eventId`
- **THEN** the event path SHALL be constructed using the Matter endpoint from the endpoint map

### Requirement: Initialization failure on no matching endpoint
The system SHALL fail device initialization if no Matter endpoint matches any device type in `matterMeta.deviceTypes`.

#### Scenario: No matching device type found on any endpoint
- **WHEN** `ResolveEndpointMap()` iterates all device endpoints and finds no device type match
- **THEN** the method SHALL return false and `AddDevice()` SHALL fail the device with an error log

#### Scenario: At least one match found
- **WHEN** at least one Matter endpoint has a matching device type
- **THEN** `ResolveEndpointMap()` SHALL return true and device initialization SHALL proceed
