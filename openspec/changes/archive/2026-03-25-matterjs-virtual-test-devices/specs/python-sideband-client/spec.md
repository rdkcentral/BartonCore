## ADDED Requirements

### Requirement: Sideband client class
A Python `SidebandClient` class SHALL provide a simple interface for sending side-band operations to matter.js virtual devices over HTTP. The client SHALL be initialized with the device's side-band host and port.

#### Scenario: Client sends an operation
- **WHEN** `client.send("lock")` is called
- **THEN** the client SHALL send a POST request to `http://<host>:<port>/sideband` with body `{ "operation": "lock" }`
- **AND** the response SHALL be parsed as JSON and returned

#### Scenario: Client sends an operation with parameters
- **WHEN** `client.send("setPin", {"pin": "1234", "userId": 1})` is called
- **THEN** the client SHALL send `{ "operation": "setPin", "params": {"pin": "1234", "userId": 1} }`
- **AND** the response SHALL be parsed as JSON and returned

### Requirement: Convenience methods
The `SidebandClient` SHALL provide a `get_state()` convenience method that calls the `getState` side-band operation and returns the parsed result.

#### Scenario: Get state returns device state
- **WHEN** `client.get_state()` is called
- **THEN** it SHALL send a `getState` operation and return the `result` field from the response

### Requirement: Error handling
The `SidebandClient` SHALL raise a clear exception when a side-band operation fails or the device is unreachable.

#### Scenario: Device unreachable
- **WHEN** a side-band operation is sent and the device process is not running
- **THEN** the client SHALL raise a `ConnectionError` (or subclass) with a descriptive message

#### Scenario: Operation returns failure
- **WHEN** a side-band operation returns `{ "success": false, "error": "..." }`
- **THEN** the client SHALL raise a `SidebandOperationError` with the error message

### Requirement: Timeout support
The `SidebandClient` SHALL support configurable timeouts for side-band operations, defaulting to a reasonable value (e.g., 5 seconds).

#### Scenario: Operation times out
- **WHEN** a side-band operation does not receive a response within the timeout period
- **THEN** the client SHALL raise a timeout exception
