# webrtc-signaling-endpoint Specification

## Purpose
The camera SBMD driver's WebRTC protocol endpoint (`ep/webrtc`): the signaling resources (`localSdp`, `remoteSdp`, `localIceCandidates`, `remoteIceCandidates`) that relay SDP and ICE between a Barton client and a Matter camera's WebRTC Transport clusters, the `negotiationRole` resource that tells the client whether it is the offerer or answerer, plus the `webrtcError` event that surfaces asynchronous session termination and failures. The endpoint supports both Matter WebRTC negotiation flows — client-offers (`ProvideOffer`) and camera-offers (`SolicitOffer` + `ProvideAnswer`) — behind a single client-facing contract. All in-session state and error signaling for the WebRTC protocol lives here rather than on the abstract camera endpoint.
## Requirements
### Requirement: WebRTC endpoint declares signaling resources

The camera SBMD driver SHALL declare an endpoint with id `"webrtc"` and profile `"webrtc"` containing six resources:

| Resource | Type | Modes | Purpose |
|----------|------|-------|---------|
| `localSdp` | `function` | execute | Client posts its local SDP (offer or answer) to drive signaling |
| `negotiationRole` | `string` | [read] | Reports the negotiation role (`offerer` or `answerer`) the client must adopt |
| `remoteSdp` | `string` | [] (events only) | Delivers the camera's remote SDP (offer or answer) to client |
| `localIceCandidates` | `function` | execute | Client sends local ICE candidates |
| `remoteIceCandidates` | `string` | [] (events only) | Delivers remote ICE candidates to client |
| `webrtcError` | `string` | [volatile] (events only) | Delivers asynchronous session termination/error to client |

The endpoint SHALL be declared within the same `camera.sbmd.js` file as the `ep/camera` endpoint. The `webrtcError` resource SHALL be declared with the `volatile` mode so that its events are emitted unconditionally (non-cached), independent of the previously emitted value.

#### Scenario: Endpoint appears on commissioned camera device
- **WHEN** a Matter camera device (deviceType 0x0142) with WebRTCTransportProvider cluster (0x0553) is commissioned
- **THEN** the device SHALL have an endpoint with id `"webrtc"`, profile `"webrtc"`, and all six resources registered

#### Scenario: Event-only resources are not readable
- **WHEN** a client attempts to read `remoteSdp`, `remoteIceCandidates`, or `webrtcError`
- **THEN** the read SHALL fail or return no value (modes list is empty — no read mode)

### Requirement: negotiationRole read reports the client's role

The `negotiationRole` read handler SHALL report whether the client is the `offerer` or the `answerer`, derived from the camera's advertised WebRTCTransportProvider `AcceptedCommandList`. When the camera accepts `SolicitOffer` the role SHALL be `answerer` (the camera generates the offer); otherwise, when the camera accepts `ProvideOffer`, the role SHALL be `offerer`. When the accepted-command list is unavailable, the handler SHALL default to `answerer`. The negotiation role is a WebRTC concept and lives on the `webrtc` endpoint, not in the abstract `stream` result.

#### Scenario: Camera supporting SolicitOffer yields answerer
- **WHEN** a client reads `negotiationRole` and the camera's `AcceptedCommandList` includes `SolicitOffer`
- **THEN** the read SHALL return `answerer`

#### Scenario: Camera supporting only ProvideOffer yields offerer
- **WHEN** a client reads `negotiationRole` and the camera's `AcceptedCommandList` includes `ProvideOffer` but not `SolicitOffer`
- **THEN** the read SHALL return `offerer`

### Requirement: localSdp execute drives role-appropriate signaling

The `localSdp` execute handler SHALL determine the client's negotiation role from the camera's advertised WebRTCTransportProvider commands (its `AcceptedCommandList`) and drive the corresponding Matter signaling. In every case that requires it, the handler SHALL first allocate a video stream via `VideoStreamAllocate` (cluster 0x0551, command 0x03) before the WebRTC-provider command, and SHALL pass the requestor's `originatingEndpointID` (the endpoint hosting the `WebRTCTransportRequestor` cluster) so the camera knows where to send its commands.

- **Offerer flow** (camera accepts `ProvideOffer`): the execute input is the client's SDP offer. The handler SHALL allocate a video stream and then send a `ProvideOffer` command (ID 0x02) to the camera's `WebRTCTransportProvider` cluster (0x0553), carrying the SDP and the allocated `videoStreamID`.
- **Answerer flow** (camera accepts `SolicitOffer`): while no camera `webRTCSessionID` has been recorded yet, an execute (with empty input) SHALL allocate a video stream and then send a `SolicitOffer` command (ID 0x00) so the camera generates the offer. Once the camera's offer has arrived (its `webRTCSessionID` recorded), a subsequent execute SHALL carry the client's SDP answer and send a `ProvideAnswer` command (ID 0x04) with the SDP and the recorded `webRTCSessionID`.

#### Scenario: Offerer posts an SDP offer
- **WHEN** the camera accepts `ProvideOffer` and a client executes `localSdp` with a valid SDP offer
- **THEN** the handler SHALL allocate a video stream and send a `ProvideOffer` command to the camera with the SDP and the allocated `videoStreamID`

#### Scenario: Answerer opens the flow
- **WHEN** the camera accepts `SolicitOffer` and a client executes `localSdp` with empty input before any camera offer has arrived
- **THEN** the handler SHALL allocate a video stream and send a `SolicitOffer` command so the camera generates the offer

#### Scenario: Answerer posts its SDP answer
- **WHEN** the camera has offered (its `webRTCSessionID` is recorded) and a client executes `localSdp` with an SDP answer
- **THEN** the handler SHALL send a `ProvideAnswer` command to the camera with the SDP and the recorded `webRTCSessionID`

#### Scenario: No active session
- **WHEN** a client executes `localSdp` but no session is in `streaming` state
- **THEN** the handler SHALL return an error result

### Requirement: localIceCandidates execute sends ProvideICECandidates to camera

The `localIceCandidates` execute handler SHALL send a `ProvideICECandidates` command (ID 0x05) to the camera's `WebRTCTransportProvider` cluster (0x0553). The execute input is a JSON-encoded array of ICE candidate strings.

#### Scenario: Client provides ICE candidates
- **WHEN** a client executes `localIceCandidates` with a JSON array of ICE candidate strings
- **THEN** the SBMD handler SHALL send a `ProvideICECandidates` command to the camera with the candidates in the `ICECandidates` field

### Requirement: Incoming Offer and Answer commands emit remoteSdp event

The SBMD driver SHALL register command handlers for both the `Offer` command (ID 0x00) and the `Answer` command (ID 0x01) on the `WebRTCTransportRequestor` cluster (0x0554). When either is received, the handler SHALL extract the SDP string, record the command's `webRTCSessionID` on the active session, and emit the SDP as an event on the `remoteSdp` resource of the `webrtc` endpoint. The `Offer` command carries the camera's offer (SolicitOffer flow); the `Answer` command carries the camera's answer (ProvideOffer flow).

#### Scenario: Camera sends its offer (SolicitOffer flow)
- **WHEN** the camera sends an `Offer` command (cluster 0x0554, command 0x00) containing an SDP string
- **THEN** the SBMD handler SHALL record the `webRTCSessionID` AND call `updateResource('webrtc', 'remoteSdp', sdpString)` to emit an event to subscribed clients

#### Scenario: Camera sends its answer (ProvideOffer flow)
- **WHEN** the camera sends an `Answer` command (cluster 0x0554, command 0x01) containing an SDP string
- **THEN** the SBMD handler SHALL record the `webRTCSessionID` AND call `updateResource('webrtc', 'remoteSdp', sdpString)` to emit an event to subscribed clients

### Requirement: Incoming ICECandidates command emits remoteIceCandidates event

The SBMD driver SHALL register a command handler for the `ICECandidates` command (ID 0x02) on the `WebRTCTransportRequestor` cluster (0x0554). When received, the handler SHALL extract the candidate list and emit it as a JSON-encoded array on the `remoteIceCandidates` resource.

#### Scenario: Camera sends ICE candidates
- **WHEN** the camera sends an `ICECandidates` command (cluster 0x0554, command 0x02) containing ICE candidates
- **THEN** the SBMD handler SHALL call `updateResource('webrtc', 'remoteIceCandidates', jsonCandidates)` to emit an event to subscribed clients

### Requirement: webrtcError resource emits every event unconditionally

The `webrtcError` resource SHALL be declared with the `volatile` mode, which registers it with `CACHING_POLICY_NEVER` so that `updateResource` bypasses value-change detection and each emission delivers a `resourceUpdated` event to subscribers even when consecutive values are identical (including across sessions where a prior value persists).

#### Scenario: Repeated identical values still emit
- **WHEN** the driver emits `webrtcError` twice in succession with the same value
- **THEN** the client SHALL receive two distinct `resourceUpdated` events (no no-change suppression)

### Requirement: Incoming End command emits webrtcError event

The SBMD driver SHALL register a command handler for the `End` command (ID 0x03) on the `WebRTCTransportRequestor` cluster (0x0554). When received, the handler SHALL clean up the associated session and emit a `webrtcError` event on the `webrtc` endpoint with a value indicating the session ended and metadata carrying the reason.

#### Scenario: Camera ends session
- **WHEN** the camera sends an `End` command with a reason code
- **THEN** the SBMD handler SHALL call `updateResource('webrtc', 'webrtcError', <endedValue>, { "reason": "<reason>", "detail": "<text>" })` AND remove the associated session from transient data

### Requirement: Asynchronous signaling failures emit webrtcError event

Each asynchronous WebRTC signaling failure that occurs after the originating execute has returned SHALL emit a `webrtcError` event so the client is notified rather than left to time out. This SHALL cover at least: a `VideoStreamAllocate` error, a `ProvideOffer` error (offerer flow), a `SolicitOffer` error (answerer flow), and a `requestCommand` overall-deadline timeout in the signaling flow.

#### Scenario: VideoStreamAllocate rejected by camera
- **WHEN** the camera rejects the `VideoStreamAllocate` command during the `localSdp` flow
- **THEN** the SBMD handler SHALL emit a `webrtcError` event with a failure value and metadata describing the allocate error

#### Scenario: ProvideOffer rejected by camera
- **WHEN** the camera rejects the `ProvideOffer` command during the offerer (`ProvideOffer`) flow
- **THEN** the SBMD handler SHALL emit a `webrtcError` event with a failure value and metadata describing the provide-offer error

#### Scenario: SolicitOffer rejected by camera
- **WHEN** the camera rejects the `SolicitOffer` command during the answerer (`SolicitOffer`) flow
- **THEN** the SBMD handler SHALL emit a `webrtcError` event with a failure value and metadata describing the solicit-offer error

#### Scenario: Signaling command times out
- **WHEN** a `requestCommand` in the `localSdp` flow exceeds its overall deadline
- **THEN** the SBMD handler SHALL emit a `webrtcError` event with a failure value and a timeout reason

### Requirement: destroySession sends EndSession to camera

When the camera session endpoint's `destroySession` is executed for a session that has progressed to WebRTC signaling, the handler SHALL send an `EndSession` command (ID 0x06) to the camera's `WebRTCTransportProvider` cluster (0x0553) before cleaning up local session state.

#### Scenario: Client destroys active streaming session
- **WHEN** a client executes `destroySession` for a session in `streaming` state
- **THEN** the handler SHALL send `EndSession` to the camera AND remove the session from transient data

#### Scenario: Client destroys session that never started streaming
- **WHEN** a client executes `destroySession` for a session in `created` state (never executed `stream`)
- **THEN** the handler SHALL only remove the session from transient data (no Matter command needed)

### Requirement: WebRTC constants use correct Matter cluster and command IDs

The SBMD driver SHALL define constants for all WebRTC cluster and command identifiers:

| Constant | Value | Description |
|----------|-------|-------------|
| CL_WEBRTC_TRANSPORT_PROVIDER | 0x0553 | Camera's provider cluster |
| CL_WEBRTC_TRANSPORT_REQUESTOR | 0x0554 | Barton's requestor cluster |
| CL_CAMERA_AV_STREAM_MGMT | 0x0551 | Camera A/V stream management (video stream allocation) |
| CMD_VIDEO_STREAM_ALLOCATE | 0x03 | Allocate a video stream before offer/solicit |
| CMD_SOLICIT_OFFER | 0x00 | Ask the camera to generate the offer (answerer flow) |
| CMD_SOLICIT_OFFER_RESP | 0x01 | Camera's response to SolicitOffer |
| CMD_PROVIDE_OFFER | 0x02 | Send SDP offer to camera (offerer flow) |
| CMD_PROVIDE_ANSWER | 0x04 | Send SDP answer to camera (answerer flow) |
| CMD_PROVIDE_ICE | 0x05 | Send ICE candidates to camera |
| CMD_END_SESSION | 0x06 | End a WebRTC session |
| CMD_OFFER | 0x00 | Incoming offer from camera |
| CMD_ANSWER | 0x01 | Incoming answer from camera |
| CMD_ICE_CANDIDATES | 0x02 | Incoming ICE from camera |
| CMD_END | 0x03 | Incoming end from camera |

#### Scenario: Constants match Matter specification
- **WHEN** the SBMD driver is loaded
- **THEN** all cluster and command ID constants SHALL match the values defined in the Matter 1.5 WebRTC Transport cluster specification

### Requirement: WebRTC endpoint is separable by design

The webrtc endpoint resources, constants, and handler functions SHALL be grouped together and access session state only through transient data supplements. No direct coupling between camera endpoint handlers and webrtc endpoint handlers beyond shared transient data keys.

#### Scenario: Code organization supports extraction
- **WHEN** the webrtc endpoint code is reviewed
- **THEN** all webrtc-specific constants, resources, and handlers SHALL be identifiable as a cohesive group that could be moved to a separate file with only transient data key sharing as the interface

