## MODIFIED Requirements

### Requirement: WebRTC endpoint declares signaling resources

The camera SBMD driver SHALL declare an endpoint with id `"webrtc"` and profile `"webrtc"` containing six resources:

| Resource | Type | Modes | Purpose |
|----------|------|-------|---------|
| `localSdp` | `function` | execute | Client posts its local SDP (offer or answer) to drive signaling |
| `negotiationRole` | `string` | [read] | Reports the **camera's** negotiation role (`offerer` or `answerer`); the client adopts the opposite role |
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

### Requirement: negotiationRole read reports the camera's role

The `negotiationRole` read handler SHALL report the **camera's** WebRTC negotiation role — `offerer` when the camera generates the SDP offer, or `answerer` when the camera answers the client's offer — derived from the camera's advertised WebRTCTransportProvider `AcceptedCommandList`. When the camera accepts `SolicitOffer` the role SHALL be `offerer` (the camera generates the offer); otherwise, when the camera accepts `ProvideOffer`, the role SHALL be `answerer` (the camera answers). When the accepted-command list is unavailable, the handler SHALL default to `offerer` (the SolicitOffer flow). The resource describes the camera because `ep/webrtc` is the camera's data model; the consuming client is responsible for adopting the opposite role. The negotiation role is a WebRTC concept and lives on the `webrtc` endpoint, not in the abstract `stream` result.

#### Scenario: Camera supporting SolicitOffer reports offerer
- **WHEN** a client reads `negotiationRole` and the camera's `AcceptedCommandList` includes `SolicitOffer`
- **THEN** the read SHALL return `offerer` (the camera generates the offer and the client answers)

#### Scenario: Camera supporting only ProvideOffer reports answerer
- **WHEN** a client reads `negotiationRole` and the camera's `AcceptedCommandList` includes `ProvideOffer` but not `SolicitOffer`
- **THEN** the read SHALL return `answerer` (the camera answers and the client offers)

#### Scenario: Unavailable accepted-command list defaults to offerer
- **WHEN** a client reads `negotiationRole` and the camera's `AcceptedCommandList` is unavailable
- **THEN** the read SHALL return `offerer` (the default SolicitOffer flow, in which the camera generates the offer)

## RENAMED Requirements

- FROM: `### Requirement: negotiationRole read reports the client's role`
- TO: `### Requirement: negotiationRole read reports the camera's role`
