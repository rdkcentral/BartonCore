## Why

Barton needs camera support across multiple protocols — Matter 1.5 introduces WebRTC-based cameras, while OpenHome cameras use media tunnel URLs. Today the existing `camera` profile in `commonDeviceDefs.h` is tightly coupled to the OpenHome IP camera model (credentials, port numbers, media tunnel functions). There is no shared session lifecycle contract that works across protocols. Without a protocol-agnostic session layer, every new camera technology requires bespoke client integration, duplicating session management logic and preventing a unified camera experience.

The team has agreed on an architecture: an abstract camera endpoint drives session lifecycle through events, while protocol-specific endpoints carry the signaling details. This keeps the client model clean — clients follow a single session flow regardless of whether the underlying camera uses WebRTC or OpenHome tunnels.

## What Changes

- **Abstract camera endpoint profile** (`profile: "camera"`): Define three resources on the camera endpoint — `startSession` [execute], `stopSession` [execute], and `sessionStatus` [emit events only]. `sessionStatus` is the "traffic controller" that directs clients through the session flow. Events carry metadata with `sessionId`, `technology`, and `nextAction` (full URI to the next resource the client should interact with).
- **WebRTC endpoint profile** (`profile: "webrtc"`): Protocol-specific endpoint with resources for WebRTC signaling — `peerSdp` [write], `cameraSdp` [events], `peerIceCandidates` [write], `cameraIceCandidates` [events]. This endpoint exists only on devices that use WebRTC transport (Matter 1.5 cameras).
- **OpenHome endpoint profile** (`profile: "openhome"`): Protocol-specific endpoint with `tunnelUrl` [events]. This endpoint exists only on devices that use OpenHome media tunnels.
- **Session coordination via `updateResource()` event metadata**: Reuse the existing `cJSON *metadata` mechanism already used by door lock, sensor, and link quality drivers. The metadata carries session state as a JSON convention between driver and client — not enforced by Barton's type system. Events are the source of truth for session state, not resource values.
- **Driver-determined protocol selection**: The driver inspects which protocol endpoints exist on the device to determine the signaling flow. For MVP, this is hardcoded per driver (Matter camera driver → WebRTC, OpenHome camera driver → openhome).
- **Existing camera profile constants retained**: The existing `CAMERA_PROFILE_*` definitions in `commonDeviceDefs.h` remain for backward compatibility with OpenHome cameras. The new session contract adds to — not replaces — the existing camera profile.

## Non-goals

- SBMD feasibility for Matter cameras needs evaluation but is out of scope for this proposal
- Strongly typed metadata (enforced by Barton's type system) is a future improvement — metadata is "over the top" JSON convention for now
- Other camera technologies (RTSP, plain HTTP) are not yet designed for, though the pattern should extend to them
- Video rendering, decoding, or media pipeline — clients own that
- Cloud relay or TURN server integration
- Doorbell-specific device types (Video Doorbell 0x0143, Audio Doorbell 0x0141) — future work
- Parallel session management design (future — the contract supports it via session IDs, but initial implementation is single-session)

## Capabilities

### New Capabilities
- `camera-session-contract`: The abstract camera endpoint profile with `startSession`, `stopSession`, and `sessionStatus` resources, event metadata schema, and session lifecycle flow. This is the protocol-agnostic core of the design.
- `webrtc-endpoint-profile`: The WebRTC protocol-specific endpoint profile (`peerSdp`, `cameraSdp`, `peerIceCandidates`, `cameraIceCandidates`) and the Matter 1.5 WebRTC signaling flow that drives it.
- `openhome-session-adapter`: Adaptation of the existing OpenHome camera driver to emit session events through the new abstract camera endpoint contract, with the `openhome` profile providing `tunnelUrl`.

### Modified Capabilities
_(none — no existing spec-level requirements are changing)_

## Impact

- **Layers affected**: Public API definitions (`api/c/public/private/commonDeviceDefs.h`, `resourceTypes.h`), core device service (`core/src/deviceService.c` — `updateResource` event metadata path), device drivers (OpenHome camera driver adaptation, new Matter camera driver), Matter subsystem (`core/src/subsystems/matter/` — WebRTC signaling)
- **Existing `camera` profile**: Extended, not replaced. New session resources coexist with existing OpenHome camera resources.
- **Event metadata contract**: New JSON keys (`sessionId`, `technology`, `nextAction`) added as string constants in `commonDeviceDefs.h`. These are conventions, not type-system enforced.
- **`sessionStatus` must always emit events**: Even when the value doesn't change. This is supported by registering the resource with `CACHING_POLICY_NEVER` and `RESOURCE_MODE_EMIT_EVENTS`, which skips value comparison entirely.
- **CMake flags**: `BCORE_MATTER` for Matter camera support. WebRTC media dependencies TBD (to be designed in the webrtc-endpoint-profile capability).
- **Backward compatibility**: Existing OpenHome camera clients continue to work. The new session contract is additive — clients that don't understand `sessionStatus` events can ignore them.
- **No new external dependencies in this proposal**: libdatachannel and WebRTC media stack decisions are deferred to the webrtc-endpoint-profile capability design.
