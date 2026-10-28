## ADDED Requirements

### Requirement: BCoreClient lifecycle management
The system SHALL provide a `BCoreClient` GObject type that manages the complete device service lifecycle. A client SHALL be created with `b_core_client_new()` accepting a `BCoreInitializeParamsContainer`, started with `b_core_client_start()`, and stopped with `b_core_client_stop()`.

#### Scenario: Client creation and start
- **WHEN** a caller creates a `BCoreClient` with valid init params and calls `b_core_client_start()`
- **THEN** the device service SHALL initialize the database, start event handlers, initialize device drivers and subsystems, and return `TRUE`

#### Scenario: Client stop
- **WHEN** `b_core_client_stop()` is called on a running client
- **THEN** the service SHALL shut down subsystems and drivers in reverse startup order and release resources

#### Scenario: Dependencies ready notification
- **WHEN** `b_core_client_dependencies_ready()` is called after start
- **THEN** the service SHALL proceed with operations that depend on external services being available

### Requirement: Device discovery
The system SHALL support device discovery via `b_core_client_discover_start()` with device class filters, discovery filters (URI/value regex patterns), and a timeout in seconds. Discovery SHALL be stoppable via `b_core_client_discover_stop()`.

#### Scenario: Start discovery with filters
- **WHEN** `b_core_client_discover_start()` is called with device classes, filters, and a timeout
- **THEN** the service SHALL emit a `discovery-started` signal and begin scanning for devices matching the criteria

#### Scenario: Device found during discovery
- **WHEN** a device is discovered that matches the requested device classes and filters
- **THEN** the service SHALL emit a `device-discovered` signal with `BCoreDeviceFoundDetails` containing uuid, manufacturer, model, hardware version, firmware version, device class, endpoint profiles, and metadata (GHashTable of string→string)

#### Scenario: Stop discovery
- **WHEN** `b_core_client_discover_stop()` is called (optionally with a `GList *deviceClasses` to stop only specific device class discoveries)
- **THEN** the service SHALL stop scanning, emit a `discovery-stopped` signal, and return `TRUE`

### Requirement: Device recovery
The system SHALL support device recovery via `b_core_client_recover_start()` to re-discover devices that were previously paired but lost. Recovery SHALL use the same filter mechanism as discovery.

#### Scenario: Recover lost device
- **WHEN** `b_core_client_recover_start()` is called and a previously-paired device is found
- **THEN** the service SHALL emit a `device-recovered` signal and restore the device's state

### Requirement: Matter commissioning
The system SHALL support Matter device commissioning via `b_core_client_commission_device()` accepting a setup payload string and timeout. Pre-commissioned devices SHALL be addable via `b_core_client_add_matter_device()` with a node ID.

#### Scenario: Commission Matter device with setup code
- **WHEN** `b_core_client_commission_device()` is called with a valid Matter setup payload
- **THEN** the service SHALL commission the device, emit configuration events (started, completed), and emit a `device-added` signal

#### Scenario: Add pre-commissioned Matter device
- **WHEN** `b_core_client_add_matter_device()` is called with a valid node ID
- **THEN** the service SHALL add the device without re-commissioning and emit a `device-added` signal

#### Scenario: Open commissioning window
- **WHEN** `b_core_client_open_commissioning_window()` is called with a device ID and timeout
- **THEN** the service SHALL return a `BCoreCommissioningInfo` containing `manual-code` and `qr-code` strings

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

### Requirement: System properties
The system SHALL provide key-value system properties via `b_core_client_get_system_property()` and `b_core_client_set_system_property()`.

#### Scenario: Get system property
- **WHEN** `b_core_client_get_system_property()` is called with a known key
- **THEN** the service SHALL return the property value as a string

### Requirement: Zigbee channel management
The system SHALL support Zigbee channel operations: `change_zigbee_channel()` (with dry-run mode), `zigbee_energy_scan()`, and `zigbee_test()`.

#### Scenario: Energy scan
- **WHEN** `b_core_client_zigbee_energy_scan()` is called with channel list, duration, and scan count
- **THEN** the service SHALL return a `GList*` of `BCoreZigbeeEnergyScanResult` objects with channel, max-rssi, min-rssi, avg-rssi, and score

#### Scenario: Dry-run channel change
- **WHEN** `b_core_client_change_zigbee_channel()` is called with `dryRun=TRUE`
- **THEN** the service SHALL calculate the best channel without performing the change and return the channel number

### Requirement: Thread operations
The system SHALL support Thread network operations: `thread_set_nat64_enabled()` and `thread_activate_ephemeral_key_mode()`.

#### Scenario: Activate ephemeral key mode
- **WHEN** `b_core_client_thread_activate_ephemeral_key_mode()` is called
- **THEN** the service SHALL activate ephemeral key mode on the Thread border router and return the ephemeral key string

### Requirement: Configuration restore
The system SHALL support restoring device configuration from a backup directory via `b_core_client_config_restore()`.

#### Scenario: Restore from backup
- **WHEN** `b_core_client_config_restore()` is called with a valid temporary restore directory
- **THEN** the service SHALL restore all device data, system properties, and subsystem configurations from the backup

### Requirement: BCoreDevice GObject type
The system SHALL provide a `BCoreDevice` GObject type with properties: `uuid` (string), `device-class` (string), `device-class-version` (uint), `uri` (string), `managing-device-driver` (string), `endpoints` (GList of BCoreEndpoint), `resources` (GList of BCoreResource), `metadata` (GList of BCoreMetadata).

#### Scenario: Access device properties
- **WHEN** a `BCoreDevice` object is obtained from the client
- **THEN** all properties SHALL be accessible via GObject property getters and contain the device's current state

### Requirement: BCoreEndpoint GObject type
The system SHALL provide a `BCoreEndpoint` GObject type with properties: `id` (string), `uri` (string), `profile` (string), `profile-version` (uint), `device-uuid` (string), `enabled` (bool), `resources` (GList), `metadata` (GList).

#### Scenario: Access endpoint properties
- **WHEN** a `BCoreEndpoint` object is obtained from a device or query
- **THEN** all properties SHALL be accessible and reflect the endpoint's current configuration

### Requirement: BCoreResource GObject type
The system SHALL provide a `BCoreResource` GObject type with properties: `id` (string), `uri` (string), `endpoint-id` (string), `device-uuid` (string), `value` (string), `type` (string), `mode` (uint bitmask), `caching-policy` (BCoreResourceCachingPolicy enum: NEVER=1, ALWAYS=2), `date-of-last-sync-millis` (uint64). Note: the internal C enum uses NEVER=0/ALWAYS=1, but the GObject public API enum starts at 1.

#### Scenario: Access resource value and mode
- **WHEN** a `BCoreResource` is obtained
- **THEN** the `value` property SHALL contain the string-serialized current value and `mode` SHALL reflect the 8-bit bitmask of READABLE(1)/WRITEABLE(2)/EXECUTABLE(4)/DYNAMIC_CAPABLE(8)/DYNAMIC(16)/EMIT_EVENTS(32)/LAZY_SAVE_NEXT(64)/SENSITIVE(128)

### Requirement: BCoreInitializeParamsContainer
The system SHALL provide a `BCoreInitializeParamsContainer` GObject type with configurable properties for: `storage-dir`, `firmware-file-dir`, `matter-storage-dir`, `matter-attestation-trust-store-dir`, `sbmd-dir`, `account-id`, `token-provider` (BCoreTokenProvider), `network-credentials-provider` (BCoreNetworkCredentialsProvider), `property-provider` (BCorePropertyProvider).

#### Scenario: Configure init params
- **WHEN** a `BCoreInitializeParamsContainer` is created and properties are set
- **THEN** all configured values SHALL be passed to the device service during initialization

### Requirement: Provider interfaces
The system SHALL define three GObject interfaces for dependency injection: `BCoreTokenProvider` (get_token by token type, with `BCoreTokenProviderError` enum: GENERIC, ARG), `BCoreNetworkCredentialsProvider` (get_wifi_network_credentials returning `BCoreWifiNetworkCredentials`, with `BCoreNetworkCredentialsProviderError` enum: GENERIC, ARG), and `BCorePropertyProvider` (get/set properties with typed convenience wrappers, `get_all_properties()`, `has_property()`, property-changed signal, and `BCorePropertyProviderError` enum: GENERIC, ARG, NOT_FOUND, INTERNAL).

#### Scenario: Token provider
- **WHEN** `b_core_token_provider_get_token()` is called with `B_CORE_TOKEN_TYPE_XPKI_MATTER`
- **THEN** the implementation SHALL return an authentication token string or set a `BCoreTokenProviderError`

#### Scenario: Property provider typed access
- **WHEN** `b_core_property_provider_get_property_as_uint16()` is called
- **THEN** the implementation SHALL return the property value converted to `guint16`

#### Scenario: Property change notification
- **WHEN** a property value changes in the property provider
- **THEN** the provider SHALL emit a `property-changed` signal with the property name, old value, and new value

### Requirement: Event hierarchy
The system SHALL provide a hierarchy of GObject event types rooted at `BCoreEvent` (with `id` and `timestamp` properties). Client signals SHALL emit the corresponding typed event object. The BCoreClient defines 25+ signal macros covering status changes, discovery/recovery lifecycle, device configuration, device add/remove, resource/metadata updates, Zigbee events (channel change, interference, PAN ID attack), storage changes, and database failures.

#### Scenario: Status changed event
- **WHEN** the service status changes
- **THEN** a `BCoreStatusEvent` SHALL be emitted with `status` (BCoreStatus) and `reason` (BCoreStatusChangedReason enum: INVALID, READY_FOR_PAIRING, READY_FOR_DEVICE_OPERATION, SUBSYSTEM_STATUS)

#### Scenario: Resource updated event
- **WHEN** a resource value changes on a device
- **THEN** a `BCoreResourceUpdatedEvent` SHALL be emitted containing the updated `BCoreResource` and optional metadata JSON string

### Requirement: GObject Introspection compatibility
The system SHALL support GObject Introspection (GIR) generation under the `BCore` namespace when `BCORE_GEN_GIR=ON`. All public GObject types, signals, properties, and enums SHALL be introspectable.

#### Scenario: Python bindings via GIR
- **WHEN** GIR is generated and a Python client imports `gi.repository.BCore`
- **THEN** all public types, functions, signals, and properties SHALL be accessible from Python

### Requirement: Additional public GObject types
The system SHALL provide the following additional GObject types:
- `BCoreMetadata` with properties: `id`, `uri`, `endpoint-id`, `device-uuid`, `value`
- `BCoreStatus` with properties: `device-classes`, `discovery-type`, `searching-device-classes`, `discovery-seconds`, `ready-for-operation`, `ready-for-pairing`, `subsystems`, `json`
- `BCoreDiscoveryFilter` with properties: `uri` (regex pattern), `value` (regex pattern)
- `BCoreDiscoveryType` enum: NONE, DISCOVERY, RECOVERY
- `BCoreWifiNetworkCredentials` with properties: `ssid`, `psk`
- `BCoreDefaultPropertyProvider` (default implementation of `BCorePropertyProvider`)

#### Scenario: Query service status
- **WHEN** `b_core_client_get_status()` is called
- **THEN** the service SHALL return a `BCoreStatus` object reflecting the current operational state

### Requirement: Additional client functions
The system SHALL provide additional client functions:
- `b_core_client_recover_stop()` — stop device recovery
- `b_core_client_get_status()` — get current service status as `BCoreStatus`
- `b_core_client_get_discovery_type()` — get current discovery type
- `b_core_client_get_resource_by_uri()` — get a single resource by URI
- `b_core_client_write_metadata()` — write metadata value by URI
- `b_core_change_resource_mode()` — change a resource's mode bitmask
- `b_core_process_device_descriptors()` — process device descriptor list
- `b_core_client_set_account_id()` — set the account ID

### Requirement: Additional event types
The system SHALL provide intermediate event base types:
- `BCoreDiscoverySessionInfoEvent` with `session-discovery-type` property
- `BCoreDeviceConfigurationEvent` with `uuid` and `device-class` properties

### Requirement: Well-known property constants
The system SHALL define string constants for all Matter and device properties: vendor name/ID, product name/ID, hardware version, serial number, manufacturing date, setup discriminator, setup passcode, SPAKE2+ parameters, 802.15.4 EUI64, Matter part number, Matter product URL, Matter product label, Matter hardware version string, and default Thread network name. These SHALL be defined as C macros with the `B_CORE_BARTON_` prefix.

#### Scenario: Access Matter property constant
- **WHEN** a client uses `B_CORE_BARTON_MATTER_VENDOR_ID`
- **THEN** it SHALL resolve to the string `"barton.matter.vid"`
