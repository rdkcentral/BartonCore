_These tasks represent follow-on implementation work. Each is expected to go through its own explore → propose → apply cycle._

## 1. Camera session contract and OpenHome adapter

**Capabilities**: `camera-session-contract`, `openhome-session-adapter`

Define the protocol-agnostic camera session contract and prove it end-to-end by adapting the existing OpenHome camera driver. This is the foundational work — it lands the architecture in code using existing hardware with no new external dependencies.

Scope includes:
- Session lifecycle resources (`startSession`, `stopSession`, `sessionStatus`) and metadata constants (`sessionId`, `technology`, `nextAction`, status values) in `commonDeviceDefs.h`
- Coexistence with existing `CAMERA_PROFILE_*` definitions (backward compatibility)
- `openhome` endpoint profile with `tunnelUrl` resource and `OPENHOME_PROFILE_*` constants
- OpenHome camera driver adaptation: `startSession` internally calls `createMediaTunnel`, emits `sessionStatus` events with metadata. Existing `createMediaTunnel` clients continue to work unchanged.
- Session ID lifecycle for MVP (single-session-per-device enforcement, cleanup on driver restart/device removal)

## 2. Matter camera support via WebRTC

**Capabilities**: `webrtc-endpoint-profile`
**Depends on**: Task 1 (camera session contract)

Implement the Matter 1.5 WebRTC camera driver and signaling flow on top of the session contract from Task 1.

Scope includes:
- `webrtc` endpoint profile with `peerSdp`, `cameraSdp`, `peerIceCandidates`, `cameraIceCandidates` resources and `WEBRTC_PROFILE_*` constants
- Matter camera driver (native C/C++) — including SBMD feasibility evaluation to determine whether SBMD can express stateful, bidirectional WebRTC signaling or whether a native driver is required
- Cluster server hosting pattern (`WebRtcTransportRequestorServer`) — first server-side cluster in Barton's Matter subsystem
- SDP offer/answer exchange and ICE candidate negotiation
- WebRTC media stack integration (libdatachannel or alternative)
- Thread marshalling from CHIP event loop to GLib main loop
- Integration test infrastructure: matter.js virtual camera device, Python test fixtures (`MatterCamera`) following the existing `MatterLight`/`MatterDoorLock` pattern

