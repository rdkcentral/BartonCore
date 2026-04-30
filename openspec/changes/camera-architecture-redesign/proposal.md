## Why

Barton needs camera support across multiple protocols — Matter 1.5 introduces WebRTC-based cameras, while OpenHome cameras use media tunnel URLs. Today the existing `camera` profile in `commonDeviceDefs.h` is tightly coupled to the OpenHome IP camera model (credentials, port numbers, media tunnel functions). There is no shared stream lifecycle contract that works across protocols. Without a protocol-agnostic stream layer, every new camera technology requires bespoke client integration, duplicating stream management logic and preventing a unified camera experience.

The team has agreed on an architecture: an abstract camera endpoint drives stream lifecycle through events, while protocol-specific endpoints carry the signaling details. This keeps the client model clean — clients follow a single stream flow regardless of whether the underlying camera uses WebRTC or OpenHome tunnels.

## What Changes

- **Abstract camera endpoint profile** (`profile: "camera"`): Define two resources on the camera endpoint — `stream` [execute] and `streamStatus` [emit events only, non-readable]. `stream` returns a `streamId` synchronously so the client has a correlation ID before events fire. `streamStatus` is the "traffic controller" that emits lifecycle state transitions (`setup`, `done`, `error`) with metadata including `streamId` and `nextAction` (the next resource the client should interact with). The driver self-manages stream lifecycle (timeouts, cleanup) — there is no separate stop resource.
- **WebRTC endpoint profile** (`profile: "webrtc"`): Protocol-specific endpoint with resources for WebRTC signaling — `offerSdp` [execute], `remoteSdp` [events], `offerIceCandidates` [execute], `remoteIceCandidates` [events]. This endpoint exists only on devices that use WebRTC transport (Matter 1.5 cameras).
- **OpenHome endpoint profile** (`profile: "openhome"`): Protocol-specific endpoint with `getStreamUrl` [execute] and `streamUrl` [events]. This endpoint exists only on devices that use OpenHome media tunnels.
- **Stream coordination via `updateResource()` event metadata**: Reuse the existing `cJSON *metadata` mechanism already used by door lock, sensor, and link quality drivers. The metadata carries stream state as a JSON convention between driver and client — not enforced by Barton's type system. Events are the source of truth for stream state, not resource values. Protocol-specific data (SDP answers, ICE candidates, URLs) is emitted on protocol endpoint resources, not on `streamStatus`.
- **Driver-determined protocol selection**: The driver inspects which protocol endpoints exist on the device to determine the signaling flow. For MVP, this is hardcoded per driver (Matter camera driver → WebRTC, OpenHome camera driver → openhome).
- **Existing camera profile constants**: The existing `CAMERA_PROFILE_*` definitions in `commonDeviceDefs.h` are expected to be removed. We own the clients and would rather not carry forward legacy naming just for compatibility. The clean break decision is confirmed at implementation time once the client migration cost is understood.

## Non-goals

- SBMD composition for Matter cameras needs solving but is out of scope for this proposal
- Strongly typed metadata (enforced by Barton's type system) is a future improvement — metadata is "over the top" JSON convention for now
- Other camera technologies (RTSP, plain HTTP) are not yet designed for, though the pattern should extend to them
- Video rendering, decoding, or media pipeline — clients own that
- Cloud relay or TURN server integration
- Doorbell-specific device types (Video Doorbell 0x0143, Audio Doorbell 0x0141) — future work

## Capabilities

### New Capabilities
- `camera-stream-contract`: The abstract camera endpoint profile with `stream` and `streamStatus` resources, event metadata schema, and stream lifecycle flow. This is the protocol-agnostic core of the design.
- `webrtc-endpoint-profile`: The WebRTC protocol-specific endpoint profile (`offerSdp`, `remoteSdp`, `offerIceCandidates`, `remoteIceCandidates`) and the Matter 1.5 WebRTC signaling flow that drives it.
- `openhome-stream-adapter`: Adaptation of the existing OpenHome camera driver to emit stream events through the new abstract camera endpoint contract, with the `openhome` profile providing `streamUrl`.

### Modified Capabilities
_(none — no existing spec-level requirements are changing)_

## Impact

- **Layers affected**: Public API definitions (`api/c/public/private/commonDeviceDefs.h`, `resourceTypes.h`), core device service (`core/src/deviceService.c` — `updateResource` event metadata path), device drivers (OpenHome camera driver adaptation, new Matter camera driver), Matter subsystem (`core/src/subsystems/matter/` — WebRTC signaling)
- **Existing `camera` profile**: Extended, not replaced. New stream resources coexist with existing OpenHome camera resources.
- **Event metadata contract**: New JSON keys (`streamId`, `nextAction`) added as string constants in `commonDeviceDefs.h`. These are conventions, not type-system enforced.
- **`streamStatus` is non-readable, events only**: Registered with `CACHING_POLICY_NEVER` and `RESOURCE_MODE_EMIT_EVENTS`, which skips value comparison entirely. Every `updateResource()` call fires an event.
- **CMake flags**: `BCORE_MATTER` for Matter camera support. WebRTC media dependencies TBD (to be designed in the webrtc-endpoint-profile capability).
- **Backward compatibility**: Not a goal. We own the clients and would rather not carry forward ugly legacy naming. Clean break is the default — old resources are removed unless there's a compelling reason to keep them.
- **No new external dependencies in this proposal**: libdatachannel and WebRTC media stack decisions are deferred to the webrtc-endpoint-profile capability design.
