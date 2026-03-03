## ADDED Requirements

### Requirement: URI-based resource addressing
The system SHALL address all device data using a hierarchical URI scheme: `/<deviceId>` for devices, `/<deviceId>/ep/<N>` for endpoints, `/<deviceId>/ep/<N>/r/<resourceName>` for endpoint resources, `/<deviceId>/r/<resourceName>` for device-level resources, and `/<deviceId>/ep/<N>/m/<metadataName>` or `/<deviceId>/m/<metadataName>` for metadata.

#### Scenario: Device-level resource URI
- **WHEN** a device has a device-level resource named `manufacturer`
- **THEN** its URI SHALL be `/<deviceId>/r/manufacturer`

#### Scenario: Endpoint-level resource URI
- **WHEN** endpoint `1` of a device has a resource named `isOn`
- **THEN** its URI SHALL be `/<deviceId>/ep/1/r/isOn`

#### Scenario: Metadata URI
- **WHEN** endpoint `1` of a device has metadata named `zigbee_epid`
- **THEN** its URI SHALL be `/<deviceId>/ep/1/m/zigbee_epid`

### Requirement: String-serialized resource values
All resource values SHALL be stored and transmitted as strings, regardless of the underlying data type. Boolean values SHALL be `"true"` or `"false"`. Numeric values SHALL be decimal string representations. Complex values (JSON objects) SHALL be serialized as JSON strings.

#### Scenario: Boolean resource value
- **WHEN** a boolean resource `isOn` is set to true
- **THEN** its value SHALL be the string `"true"`

#### Scenario: Temperature resource value
- **WHEN** a temperature resource has a value of 1900 (hundredths of degrees)
- **THEN** its value SHALL be the string `"1900"`

### Requirement: Resource type system
Each resource SHALL have a `type` property indicating its semantic data type. Standard types SHALL include: `com.icontrol.boolean`, `com.icontrol.string`, `com.icontrol.label`, `com.icontrol.dateTime`, `com.icontrol.version`, `com.icontrol.temperature`, `com.icontrol.firmwareVersionStatus`, `com.icontrol.trouble`, `com.icontrol.sensorType`, `com.icontrol.resetToFactoryOperation`, `rssi`, `lqi`, `battery.voltage`.

#### Scenario: Resource with type
- **WHEN** a resource is created for a battery voltage reading
- **THEN** its `type` property SHALL be `"battery.voltage"`

### Requirement: Resource mode bitmask
Each resource SHALL have a `mode` property as an 8-bit bitmask defining its capabilities. Bit values: READABLE=1, WRITEABLE=2, EXECUTABLE=4, DYNAMIC_CAPABLE=8, DYNAMIC=16, EMIT_EVENTS=32, LAZY_SAVE_NEXT=64, SENSITIVE=128. `DYNAMIC_CAPABLE` indicates the resource can become dynamic; `DYNAMIC` indicates it is currently participating in dynamic behavior.

#### Scenario: Read-only resource
- **WHEN** a resource is read-only
- **THEN** its mode SHALL have bit 0 set (mode & 1 == 1) and bit 1 unset (mode & 2 == 0)

#### Scenario: Readable, writable, emitting resource
- **WHEN** a resource is readable, writable, and emits events
- **THEN** its mode SHALL be at least 35 (READABLE=1 + WRITEABLE=2 + EMIT_EVENTS=32)

#### Scenario: Executable resource
- **WHEN** a resource is execute-only (e.g., resetToFactoryDefaults)
- **THEN** its mode SHALL have bit 2 set (mode & 4 == 4)

### Requirement: Resource caching policy
Each resource SHALL have a `caching-policy` of either `NEVER` or `ALWAYS`. The internal C enum uses CACHING_POLICY_NEVER=0/CACHING_POLICY_ALWAYS=1; the GObject public API uses B_CORE_RESOURCE_CACHING_POLICY_NEVER=1/B_CORE_RESOURCE_CACHING_POLICY_ALWAYS=2. Resources with `ALWAYS` caching SHALL persist their values in the database. Resources with `NEVER` caching SHALL always read from the device.

#### Scenario: Cached resource
- **WHEN** a resource has `caching-policy` of `ALWAYS`
- **THEN** the database SHALL store and return the cached value without contacting the device for `get_resource_by_uri()` calls

#### Scenario: Uncached resource
- **WHEN** a resource has `caching-policy` of `NEVER`
- **THEN** `read_resource()` SHALL always contact the physical device to obtain the current value

### Requirement: Lazy save for high-frequency resources
Resources with the `LAZY_SAVE_NEXT` mode bit (64) set SHALL defer database persistence. The value SHALL be saved on the next database save cycle rather than immediately, reducing I/O for frequently-updated resources like `dateLastContacted`.

#### Scenario: Lazy save resource update
- **WHEN** a resource with `LAZY_SAVE_NEXT` mode is updated
- **THEN** the new value SHALL be held in memory and persisted on the next save cycle, not immediately

### Requirement: Device class contract
Each device SHALL belong to a Device Class (e.g., `light`, `sensor`, `doorLock`, `thermostat`, `windowCovering`, `presence`, `lightController`, `airQualitySensor`) that defines the type of device and acts as a contract for its endpoint composition.

#### Scenario: Light device class
- **WHEN** a device has device class `light`
- **THEN** it SHALL have at least one endpoint with profile `light` containing resources for light control (e.g., `isOn`)

#### Scenario: Sensor device class
- **WHEN** a device has device class `sensor`
- **THEN** it SHALL have at least one endpoint with profile `sensor` containing resources `faulted`, `tampered`, `bypassed`, `type`, `qualified`, and `label`

### Requirement: Endpoint profile contract
Each endpoint SHALL belong to an Endpoint Profile that defines a contract for the endpoint's resource composition. The profile determines which resources are mandatory and optional for that endpoint.

#### Scenario: Sensor endpoint profile
- **WHEN** an endpoint has profile `sensor`
- **THEN** it SHALL contain at minimum resources: `faulted` (boolean), `type` (sensorType), `label` (label)

### Requirement: Device class versioning
Each device class SHALL have a version number. This version SHALL be incremented when the resource contract for that device class changes (new mandatory resources, changed semantics).

#### Scenario: Device class version tracking
- **WHEN** a device is persisted
- **THEN** the database record SHALL include `deviceClassVersion` to track which version of the device class contract was used

### Requirement: Common device-level resources
All devices, regardless of device class, SHALL support a common set of device-level resources: `manufacturer` (string), `model` (string), `hardwareVersion` (version), `firmwareVersion` (version), `dateAdded` (dateTime), `dateLastContacted` (dateTime), `communicationFailure` (trouble).

#### Scenario: New device added
- **WHEN** a device is added to the system
- **THEN** it SHALL have device-level resources for manufacturer, model, hardware version, firmware version, date added, and communication failure status

### Requirement: Protocol-agnostic resource access
Clients SHALL be able to read, write, and execute resources without knowledge of the underlying protocol (Zigbee, Matter, IP). The resource URI and string value SHALL be the only interface needed.

#### Scenario: Toggle light via resource write
- **WHEN** a client writes `"true"` to `/<deviceId>/ep/1/r/isOn`
- **THEN** the system SHALL translate this to the appropriate protocol command (Zigbee OnOff cluster command or Matter OnOff cluster invoke) transparently
