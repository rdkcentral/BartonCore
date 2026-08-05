# camera-session-lifecycle Specification

## Purpose
The protocol-agnostic abstract camera endpoint contract: the session lifecycle executes (`createSession`, `stream`, `takePicture`, `destroySession`) with no `sessionStatus` resource. The `stream` execute returns the active protocol and its entry-point URI; all in-session state, error, and teardown signaling lives on the protocol-specific endpoint (e.g. `ep/webrtc`), not the abstract one.

## Requirements
### Requirement: Abstract camera endpoint provides protocol-agnostic session lifecycle

The camera SBMD driver SHALL declare an endpoint with id `"camera"` and profile `"camera"` exposing the session lifecycle as execute resources only: `createSession`, `stream`, `takePicture`, and `destroySession`. The abstract endpoint SHALL NOT declare a `sessionStatus` resource, and SHALL NOT carry any protocol-specific signaling or in-session state. All in-session state, error, and teardown signaling SHALL live on the protocol-specific endpoint (e.g. `ep/webrtc`).

#### Scenario: Camera endpoint exposes only lifecycle executes
- **WHEN** a Matter camera device (deviceType 0x0142) is commissioned
- **THEN** the device SHALL have an endpoint with id `"camera"` exposing `createSession`, `stream`, `takePicture`, and `destroySession` as execute resources AND SHALL NOT expose a `sessionStatus` resource

#### Scenario: No protocol coupling on the abstract endpoint
- **WHEN** the `ep/camera` endpoint is inspected
- **THEN** none of its resources SHALL reference a specific streaming protocol; protocol identity is carried only in the `stream` execute result and on the protocol endpoint

### Requirement: createSession allocates a session and returns its identifier

The `createSession` execute handler SHALL allocate a new session, persist it in transient data, and return the new `sessionId` synchronously as the execute result so the client holds a correlation identifier before invoking any further resource.

#### Scenario: Client creates a session
- **WHEN** a client executes `createSession`
- **THEN** the handler SHALL return a non-empty `sessionId` string AND record the session in transient data with an initial state

#### Scenario: Corrupt session data is reset
- **WHEN** `createSession` is executed and the stored session data cannot be parsed
- **THEN** the handler SHALL reset the session store and return an error result

### Requirement: stream execute returns the active protocol and entry point

The `stream` execute handler SHALL mark the identified session as streaming and return, as its synchronous execute result, a JSON object identifying the active protocol and the entry-point resource URI the client must use next: `{ "protocol": "<protocol>", "entryPoint": "/<deviceId>/ep/<protocol>/r/<resource>" }`. The handler SHALL NOT emit a separate event to convey the next action.

#### Scenario: Stream returns protocol and entry point for a WebRTC camera
- **WHEN** a client executes `stream` with a valid `sessionId` on a Matter WebRTC camera
- **THEN** the handler SHALL return `{ "protocol": "webrtc", "entryPoint": "/<deviceId>/ep/webrtc/r/localSdp" }` AND mark the session `streaming`

#### Scenario: Stream on unknown session
- **WHEN** a client executes `stream` with a `sessionId` that does not exist
- **THEN** the handler SHALL return an error result and SHALL NOT mark any session streaming

### Requirement: destroySession releases session state

The `destroySession` execute handler SHALL remove the identified session from transient data and trigger any protocol-specific teardown required for a session that reached streaming.

#### Scenario: Client destroys a session
- **WHEN** a client executes `destroySession` with a valid `sessionId`
- **THEN** the handler SHALL remove the session from transient data

### Requirement: Client discovers next steps without server-pushed status events

A client SHALL be able to drive the full session flow using only execute results and protocol-endpoint event subscriptions, without reading or subscribing to any status resource on the abstract endpoint.

#### Scenario: Reference client drives flow without sessionStatus
- **WHEN** the reference app runs a camera stream
- **THEN** it SHALL obtain the protocol and entry point from the `stream` execute result and subscribe to protocol-endpoint events, and SHALL NOT subscribe to any `sessionStatus` resource
