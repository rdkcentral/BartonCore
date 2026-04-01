## ADDED Requirements

### Requirement: SBMD spec file format
The system SHALL support declarative device driver specifications in YAML format with the `.sbmd` file extension. Each spec SHALL define: `schemaVersion` (string, e.g., `"1.0"`), `driverVersion` (string, e.g., `"1.0"`), `name` (string), `bartonMeta` (device class mapping), `matterMeta` (Matter device type matching), optional `reporting` (subscription intervals), and `endpoints` (resource definitions with mappers).

#### Scenario: Valid SBMD spec
- **WHEN** an `.sbmd` file contains all required top-level fields with valid values
- **THEN** the parser SHALL produce a valid `SbmdSpec` data structure

#### Scenario: Missing required field
- **WHEN** an `.sbmd` file is missing `bartonMeta` or `matterMeta`
- **THEN** the parser SHALL reject the file with an error

### Requirement: Barton metadata in SBMD
Each SBMD spec SHALL define a `bartonMeta` section containing `deviceClass` (string, e.g., `"light"`, `"doorLock"`, `"sensor"`) and `deviceClassVersion` (integer).

#### Scenario: Device class mapping
- **WHEN** an SBMD spec has `bartonMeta.deviceClass: "light"` and `bartonMeta.deviceClassVersion: 1`
- **THEN** the resulting driver SHALL claim devices matching the light device class with version 1

### Requirement: Matter metadata in SBMD
Each SBMD spec SHALL define a `matterMeta` section containing `revision` (integer, the Matter device revision) and `deviceTypes` (flat list of Matter device type IDs as hex or decimal values, e.g., `- 0x0100`). Optionally, `featureClusters` (list of cluster IDs whose FeatureMap attributes to read at init time).

#### Scenario: Multiple device type support
- **WHEN** an SBMD spec lists `deviceTypes` with IDs `0x0100` and `0x010a`
- **THEN** the resulting driver SHALL claim devices matching either Matter device type

#### Scenario: Feature cluster discovery
- **WHEN** an SBMD spec includes `featureClusters: [0x0006, 0x0008]`
- **THEN** the driver SHALL read the FeatureMap attribute from clusters 6 and 8 at device initialization and make the feature maps available to scripts

### Requirement: Reporting configuration
SBMD specs MAY define a `reporting` section with `minSecs` and `maxSecs` controlling Matter subscription intervals.

#### Scenario: Subscription intervals
- **WHEN** a spec defines `reporting.minSecs: 0` and `reporting.maxSecs: 900`
- **THEN** the driver SHALL configure Matter subscriptions with those min/max intervals

### Requirement: Endpoint definitions
Each SBMD spec SHALL define one or more `endpoints`, each with `id` (string), `profile` (string), `profileVersion` (integer), and `resources` (list of resource definitions).

#### Scenario: Single endpoint spec
- **WHEN** an SBMD spec defines one endpoint with id `"1"` and profile `"light"`
- **THEN** the driver SHALL create one Barton endpoint with that profile on the device

### Requirement: Resource definitions with mappers
Each resource in an SBMD endpoint SHALL have `id` (string), `type` (string), and a `mapper` object. Resources MAY also specify `modes` (array of mode strings: `"read"`, `"write"`, `"execute"`, `"dynamic"`, `"emitEvents"`, `"lazySaveNext"`, `"sensitive"`) — if omitted, a default set is used. Note: there is no `"dynamicCapable"` mode string because the core automatically sets the `DYNAMIC_CAPABLE` bit whenever `DYNAMIC` is set (see `deviceModelHelper.c`). Resources MAY be marked `optional: true`.

#### Scenario: Required resource
- **WHEN** a resource is defined without `optional: true`
- **THEN** the driver SHALL fail device configuration if the resource's mapper cannot be set up (e.g., cluster not present on device)

#### Scenario: Optional resource
- **WHEN** a resource is defined with `optional: true` and the required cluster is not present
- **THEN** the driver SHALL skip the resource and continue with remaining resources

### Requirement: Read mapper
A resource's mapper MAY contain a `read` section with an `attribute` (specifying `clusterId`, `attributeId`, `name`, and `type` — where `type` is the Matter data type such as `"bool"`, `"uint8"`, `"uint16"`, `"int16"`, `"string"`, `"enum8"`, `"bitmap8/16/32"`, `"struct"`, `"array"`) and a `script` (JavaScript string). Alternatively, a `command` may be specified instead of `attribute`. The script SHALL receive the attribute value as TLV base64 via `sbmdReadArgs.tlvBase64` along with additional context fields (`clusterId`, `attributeId`, `attributeName`, `attributeType`, `endpointId`, `deviceUuid`, `clusterFeatureMaps`) and return the Barton string value.

#### Scenario: Read boolean attribute
- **WHEN** a read mapper has `attribute.type: bool` and the Matter attribute value is `true`
- **THEN** the script SHALL receive the TLV-encoded boolean as base64 and return `"true"`

#### Scenario: Read integer attribute
- **WHEN** a read mapper has `attribute.type: uint8` and the Matter attribute value is `254`
- **THEN** the script SHALL receive the TLV-encoded uint8 as base64 and return the appropriate string representation

### Requirement: Write mapper
A resource's mapper MAY contain a `write` section with a `script` (JavaScript string). The script SHALL receive the Barton string value via `sbmdWriteArgs.input` (along with `resourceId`, `endpointId`, `deviceUuid`, `clusterFeatureMaps`) and return a JSON object describing the operation: `{write: {clusterId, attributeId, tlvBase64}}` for attribute writes, or `{invoke: {clusterId, commandId, tlvBase64}}` for command invocations. Optional `timedInvokeTimeoutMs` for timed commands.

#### Scenario: Write resource as command invoke
- **WHEN** a write mapper script receives value `"true"` for an OnOff resource
- **THEN** the script SHALL return `{invoke: {clusterId: 6, commandId: 1, tlvBase64: <null TLV>}}` to send the On command

#### Scenario: Write resource as attribute write
- **WHEN** a write mapper script receives a level value `"128"`
- **THEN** the script MAY return `{write: {clusterId: 8, attributeId: 0, tlvBase64: <uint8 TLV>}}` for a direct attribute write

#### Scenario: Timed invoke
- **WHEN** a write mapper returns `{invoke: {..., timedInvokeTimeoutMs: 10000}}`
- **THEN** the driver SHALL send the command as a Matter timed invoke with the specified timeout

### Requirement: Execute mapper
A resource's mapper MAY contain an `execute` section with a `script` and optional `scriptResponse`. The execute script SHALL receive arguments via `sbmdExecuteArgs` and return an invoke operation JSON. If `scriptResponse` is defined, it SHALL process the command response TLV.

#### Scenario: Execute resource with response
- **WHEN** an execute mapper with `scriptResponse` is invoked and the Matter command returns a response
- **THEN** the response TLV SHALL be passed to `scriptResponse` as base64, and the script's return value SHALL be the execute response string

### Requirement: Event mapper
A resource's mapper MAY contain an `event` section specifying a Matter cluster event (`clusterId`, `eventId`, `name`) and a `script`. When the event fires, the script SHALL receive the event TLV via `sbmdEventArgs.tlvBase64` and return the updated resource value.

#### Scenario: Matter event updates resource
- **WHEN** a Matter event fires for a cluster/event matching an event mapper
- **THEN** the script SHALL be invoked with the event TLV, and the returned value SHALL update the Barton resource

### Requirement: SBMD schema validation
SBMD spec files SHALL be validated against the JSON schema defined in `sbmd-spec-schema.json` during the build process. The schema SHALL enforce required fields, valid `matterType` enumerations, and structural constraints. The `scriptType` field SHALL only accept the value `"JavaScript"`.

#### Scenario: Schema validation at build time
- **WHEN** the project is built with SBMD specs present
- **THEN** each `.sbmd` file SHALL be validated against the JSON schema, and build SHALL fail if any spec is invalid

#### Scenario: Invalid scriptType rejected
- **WHEN** an SBMD spec contains `scriptType: "JavaScript+matterjs"`
- **THEN** schema validation SHALL reject the spec

### Requirement: SBMD parser pipeline
The system SHALL include an `SbmdParser` that reads `.sbmd` YAML files using yaml-cpp and produces `SbmdSpec` C++ data structures. The parser SHALL support both `ParseFile(path)` and `ParseString(yaml)` static methods.

#### Scenario: Parse SBMD from file
- **WHEN** `SbmdParser::ParseFile()` is called with a valid `.sbmd` file path
- **THEN** it SHALL return a `shared_ptr<SbmdSpec>` with all spec data populated

#### Scenario: Hex and decimal ID parsing
- **WHEN** an SBMD spec uses `0x0006` for a cluster ID
- **THEN** the parser SHALL correctly interpret it as decimal 6

### Requirement: SbmdFactory driver registration
The system SHALL include an `SbmdFactory` that scans the configured SBMD directory at startup, parses all `.sbmd` files, creates a `SpecBasedMatterDeviceDriver` for each, and registers them with `MatterDriverFactory`.

#### Scenario: SBMD directory scan
- **WHEN** the Matter subsystem initializes with an SBMD directory containing `.sbmd` files
- **THEN** `SbmdFactory` SHALL parse each file and register a corresponding driver

### Requirement: Configurable JavaScript engine
The system SHALL support a build-time configurable JavaScript engine for SBMD scripts via the `BCORE_MATTER_SBMD_JS_ENGINE` CMake option. Valid values SHALL be `"quickjs"` (standard QuickJS) and `"mquickjs"` (MicroQuickJS). The default SHALL be `"mquickjs"`. If no value or an invalid value is provided when `BCORE_MATTER` is ON, configuration SHALL fail with a fatal error.

All devices SHALL share a single runtime singleton context regardless of engine choice, with per-device isolation via `SbmdScriptImpl` instances (one per engine, both named `SbmdScriptImpl` for interface uniformity) that use IIFEs (Immediately Invoked Function Expressions) for scope isolation. Thread safety SHALL be ensured via two mutexes: a per-instance `scriptsMutex` for script collections and a shared runtime mutex for context access.

#### Scenario: Default engine selection
- **WHEN** `BCORE_MATTER_SBMD_JS_ENGINE` is not explicitly set
- **THEN** the default engine SHALL be `"mquickjs"`

#### Scenario: Invalid engine selection
- **WHEN** `BCORE_MATTER_SBMD_JS_ENGINE` is set to a value other than `"quickjs"` or `"mquickjs"`
- **THEN** CMake configuration SHALL fail with a `FATAL_ERROR`

#### Scenario: Thread-safe script execution
- **WHEN** multiple threads invoke script mappers for the same device concurrently
- **THEN** the script instance SHALL serialize access via its internal mutex

### Requirement: mquickjs memory configuration
When using the mquickjs engine, the system SHALL support configuring the pre-allocated memory buffer size via the `BCORE_MQUICKJS_MEMSIZE_BYTES` CMake integer option (default: 1048576 bytes = 1 MB). The mquickjs engine uses a fixed-size, non-growing memory buffer.

#### Scenario: Custom mquickjs memory size
- **WHEN** `BCORE_MQUICKJS_MEMSIZE_BYTES=4194304` is set
- **THEN** the mquickjs engine SHALL allocate a 4 MB memory buffer

### Requirement: SbmdUtils built-in library
The system SHALL provide a built-in JavaScript library `SbmdUtils` (loaded into every QuickJS context) with: `SbmdUtils.Tlv.decode(base64)` for Matter TLV decoding, `SbmdUtils.Tlv.decodeStruct(base64)` for struct TLV decoding, `SbmdUtils.Tlv.encode(value, type)` for TLV encoding, `SbmdUtils.Tlv.encodeStruct(obj, schema)` for struct encoding, `SbmdUtils.Tlv.emptyStruct()` for empty struct TLV, `SbmdUtils.Response.write(clusterId, attributeId, tlvBase64, options?)` for write operation construction, `SbmdUtils.Response.invoke(clusterId, commandId, tlvBase64, opts)` for invoke operation construction, `SbmdUtils.Base64` for base64 encode/decode, and `SbmdUtils.TLV_TYPE` with TLV type constants.

#### Scenario: Decode boolean TLV
- **WHEN** `SbmdUtils.Tlv.decode(base64)` is called with a TLV-encoded boolean `true`
- **THEN** it SHALL return JavaScript `true`

#### Scenario: Encode uint8 TLV
- **WHEN** `SbmdUtils.Tlv.encode(128, 'uint8')` is called
- **THEN** it SHALL return a base64 string containing the TLV-encoded uint8 value 128

#### Scenario: Construct invoke response
- **WHEN** `SbmdUtils.Response.invoke(6, 1, tlvBase64)` is called
- **THEN** it SHALL return `{invoke: {clusterId: 6, commandId: 1, tlvBase64: <value>}}`

### Requirement: Script context variables
SBMD scripts SHALL receive context via global JavaScript variables: `sbmdReadArgs` (with `tlvBase64`, `endpointId`, `deviceUuid`, `clusterFeatureMaps`, `clusterId`, `attributeId`, `attributeName`, `attributeType`), `sbmdWriteArgs` (with `input`, `resourceId`, `endpointId`, `deviceUuid`, `clusterFeatureMaps`), `sbmdExecuteArgs`, `sbmdEventArgs`, and `sbmdCommandResponseArgs`.

#### Scenario: Read script receives feature maps
- **WHEN** a read script is invoked on a device with FeatureMap data for cluster 6
- **THEN** `sbmdReadArgs.clusterFeatureMaps` SHALL contain an object mapping cluster IDs to their feature map values

### Requirement: Current SBMD spec catalog
The system SHALL ship with SBMD specs for: `light` (13 Matter device types, JavaScript), `door-lock` (device type 0x000a, JavaScript), `air-quality-sensor` (device type 0x002c, JavaScript), `occupancy-sensor` (device type 0x0107, JavaScript), `water-leak-detector` (device type 0x0043, JavaScript), `contact-sensor` (device type 0x0015, JavaScript).

All specs SHALL use `scriptType: "JavaScript"` and the `SbmdUtils` built-in library for TLV encoding/decoding.

#### Scenario: Light SBMD spec coverage
- **WHEN** a Matter device with device type 0x0100 (On/Off Light) is commissioned
- **THEN** the `light.sbmd` driver SHALL claim it and register resources including `isOn`, `level`, and conditional color resources

#### Scenario: Door lock SBMD spec
- **WHEN** a Matter device with device type 0x000a (Door Lock) is commissioned
- **THEN** the `door-lock.sbmd` driver SHALL claim it and register lock-related resources using `SbmdUtils.Tlv` for TLV encoding

#### Scenario: Air quality sensor SBMD spec
- **WHEN** a Matter device with device type 0x002c (Air Quality Sensor) is commissioned
- **THEN** the `air-quality-sensor.sbmd` driver SHALL claim it and register air quality, temperature, humidity, CO2, and PM2.5 resources

#### Scenario: All specs use standard JavaScript
- **WHEN** any SBMD spec is loaded
- **THEN** it SHALL use `scriptType: "JavaScript"` and SHALL NOT require `MatterClusters` or any matter.js bundle
