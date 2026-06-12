## REMOVED Requirements

### Requirement: matter.js engine compatibility
**Reason**: The matter.js integration (`BCORE_MATTER_USE_MATTERJS`) is being removed entirely. The `SbmdUtils.Tlv` built-in helpers now cover all TLV encoding/decoding needs without the ~1 MB RAM overhead and Node.js build dependency.
**Migration**: Specs using `scriptType: "JavaScript+matterjs"` have been rewritten to use `scriptType: "JavaScript"` with `SbmdUtils.Tlv` and `SbmdUtils.Response` helpers.

### Requirement: matter.js integration
**Reason**: The `MatterClusters` global object, `MatterJsClusterLoader`, and `JavaScript+matterjs` scriptType are removed. All TLV encoding/decoding is handled by the built-in `SbmdUtils` library.
**Migration**: Replace `MatterClusters.TlvUInt16.decode(bytes)` with `SbmdUtils.Tlv.decode(base64)`. Replace `MatterClusters.TlvUInt16.encode(val)` with `SbmdUtils.Tlv.encode(val, 'uint16')`. Replace `TlvObject`/`TlvField` patterns with `SbmdUtils.Tlv.encodeStruct(obj, schema)` / `SbmdUtils.Tlv.decodeStruct(base64)`. Replace explicit return objects with `SbmdUtils.Response.write()` / `SbmdUtils.Response.invoke()`.

## MODIFIED Requirements

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

### Requirement: SBMD schema validation
SBMD spec files SHALL be validated against the JSON schema defined in `sbmd-spec-schema.json` during the build process. The schema SHALL enforce required fields, valid `matterType` enumerations, and structural constraints. The `scriptType` field SHALL only accept the value `"JavaScript"`.

#### Scenario: Schema validation at build time
- **WHEN** the project is built with SBMD specs present
- **THEN** each `.sbmd` file SHALL be validated against the JSON schema, and build SHALL fail if any spec is invalid

#### Scenario: Invalid scriptType rejected
- **WHEN** an SBMD spec contains `scriptType: "JavaScript+matterjs"`
- **THEN** schema validation SHALL reject the spec

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
