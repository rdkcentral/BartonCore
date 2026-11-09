## MODIFIED Requirements

### Requirement: Registration object extraction
The runtime SHALL extract the following from the `SbmdDriver({...})` registration object by walking JSValue properties directly (no JSON serialization): `schemaVersion`, `driverVersion`, `name`, `constants`, `aliases`, `barton`, `matter`, `reporting`, `resources`, `endpoints`, `attributeHandlers`, `eventHandlers`, `commandHandlers`, `onTransactionFailure`. Handler function JSValues, including the `onTransactionFailure` handler and its declared supplements, SHALL be stored for later invocation.

#### Scenario: Metadata extracted to C++ structs
- **WHEN** a registration object contains `barton: { deviceClass: "light", deviceClassVersion: 0 }`
- **THEN** the runtime extracts `deviceClass = "light"` and `deviceClassVersion = 0` into C++ data structures

#### Scenario: Handler function references preserved
- **WHEN** a resource declares `write: writeIsOn` and `writeIsOn` is a function defined in the file
- **THEN** the runtime stores the JSValue reference to `writeIsOn` for later invocation

#### Scenario: Transaction failure handler extracted
- **WHEN** a registration object contains `onTransactionFailure: { supplements: { transientData: ["sessions"] }, handler: handleFailure }`
- **THEN** the runtime stores the JSValue reference to `handleFailure` and its supplement declaration as the driver's transaction failure handler
