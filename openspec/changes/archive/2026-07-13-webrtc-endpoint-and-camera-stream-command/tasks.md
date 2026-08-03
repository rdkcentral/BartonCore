## 1. ZAP — Add WebRTCTransportRequestor Cluster

- [x] 1.1 Add `WebRTCTransportRequestor` cluster definition (0x0554) with commands (Offer, Answer, ICECandidates, End) to `barton-library.matter`
- [x] 1.2 Add the cluster as a server on Barton's endpoint in the ZAP endpoint configuration section of `barton-library.matter`, handle commands (Offer, Answer, ICECandidates, End)
- [x] 1.3 Regenerate ZAP artifacts (`barton-library.zap` if needed) and verify the Matter build compiles with the new cluster

## 2. SBMD WebRTC Endpoint — Constants and Resource Declarations

- [x] 2.1 Add WebRTC cluster and command ID constants to camera.sbmd.js (CL_WEBRTC_TRANSPORT_PROVIDER, CL_WEBRTC_TRANSPORT_REQUESTOR, CMD_PROVIDE_OFFER, CMD_PROVIDE_ICE, CMD_END_SESSION, CMD_OFFER, CMD_ICE_CANDIDATES, CMD_END, etc.)
- [x] 2.2 Declare the `webrtc` endpoint in the `endpoints` block with profile `"webrtc"`, resources: `offerSdp` (function/execute), `remoteSdp` (string, modes:[]), `offerIceCandidates` (function/execute), `remoteIceCandidates` (string, modes:[])
- [x] 2.3 Add `CL_WEBRTC_TRANSPORT_REQUESTOR` to the `featureClusters` array in the matter config (enables incoming command handler registration)

## 3. SBMD WebRTC Endpoint — Execute Handlers

- [x] 3.1 Implement `executeOfferSdp` handler: validate active session in transient data, extract SDP from input, build TLV payload for ProvideOffer command (webRTCSessionID + sdp + streamUsage + originatingEndpointID), return `device.sendCommand(CL_WEBRTC_TRANSPORT_PROVIDER, CMD_PROVIDE_OFFER, tlv)`
- [x] 3.2 Implement `executeOfferIceCandidates` handler: parse JSON array of ICE candidate strings from input, build TLV payload for ProvideICECandidates command (webRTCSessionID + ICECandidates array), return `device.sendCommand(CL_WEBRTC_TRANSPORT_PROVIDER, CMD_PROVIDE_ICE, tlv)`
- [x] 3.3 Update `executeDestroySession` to send EndSession command (CMD_END_SESSION) to the camera when session state is `streaming` before removing session from transient data

## 4. SBMD WebRTC Endpoint — Command Handlers (Incoming from Camera)

- [x] 4.1 Add `commandHandlers` block to the SbmdDriver registration with aliases for cluster 0x0554 commands (Offer, ICECandidates, End)
- [x] 4.2 Implement `handleIncomingOffer` command handler: decode TLV to extract SDP string, call `updateResource('webrtc', 'remoteSdp', sdp)`, store the Matter webRTCSessionID in transient data for correlation
- [x] 4.3 Implement `handleIncomingIceCandidates` command handler: decode TLV to extract ICE candidate list, JSON-encode, call `updateResource('webrtc', 'remoteIceCandidates', jsonCandidates)`
- [x] 4.4 Implement `handleIncomingEnd` command handler: extract reason code, emit `sessionStatus` error event with metadata `{sessionId, error}`, clean up session state

## 5. SBMD WebRTC Endpoint — Unit Testing

- [x] 5.1 Write unit tests for `executeOfferSdp`: valid session produces sendCommand result, missing session returns error, missing input returns error
- [x] 5.2 Write unit tests for `executeOfferIceCandidates`: valid JSON array produces sendCommand, invalid JSON returns error
- [x] 5.3 Write unit tests for incoming command handlers: verify updateResource calls with correct endpoint/resource/value for Offer, ICECandidates, and End commands
- [x] 5.4 Write unit test for `executeDestroySession` with streaming session: verify EndSession command is sent before cleanup

## 6. Reference App — Build System and Category Setup

- [x] 6.1 Add `BCORE_CAMERA_STREAM` CMake option (default OFF) to `reference/CMakeLists.txt`
- [x] 6.2 When `BCORE_CAMERA_STREAM=ON`: find GStreamer packages (gstreamer-1.0, gstreamer-webrtc-1.0, gstreamer-sdp-1.0), add to link dependencies, define `HAVE_CAMERA_STREAM` compile definition
- [x] 6.3 Create `cameraCategory.h` / `cameraCategory.c` with `buildCameraCategory()` returning a Category with `cameraStream` / `cs` command
- [x] 6.4 Register camera category in the reference app's main category list (gated by `#ifdef HAVE_CAMERA_STREAM`)

## 7. Reference App — GStreamer WebRTC Pipeline

- [x] 7.1 Create `cameraStreamPipeline.h` / `cameraStreamPipeline.c` — GStreamer pipeline management: create pipeline with webrtcbin, extract local SDP offer, set remote SDP, add ICE candidates, connect to autovideosink or filesink based on mode
- [x] 7.2 Implement SDP offer extraction: connect to webrtcbin's `on-negotiation-needed` signal, call `create-offer` action, extract SDP string from the local description
- [x] 7.3 Implement remote SDP handling: parse SDP answer string into GstWebRTCSessionDescription, call `set-remote-description` on webrtcbin
- [x] 7.4 Implement ICE candidate handling: connect to `on-ice-candidate` signal for local candidates, provide `add-ice-candidate` for remote candidates
- [x] 7.5 Implement pipeline teardown: stop pipeline, free resources, handle GStreamer state changes

## 8. Reference App — cameraStream Command Orchestration

- [x] 8.1 Implement `cameraStreamFunc`: parse args (deviceId, optional --file), verify device has camera endpoint, call createSession, call stream, subscribe to events
- [x] 8.2 Implement event handling loop: wait for sessionStatus(setup) → create pipeline → extract SDP → execute offerSdp → wait for remoteSdp event → set remote → exchange ICE → wait for connection
- [x] 8.3 Implement Ctrl+C / `q` handling: signal handler sets teardown flag, calls destroySession, stops GStreamer pipeline, returns to prompt
- [x] 8.4 Implement progress output: print human-readable status at each stage (session created, SDP sent, answer received, ICE exchanged, media flowing, stream ended)
- [x] 8.5 Implement error handling: timeout on signaling steps, graceful cleanup on failure, clear error messages
