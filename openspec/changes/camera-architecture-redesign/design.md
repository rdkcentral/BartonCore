## Context

BartonCore provides protocol-agnostic IoT device management through a URI-based resource model (`/deviceId/ep/N/r/resourceName`). Devices have endpoints with profiles (e.g., `doorLock`, `sensor`, `light`) that define mandatory and optional resources. Drivers communicate state changes via `updateResource()`, which can carry optional `cJSON *metadata` — a pattern already used by door lock (source, userId), sensor (test flag), and link quality drivers.

Barton currently supports cameras only through the OpenHome IP camera model. The existing `camera` profile in `commonDeviceDefs.h` defines resources tightly coupled to that model: credentials (`adminUserId`, `adminPassword`), media tunnels (`createMediaTunnel`, `destroyMediaTunnel`), and snapshot URLs (`pictureURL`). There is no shared session lifecycle that works across camera technologies.

Matter 1.5 introduces WebRTC-based cameras with a fundamentally different signaling pattern: SDP offer/answer exchange, ICE candidate negotiation, and a bidirectional transport flow. The team has agreed that rather than building another technology-specific camera model, Barton should define a protocol-agnostic session contract that both WebRTC and OpenHome cameras (and future technologies) can implement.

## Goals / Non-Goals

**Goals:**
- Define an abstract camera session contract that is independent of any specific streaming protocol
- Support WebRTC (Matter 1.5) and OpenHome cameras through the same client-facing session flow
- Use `updateResource()` event metadata as the session coordination mechanism — no new infrastructure needed
- Keep events as the source of truth for session state, enabling future parallel session support
- Maintain full backward compatibility with existing OpenHome camera clients
- Ensure the pattern extends naturally to future technologies (RTSP, HTTP, etc.)

**Non-Goals:**
- Strongly typed metadata enforcement (future improvement)
- SBMD feasibility evaluation for Matter cameras (separate investigation)
- Parallel session management implementation (the contract supports it; implementation is MVP single-session)
- Video rendering, decoding, or media pipeline integration
- WebRTC media stack selection (libdatachannel vs. alternatives — deferred to webrtc-endpoint-profile design)

## Decisions

### D1: Two-layer endpoint architecture — abstract + protocol-specific

**Decision**: Every camera device has an abstract **camera endpoint** (`profile: "camera"`) that owns session lifecycle, plus one or more **protocol-specific endpoints** that carry the signaling details for a particular technology.

**Rationale**: This separation keeps the client-facing session flow identical regardless of protocol. A client that only cares about "start a camera session and follow the events" never needs to know whether WebRTC or OpenHome is underneath. Protocol-specific concerns are isolated to their own endpoints, which the client interacts with only when directed by `sessionStatus` event metadata.

**Alternatives considered**:
- Single fat endpoint with all protocol resources: Would create a sprawling profile with mostly-unused resources per device. Violates the existing Barton pattern where profiles have clear, bounded semantics.
- Protocol-only endpoints with no abstract layer: Clients would need technology-specific session logic, duplicating coordination across every protocol. Defeats the purpose of Barton's protocol-agnostic model.

**Endpoint structure per device class**:
```
Camera device (deviceClass: "camera")
├── ep/camera  (profile: "camera")         ← abstract session lifecycle
│   ├── r/startSession    [execute]
│   ├── r/stopSession     [execute]
│   └── r/sessionStatus   [events only]    ← traffic controller
│
├── ep/webrtc  (profile: "webrtc")         ← only on WebRTC cameras
│   ├── r/peerSdp              [write]
│   ├── r/cameraSdp            [events]
│   ├── r/peerIceCandidates    [write]
│   └── r/cameraIceCandidates  [events]
│
└── ep/openhome  (profile: "openhome")  ← only on OpenHome cameras
    └── r/tunnelUrl   [events]

Doorbell Camera device (deviceClass: "doorbellCamera")
├── ep/camera  (profile: "camera")         ← same abstract contract
├── ep/sensor  (profile: "sensor")         ← doorbell button/motion
└── ep/webrtc or ep/openhome          ← protocol-specific
```

### D2: Session coordination via `updateResource()` event metadata

**Decision**: The `sessionStatus` resource emits events with `cJSON *metadata` carrying `sessionId`, `technology`, and `nextAction`. This reuses the existing `updateResource(deviceUuid, endpointId, "sessionStatus", stateValue, metadata)` mechanism.

**Rationale**: `updateResource()` with metadata is already proven across the codebase — door lock uses it for lock source/userId, sensors for test fault flags. No new infrastructure is required. The metadata is "over the top" — a JSON convention between driver and client, not enforced by Barton's type system. This matches the team's explicit design decision.

**Metadata schema** (JSON convention):
```json
{
  "sessionId": "uuid-string",
  "technology": "webrtc" | "openhome",
  "nextAction": "/deviceId/ep/webrtc/r/peerSdp"
}
```

**Constants** (added to `commonDeviceDefs.h`):
```c
#define CAMERA_SESSION_METADATA_SESSION_ID   "sessionId"
#define CAMERA_SESSION_METADATA_TECHNOLOGY   "technology"
#define CAMERA_SESSION_METADATA_NEXT_ACTION  "nextAction"
```

**Alternatives considered**:
- GObject signals for session state: Would bypass the resource model entirely. Signals are appropriate for non-device-scoped events (system-level), but camera sessions are device-scoped and should flow through the resource/event system like all other device state.
- New metadata enforcement mechanism: Would require changes across core, API, and all drivers. Over-engineering for MVP — the JSON convention is sufficient and extensible.

### D3: Events are the source of truth, not resource values

**Decision**: `sessionStatus` must emit events even when the underlying value doesn't change. The event (with metadata) is what drives client behavior. The stored resource value is secondary.

**Rationale**: In parallel session scenarios (future), multiple sessions may be in different states simultaneously. A single resource value can't represent this. Events with per-session metadata can. Even for single-session MVP, building on events-as-truth ensures the contract doesn't need to change when parallel sessions are added.

`updateResource()` supports unconditional event emission for resources registered with `CACHING_POLICY_NEVER` and `RESOURCE_MODE_EMIT_EVENTS` — it skips value comparison entirely because non-cached resources have no previous value to compare against. Register `sessionStatus` with this combination and every `updateResource()` call will fire an event regardless of whether the value changed. No core changes are needed.

**Session flow example (WebRTC)**:
```
Client                        Driver                    Camera
  |                             |                          |
  |── executeResource           |                          |
  |   (startSession, {})  ──►   |                          |
  |                             |── (allocate stream) ───► |
  |                             |                          |
  | ◄── sessionStatus event ──  |                          |
  |   value: "offering"         |                          |
  |   meta: {sessionId: "abc", technology: "webrtc",       |
  |          nextAction: "/dev/ep/webrtc/r/peerSdp"}       |
  |                             |                          |
  |── writeResource             |                          |
  |   (peerSdp, sdpOffer) ──►   |── (SDP offer) ────────►  |
  |                             |                          |
  |                             | ◄── (SDP answer) ──────  |
  | ◄── cameraSdp event ──────  |                          |
  |   value: "<sdp-answer>"     |                          |
  |                             |                          |
  | ◄── sessionStatus event ──  |                          |
  |   value: "ice-exchange"     |                          |
  |   meta: {nextAction: "/dev/ep/webrtc/r/peerIceCandidates"}
  |                             |                          |
  |── writeResource             |                          |
  |   (peerIceCandidates) ──►   |── (ICE candidates) ───►  |
  |                             | ◄── (ICE candidates) ──  |
  | ◄── cameraIceCandidates ──  |                          |
  |                             |                          |
  | ◄── sessionStatus event ──  |                          |
  |   value: "connected"        |                          |
  |   meta: {sessionId: "abc"}  |                          |
```

**Session flow example (OpenHome)**:
```
Client                        Driver                    Camera
  |                             |                          |
  |── executeResource           |                          |
  |   (startSession, {})  ──►   |                          |
  |                             |── createMediaTunnel() ─► |
  |                             |                          |
  | ◄── sessionStatus event ──  |                          |
  |   value: "connected"        |                          |
  |   meta: {sessionId: "abc", technology: "openhome",|
  |          nextAction: "/dev/ep/openhome/r/tunnelUrl"}
  |                             |                          |
  | ◄── tunnelUrl event ──────  |                          |
  |   value: "rtsp://..."       |                          |
```

### D4: Driver determines protocol by inspecting endpoint existence

**Decision**: The driver determines which protocol to use based on which protocol-specific endpoints exist on the device. For MVP, this is hardcoded per driver — the Matter camera driver always creates `webrtc` endpoints, the OpenHome camera driver always creates `openhome` endpoints.

**Rationale**: A device's protocol capabilities are a function of its technology. Matter 1.5 cameras always use WebRTC. OpenHome cameras always use media tunnels. There's no runtime negotiation needed for MVP. The endpoint-existence pattern is forward-compatible — a future multi-protocol device could have both endpoints, and the driver (or a negotiation heuristic) could choose.

**Alternatives considered**:
- Protocol field on the device: Would require a new device-level property and introduce a code path distinct from the natural "what endpoints does this device have?" query.
- Client-specified protocol: Clients shouldn't need to know which protocol a camera uses — that defeats protocol agnosticism.

### D5: Backward compatibility with existing OpenHome camera profile

**Decision**: The existing `CAMERA_PROFILE` resources (`portNumber`, `adminUserId`, `createMediaTunnel`, etc.) remain on the camera endpoint unchanged. The new session resources (`startSession`, `stopSession`, `sessionStatus`) are added alongside them. The `openhome` endpoint is new.

**Rationale**: Existing OpenHome camera clients call `createMediaTunnel` directly. Removing those resources would be a breaking change. The new session contract is additive — the OpenHome camera driver is adapted to emit `sessionStatus` events when `startSession` is called, but the old `createMediaTunnel` path continues to work.

**Thread safety**: `updateResource()` with metadata runs on the GLib main loop. Matter subsystem callbacks marshal to the GLib main loop via `g_main_context_invoke` before calling `updateResource()`. This follows the established pattern used by Matter subscription callbacks. No additional threading concerns.

### D6: Matter 1.5 cameras use WebRTC transport

**Decision**: Matter 1.5 introduces camera streaming via WebRTC signaling clusters — `WebRTCTransportProvider` on the commissioner (Barton) and `WebRTCTransportRequestor` on the camera. Barton's Matter camera driver uses this transport, mapping the WebRTC signaling flow onto the session contract defined in D1–D3.

**Rationale**: WebRTC is the only streaming transport defined in the Matter 1.5 camera specification. There is no alternative to evaluate — this is the protocol.

**Architectural implication**: This requires Barton to host a cluster server for the first time — receiving callbacks from the camera device rather than only sending commands. The detailed signaling flow, cluster server hosting pattern, media stack selection (libdatachannel or alternative), and thread marshalling design are scoped to the `webrtc-endpoint-profile` task.

## Risks / Trade-offs

- **[Metadata is untyped]** → Mitigation: Well-documented JSON schema, string constants in `commonDeviceDefs.h`, and integration tests that verify metadata content. Strongly typed metadata is a future capability.
- **[SBMD may not support the session pattern]** → Mitigation: SBMD feasibility for Matter cameras is explicitly out of scope. The Matter camera driver may need to be a native C/C++ driver rather than SBMD. This is evaluated in the webrtc-endpoint-profile task.
- **[Parallel sessions deferred]** → Mitigation: The contract is designed for parallel sessions (sessionId in metadata, events as source of truth). Only the implementation is single-session MVP. No contract changes needed later.
- **[OpenHome adapter complexity]** → Mitigation: The OpenHome driver already implements `createMediaTunnel`. The adapter wraps this in the session contract — `startSession` calls `createMediaTunnel` internally and emits `sessionStatus` events. Incremental, low-risk change.

## Open Questions

1. **Should `sessionStatus` values be an enum (string constants) or free-form?** Current design uses string values like `"offering"`, `"ice-exchange"`, `"connected"`, `"disconnected"`, `"error"`. Constants in `commonDeviceDefs.h` are recommended.
2. **What happens to in-flight sessions on driver restart?** The driver should emit `sessionStatus: "disconnected"` for all active sessions on shutdown. Detail in the parallel session management task.
3. **Should `nextAction` be a full URI or a relative path?** Full URI (`/deviceId/ep/webrtc/r/peerSdp`) is more self-contained. Relative path is shorter. Recommendation: full URI for unambiguity.
