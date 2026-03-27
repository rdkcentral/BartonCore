## ADDED Requirements

### Requirement: Door lock device type
The matter.js door lock virtual device SHALL present itself as a Matter Door Lock device type (Device Type ID `0x000A`) with a DoorLock cluster (Cluster ID `0x0101`) on endpoint 1.

#### Scenario: Device advertises as door lock
- **WHEN** the door lock virtual device is started and commissioned
- **THEN** it SHALL be discoverable as a Matter Door Lock device type
- **AND** the DoorLock cluster SHALL be available on endpoint 1

### Requirement: Matter lock and unlock commands
The door lock virtual device SHALL respond to standard Matter DoorLock cluster `LockDoor` and `UnlockDoor` commands, updating its internal lock state accordingly.

#### Scenario: Lock door via Matter command
- **WHEN** a Matter `LockDoor` command is sent to the device
- **THEN** the device's lock state SHALL change to locked
- **AND** the device SHALL report the updated `LockState` attribute as `Locked` (value 1)

#### Scenario: Unlock door via Matter command
- **WHEN** a Matter `UnlockDoor` command is sent to the device
- **THEN** the device's lock state SHALL change to unlocked
- **AND** the device SHALL report the updated `LockState` attribute as `Unlocked` (value 2)

### Requirement: Side-band lock operation
The door lock SHALL register a side-band operation `lock` that simulates a user manually locking the device (e.g., turning the thumb-turn). This operation SHALL update the Matter cluster state, triggering standard Matter notifications.

#### Scenario: Side-band lock triggers Matter state change
- **WHEN** a side-band `lock` operation is sent to the device
- **THEN** the device's `LockState` attribute SHALL change to `Locked`
- **AND** connected Matter controllers SHALL receive a state change notification
- **AND** the side-band response SHALL be `{ "success": true, "result": { "lockState": "locked" } }`

### Requirement: Side-band unlock operation
The door lock SHALL register a side-band operation `unlock` that simulates a user manually unlocking the device. This operation SHALL update the Matter cluster state, triggering standard Matter notifications.

#### Scenario: Side-band unlock triggers Matter state change
- **WHEN** a side-band `unlock` operation is sent to the device
- **THEN** the device's `LockState` attribute SHALL change to `Unlocked`
- **AND** connected Matter controllers SHALL receive a state change notification
- **AND** the side-band response SHALL be `{ "success": true, "result": { "lockState": "unlocked" } }`

### Requirement: Side-band getState operation
The door lock SHALL register a side-band operation `getState` that returns the current state of the device, including lock state and any configured users/pin codes.

#### Scenario: Get state of locked device
- **WHEN** a side-band `getState` operation is sent and the device is locked
- **THEN** the response SHALL include `{ "success": true, "result": { "lockState": "locked", "users": [...], "pinCodes": [...] } }`

#### Scenario: Get state of unlocked device
- **WHEN** a side-band `getState` operation is sent and the device is unlocked
- **THEN** the response SHALL include `{ "success": true, "result": { "lockState": "unlocked", "users": [...], "pinCodes": [...] } }`

### Requirement: Initial lock state
The door lock virtual device SHALL start in the locked state by default.

#### Scenario: Device starts locked
- **WHEN** the door lock virtual device is started
- **THEN** the initial `LockState` attribute SHALL be `Locked`
- **AND** a side-band `getState` SHALL return `lockState: "locked"`

### Requirement: User and PIN code management via Matter
The door lock SHALL support basic user and PIN code management through the Matter DoorLock cluster, enabling tests to create users and set credentials.

#### Scenario: Set user credential via Matter
- **WHEN** a `SetCredential` command is sent via Matter with a PIN code
- **THEN** the credential SHALL be stored on the device
- **AND** a subsequent `getState` side-band operation SHALL include the credential in the `pinCodes` array
