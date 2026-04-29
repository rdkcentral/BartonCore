_These tasks represent follow-on implementation work. Each is expected to go through its own explore → propose → apply cycle._

## 1. Camera stream contract and OpenHome adapter

**Capabilities**: `camera-stream-contract`, `openhome-stream-adapter`

Define the protocol-agnostic camera stream contract and prove it end-to-end by adapting the existing OpenHome camera driver. This is the foundational work — it lands the architecture in code using existing hardware with no new external dependencies.

Scope includes:
- Stream lifecycle resources (`stream` [execute, returns streamId], `streamStatus` [events only, non-readable]) and metadata constants (`streamId`, `nextAction`, status values `setup`/`done`/`error`) in `commonDeviceDefs.h`
- Event ordering guarantee: the client must receive the `streamId` from the `stream` execute response before the first `streamStatus` event fires. Mechanism TBD (scheduled delay, deferred emit, etc.)
- Clean up existing `CAMERA_PROFILE_*` definitions — legacy resources are removed unless there's a compelling reason to keep them (we own the clients)
- `openhome` endpoint profile with `getStreamUrl` [execute] and `streamUrl` [events] resources and `OPENHOME_PROFILE_*` constants
- OpenHome camera driver adaptation: `stream` execute internally calls `createMediaTunnel`, emits `streamStatus` events with metadata. Existing `createMediaTunnel` path may be retained or removed depending on whether a clean break simplifies the implementation.
- Stream ID lifecycle for MVP (driver self-manages cleanup on timeout/restart/device removal)

## 2. Matter camera support via WebRTC

**Capabilities**: `webrtc-endpoint-profile`
**Depends on**: Task 1 (camera stream contract)

Implement the Matter 1.5 WebRTC camera driver and signaling flow on top of the stream contract from Task 1.

Scope includes:
- `webrtc` endpoint profile with `offerSdp`, `remoteSdp`, `offerIceCandidates`, `remoteIceCandidates` resources and `WEBRTC_PROFILE_*` constants
- Matter camera driver via SBMD — the key architectural challenge is SBMD composition: how to reuse the shared camera stream manager (stream lifecycle, status events, cleanup) from within an SBMD driver. Options to explore include SBMD class dependencies and exposing the stream manager via JS bindings in the SBMD context.
- Cluster server hosting pattern (`WebRtcTransportRequestorServer`) — first server-side cluster in Barton's Matter subsystem
- SDP offer/answer exchange and ICE candidate negotiation
- WebRTC media stack integration (libdatachannel or alternative)
- Integration test infrastructure: matter.js virtual camera device, Python test fixtures (`MatterCamera`) following the existing `MatterLight`/`MatterDoorLock` pattern

