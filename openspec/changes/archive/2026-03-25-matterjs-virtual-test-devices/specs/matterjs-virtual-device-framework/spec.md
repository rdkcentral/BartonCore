## ADDED Requirements

### Requirement: Virtual device base class initialization
The matter.js virtual device base class (`VirtualDevice`) SHALL initialize a Matter `ServerNode` with configurable vendor ID, product ID, device name, passcode, discriminator, and port. The class SHALL handle all common Matter device setup so that subclasses only need to define their device type and side-band operations.

#### Scenario: Base class creates a Matter server node
- **WHEN** a `VirtualDevice` subclass is instantiated with device type, vendor ID, product ID, passcode, discriminator, and port parameters
- **THEN** a Matter `ServerNode` SHALL be created with those parameters and the device SHALL be ready to accept commissioning

#### Scenario: Default values for optional parameters
- **WHEN** a `VirtualDevice` subclass is instantiated without specifying vendor ID, product ID, or port
- **THEN** the base class SHALL use sensible defaults (vendor ID `0xFFF1`, product ID `0x8000`, port `0` for dynamic assignment)

### Requirement: Side-band HTTP server
The `VirtualDevice` base class SHALL start an HTTP server on a dynamically assigned port that accepts side-band operation requests from test drivers. The server SHALL listen on `0.0.0.0` and accept JSON-encoded POST requests.

#### Scenario: Side-band server starts on dynamic port
- **WHEN** the virtual device starts
- **THEN** an HTTP server SHALL start on a dynamically assigned port (port 0)
- **AND** the actual assigned port SHALL be reported via stdout in a parseable format

#### Scenario: Side-band server accepts JSON POST requests
- **WHEN** a POST request is sent to `/sideband` with a JSON body containing an `operation` field
- **THEN** the server SHALL dispatch to the registered handler for that operation
- **AND** the response SHALL be JSON with `success` (boolean) and `result` (object) fields

#### Scenario: Side-band server rejects unknown operations
- **WHEN** a POST request is sent with an `operation` value that has no registered handler
- **THEN** the server SHALL respond with HTTP 400 and `{ "success": false, "error": "Unknown operation: <name>" }`

### Requirement: Side-band operation registration
The `VirtualDevice` base class SHALL provide a `registerOperation(name, handler)` method that subclasses use to register their device-specific side-band operations. Each handler SHALL be an async function that receives the request payload and returns a result object.

#### Scenario: Subclass registers a custom operation
- **WHEN** a subclass calls `registerOperation("myOp", handler)` during construction
- **THEN** POST requests with `{ "operation": "myOp" }` SHALL be dispatched to that handler

#### Scenario: Subclass registers multiple operations
- **WHEN** a subclass registers operations "opA" and "opB"
- **THEN** both operations SHALL be independently dispatchable via the side-band HTTP interface

### Requirement: Ready signal on stdout
The virtual device process SHALL emit a JSON ready signal on stdout when both the Matter server node and the side-band HTTP server are fully initialized and operational.

#### Scenario: Ready signal includes connection details
- **WHEN** the virtual device has completed initialization
- **THEN** it SHALL print a single JSON line to stdout containing `{ "ready": true, "sidebandPort": <port>, "matterPort": <port>, "passcode": <code>, "discriminator": <disc> }`

### Requirement: Graceful shutdown
The virtual device SHALL handle SIGTERM and SIGINT signals by cleanly shutting down the Matter server node and the side-band HTTP server.

#### Scenario: Process terminated with SIGTERM
- **WHEN** the virtual device process receives SIGTERM
- **THEN** the Matter server node SHALL be closed
- **AND** the side-band HTTP server SHALL be closed
- **AND** the process SHALL exit with code 0

### Requirement: Package configuration
A `package.json` SHALL exist in the virtual device source directory that declares matter.js v0.16.10 as a dependency and configures the project as an ES module.

#### Scenario: Package dependencies are correct
- **WHEN** `npm install` is run in the virtual device source directory
- **THEN** matter.js v0.16.10 SHALL be installed along with any required peer dependencies
