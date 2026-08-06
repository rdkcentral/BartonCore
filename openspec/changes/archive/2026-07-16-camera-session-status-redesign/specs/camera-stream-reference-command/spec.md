## MODIFIED Requirements

### Requirement: cameraStream subscribes to Barton events

The `cameraStream` command SHALL subscribe to resource events on the device to receive signaling data asynchronously. Specifically:
- `remoteSdp` events on `ep/webrtc` (for the camera's SDP answer)
- `remoteIceCandidates` events on `ep/webrtc` (for the camera's ICE candidates)
- `webrtcError` events on `ep/webrtc` (for asynchronous session termination and errors)

The command SHALL NOT subscribe to any `sessionStatus` resource on `ep/camera`. It SHALL obtain the active protocol and entry-point URI from the `stream` execute result.

#### Scenario: Remote SDP delivered via event
- **WHEN** the camera responds with an SDP answer
- **THEN** the reference app SHALL receive it as a `remoteSdp` event and feed it to webrtcbin as the remote description

#### Scenario: Remote ICE candidates delivered via events
- **WHEN** the camera sends ICE candidates
- **THEN** the reference app SHALL receive them as `remoteIceCandidates` events and add each candidate to webrtcbin

#### Scenario: Session error delivered via webrtcError event
- **WHEN** the driver emits a `webrtcError` event with an ended or failed value
- **THEN** the reference app SHALL treat it as a session-terminated signal and begin graceful teardown, reporting the reason from the event metadata

### Requirement: cameraStream reports progress to user

The command SHALL emit human-readable progress messages to stdout at each stage of the flow:
- Session created (sessionId)
- Streaming initiated (protocol, entryPoint)
- SDP offer sent
- SDP answer received
- ICE candidates exchanged
- Media flowing / connected
- Stream ended (reason)

#### Scenario: Progress output during successful stream
- **WHEN** `cameraStream` completes signaling and media begins flowing
- **THEN** the user SHALL see step-by-step status messages indicating progress through the flow

### Requirement: cameraStream handles errors gracefully

The command SHALL handle failures at any stage (session creation failure, signaling timeout, asynchronous `webrtcError` failure, and connectivity/ICE failure) by printing an error message, cleaning up any partial state (destroying the session if created), and returning to the command prompt. In particular, because Matter signaling cannot observe media-plane connectivity, the command SHALL enforce a client-side connectivity timeout: if the WebRTC peer connection does not reach a connected state within a bounded window after signaling completes — or transitions to a failed state — the command SHALL tear down and report the failure rather than wait indefinitely.

#### Scenario: Device does not support camera streaming
- **WHEN** `cameraStream` is executed on a device without a `camera` endpoint
- **THEN** the command SHALL print an error and exit without crashing

#### Scenario: Signaling timeout
- **WHEN** the camera does not respond to signaling within a reasonable timeout
- **THEN** the command SHALL print a timeout error, destroy the session, and exit

#### Scenario: Connectivity never established
- **WHEN** signaling completes but the WebRTC peer connection does not reach a connected state within the connectivity timeout window
- **THEN** the command SHALL print a connectivity-failure message, destroy the session, and exit rather than hang on a blank window

#### Scenario: Peer connection fails
- **WHEN** the WebRTC peer connection transitions to a failed state during or after ICE exchange
- **THEN** the command SHALL print a failure message, destroy the session, and exit

### Requirement: cameraStream command exists in reference app

The reference app SHALL provide a command named `cameraStream` with short alias `cs` in a dedicated camera command category. The command SHALL accept a device ID as a required argument and an optional `--out <uri>` flag that selects the media destination.

#### Scenario: Command appears in help
- **WHEN** a user types `help` in the reference app
- **THEN** the camera category SHALL list `cameraStream` with usage: `<deviceId> [--out <uri>]`

#### Scenario: Command with short alias
- **WHEN** a user types `cs <deviceId>`
- **THEN** the command SHALL execute identically to `cameraStream <deviceId>`

#### Scenario: Output URI selects the media destination
- **WHEN** the command is invoked with `--out file://<path>`
- **THEN** the stream SHALL be recorded to that file path
- **WHEN** the command is invoked with `--out <host>[:<port>]` (optionally prefixed with `http://`), or without `--out`
- **THEN** the stream SHALL be served over HTTP for a browser to play, defaulting to a loopback host and port when `--out` is omitted

### Requirement: cameraStream orchestrates full session lifecycle

The `cameraStream` command SHALL orchestrate the complete camera streaming flow through Barton's resource API:
1. Execute `createSession` on the device's `ep/camera` endpoint
2. Execute `stream` and obtain the active protocol and entry-point URI from its `{ protocol, entryPoint }` result
3. Create a local GStreamer `webrtcbin` peer connection using host candidates only (no STUN/TURN)
4. Generate a local SDP offer from webrtcbin
5. Execute `offerSdp` on the device's `ep/webrtc` endpoint with the local SDP
6. Wait for a `remoteSdp` event and set the remote description on webrtcbin
7. Exchange ICE candidates (local → `offerIceCandidates`, remote ← `remoteIceCandidates` events)
8. Wait for the peer connection to reach the connected state, subject to a bounded connectivity timeout
9. Route the received media to the destination selected by `--out`: serve it over the built-in HTTP server or record it to a file
10. On user interrupt (Ctrl+C or `q`) or a `webrtcError` event: execute `destroySession` and tear down the pipeline

#### Scenario: Successful camera stream served to a browser
- **WHEN** a user executes `cameraStream <deviceId>` (or with an `http://` `--out`) and the camera responds to signaling
- **THEN** the reference app SHALL serve the live stream over its built-in HTTP server for a browser to play and print status messages for each step, without opening a local display window

#### Scenario: Successful camera stream to file
- **WHEN** a user executes `cameraStream <deviceId> --out file://recording.mp4`
- **THEN** the reference app SHALL record the video stream to the specified file path

#### Scenario: User stops the stream
- **WHEN** a user presses Ctrl+C or types `q` during an active stream
- **THEN** the reference app SHALL execute `destroySession`, stop the GStreamer pipeline, and return to the command prompt

### Requirement: cameraStream uses GStreamer webrtcbin for media

The `cameraStream` command SHALL use GStreamer's `webrtcbin` element as its local WebRTC peer connection (host candidates only). The received H.264 SHALL be handled by a passthrough pipeline that neither decodes nor renders locally: `rtph264depay → h264parse → mp4mux` (fragmented, streamable) `→ appsink`. The muxed fragmented-MP4 buffers SHALL be delivered either to the built-in HTTP media server or to a file, according to `--out`.

#### Scenario: Serve mode pipeline
- **WHEN** `cameraStream` runs in serve mode (the default, or an `http://` `--out`)
- **THEN** the muxed fragmented-MP4 buffers SHALL be pushed to the built-in HTTP media server, which serves them to a browser that decodes and plays them via Media Source Extensions

#### Scenario: Record mode pipeline
- **WHEN** `cameraStream` is invoked with `--out file://<path>`
- **THEN** the muxed fragmented-MP4 buffers SHALL be written to the file at that path

#### Scenario: GStreamer not available
- **WHEN** GStreamer libraries (with `webrtcbin`) are not available at runtime
- **THEN** the command SHALL print an error explaining the requirement and exit gracefully

### Requirement: Camera command category is gated by CMake flag

The camera stream command and its GStreamer dependencies SHALL be gated behind a `BCORE_REFERENCE_CAMERA_SUPPORT` CMake option (default OFF). When disabled, the reference app builds without GStreamer dependencies and without the camera category.

#### Scenario: Build without camera stream support
- **WHEN** `BCORE_REFERENCE_CAMERA_SUPPORT=OFF` (default)
- **THEN** the reference app SHALL build successfully without GStreamer development libraries

#### Scenario: Build with camera stream support
- **WHEN** `BCORE_REFERENCE_CAMERA_SUPPORT=ON`
- **THEN** the reference app SHALL link against gstreamer-1.0, gstreamer-webrtc-1.0, gstreamer-sdp-1.0, gstreamer-app-1.0, and gio-2.0, and include the camera category
