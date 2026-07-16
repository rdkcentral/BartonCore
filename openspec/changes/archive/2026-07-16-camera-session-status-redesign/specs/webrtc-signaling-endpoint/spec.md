## MODIFIED Requirements

### Requirement: WebRTC endpoint declares signaling resources

The camera SBMD driver SHALL declare an endpoint with id `"webrtc"` and profile `"webrtc"` containing five resources:

| Resource | Type | Modes | Purpose |
|----------|------|-------|---------|
| `offerSdp` | `function` | execute | Client sends local SDP offer |
| `remoteSdp` | `string` | [] (events only) | Delivers remote SDP answer to client |
| `offerIceCandidates` | `function` | execute | Client sends local ICE candidates |
| `remoteIceCandidates` | `string` | [] (events only) | Delivers remote ICE candidates to client |
| `webrtcError` | `string` | [volatile] (events only) | Delivers asynchronous session termination/error to client |

The endpoint SHALL be declared within the same `camera.sbmd.js` file as the `ep/camera` endpoint. The `webrtcError` resource SHALL be declared with the `volatile` mode so that its events are emitted unconditionally (non-cached), independent of the previously emitted value.

#### Scenario: Endpoint appears on commissioned camera device
- **WHEN** a Matter camera device (deviceType 0x0142) with WebRTCTransportProvider cluster (0x0553) is commissioned
- **THEN** the device SHALL have an endpoint with id `"webrtc"`, profile `"webrtc"`, and all five resources registered

#### Scenario: Event-only resources are not readable
- **WHEN** a client attempts to read `remoteSdp`, `remoteIceCandidates`, or `webrtcError`
- **THEN** the read SHALL fail or return no value (modes list is empty — no read mode)

## ADDED Requirements

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

Each asynchronous WebRTC signaling failure that occurs after the originating execute has returned SHALL emit a `webrtcError` event so the client is notified rather than left to time out. This SHALL cover at least: a `VideoStreamAllocate` error, a `ProvideOffer` error, and a `requestCommand` overall-deadline timeout in the offer flow.

#### Scenario: VideoStreamAllocate rejected by camera
- **WHEN** the camera rejects the `VideoStreamAllocate` command during the `offerSdp` flow
- **THEN** the SBMD handler SHALL emit a `webrtcError` event with a failure value and metadata describing the allocate error

#### Scenario: ProvideOffer rejected by camera
- **WHEN** the camera rejects the `ProvideOffer` command during the `offerSdp` flow
- **THEN** the SBMD handler SHALL emit a `webrtcError` event with a failure value and metadata describing the provide-offer error

#### Scenario: Offer-flow command times out
- **WHEN** a `requestCommand` in the `offerSdp` flow exceeds its overall deadline
- **THEN** the SBMD handler SHALL emit a `webrtcError` event with a failure value and a timeout reason

## REMOVED Requirements

### Requirement: Incoming End command emits sessionStatus error

**Reason**: The `sessionStatus` resource is removed from the abstract `ep/camera` endpoint. Session teardown and error signaling for the WebRTC protocol now live on the protocol endpoint as the `webrtcError` event (see "Incoming End command emits webrtcError event").

**Migration**: Clients that subscribed to `ep/camera/r/sessionStatus` for the `"error"` value SHALL instead subscribe to `ep/webrtc/r/webrtcError` and treat its ended/failed values as the session-terminated signal.
