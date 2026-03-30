## Context

BartonCore provides protocol-agnostic IoT device management through a URI-based resource model (`/deviceId/ep/N/r/resourceName`). Devices have endpoints with profiles (e.g., `doorLock`, `sensor`, `light`) that define mandatory and optional resources. Drivers communicate state changes via `updateResource()`, which can carry optional `cJSON *metadata` — a pattern already used by door lock (source, userId), sensor (test flag), and link quality drivers.

Barton currently supports cameras only through the OpenHome IP camera model. The existing `camera` profile in `commonDeviceDefs.h` defines resources tightly coupled to that model: credentials (`adminUserId`, `adminPassword`), media tunnels (`createMediaTunnel`, `destroyMediaTunnel`), and snapshot URLs (`pictureURL`). There is no shared stream lifecycle that works across camera technologies.

Matter 1.5 introduces WebRTC-based cameras with a fundamentally different signaling pattern: SDP offer/answer exchange, ICE candidate negotiation, and a bidirectional transport flow. The team has agreed that rather than building another technology-specific camera model, Barton should define a protocol-agnostic stream contract that both WebRTC and OpenHome cameras (and future technologies) can implement.

## Goals / Non-Goals

**Goals:**
- Define an abstract camera stream contract that is independent of any specific streaming protocol
- Support WebRTC (Matter 1.5) and OpenHome cameras through the same client-facing stream flow
- Support local-only cameras that require no negotiation (e.g., ONVIF, Reolink) — the contract handles both multi-step signaling and immediate URL delivery
- Design the WebRTC endpoint profile to be reusable beyond cameras (e.g., ssh, binary transfer, audio streaming)
- Use `updateResource()` event metadata as the stream coordination mechanism — no new infrastructure needed
- Keep events as the source of truth for stream state, with per-stream correlation via `streamId`
- Maintain full backward compatibility with existing OpenHome camera clients
- Ensure the pattern extends naturally to future technologies (RTSP, HTTP, etc.)

**Non-Goals:**
- Strongly typed metadata enforcement (future improvement)
- SBMD feasibility evaluation for Matter cameras (separate investigation)
- Video rendering, decoding, or media pipeline integration
- WebRTC media stack selection (libdatachannel vs. alternatives — deferred to webrtc-endpoint-profile design)

## Decisions

### D1: Two-layer endpoint architecture — abstract + protocol-specific

**Decision**: Every camera device has an abstract **camera endpoint** (`profile: "camera"`) that owns stream lifecycle, plus one or more **protocol-specific endpoints** that carry the signaling details for a particular technology.

**Rationale**: This separation keeps the client-facing stream flow identical regardless of protocol. A client that only cares about "give me a stream and follow the events" never needs to know whether WebRTC or OpenHome is underneath. Protocol-specific concerns are isolated to their own endpoints, which the client interacts with only when directed by `streamStatus` event metadata.

**Alternatives considered**:
- Single fat endpoint with all protocol resources: Would create a sprawling profile with mostly-unused resources per device. Violates the existing Barton pattern where profiles have clear, bounded semantics.
- Protocol-only endpoints with no abstract layer: Clients would need technology-specific session logic, duplicating coordination across every protocol. Defeats the purpose of Barton's protocol-agnostic model.

**Endpoint structure per device class**:
```
Camera device (deviceClass: "camera")
├── ep/camera  (profile: "camera")         ← abstract stream lifecycle
│   ├── r/stream          [execute]        ← returns streamId
│   └── r/streamStatus    [events only]    ← traffic controller
│
├── ep/webrtc  (profile: "webrtc")         ← only on WebRTC cameras
│   ├── r/offerSdp             [execute]
│   ├── r/remoteSdp            [events]
│   ├── r/offerIceCandidates   [execute]
│   └── r/remoteIceCandidates  [events]
│
└── ep/openhome  (profile: "openhome")  ← only on OpenHome cameras
    ├── r/getStreamUrl   [execute]
    └── r/streamUrl      [events]

Doorbell Camera device (deviceClass: "doorbellCamera")
├── ep/camera  (profile: "camera")         ← same abstract contract
├── ep/sensor  (profile: "sensor")         ← doorbell button/motion
└── ep/webrtc or ep/openhome          ← protocol-specific
```

### D2: Stream coordination via `updateResource()` event metadata

**Decision**: The `streamStatus` resource emits events with `cJSON *metadata` carrying `streamId` and `nextAction`. This reuses the existing `updateResource(deviceUuid, endpointId, "streamStatus", stateValue, metadata)` mechanism. The `stream` execute resource returns the `streamId` synchronously so the client has a correlation ID before any events fire.

**Rationale**: `updateResource()` with metadata is already proven across the codebase — door lock uses it for lock source/userId, sensors for test fault flags. No new infrastructure is required. The metadata is "over the top" — a JSON convention between driver and client, not enforced by Barton's type system. This matches the team's explicit design decision.

**Stream status values**: `setup`, `done`, `error` — intentionally minimal to work across all camera types regardless of protocol complexity.

**Metadata schema** (JSON convention):
```json
{
  "streamId": "<uint64>",
  "nextAction": "/deviceId/ep/webrtc/r/offerSdp"
}
```

On error:
```json
{
  "streamId": "<uint64>",
  "error": "<description>"
}
```

**Constants** (added to `commonDeviceDefs.h`):
```c
#define CAMERA_STREAM_METADATA_STREAM_ID    "streamId"
#define CAMERA_STREAM_METADATA_NEXT_ACTION  "nextAction"
#define CAMERA_STREAM_METADATA_ERROR        "error"

#define CAMERA_STREAM_STATUS_SETUP  "setup"
#define CAMERA_STREAM_STATUS_DONE   "done"
#define CAMERA_STREAM_STATUS_ERROR  "error"
```

**Alternatives considered**:
- GObject signals for stream state: Would bypass the resource model entirely. Signals are appropriate for non-device-scoped events (system-level), but camera streams are device-scoped and should flow through the resource/event system like all other device state.
- New metadata enforcement mechanism: Would require changes across core, API, and all drivers. Over-engineering for MVP — the JSON convention is sufficient and extensible.
- Granular status values (`negotiating`, `ice-exchange`, `connected`): Too protocol-specific — `negotiating` doesn't apply to OpenHome or direct cameras. `setup`/`done`/`error` works universally.

### D3: Events are the source of truth, not resource values

**Decision**: `streamStatus` is non-readable (no stored value) and emits events unconditionally. The event (with metadata) is what drives client behavior.

**Rationale**: In parallel stream scenarios (future), multiple streams may be in different states simultaneously. A single resource value can't represent this. Events with per-stream metadata can. Even for single-stream MVP, building on events-as-truth ensures the contract doesn't need to change when parallel streams are added.

`updateResource()` supports unconditional event emission for resources registered with `CACHING_POLICY_NEVER` and `RESOURCE_MODE_EMIT_EVENTS` — it skips value comparison entirely because non-cached resources have no previous value to compare against. Register `streamStatus` with this combination and every `updateResource()` call will fire an event regardless of whether the value changed. No core changes are needed.

**Stream flow example (WebRTC)**:
```
Client                        Driver                    Camera
  |                             |                          |
  |── executeResource           |                          |
  |   (stream, {})  ──────►     |                          |
  |   ◄── returns streamId      |                          |
  |                             |── (allocate stream) ───► |
  |                             |                          |
  | ◄── streamStatus event ───  |                          |
  |   value: "setup"            |                          |
  |   meta: {streamId, nextAction: "ep/webrtc/r/offerSdp"} |
  |                             |                          |
  |── executeResource           |                          |
  |   (offerSdp, sdpOffer) ──►  |── (SDP offer) ────────►  |
  |                             |                          |
  |                             | ◄── (SDP answer) ──────  |
  | ◄── remoteSdp event ──────  |                          |
  |   value: "<sdp-answer>"     |   ← protocol endpoint    |
  |                             |                          |
  | ◄── streamStatus event ───  |                          |
  |   value: "setup"            |                          |
  |   meta: {streamId, nextAction: "ep/webrtc/r/offerIceCandidates"}
  |                             |                          |
  |── executeResource           |                          |
  |   (offerIceCandidates) ──►  |── (ICE candidates) ───►  |
  |                             | ◄── (ICE candidates) ──  |
  | ◄── remoteIceCandidates ──  |   ← protocol endpoint    |
  |                             |                          |
  | ◄── streamStatus event ───  |                          |
  |   value: "done"             |                          |
  |   meta: {streamId}          |                          |
```

**Stream flow example (OpenHome)**:
```
Client                        Driver                    Camera
  |                             |                          |
  |── executeResource           |                          |
  |   (stream, {})  ──────►     |                          |
  |   ◄── returns streamId      |                          |
  |                             |                          |
  | ◄── streamStatus event ───  |                          |
  |   value: "setup"            |                          |
  |   meta: {streamId, nextAction: "ep/openhome/r/getStreamUrl"}
  |                             |                          |
  |── executeResource           |                          |
  |   (getStreamUrl) ──────►    |── createMediaTunnel() ─► |
  |                             |                          |
  | ◄── streamUrl event ──────  |   ← protocol endpoint    |
  |   value: "rtsp://..."       |                          |
  |                             |                          |
  | ◄── streamStatus event ───  |                          |
  |   value: "done"             |                          |
  |   meta: {streamId}          |                          |
```

### D4: Driver determines protocol by inspecting endpoint existence

**Decision**: The driver determines which protocol to use based on which protocol-specific endpoints exist on the device. For MVP, this is hardcoded per driver — the Matter camera driver always creates `webrtc` endpoints, the OpenHome camera driver always creates `openhome` endpoints.

**Rationale**: A device's protocol capabilities are a function of its technology. Matter 1.5 cameras always use WebRTC. OpenHome cameras always use media tunnels. There's no runtime negotiation needed for MVP. The endpoint-existence pattern is forward-compatible — a future multi-protocol device could have both endpoints, and the driver (or a negotiation heuristic) could choose.

**Alternatives considered**:
- Protocol field on the device: Would require a new device-level property and introduce a code path distinct from the natural "what endpoints does this device have?" query.
- Client-specified protocol: Clients shouldn't need to know which protocol a camera uses — that defeats protocol agnosticism.

### D5: Backward compatibility with existing OpenHome camera profile

**Decision**: The new stream resources (`stream`, `streamStatus`) are added to the camera endpoint. The `openhome` endpoint is new. Legacy resources (`createMediaTunnel`, `adminUserId`, etc.) are expected to be removed unless there's a compelling reason to keep them — we own the clients and would rather not carry forward ugly naming just for compatibility.

**Rationale**: The team owns all camera clients and can update them alongside the service. A clean break is the default — backward compatibility is only preserved if the cost of removing it is surprisingly high.

**Thread safety**: `updateResource()` with metadata runs on the GLib main loop. Matter subsystem callbacks marshal to the GLib main loop via `g_main_context_invoke` before calling `updateResource()`. This follows the established pattern used by Matter subscription callbacks. No additional threading concerns.

### D6: Matter 1.5 cameras use WebRTC transport

**Decision**: Matter 1.5 introduces camera streaming via WebRTC signaling clusters — `WebRTCTransportProvider` on the commissioner (Barton) and `WebRTCTransportRequestor` on the camera. Barton's Matter camera driver uses this transport, mapping the WebRTC signaling flow onto the stream contract defined in D1–D3.

**Rationale**: WebRTC is the only streaming transport defined in the Matter 1.5 camera specification. There is no alternative to evaluate — this is the protocol.

**Architectural implication**: This requires Barton to host a cluster server for the first time — receiving callbacks from the camera device rather than only sending commands. The detailed signaling flow, cluster server hosting pattern, media stack selection (libdatachannel or alternative), and thread marshalling design are scoped to the `webrtc-endpoint-profile` task.

### D7: Driver self-manages stream lifecycle

**Decision**: There is no `stopStream` resource. The driver owns stream lifecycle — it handles timeouts, device disconnects, and cleanup internally. When a stream ends (for any reason), the driver emits `streamStatus: "done"` (or `"error"`).

**Rationale**: Most streams end naturally (timeout, device disconnect, driver restart). Explicit client-initiated stop is the uncommon case and doesn't warrant a top-level resource. This keeps the interface minimal and puts the driver in control of cleanup.

**Parallel streams**: The interface supports multiple concurrent streams - each `stream` execute returns a unique `streamId`, and all events are correlated by ID. The driver manages each stream independently (timeouts, cleanup). Multiple clients can open streams to the same camera simultaneously.

## Risks / Trade-offs

- **[Metadata is untyped]** → Mitigation: Well-documented JSON schema, string constants in `commonDeviceDefs.h`, and integration tests that verify metadata content. Strongly typed metadata is a future capability.
- **[SBMD composition for Matter cameras]** → Mitigation: The Matter camera driver should be SBMD, but SBMD doesn't currently support class dependencies. The key challenge is making the shared camera stream manager available in the SBMD JS context. This is scoped to the webrtc-endpoint-profile task.
- **[OpenHome adapter complexity]** → Mitigation: The OpenHome driver already implements `createMediaTunnel`. The adapter wraps this in the stream contract — `stream` execute calls `createMediaTunnel` internally and emits `streamStatus` events. Incremental, low-risk change.

## Open Questions

1. **Should `nextAction` be a full URI or a relative path?** Full URI (`/deviceId/ep/webrtc/r/offerSdp`) is more self-contained. Relative path (`ep/webrtc/r/offerSdp`) is shorter. Recommendation: full URI for unambiguity.
2. **What happens to in-flight streams on driver restart?** The driver should emit `streamStatus: "error"` for all active streams on shutdown. Detail in the parallel stream management task.
3. **How does the driver ensure `streamId` is returned before the first `streamStatus` event fires?** The client needs the correlation ID in hand before events arrive. The exact mechanism (scheduled delay, deferred emit, etc.) is TBD at implementation time.
4. **Do clients need explicit stream teardown, and if so, how?** The driver self-manages lifecycle (timeouts, disconnects), so explicit stop may not be necessary. If it is, whether it's a payload on the existing `stream` resource, a separate resource, or something else is TBD.
