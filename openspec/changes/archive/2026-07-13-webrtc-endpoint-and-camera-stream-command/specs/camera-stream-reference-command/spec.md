## ADDED Requirements

### Requirement: cameraStream command exists in reference app

The reference app SHALL provide a command named `cameraStream` with short alias `cs` in a dedicated camera command category. The command SHALL accept a device ID as a required argument and an optional `--file <path>` flag for recording.

#### Scenario: Command appears in help
- **WHEN** a user types `help` in the reference app
- **THEN** the camera category SHALL list `cameraStream` with usage: `<deviceId> [--file <path.mp4>]`

#### Scenario: Command with short alias
- **WHEN** a user types `cs <deviceId>`
- **THEN** the command SHALL execute identically to `cameraStream <deviceId>`

### Requirement: cameraStream orchestrates full session lifecycle

The `cameraStream` command SHALL orchestrate the complete camera streaming flow through Barton's resource API:
1. Execute `createSession` on the device's `ep/camera` endpoint
2. Execute `stream` with the returned sessionId
3. Wait for `sessionStatus` event with status `"setup"` and extract `nextAction`
4. Create a local GStreamer webrtcbin peer connection
5. Generate a local SDP offer from webrtcbin
6. Execute `offerSdp` on the device's `ep/webrtc` endpoint with the local SDP
7. Wait for `remoteSdp` event and set the remote description on webrtcbin
8. Exchange ICE candidates (local → `offerIceCandidates`, remote ← `remoteIceCandidates` events)
9. Wait for media to flow (peer connection state: connected)
10. On user interrupt (Ctrl+C or `q`): execute `destroySession` and tear down the pipeline

#### Scenario: Successful camera stream to display
- **WHEN** user executes `cameraStream <deviceId>` and the camera responds to signaling
- **THEN** the reference app SHALL display video output via GStreamer autovideosink and print status messages for each step

#### Scenario: Successful camera stream to file
- **WHEN** user executes `cameraStream <deviceId> --file recording.mp4`
- **THEN** the reference app SHALL record the video stream to the specified file path

#### Scenario: User stops the stream
- **WHEN** user presses Ctrl+C or types `q` during an active stream
- **THEN** the reference app SHALL execute `destroySession`, stop the GStreamer pipeline, and return to the command prompt

### Requirement: cameraStream uses only BCoreClient API for signaling

The `cameraStream` command SHALL interact with Barton exclusively through `BCoreClient` APIs (`b_core_client_execute_resource`, event subscriptions). It SHALL NOT use Matter SDK APIs, link against Matter libraries, or reference Matter-specific types.

#### Scenario: No Matter SDK dependency
- **WHEN** the reference app is compiled
- **THEN** the camera stream module SHALL compile without any Matter SDK headers in its include path

### Requirement: cameraStream uses GStreamer webrtcbin for media

The `cameraStream` command SHALL use GStreamer's `webrtcbin` element as its local WebRTC peer connection. webrtcbin handles SDP generation, ICE gathering, DTLS/SRTP negotiation, and media decoding.

#### Scenario: GStreamer pipeline for display
- **WHEN** `cameraStream` is invoked without `--file`
- **THEN** a GStreamer pipeline SHALL be created with `webrtcbin` connected to `decodebin` and `autovideosink`

#### Scenario: GStreamer pipeline for file recording
- **WHEN** `cameraStream` is invoked with `--file <path>`
- **THEN** a GStreamer pipeline SHALL be created with `webrtcbin` connected to appropriate muxing and `filesink` elements

#### Scenario: GStreamer not available
- **WHEN** GStreamer libraries are not found at runtime
- **THEN** the command SHALL print an error message explaining that GStreamer with webrtcbin is required and exit gracefully

### Requirement: cameraStream subscribes to Barton events

The `cameraStream` command SHALL subscribe to resource events on the device to receive signaling data asynchronously. Specifically:
- `sessionStatus` events on `ep/camera` (for session state transitions)
- `remoteSdp` events on `ep/webrtc` (for the camera's SDP answer)
- `remoteIceCandidates` events on `ep/webrtc` (for the camera's ICE candidates)

#### Scenario: Remote SDP delivered via event
- **WHEN** the camera responds with an SDP answer
- **THEN** the reference app SHALL receive it as a `remoteSdp` event and feed it to webrtcbin as the remote description

#### Scenario: Remote ICE candidates delivered via events
- **WHEN** the camera sends ICE candidates
- **THEN** the reference app SHALL receive them as `remoteIceCandidates` events and add each candidate to webrtcbin

### Requirement: cameraStream reports progress to user

The command SHALL emit human-readable progress messages to stdout at each stage of the flow:
- Session created (sessionId)
- Streaming initiated (protocol, nextAction)
- SDP offer sent
- SDP answer received
- ICE candidates exchanged
- Media flowing / connected
- Stream ended (reason)

#### Scenario: Progress output during successful stream
- **WHEN** `cameraStream` completes signaling and media begins flowing
- **THEN** the user SHALL see step-by-step status messages indicating progress through the flow

### Requirement: cameraStream handles errors gracefully

The command SHALL handle failures at any stage (session creation failure, signaling timeout, peer connection failure) by printing an error message, cleaning up any partial state (destroying the session if created), and returning to the command prompt.

#### Scenario: Device does not support camera streaming
- **WHEN** `cameraStream` is executed on a device without a `camera` endpoint
- **THEN** the command SHALL print an error and exit without crashing

#### Scenario: Signaling timeout
- **WHEN** the camera does not respond to signaling within a reasonable timeout
- **THEN** the command SHALL print a timeout error, destroy the session, and exit

### Requirement: Camera command category is gated by CMake flag

The camera stream command and its GStreamer dependencies SHALL be gated behind a `BCORE_CAMERA_STREAM` CMake option (default OFF). When disabled, the reference app builds without GStreamer dependencies and without the camera category.

#### Scenario: Build without camera stream support
- **WHEN** `BCORE_CAMERA_STREAM=OFF` (default)
- **THEN** the reference app SHALL build successfully without GStreamer development libraries

#### Scenario: Build with camera stream support
- **WHEN** `BCORE_CAMERA_STREAM=ON`
- **THEN** the reference app SHALL link against gstreamer-1.0, gstreamer-webrtc-1.0, gstreamer-sdp-1.0 and include the camera category
