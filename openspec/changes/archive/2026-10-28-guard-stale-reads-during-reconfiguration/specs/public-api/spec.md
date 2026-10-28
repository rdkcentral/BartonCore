## ADDED Requirements

### Requirement: Readiness and reconfiguration error domain
The public API SHALL define a `BCoreServiceStateError` error domain (reported via the existing
`B_CORE_CLIENT_ERROR` quark) in the error-code range 300–399, with `B_CORE_SERVICE_STATE_ERROR_NOT_READY = 300` and
`B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING = 301`. The gated functions are exactly the single-device reads
`get_device_by_id()`, `get_device_by_uri()`, `get_endpoint_by_id()`, `get_endpoint_by_uri()`,
`get_resource_by_uri()`, `read_resource()`, `read_metadata()`, and the metadata mutation `write_metadata()`.
Each SHALL accept a `GError **err`, SHALL set `B_CORE_SERVICE_STATE_ERROR_NOT_READY` while the service does not
report itself ready for device operation, and otherwise SHALL set `B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING`
when the device that owns the target (the device UUID, or the first path segment of the URI) has a
pending or in-progress reconfiguration. A gated function that refuses SHALL NOT call into the device
service backend. Beyond these two codes, the gated functions keep their existing error behavior: a
target that does not exist or a backend write that fails is reported by a `NULL`/`FALSE` return
without setting `err`, except where an error was already set before this change (`read_resource()`
for an unreadable resource, `read_metadata()` for inaccessible metadata). All other public functions, including list getters, `write_resource()`,
`execute_resource()`, `change_resource_mode()`, and `open_commissioning_window()`, are unaffected.

#### Scenario: Service not ready
- **WHEN** a gated function is called while `b_core_client_get_status()` would report `ready-for-operation` as false (before the service has published readiness for device operation, or after shutdown begins)
- **THEN** the function SHALL fail, set a `GError` with domain `B_CORE_CLIENT_ERROR` and code
  `B_CORE_SERVICE_STATE_ERROR_NOT_READY`, and return `NULL`/`FALSE`

#### Scenario: Targeted device is reconfiguring
- **WHEN** a gated function is called for a device whose reconfiguration is pending
  or in progress
- **THEN** the function SHALL fail, set a `GError` with code `B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING`, and return
  `NULL`/`FALSE` rather than returning soon-to-be-stale data

#### Scenario: Another device is reconfiguring
- **WHEN** a gated function is called for a device that is not reconfiguring while a different device is
- **THEN** the function SHALL behave as if no device were reconfiguring

#### Scenario: Target not found
- **WHEN** a gated function that returns an object (`get_device_by_id()`, `get_device_by_uri()`, `get_endpoint_by_id()`, `get_endpoint_by_uri()`, `get_resource_by_uri()`, or `read_resource()`) is called while the service is ready, for a target that does not exist and whose owning device is not reconfiguring
- **THEN** the function SHALL return `NULL` without setting `err`

#### Scenario: URI without an owning device
- **WHEN** a URI-based gated function is called with a URI that does not begin with `/<uuid>` (for example `/` or a URI without a leading `/`)
- **THEN** only the readiness check SHALL apply, and the backend lookup SHALL report the target as not found

### Requirement: Reconfiguration completion notification
When a reconfiguration of an existing device finishes, the service SHALL emit
`device-configuration-completed` (on success) or `device-configuration-failed` (on failure) with the
device's UUID and device class and a session discovery type of `B_CORE_DISCOVERY_TYPE_NONE`, so that
clients holding data about the device know to re-query it. The event SHALL be emitted only after the
device is no longer reported as reconfiguring, and SHALL NOT be emitted for a reconfiguration
that finishes after the service has begun shutting down, or for a device that no longer has a stored record
(for example because saving the rebuilt device failed). A failure to save the rebuilt device SHALL NOT
be reported as success. If the device is removed while its reconfiguration is running or finishing,
the event SHALL be emitted before `device-removed`, provided the reconfiguration finishes within the
removal's 60-second wait and the removal is not made from that event's own handler. For a
reconfiguration started during startup, the event MAY be emitted before `READY_FOR_DEVICE_OPERATION`;
a client that waits for readiness and then queries the device observes the reconfigured data.

#### Scenario: Reconfiguration succeeds
- **WHEN** a reconfiguration of an existing device finishes successfully
- **THEN** the service SHALL emit `device-configuration-completed` with session discovery type `B_CORE_DISCOVERY_TYPE_NONE`, and, once the service is ready for device operation, a subsequent single-device query for that device SHALL succeed

#### Scenario: Reconfiguration fails
- **WHEN** a reconfiguration of an existing device fails
- **THEN** the service SHALL emit `device-configuration-failed` with session discovery type `B_CORE_DISCOVERY_TYPE_NONE`, and the device SHALL no longer be reported as reconfiguring

#### Scenario: Device record gone when the reconfiguration finishes
- **WHEN** a reconfiguration finishes and the device no longer has a stored record
- **THEN** the service SHALL NOT emit a configuration event for it

#### Scenario: Device removed while its reconfiguration finishes
- **WHEN** a device is removed while its reconfiguration's completion event is being sent
- **THEN** the removal SHALL wait (for up to 60 seconds) until that event has been sent before removing the device and emitting `device-removed`, unless the removal is made from that event's own handler

## MODIFIED Requirements

### Requirement: Device CRUD operations
The system SHALL provide functions to query, lookup, and remove devices: `get_devices()`, `get_device_by_id()`, `get_device_by_uri()`, `get_devices_by_device_class()`, `get_devices_by_subsystem()`, `remove_device()`. Single-device lookups (`get_device_by_id()`, `get_device_by_uri()`) SHALL accept a `GError **err` out-parameter and are gated as described in "Readiness and reconfiguration error domain". List getters (`get_devices()`, `get_devices_by_device_class()`, `get_devices_by_subsystem()`) SHALL NOT be gated and SHALL include devices that are reconfiguring: a reconfiguring device still exists, and an omitted or empty result would be indistinguishable from removal.

#### Scenario: Get all devices
- **WHEN** `b_core_client_get_devices()` is called, whether or not the service is ready and whether or not any device is reconfiguring
- **THEN** the service SHALL return a `GList*` of all known `BCoreDevice` objects, including any device currently undergoing reconfiguration

#### Scenario: Get device by UUID
- **WHEN** `b_core_client_get_device_by_id()` is called with a valid UUID while the service is ready and the device is not reconfiguring
- **THEN** the service SHALL return the matching `BCoreDevice` or `NULL` if not found

#### Scenario: Get device by UUID while reconfiguring
- **WHEN** `b_core_client_get_device_by_id()` is called for a device with a pending or in-progress reconfiguration
- **THEN** the service SHALL return `NULL` and set a `GError` with code `B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING`

#### Scenario: Remove device
- **WHEN** `b_core_client_remove_device()` is called with a valid UUID
- **THEN** the service SHALL remove the device from the database, notify the driver, emit a `device-removed` signal, and return `TRUE`

### Requirement: Endpoint access
The system SHALL provide endpoint query functions: `get_endpoints_by_profile()`, `get_endpoint_by_id()`, `get_endpoint_by_uri()`, `remove_endpoint_by_id()`. Single-endpoint lookups (`get_endpoint_by_id()`, `get_endpoint_by_uri()`) SHALL accept a `GError **err` and are gated as described in "Readiness and reconfiguration error domain". `get_endpoints_by_profile()` SHALL NOT be gated.

#### Scenario: Get endpoints by profile
- **WHEN** `b_core_client_get_endpoints_by_profile()` is called with a profile name
- **THEN** the service SHALL return a `GList*` of all `BCoreEndpoint` objects matching that profile across all devices, including endpoints of devices undergoing reconfiguration

#### Scenario: Get endpoint while reconfiguring
- **WHEN** `b_core_client_get_endpoint_by_id()` or `b_core_client_get_endpoint_by_uri()` is called for a device with a pending or in-progress reconfiguration
- **THEN** the service SHALL return `NULL` and set a `GError` with code `B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING`

### Requirement: Resource operations
The system SHALL support reading, writing, and executing resources by URI. `b_core_client_read_resource()` SHALL return the resource's stored value, except that a readable resource whose caching policy is never-cache SHALL be read from the device through its driver. `b_core_client_write_resource()` SHALL send a write command to the device. `b_core_client_execute_resource()` SHALL invoke an executable resource. `b_core_client_get_resource_by_uri()` SHALL look up a single resource. `read_resource()` and `get_resource_by_uri()` SHALL accept a `GError **err` and are gated as described in "Readiness and reconfiguration error domain". `write_resource()` and `execute_resource()` SHALL NOT be gated and keep their existing signatures: they act on the live device, and resource values written while a reconfiguration is pending are re-read when the replacement device is built, unless the write lands after the replacement has read its initial values; such a late write reaches the device but its stored value is replaced by the earlier reading until the device next reports.

#### Scenario: Read resource
- **WHEN** `b_core_client_read_resource()` is called with a valid resource URI while the service is ready and the owning device is not reconfiguring
- **THEN** the service SHALL return the resource's value as a string (read from the device through its driver if the resource is never-cache), or set a `BCoreReadResourceError` if the resource is not readable

#### Scenario: Write resource
- **WHEN** `b_core_client_write_resource()` is called with a valid URI and string value, including while the owning device is reconfiguring
- **THEN** the service SHALL send the value to the device driver, which translates it to the appropriate protocol command

#### Scenario: Execute resource
- **WHEN** `b_core_client_execute_resource()` is called with a valid URI, including while the owning device is reconfiguring
- **THEN** the service SHALL invoke the executable resource on the device and optionally return a response string

#### Scenario: Resource read while reconfiguring
- **WHEN** `b_core_client_read_resource()` or `b_core_client_get_resource_by_uri()` is called for a device with a pending or in-progress reconfiguration
- **THEN** the service SHALL fail, set a `GError` with code `B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING`, and return `NULL`

#### Scenario: Query resources by URI pattern
- **WHEN** `b_core_client_query_resources_by_uri()` is called with a URI pattern
- **THEN** the service SHALL return a `GList*` of all `BCoreResource` objects matching the pattern, including resources of devices undergoing reconfiguration

### Requirement: Metadata operations
The system SHALL support reading and writing metadata by URI. Metadata stores non-functional device information (migration data, discovered details, security zone config). `b_core_client_read_metadata()` and `b_core_client_write_metadata()` are gated as described in "Readiness and reconfiguration error domain"; `write_metadata()` gains a `GError **err`. `write_metadata()` is gated because a reconfiguration carries metadata over from a snapshot taken when it starts, so a metadata write made while it is pending would be silently lost. `b_core_client_get_metadata_by_uri()` SHALL NOT be gated.

#### Scenario: Read metadata
- **WHEN** `b_core_client_read_metadata()` is called with a valid metadata URI while the service is ready and the owning device is not reconfiguring
- **THEN** the service SHALL return the metadata value as a string, or set a `BCoreReadMetadataError` if not accessible

#### Scenario: Metadata operation while reconfiguring
- **WHEN** `b_core_client_read_metadata()` or `b_core_client_write_metadata()` is called for a device with a pending or in-progress reconfiguration
- **THEN** the service SHALL fail and set a `GError` with code `B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING`

#### Scenario: Get metadata by URI pattern
- **WHEN** `b_core_client_get_metadata_by_uri()` is called with a URI pattern
- **THEN** the service SHALL return a `GList*` of all matching `BCoreMetadata` objects, including metadata of devices undergoing reconfiguration
