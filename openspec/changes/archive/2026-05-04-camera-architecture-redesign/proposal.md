## Why

Barton needs camera support across multiple protocols — Matter 1.5 introduces WebRTC-based cameras, while OpenHome cameras use media tunnel URLs. Today the existing `camera` profile in `commonDeviceDefs.h` is tightly coupled to the OpenHome IP camera model (credentials, port numbers, media tunnel functions). There is no shared session lifecycle contract that works across protocols. Without a protocol-agnostic session layer, every new camera technology requires bespoke client integration, duplicating session management logic and preventing a unified camera experience.

The team has agreed on an architecture: an abstract camera endpoint drives session lifecycle through events, while protocol-specific endpoints carry the signaling details. This keeps the client model clean — clients follow a single session flow regardless of whether the underlying camera uses WebRTC or OpenHome tunnels.

## What Changes

- **Abstract camera endpoint profile** (`profile: "camera"`): Define three resources on the camera endpoint — `createSession` [execute], `destroySession` [execute], and `sessionStatus` [emit events only, non-readable]. `createSession` returns a `sessionId` synchronously so the client has a correlation ID before events fire. `sessionStatus` is the "traffic controller" that emits lifecycle state transitions (`setup`, `done`, `error`) with metadata including `sessionId`, `protocol` (the technology in use), and `nextAction` (the next resource the client should interact with). Clients are required to call `destroySession` when done.
- **WebRTC endpoint profile** (`profile: "webrtc"`): Protocol-specific endpoint with resources for WebRTC signaling — `offerSdp` [execute], `remoteSdp` [events], `offerIceCandidates` [execute], `remoteIceCandidates` [events]. This endpoint exists only on devices that use WebRTC transport (Matter 1.5 cameras).
- **OpenHome endpoint profile** (`profile: "openhome"`): Protocol-specific endpoint with `getMediaUrl` [execute] and `mediaUrl` [events]. This endpoint exists only on devices that use OpenHome media tunnels.
- **Direct endpoint profile** (`profile: "direct"`): Protocol-specific endpoint for local cameras (ONVIF, Reolink, plain RTSP/HTTP) with `getMediaUrl` [execute] and `mediaUrl` [events]. Same resource pattern as OpenHome — the URL is simply available immediately without async negotiation.
- **Session coordination via `updateResource()` event metadata**: Reuse the existing `cJSON *metadata` mechanism already used by door lock, sensor, and link quality drivers. The metadata carries session state as a JSON convention between driver and client — not enforced by Barton's type system. Events are the source of truth for session state, not resource values. Protocol-specific data (SDP answers, ICE candidates, URLs) is emitted on protocol endpoint resources, not on `sessionStatus`.
- **Driver-determined protocol selection**: The driver inspects which protocol endpoints exist on the device to determine the signaling flow. For MVP, this is hardcoded per driver (Matter camera driver → WebRTC, OpenHome camera driver → openhome). The `protocol` field in session metadata tells the client which technology is in use without requiring data model inspection.
- **Existing camera profile constants**: The existing `CAMERA_PROFILE_*` definitions in `commonDeviceDefs.h` are expected to be removed. We own the clients and would rather not carry forward legacy naming just for compatibility. The clean break decision is confirmed at implementation time once the client migration cost is understood.

## Non-goals

- SBMD composition for Matter cameras needs solving but is out of scope for this proposal
- Video rendering, decoding, or media pipeline — clients own that
- Cloud relay or TURN server integration

## Capabilities

### New Capabilities
- `camera-session-contract`: The abstract camera endpoint profile with `createSession`, `destroySession`, and `sessionStatus` resources, event metadata schema, and session lifecycle flow. This is the protocol-agnostic core of the design.
- `webrtc-endpoint-profile`: The WebRTC protocol-specific endpoint profile (`offerSdp`, `remoteSdp`, `offerIceCandidates`, `remoteIceCandidates`) and the Matter 1.5 WebRTC signaling flow that drives it.
- `openhome-stream-adapter`: Adaptation of the existing OpenHome camera driver to emit session events through the new abstract camera endpoint contract, with the `openhome` profile providing `mediaUrl`.

### Modified Capabilities
_(none — no existing spec-level requirements are changing)_

## Impact

- **Layers affected**: Public API definitions (`api/c/public/private/commonDeviceDefs.h`, `resourceTypes.h`), core device service (`core/src/deviceService.c` — `updateResource` event metadata path), device drivers (OpenHome camera driver adaptation, new Matter camera driver), Matter subsystem (`core/src/subsystems/matter/` — WebRTC signaling)
- **Existing `camera` profile**: Extended, not replaced. New session resources coexist with existing OpenHome camera resources.
- **Event metadata contract**: New JSON keys (`sessionId`, `protocol`, `nextAction`) added as string constants in `commonDeviceDefs.h`. These are conventions, not type-system enforced.
- **`sessionStatus` is non-readable, events only**: Registered with `CACHING_POLICY_NEVER` and `RESOURCE_MODE_EMIT_EVENTS`, which skips value comparison entirely. Every `updateResource()` call fires an event.
- **CMake flags**: `BCORE_MATTER` for Matter camera support. WebRTC media dependencies TBD (to be designed in the webrtc-endpoint-profile capability).
- **Backward compatibility**: Not a goal. We own the clients and would rather not carry forward ugly legacy naming. Clean break is the default — old resources are removed unless there's a compelling reason to keep them.
- **No new external dependencies in this proposal**: libdatachannel and WebRTC media stack decisions are deferred to the webrtc-endpoint-profile capability design.
