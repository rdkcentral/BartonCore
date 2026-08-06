## ADDED Requirements

### Requirement: WebRTC endpoint declares signaling resources

The camera SBMD driver SHALL declare an endpoint with id `"webrtc"` and profile `"webrtc"` containing four resources:

| Resource | Type | Modes | Purpose |
|----------|------|-------|---------|
| `offerSdp` | `function` | execute | Client sends local SDP offer |
| `remoteSdp` | `string` | [] (events only) | Delivers remote SDP answer to client |
| `offerIceCandidates` | `function` | execute | Client sends local ICE candidates |
| `remoteIceCandidates` | `string` | [] (events only) | Delivers remote ICE candidates to client |

The endpoint SHALL be declared within the same `camera.sbmd.js` file as the `ep/camera` endpoint.

#### Scenario: Endpoint appears on commissioned camera device
- **WHEN** a Matter camera device (deviceType 0x0142) with WebRTCTransportProvider cluster (0x0553) is commissioned
- **THEN** the device SHALL have an endpoint with id `"webrtc"`, profile `"webrtc"`, and all four resources registered

#### Scenario: Event-only resources are not readable
- **WHEN** a client attempts to read `remoteSdp` or `remoteIceCandidates`
- **THEN** the read SHALL fail or return no value (modes list is empty — no read mode)

### Requirement: offerSdp execute sends ProvideOffer to camera

The `offerSdp` execute handler SHALL send a `ProvideOffer` command (ID 0x02) to the camera's `WebRTCTransportProvider` cluster (0x0553). The execute input is the client's SDP offer string. The handler SHALL package the SDP into the command's TLV payload along with the active session's `webRTCSessionID`.

#### Scenario: Client provides SDP offer
- **WHEN** a client executes `offerSdp` with a valid SDP string as input
- **THEN** the SBMD handler SHALL send a `ProvideOffer` command to the camera with the SDP in the `sdp` field and the Matter-side webRTCSessionID in the `webRTCSessionID` field

#### Scenario: No active session
- **WHEN** a client executes `offerSdp` but no session is in `streaming` state
- **THEN** the handler SHALL return an error result

### Requirement: offerIceCandidates execute sends ProvideICECandidates to camera

The `offerIceCandidates` execute handler SHALL send a `ProvideICECandidates` command (ID 0x05) to the camera's `WebRTCTransportProvider` cluster (0x0553). The execute input is a JSON-encoded array of ICE candidate strings.

#### Scenario: Client provides ICE candidates
- **WHEN** a client executes `offerIceCandidates` with a JSON array of ICE candidate strings
- **THEN** the SBMD handler SHALL send a `ProvideICECandidates` command to the camera with the candidates in the `ICECandidates` field

### Requirement: Incoming Offer command emits remoteSdp event

The SBMD driver SHALL register a command handler for the `Offer` command (ID 0x00) on the `WebRTCTransportRequestor` cluster (0x0554). When received, the handler SHALL extract the SDP string and emit it as an event on the `remoteSdp` resource of the `webrtc` endpoint.

#### Scenario: Camera sends SDP answer
- **WHEN** the camera sends an `Offer` command (cluster 0x0554, command 0x00) containing an SDP string
- **THEN** the SBMD handler SHALL call `updateResource('webrtc', 'remoteSdp', sdpString)` to emit an event to subscribed clients

### Requirement: Incoming ICECandidates command emits remoteIceCandidates event

The SBMD driver SHALL register a command handler for the `ICECandidates` command (ID 0x02) on the `WebRTCTransportRequestor` cluster (0x0554). When received, the handler SHALL extract the candidate list and emit it as a JSON-encoded array on the `remoteIceCandidates` resource.

#### Scenario: Camera sends ICE candidates
- **WHEN** the camera sends an `ICECandidates` command (cluster 0x0554, command 0x02) containing ICE candidates
- **THEN** the SBMD handler SHALL call `updateResource('webrtc', 'remoteIceCandidates', jsonCandidates)` to emit an event to subscribed clients

### Requirement: Incoming End command emits sessionStatus error

The SBMD driver SHALL register a command handler for the `End` command (ID 0x03) on the `WebRTCTransportRequestor` cluster (0x0554). When received, the handler SHALL emit a `sessionStatus` event with value `"error"` and metadata containing the session ID and error reason.

#### Scenario: Camera ends session
- **WHEN** the camera sends an `End` command with a reason code
- **THEN** the SBMD handler SHALL emit a `sessionStatus` event with value `"error"` and metadata `{ "sessionId": "<id>", "error": "<reason>" }`

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
| CMD_PROVIDE_OFFER | 0x02 | Send SDP offer to camera |
| CMD_PROVIDE_ANSWER | 0x04 | Send SDP answer to camera |
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
