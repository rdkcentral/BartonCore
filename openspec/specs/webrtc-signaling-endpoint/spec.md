# webrtc-signaling-endpoint Specification

## Purpose
The camera SBMD driver's WebRTC protocol endpoint (`ep/webrtc`): the signaling resources (`localSdp`, `remoteSdp`, `localIceCandidates`, `remoteIceCandidates`) that relay SDP and ICE between a Barton client and a Matter camera's WebRTC Transport clusters, the `negotiationRole` resource that tells the client whether it is the offerer or answerer, plus the `webrtcError` event that surfaces asynchronous session termination and failures. The endpoint supports both Matter WebRTC negotiation flows — client-offers (`ProvideOffer`) and camera-offers (`SolicitOffer` + `ProvideAnswer`) — behind a single client-facing contract. All in-session state and error signaling for the WebRTC protocol lives here rather than on the abstract camera endpoint.
## Requirements
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

### Requirement: localSdp execute relays role-appropriate SDP

The `localSdp` execute handler SHALL accept only a non-empty client-produced SDP. It SHALL determine the client's negotiation role from the camera's advertised WebRTCTransportProvider commands (its `AcceptedCommandList`) and relay that SDP through the corresponding Matter signaling. Whenever a flow allocates a video stream, the driver SHALL allocate it via `VideoStreamAllocate` (cluster 0x0551, command 0x03) before the WebRTC-provider command, and SHALL pass the requestor's `originatingEndpointID` (the endpoint hosting the `WebRTCTransportRequestor` cluster) so the camera knows where to send its commands.

- **Offerer flow** (camera accepts `ProvideOffer`): the execute input is the client's SDP offer. The handler SHALL allocate a video stream and then send a `ProvideOffer` command (ID 0x02) to the camera's `WebRTCTransportProvider` cluster (0x0553), carrying the SDP and the allocated `videoStreamID`.
- **Answerer flow** (camera accepts `SolicitOffer`): the `stream` execute SHALL allocate a video stream and then send a `SolicitOffer` command (ID 0x00) so the camera generates the offer. Once the camera's offer has arrived (its `webRTCSessionID` recorded), `localSdp` SHALL carry the client's SDP answer and send a `ProvideAnswer` command (ID 0x04) with the SDP and the recorded `webRTCSessionID`.

#### Scenario: Offerer posts an SDP offer
- **WHEN** the camera accepts `ProvideOffer` and a client executes `localSdp` with a valid SDP offer
- **THEN** the handler SHALL allocate a video stream and send a `ProvideOffer` command to the camera with the SDP and the allocated `videoStreamID`

#### Scenario: Answerer flow is opened by stream
- **WHEN** the camera accepts `SolicitOffer` and a client executes `stream` for a valid session
- **THEN** the handler SHALL allocate a video stream and send a `SolicitOffer` command before returning the stream result so the camera generates the offer

#### Scenario: Answerer posts its SDP answer
- **WHEN** the camera has offered (its `webRTCSessionID` is recorded) and a client executes `localSdp` with an SDP answer
- **THEN** the handler SHALL send a `ProvideAnswer` command to the camera with the SDP and the recorded `webRTCSessionID`

#### Scenario: Empty local SDP is rejected
- **WHEN** a client executes `localSdp` with empty input
- **THEN** the handler SHALL return an error result

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

### Requirement: Signaling failures are reported on the channel that carries success

A signaling failure SHALL be reported on the same channel the corresponding success would have used, so that one failure never produces two notifications for the client to reconcile.

The `stream` execute stays parked for the whole camera-offerer chain and returns the stream result itself. `VideoStreamAllocate` and `SolicitOffer` failures in that chain SHALL therefore fail the pending `stream` execute and SHALL NOT emit a `webrtcError` event.

The `localSdp` execute signals success asynchronously — the camera's answer arrives later as a `remoteSdp` event — so there is no meaningful result to fail. `VideoStreamAllocate` and `ProvideOffer` failures in the camera-answerer chain SHALL therefore emit a `webrtcError` event with a failure value and metadata describing the error, rather than leaving the client to time out. This SHALL include a `requestCommand` overall-deadline timeout in that chain, reported with a timeout reason.

Because the runtime discards the message carried by an error terminal returned from a deferred handler, a handler that fails the pending execute SHALL also record the failure detail as a log operation.

#### Scenario: VideoStreamAllocate rejected during the camera-offerer chain
- **WHEN** the camera rejects `VideoStreamAllocate` while `stream` is parked on the `SolicitOffer` chain
- **THEN** the SBMD handler SHALL fail the pending `stream` execute AND SHALL NOT emit a `webrtcError` event

#### Scenario: SolicitOffer rejected by camera
- **WHEN** the camera rejects the `SolicitOffer` command while `stream` is parked on that chain
- **THEN** the SBMD handler SHALL fail the pending `stream` execute AND SHALL NOT emit a `webrtcError` event

#### Scenario: VideoStreamAllocate rejected during the camera-answerer chain
- **WHEN** the camera rejects `VideoStreamAllocate` after a client executed `localSdp` with its offer
- **THEN** the SBMD handler SHALL emit a `webrtcError` event with a failure value and metadata describing the allocate error

#### Scenario: ProvideOffer rejected by camera
- **WHEN** the camera rejects the `ProvideOffer` command during the camera-answerer flow
- **THEN** the SBMD handler SHALL emit a `webrtcError` event with a failure value and metadata describing the provide-offer error

#### Scenario: Signaling command times out
- **WHEN** a `requestCommand` in the camera-answerer chain exceeds its overall deadline
- **THEN** the SBMD handler SHALL emit a `webrtcError` event with a failure value and a timeout reason

### Requirement: Failed signaling rolls the session back and defers stream release

When a `VideoStreamAllocate`, `ProvideOffer`, or `SolicitOffer` command fails, the SBMD driver SHALL restore the associated local camera session to `created` state and remove its stored WebRTC identifier. The driver SHALL record this rollback as result operations rather than as a chained device command, because the deferred runtime discards a terminal returned from an error handler once the chain's overall deadline has expired.

The driver SHALL retain any `videoStreamID` supplied by a `VideoStreamAllocate` response and SHALL NOT issue `VideoStreamDeallocate` from the failing signaling chain. Releasing that stream on the camera is `destroySession`'s responsibility.

#### Scenario: Allocation fails before a video stream is assigned
- **WHEN** `VideoStreamAllocate` fails
- **THEN** the driver SHALL restore the local session to `created` state and clear any stored `webRTCSessionID`

#### Scenario: Soliciting an offer fails after allocation
- **WHEN** `SolicitOffer` fails after `VideoStreamAllocate` returned a video stream ID
- **THEN** the driver SHALL restore the local session to `created` state, retain the `videoStreamID`, and return the `SolicitOffer` failure to the pending `stream` execute without issuing `VideoStreamDeallocate`

#### Scenario: Providing an offer fails after allocation
- **WHEN** `ProvideOffer` fails after `VideoStreamAllocate` returned a video stream ID
- **THEN** the driver SHALL restore the local session to `created` state, retain the `videoStreamID`, and emit the `webrtcError` failure event without issuing `VideoStreamDeallocate`

### Requirement: stream may be retried while a video stream is allocated

`VideoStreamAllocate` is idempotent for a given set of stream parameters: the camera reuses a matching allocated stream and returns its existing identifier rather than creating a second one. When `stream` is executed for a session that already holds a `videoStreamID`, the handler SHALL proceed with the normal allocate-then-negotiate flow and SHALL NOT require the client to tear the session down first.

#### Scenario: stream re-executed after a failed negotiation
- **WHEN** `stream` is executed for a session that retains a `videoStreamID` from a failed negotiation
- **THEN** the handler SHALL issue `VideoStreamAllocate` and continue the negotiation flow

### Requirement: Teardown releases the camera's video stream allocation

Every `destroySession` SHALL leave the camera holding no video stream allocation attributable to that session, and SHALL remove the session from transient data only once the camera has confirmed the release.

A video stream allocated by `VideoStreamAllocate` is reference counted by the camera. `SolicitOffer` and `ProvideOffer` increment that count, `EndSession` decrements it, and the allocation itself is released only by `VideoStreamDeallocate`, which the camera rejects while the count is non-zero. Nothing releases the allocation implicitly, so an allocation that is never deallocated consumes one of the camera's encoders until it restarts.

#### Scenario: Client destroys an active streaming session
- **WHEN** a client executes `destroySession` for a session in `streaming` state holding a `videoStreamID`
- **THEN** the handler SHALL send `EndSession` (ID 0x06) to the `WebRTCTransportProvider` cluster, AND after the camera confirms it SHALL issue `VideoStreamDeallocate` (ID 0x06) to the Camera AV Stream Management cluster, AND SHALL remove the session from transient data only once that deallocation is confirmed

#### Scenario: Client destroys a session left by a failed negotiation
- **WHEN** `destroySession` is executed for a `created` session holding a `videoStreamID`
- **THEN** the handler SHALL issue `VideoStreamDeallocate` without sending `EndSession`, because no WebRTC session holds a reference, AND SHALL remove the session once the camera responds

#### Scenario: Client destroys a session that never allocated a stream
- **WHEN** a client executes `destroySession` for a session with no stored `videoStreamID` and no `webRTCSessionID`
- **THEN** the handler SHALL only remove the session from transient data (no Matter command needed)

#### Scenario: EndSession fails during teardown
- **WHEN** `EndSession` issued by `destroySession` fails
- **THEN** the handler SHALL fail the execute AND SHALL NOT issue `VideoStreamDeallocate`, because the stream still carries the session's reference, AND SHALL leave the session unchanged so a subsequent `destroySession` can retry the whole teardown

#### Scenario: Stream release fails during teardown
- **WHEN** `VideoStreamDeallocate` issued by `destroySession` fails
- **THEN** the handler SHALL fail the execute AND leave the session and its `videoStreamID` in transient data so a subsequent `destroySession` can retry the release alone

### Requirement: A camera-ended session retains its stream allocation for teardown

When an `End` command names a session holding a `videoStreamID`, the handler SHALL retain that session in transient data, restore it to `created` state, and clear its `webRTCSessionID`, so that a later `destroySession` releases the allocation. When the named session holds no `videoStreamID`, the handler SHALL remove it.

Ending the session drops the camera's own reference count on the stream, but the stream remains allocated. A command handler cannot issue device commands, so the driver cannot release it at that point.

#### Scenario: Camera ends a session holding a video stream
- **WHEN** the camera sends `End` for a session with a stored `videoStreamID`
- **THEN** the handler SHALL emit the `webrtcError` ended event AND retain the session in `created` state with its `videoStreamID` and without its `webRTCSessionID`

#### Scenario: Camera ends a session with no allocation
- **WHEN** the camera sends `End` for a session with no stored `videoStreamID`
- **THEN** the handler SHALL emit the `webrtcError` ended event AND remove the session from transient data

### Requirement: Clients release camera resources after a session ends

On receiving a `webrtcError` event with the `ended` value, a client SHALL execute `destroySession` for the session named in the event metadata. Until it does, the camera keeps that session's video stream allocated.

This places resource release on the client, which is a known weakness rather than a deliberate design. The driver learns that a session ended through a command handler, and a command handler cannot issue device commands, so it has no way to release the stream itself. If the client never calls `destroySession`, the allocation survives until the camera restarts; worse, once the session's transient data expires the stored `videoStreamID` is lost and the driver can no longer release it at all. This contract should be revisited if the driver gains a way to release device resources without client involvement.

#### Scenario: Client tears down a camera-ended session
- **WHEN** a client receives a `webrtcError` event with the `ended` value
- **THEN** the client SHALL execute `destroySession` for the `sessionId` carried in the event metadata

#### Scenario: Session ends without client teardown
- **WHEN** a client does not execute `destroySession` after an `ended` event
- **THEN** the camera SHALL retain the video stream allocation AND the driver SHALL NOT release it

### Requirement: Matter identifiers come from the Matter specification

The SBMD driver SHALL use the cluster, command, and response identifiers defined by the Matter WebRTC Transport and Camera AV Stream Management cluster specifications, at the SDK revision pinned in `matter-version`. This specification does not restate those values as a table; the driver source is the single place they are enumerated, and scenarios above cite an identifier only where it disambiguates the command being sent.

#### Scenario: Constants match the Matter specification
- **WHEN** the SBMD driver is loaded
- **THEN** all cluster, command, and response ID constants SHALL match the values defined by the Matter WebRTC Transport and Camera AV Stream Management cluster specifications for the pinned SDK revision

### Requirement: WebRTC endpoint is separable by design

The webrtc endpoint resources, constants, and handler functions SHALL be grouped together and access session state only through transient data supplements. No direct coupling between camera endpoint handlers and webrtc endpoint handlers beyond shared transient data keys.

#### Scenario: Code organization supports extraction
- **WHEN** the webrtc endpoint code is reviewed
- **THEN** all webrtc-specific constants, resources, and handlers SHALL be identifiable as a cohesive group that could be moved to a separate file with only transient data key sharing as the interface

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

