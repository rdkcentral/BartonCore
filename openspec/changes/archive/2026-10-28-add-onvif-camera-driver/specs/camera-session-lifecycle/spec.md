## MODIFIED Requirements

### Requirement: Abstract camera endpoint provides protocol-agnostic session lifecycle

A camera device driver SHALL declare an endpoint with id `"camera"` and profile `"camera"` exposing
the session lifecycle as execute resources only: `createSession`, `stream`, `takePicture`, and
`destroySession`. The abstract endpoint SHALL NOT declare a `sessionStatus` resource, and SHALL NOT
carry any protocol-specific signaling or in-session state. All in-session state, error, and teardown
signaling SHALL live on the protocol-specific endpoint (e.g. `ep/webrtc`, `ep/onvif`). This contract
MAY be implemented by an SBMD/Matter driver or by a native driver, and by any streaming protocol.

#### Scenario: Camera endpoint exposes only lifecycle executes
- **WHEN** a Matter camera device (deviceType 0x0142) is commissioned
- **THEN** the device SHALL have an endpoint with id `"camera"` exposing `createSession`, `stream`, `takePicture`, and `destroySession` as execute resources AND SHALL NOT expose a `sessionStatus` resource

#### Scenario: Native driver implements the same abstract endpoint
- **WHEN** a native (non-SBMD) camera driver configures a camera it manages
- **THEN** the device SHALL have an endpoint with id `"camera"` exposing the same four lifecycle executes AND SHALL NOT expose a `sessionStatus` resource

#### Scenario: No protocol coupling on the abstract endpoint
- **WHEN** the `ep/camera` endpoint is inspected
- **THEN** none of its resources SHALL reference a specific streaming protocol; protocol identity is carried only in the `stream` execute result and on the protocol endpoint

### Requirement: createSession allocates a session and returns its identifier

The `createSession` execute handler SHALL return a non-empty `sessionId` string synchronously as the
execute result so the client holds a correlation identifier before invoking any further resource. A
driver MAY persist per-session state (as the Matter driver does in transient data); a driver whose
protocol is stateless MAY return an identifier without persisting session state.

#### Scenario: Client creates a session
- **WHEN** a client executes `createSession`
- **THEN** the handler SHALL return a non-empty `sessionId` string

#### Scenario: Corrupt session data is reset
- **WHEN** `createSession` is executed on a driver that persists session state and the stored session data cannot be parsed
- **THEN** the handler SHALL reset the session store and return an error result

### Requirement: stream execute returns the active protocol and entry point

The `stream` execute handler SHALL return, as its synchronous execute result, a JSON object
identifying the active protocol and the entry-point resource URI the client must use next:
`{ "protocol": "<protocol>", "entryPoint": "/<deviceId>/ep/<protocol>/r/<resource>" }`. The handler
SHALL NOT emit a separate event to convey the next action. A driver that tracks sessions SHALL mark
the identified session as streaming; a driver whose protocol is stateless MAY ignore the `sessionId`
argument.

#### Scenario: Stream returns protocol and entry point for a WebRTC camera
- **WHEN** a client executes `stream` with a valid `sessionId` on a Matter WebRTC camera
- **THEN** the handler SHALL return `{ "protocol": "webrtc", "entryPoint": "/<deviceId>/ep/webrtc/r/localSdp" }` AND mark the session `streaming`

#### Scenario: Stream returns protocol and entry point for an ONVIF camera
- **WHEN** a client executes `stream` on an ONVIF camera with any or no `sessionId`
- **THEN** the handler SHALL return `{ "protocol": "onvif", "entryPoint": "/<deviceId>/ep/onvif/r/getMediaUrl" }`

#### Scenario: Stream on unknown session
- **WHEN** a client executes `stream` with a `sessionId` that does not exist on a driver that tracks sessions
- **THEN** the handler SHALL return an error result and SHALL NOT mark any session streaming

### Requirement: destroySession releases session state

The `destroySession` execute handler SHALL release any session state it holds for the identified
session and trigger any protocol-specific teardown required for a session that reached streaming. A
driver whose protocol is stateless SHALL treat `destroySession` as a successful no-op.

#### Scenario: Client destroys a session
- **WHEN** a client executes `destroySession` with a valid `sessionId` on a driver that tracks sessions
- **THEN** the handler SHALL remove the session from its session state

#### Scenario: destroySession on a stateless driver
- **WHEN** a client executes `destroySession` on a driver whose protocol holds no session state
- **THEN** the handler SHALL return success without error
