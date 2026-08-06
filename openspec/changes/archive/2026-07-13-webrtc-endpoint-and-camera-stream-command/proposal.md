## Why

The camera SBMD driver defines an abstract session lifecycle (createSession, stream, destroySession, sessionStatus) but has no protocol-specific endpoint to carry WebRTC signaling. Clients currently have no way to exchange SDP offers/answers or ICE candidates through Barton's resource model. Additionally, the reference app has no camera-specific command — testing camera streams requires manual multi-step `execResource` calls with no media rendering.

## What Changes

- **WebRTC endpoint in camera SBMD**: Add an `ep/webrtc` endpoint (profile: `"webrtc"`) to `camera.sbmd.js` with four resources — `offerSdp` [execute], `remoteSdp` [events], `offerIceCandidates` [execute], `remoteIceCandidates` [events]. Execute handlers send Matter commands to the camera's `WebRTCTransportProvider` cluster (0x0553). SBMD command handlers receive incoming signaling from the camera's `WebRTCTransportRequestor` cluster (0x0554) and emit events on the protocol endpoint resources.
- **Reference app `cameraStream` command**: A convenience orchestrator (`cameraStream` / `cs`) that automates the full camera streaming flow — session creation, WebRTC signaling via Barton's resource API, and media rendering via a GStreamer `webrtcbin` pipeline. Supports display output (autovideosink) and file recording (filesink). Barton remains a signaling relay; the reference app is the WebRTC peer and media consumer.
- **Separability by design**: The webrtc endpoint code is structured within `camera.sbmd.js` so it can be extracted to a standalone SBMD driver in the future without breaking the contract.

## Non-goals

- Media stack in Barton core — Barton relays signaling only, never receives media
- OpenHome or direct camera endpoint implementation (separate future work)
- STUN/TURN server integration
- Multi-stream or bidirectional audio support
- Changes to the public C API (BCoreClient already supports executeResource and event subscriptions)

## Capabilities

### New Capabilities
- `webrtc-signaling-endpoint`: The WebRTC protocol-specific endpoint (ep/webrtc) with resources for SDP and ICE exchange, SBMD command handlers for incoming Matter signaling, and execute handlers for outgoing Matter commands. Designed as a reusable pattern for any device type that uses WebRTC transport.
- `camera-stream-reference-command`: The reference app `cameraStream` command that orchestrates session lifecycle, WebRTC signaling through Barton, and media rendering via GStreamer webrtcbin. Demonstrates end-to-end camera streaming without direct Matter SDK or WebRTC library coupling in the client.

### Modified Capabilities
_(none — no existing spec-level requirements are changing)_

## Impact

- **SBMD driver**: `core/deviceDrivers/matter/sbmd/specs/camera.sbmd.js` — new endpoint, resources, constants, command handlers, execute handlers
- **Reference app**: `reference/src/` — new `cameraCategory.c/.h` for the `cameraStream` command, GStreamer pipeline management, event subscription handling
- **Reference app build**: `reference/CMakeLists.txt` — link against GStreamer (gstreamer-1.0, gstreamer-webrtc-1.0, gstreamer-sdp-1.0)
- **Docker image**: `gstreamer1.0-tools` and `libgstreamer-plugins-bad1.0-dev` added (version 2.12)
- **CMake flags**: Gated behind `BCORE_MATTER` (webrtc endpoint is Matter-only); reference app GStreamer support gated behind a new `BCORE_CAMERA_STREAM` flag
- **ZAP changes**: Add `WebRTCTransportRequestor` cluster (0x0554) as a server cluster on Barton's endpoint in `barton-library.matter` / `barton-library.zap`. The camera needs to send signaling commands (Offer, Answer, ICECandidates, End) back to Barton — this requires Barton to advertise the cluster so the camera knows where to target those commands.
- **Matter SDK headers used**: `WebRTCTransportProvider/CommandIds.h`, `WebRTCTransportRequestor/CommandIds.h` for cluster/command ID constants (already in build/matter-install)
