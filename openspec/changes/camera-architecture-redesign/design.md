## Context

BartonCore provides protocol-agnostic IoT device management through a URI-based resource model (`/deviceId/ep/<endpointId>/r/<resourceName>`). Devices have endpoints with profiles (e.g., `doorLock`, `sensor`, `light`) that define mandatory and optional resources. Drivers communicate state changes via `updateResource()`, which can carry optional `cJSON *metadata` — a pattern already used by door lock (source, userId), sensor (test flag), and link quality drivers.

Barton currently supports cameras only through the OpenHome IP camera model. The existing `camera` profile in `commonDeviceDefs.h` defines resources tightly coupled to that model: credentials (`adminUserId`, `adminPassword`), media tunnels (`createMediaTunnel`, `destroyMediaTunnel`), and snapshot URLs (`pictureURL`). There is no shared session lifecycle that works across camera technologies.

Matter 1.5 introduces WebRTC-based cameras with a fundamentally different signaling pattern: SDP offer/answer exchange, ICE candidate negotiation, and a bidirectional transport flow. The team has agreed that rather than building another technology-specific camera model, Barton should define a protocol-agnostic session contract that both WebRTC and OpenHome cameras (and future technologies) can implement.

## Goals / Non-Goals

**Goals:**
- Define an abstract camera session contract that is independent of any specific streaming protocol
- Support WebRTC (Matter 1.5) and OpenHome cameras through the same client-facing session flow
- Support local-only cameras that require no negotiation (e.g., ONVIF, Reolink) — the contract handles both multi-step signaling and immediate URL delivery
- Design the WebRTC endpoint profile to be reusable beyond cameras (e.g., ssh, binary transfer, audio streaming)
- Use `updateResource()` event metadata as the session coordination mechanism — no new infrastructure needed
- Keep events as the source of truth for session state, with per-session correlation via `sessionId`
- Provide a clear migration path for existing OpenHome camera clients as legacy OpenHome-specific resources are deprecated or removed by default
- Ensure the pattern extends naturally to future technologies (RTSP, HTTP, etc.)

**Non-Goals:**
- Strongly typed metadata enforcement (future improvement)
- SBMD feasibility evaluation for Matter cameras (separate investigation)
- Video rendering, decoding, or media pipeline integration
- WebRTC media stack selection (libdatachannel vs. alternatives — deferred to webrtc-endpoint-profile design)
- Broadening actions beyond streaming — the camera endpoint currently defines `stream` and `takePicture`. Future actions (e.g., PTZ control, two-way audio) may be added to the profile as needed. The session contract supports this naturally since each action triggers its own `sessionStatus` flow.

## Decisions

### D1: Two-layer endpoint architecture — abstract + protocol-specific

**Decision**: Every camera device has an abstract **camera endpoint** (`profile: "camera"`) that owns session lifecycle, plus one or more **protocol-specific endpoints** that carry the signaling details for a particular technology.

**Rationale**: This separation keeps the client-facing session flow identical regardless of protocol. A client that only cares about "give me a session and follow the events" never needs to know whether WebRTC or OpenHome is underneath. Protocol-specific concerns are isolated to their own endpoints, which the client interacts with only when directed by `sessionStatus` event metadata.

**Alternatives considered**:
- Single fat endpoint with all protocol resources: Would create a sprawling profile with mostly-unused resources per device. Violates the existing Barton pattern where profiles have clear, bounded semantics.
- Protocol-only endpoints with no abstract layer: Clients would need technology-specific session logic, duplicating coordination across every protocol. Defeats the purpose of Barton's protocol-agnostic model.

**Endpoint structure per device class**:
```
Camera device (deviceClass: "camera")
├── ep/camera  (profile: "camera")         ← abstract session lifecycle
│   ├── r/createSession   [execute]        ← returns sessionId
│   ├── r/stream          [execute]        ← start streaming (takes sessionId)
│   ├── r/takePicture     [execute]        ← take snapshot (takes sessionId)
│   ├── r/destroySession  [execute]        ← client cleanup
│   └── r/sessionStatus   [events only]    ← traffic controller
│
├── ep/webrtc  (profile: "webrtc")         ← only on WebRTC cameras
│   ├── r/offerSdp             [execute]   ← client sends SDP offer
│   ├── r/remoteSdp            [events]    ← remote SDP answer delivered here
│   ├── r/offerIceCandidates   [execute]   ← client sends ICE candidates
│   └── r/remoteIceCandidates  [events]    ← remote ICE candidates delivered here
│
├── ep/openhome  (profile: "openhome")     ← only on OpenHome cameras
│   ├── r/getMediaUrl    [execute]         ← triggers media tunnel creation
│   └── r/mediaUrl       [events]          ← URL delivered here
│
└── ep/direct  (profile: "direct")         ← only on local/direct cameras
    ├── r/getMediaUrl    [execute]         ← triggers immediate event
    └── r/mediaUrl       [events]          ← URL delivered here

Doorbell Camera device (deviceClass: "doorbellCamera")
├── ep/camera  (profile: "camera")         ← same abstract contract
├── ep/sensor  (profile: "sensor")         ← doorbell button/motion
└── ep/webrtc or ep/openhome               ← protocol-specific
```

### D2: Session coordination via `updateResource()` event metadata

**Decision**: The `sessionStatus` resource emits events with `cJSON *metadata` carrying `sessionId`, `protocol`, and `nextAction`. This reuses the existing `updateResource(deviceUuid, endpointId, "sessionStatus", stateValue, metadata)` mechanism. The `createSession` execute resource returns the `sessionId` synchronously so the client has a correlation ID before any events fire.

**Rationale**: `updateResource()` with metadata is already proven across the codebase — door lock uses it for lock source/userId, sensors for test fault flags. No new infrastructure is required. The metadata is "over the top" — a JSON convention between driver and client, not enforced by Barton's type system. This matches the team's explicit design decision.

**Session status values**: `setup`, `done`, `error` — intentionally minimal to work across all camera types regardless of protocol complexity.

**Metadata schema** (JSON convention — `sessionId` is a driver-generated unique ID, scoped per device):
```json
{
  "sessionId": 1,
  "protocol": "webrtc",
  "nextAction": "/deviceId/ep/webrtc/r/offerSdp"
}
```

On error:
```json
{
  "sessionId": 1,
  "error": "<description>"
}
```

`protocol` tells the client what technology is in use (e.g., `"webrtc"`, `"openhome"`, `"direct"`). This allows clients to abort early if they don't support the protocol, without having to inspect the data model.

**Constants** (added to `commonDeviceDefs.h`):
```c
#define CAMERA_SESSION_METADATA_SESSION_ID  "sessionId"
#define CAMERA_SESSION_METADATA_PROTOCOL    "protocol"
#define CAMERA_SESSION_METADATA_NEXT_ACTION "nextAction"
#define CAMERA_SESSION_METADATA_ERROR       "error"

#define CAMERA_SESSION_STATUS_SETUP  "setup"
#define CAMERA_SESSION_STATUS_DONE   "done"
#define CAMERA_SESSION_STATUS_ERROR  "error"
```

**Alternatives considered**:
- GObject signals for session state: Would bypass the resource model entirely. Signals are appropriate for non-device-scoped events (system-level), but camera sessions are device-scoped and should flow through the resource/event system like all other device state.
- New metadata enforcement mechanism: Would require changes across core, API, and all drivers. Over-engineering for MVP — the JSON convention is sufficient and extensible.
- Granular status values (`negotiating`, `ice-exchange`, `connected`): Too protocol-specific — `negotiating` doesn't apply to OpenHome or direct cameras. `setup`/`done`/`error` works universally.

### D3: Events are the source of truth, not resource values

**Decision**: `sessionStatus` is non-readable (no stored value) and emits events unconditionally. The event (with metadata) is what drives client behavior.

**Rationale**: Multiple sessions may be in different states simultaneously. A single resource value can't represent this. Events with per-session metadata can. The contract inherently supports parallel sessions without special handling.

`updateResource()` supports unconditional event emission for resources registered with `CACHING_POLICY_NEVER` and `RESOURCE_MODE_EMIT_EVENTS` — it skips value comparison entirely because non-cached resources have no previous value to compare against. Register `sessionStatus` with this combination and every `updateResource()` call will fire an event regardless of whether the value changed. No core changes are needed.

**Session flow example (WebRTC)**:
```
Client                        Driver                    Camera
  |                             |                          |
  |── executeResource           |                          |
  |   (createSession, {}) ───►  |                          |
  |   ◄── returns sessionId     |                          |
  |                             |                          |
  |── executeResource           |                          |
  |   (stream, {sessionId}) ──► |                          |
  |                             |                          |
  | ◄── sessionStatus event ──  |                          |
  |   value: "setup"            |                          |
  |   meta: {sessionId, protocol: "webrtc", nextAction: "/deviceId/ep/webrtc/r/offerSdp"}
  |                             |                          |
  |── executeResource           |                          |
  |   (offerSdp, sdpOffer) ──►  |── (SDP offer) ────────►  |
  |                             |                          |
  |                             | ◄── (SDP answer) ──────  |
  | ◄── remoteSdp event ──────  |                          |
  |   value: "<sdp-answer>"     |   ← protocol endpoint    |
  |                             |                          |
  | ◄── sessionStatus event ──  |                          |
  |   value: "setup"            |                          |
  |   meta: {sessionId, protocol: "webrtc", nextAction: "/deviceId/ep/webrtc/r/offerIceCandidates"}
  |                             |                          |
  |── executeResource           |                          |
  |   (offerIceCandidates) ──►  |── (ICE candidates) ───►  |
  |                             | ◄── (ICE candidates) ──  |
  | ◄── remoteIceCandidates ──  |   ← protocol endpoint    |
  |                             |                          |
  | ◄── sessionStatus event ──  |                          |
  |   value: "done"             |                          |
  |   meta: {sessionId}         |                          |
  |                             |                          |
  |── executeResource           |                          |
  |   (destroySession, {sessionId}) ──► (cleanup)          |
```

**Session flow example (OpenHome)**:
```
Client                        Driver                    Camera
  |                             |                          |
  |── executeResource           |                          |
  |   (createSession, {}) ───►  |                          |
  |   ◄── returns sessionId     |                          |
  |                             |                          |
  |── executeResource           |                          |
  |   (stream, {sessionId}) ──► |                          |
  |                             |                          |
  | ◄── sessionStatus event ──  |                          |
  |   value: "setup"            |                          |
  |   meta: {sessionId, protocol: "openhome", nextAction: "/deviceId/ep/openhome/r/getMediaUrl"}
  |                             |                          |
  |── executeResource           |                          |
  |   (getMediaUrl) ───────►    |── createMediaTunnel() ─► |
  |                             |                          |
  | ◄── mediaUrl event ───────  |   ← protocol endpoint    |
  |   value: "rtsp://..."       |                          |
  |                             |                          |
  | ◄── sessionStatus event ──  |                          |
  |   value: "done"             |                          |
  |   meta: {sessionId}         |                          |
  |                             |                          |
  |── executeResource           |                          |
  |   (destroySession, {sessionId}) ──► (cleanup)          |
```

**Session flow example (Local/Direct — simple URL return)**:
```
Client                        Driver                    Camera
  |                             |                          |
  |── executeResource           |                          |
  |   (createSession, {}) ───►  |                          |
  |   ◄── returns sessionId     |                          |
  |                             |                          |
  |── executeResource           |                          |
  |   (stream, {sessionId}) ──► |                          |
  |                             |                          |
  | ◄── sessionStatus event ──  |                          |
  |   value: "setup"            |                          |
  |   meta: {sessionId, protocol: "direct", nextAction: "/deviceId/ep/direct/r/getMediaUrl"}
  |                             |                          |
  |── executeResource           |                          |
  |   (getMediaUrl) ───────►    |   (URL already known)    |
  |                             |                          |
  | ◄── mediaUrl event ───────  |                          |
  |   value: "rtsp://..."       |                          |
  |                             |                          |
  | ◄── sessionStatus event ──  |                          |
  |   value: "done"             |                          |
  |   meta: {sessionId}         |                          |
  |                             |                          |
  |── executeResource           |                          |
  |   (destroySession, {sessionId}) ──► (cleanup)          |
```

### D4: One driver per camera protocol, each owns its endpoint profile

**Decision**: Each camera technology has its own driver. The Matter camera driver creates `webrtc` endpoints. The OpenHome camera driver creates `openhome` endpoints. A local/ONVIF driver creates `direct` endpoints. Each driver creates the abstract `camera` endpoint (with `createSession`/`destroySession`/`sessionStatus`) plus its own protocol-specific endpoint — that's it.

**Rationale**: A device's protocol capabilities are a function of its technology. There is no single "camera driver" that supports multiple protocols — that would be a monolith. Each driver is scoped to one protocol and creates exactly the endpoints relevant to that protocol. The endpoint existence on a device is a natural consequence of which driver manages it, not a runtime decision.

**Multi-protocol devices**: If a device supports multiple protocols (e.g., a camera that can do both WebRTC and direct URL access), it could have multiple protocol endpoints created by its driver. The client doesn't need to choose — `sessionStatus` metadata tells it which protocol is active for a given session.

### D5: Legacy camera profile removal

**Decision**: The new session resources (`createSession`, `destroySession`, `sessionStatus`) are added to the camera endpoint. The `openhome` endpoint is new. Legacy resources (`createMediaTunnel`, `adminUserId`, etc.) are expected to be removed unless there's a compelling reason to keep them — we own the clients and would rather not carry forward ugly naming just for compatibility.

**Rationale**: The team owns all camera clients and can update them alongside the service. A clean break is the default — backward compatibility is only preserved if the cost of removing it is surprisingly high.

**Thread safety**: `updateResource()` with metadata runs on the GLib main loop. Matter subsystem callbacks marshal to the GLib main loop via `g_main_context_invoke` before calling `updateResource()`. This follows the established pattern used by Matter subscription callbacks. No additional threading concerns.

### D6: Matter 1.5 cameras use WebRTC transport

**Decision**: Matter 1.5 introduces camera streaming via WebRTC signaling clusters — `WebRTCTransportProvider` on the controller (Barton) and `WebRTCTransportRequestor` on the camera. Barton's Matter camera driver uses this transport, mapping the WebRTC signaling flow onto the session contract defined in D1–D3.

**Rationale**: WebRTC is the only streaming transport defined in the Matter 1.5 camera specification. There is no alternative to evaluate — this is the protocol.

**Architectural implication**: This requires Barton to host a cluster server for the first time — receiving callbacks from the camera device rather than only sending commands. The detailed signaling flow, cluster server hosting pattern, media stack selection (libdatachannel or alternative), and thread marshalling design are scoped to the `webrtc-endpoint-profile` task.

### D7: OpenHome cameras use media tunnel transport

**Decision**: The OpenHome camera driver maps the existing `createMediaTunnel` mechanism onto the session contract. When a client executes `getMediaUrl` on the `openhome` endpoint, the driver internally calls `createMediaTunnel()` on the camera. The resulting URL is emitted as a `mediaUrl` event on the `openhome` endpoint. The driver emits `sessionStatus` events to orchestrate the flow.

**Rationale**: OpenHome cameras already have a working media tunnel mechanism in the existing driver. The session contract wraps this — it doesn't replace the underlying protocol. The adaptation is a thin layer: translate `createSession` into internal session tracking, `getMediaUrl` into `createMediaTunnel()`, and emit the appropriate `sessionStatus` events at each step.

**Migration**: The migration approach for the existing OpenHome camera driver is TBD — whether it's ported to Barton or adapted in the existing client codebase is a separate decision.

### D8: Local/direct cameras use immediate URL delivery

**Decision**: Local cameras (ONVIF, Reolink, plain RTSP/HTTP) use the same session contract as all other cameras. Their driver creates an `ep/direct` endpoint with `getMediaUrl` [execute] and `mediaUrl` [events]. When the client executes `getMediaUrl`, the URL is already known — the driver emits the `mediaUrl` event immediately.

**Rationale**: The team acknowledged this is clunky for "just returning a URL" but consistency wins. Clients follow the same flow regardless of protocol: `createSession` → follow `sessionStatus` → execute `nextAction` → receive result → `destroySession`. No special-casing in client code.

**Credentials**: How the client authenticates to a local camera URL is an open question. Options include embedding credentials in the URL (query params or userinfo), separate resources on the `direct` endpoint (e.g., `username`, `password`), or a configuration-time setup that doesn't involve the session flow at all. This is TBD and may vary by camera vendor.

**No server-side stream management**: Unlike WebRTC or OpenHome, the driver for a local camera has no involvement in the media stream itself. Once the URL is handed to the client, the client connects directly — the driver doesn't manage, monitor, or terminate the stream. `destroySession` simply releases the Barton-side session state; it doesn't tear down a camera-side connection.

### D9: Client-managed session lifecycle with required cleanup

**Decision**: Clients are required to call `destroySession` when they are done with a session. The driver cleans up any allocated resources (camera-side sessions, internal state) when `destroySession` is called. The driver also emits `sessionStatus: "done"` or `"error"` when sessions end due to external factors (device disconnect, protocol error). If a client never calls `destroySession`, resources may leak — this is a contract violation.

**Rationale**: Sessions allocate resources that need explicit cleanup. The driver cannot reliably determine when a client is "done" without being told. This is a standard acquire/release contract. Higher layers (cloud integration, etc.) may add timeout-based cleanup, but Barton itself does not assume automatic cleanup.

**Parallel sessions**: The interface supports multiple concurrent sessions - each `createSession` execute returns a unique `sessionId`, and all events are correlated by ID. The driver manages each session independently. Multiple clients can open sessions to the same camera simultaneously.

**Active session enumeration**: Clients need a way to list current active sessions (e.g., for recovery after crash or for remote clients). The mechanism for this is TBD.

**Error behavior**: The exact error semantics (does the driver emit `"error"` and then auto-invalidate the session, or must the client still call `destroySession`?), creation failure behavior (does a failed `createSession` return a sessionId at all?), and timeout/leak-prevention strategies are implementation-time decisions.

## Risks / Trade-offs

- **[Metadata is untyped]** → Mitigation: Well-documented JSON schema, string constants in `commonDeviceDefs.h`, and integration tests that verify metadata content. Strongly typed metadata is a future capability.
- **[SBMD composition for Matter cameras]** → Mitigation: The Matter camera driver should be SBMD, but SBMD doesn't currently support class dependencies. The key challenge is making the shared camera session manager available in the SBMD JS context. This is scoped to the webrtc-endpoint-profile task.
- **[OpenHome adapter complexity]** → Mitigation: The OpenHome driver already implements `createMediaTunnel`. The adapter wraps this in the session contract: `createSession` establishes Barton session state and emits `sessionStatus` events, while `getMediaUrl` initiates the existing OpenHome `createMediaTunnel` flow for that session and returns the resulting URL. Incremental, low-risk change.

## Open Questions

1. **What happens to in-flight sessions on driver restart?** Likely: drivers that manage server-side streams (WebRTC, OpenHome) emit `sessionStatus: "error"` for all active sessions on shutdown; local/direct drivers simply discard session state since there's nothing to tear down. Exact behavior TBD at implementation time.
2. **How does the driver ensure `sessionId` is returned before the first `sessionStatus` event fires?** The client needs the correlation ID in hand before events arrive. The exact mechanism (scheduled delay, deferred emit, etc.) is TBD at implementation time.
3. **How do clients enumerate active sessions?** Needed for crash recovery and remote client scenarios. Whether this is a readable resource, an execute that returns a list, or something else is TBD.
