## MODIFIED Requirements

### Requirement: cameraStream orchestrates full session lifecycle

The `cameraStream` command SHALL orchestrate the complete camera streaming flow through Barton's
resource API, branching on the active protocol returned by `stream`:

1. Execute `createSession` on the device's `ep/camera` endpoint
2. Execute `stream` and obtain the active protocol and entry-point URI from its `{ protocol, entryPoint }` result
3. **If the protocol is `webrtc`**, perform the WebRTC signaling flow:
   1. Read the `negotiationRole` resource on `ep/webrtc` — which reports the **camera's** role — and adopt the opposite role for itself: act as the `answerer` when the camera is the `offerer`, or as the `offerer` when the camera is the `answerer`
   2. Create a local GStreamer `webrtcbin` peer connection using host candidates only (no STUN/TURN), configured for the client's derived role
   3. Perform the SDP exchange for the client's derived role via the `localSdp` resource on `ep/webrtc`:
      - **Client is the offerer** (camera reported `answerer`): generate a local SDP offer from webrtcbin, execute `localSdp` with it, then wait for a `remoteSdp` event (the camera's answer) and set it as the remote description
      - **Client is the answerer** (camera reported `offerer`): execute `localSdp` with empty input to open the flow, wait for a `remoteSdp` event (the camera's offer), set it as the remote description, generate a local SDP answer, and execute `localSdp` with the answer
   4. Exchange ICE candidates (local → `localIceCandidates`, remote ← `remoteIceCandidates` events)
   5. Wait for the peer connection to reach the connected state, subject to a bounded connectivity timeout
4. **If the protocol is `onvif`**, follow the ONVIF/RTSP path (see the ONVIF requirement): obtain the RTSP URL via `getMediaUrl`/`mediaUrl` and open a GStreamer `rtspsrc` source
5. Route the received media to the destination selected by `--out`: serve it over the built-in HTTP server or record it to a file — identically for both protocols
6. On user interrupt (Ctrl+C), a `webrtcError` event (WebRTC), or a stream error: execute `destroySession` and tear down the pipeline

#### Scenario: Client adopts the opposite of the camera's role
- **WHEN** the `negotiationRole` read returns `offerer` (the camera is the offerer)
- **THEN** the reference app SHALL configure `webrtcbin` as the `answerer` and answer the camera's offer
- **WHEN** the `negotiationRole` read returns `answerer` (the camera is the answerer)
- **THEN** the reference app SHALL configure `webrtcbin` as the `offerer` and generate the SDP offer

#### Scenario: Command branches on the active protocol
- **WHEN** the `stream` result reports `protocol` `"webrtc"`
- **THEN** the command SHALL run the WebRTC signaling flow
- **WHEN** the `stream` result reports `protocol` `"onvif"`
- **THEN** the command SHALL run the ONVIF/RTSP flow and SHALL NOT perform WebRTC signaling

#### Scenario: Successful camera stream served to a browser
- **WHEN** a user executes `cameraStream <deviceId>` (or with an `http://` `--out`) and the camera responds to signaling
- **THEN** the reference app SHALL serve the live stream over its built-in HTTP server for a browser to play and print status messages for each step, without opening a local display window

#### Scenario: Successful camera stream to file
- **WHEN** a user executes `cameraStream <deviceId> --out file://recording.mp4`
- **THEN** the reference app SHALL record the video stream to the specified file path

#### Scenario: User stops the stream
- **WHEN** a user presses Ctrl+C during an active stream
- **THEN** the reference app SHALL execute `destroySession`, stop the GStreamer pipeline, and return to the command prompt

### Requirement: cameraStream uses GStreamer webrtcbin for media

For **WebRTC** cameras, the `cameraStream` command SHALL use GStreamer's `webrtcbin` element as its
local WebRTC peer connection (host candidates only). For **ONVIF** cameras it SHALL instead use a
GStreamer `rtspsrc` source (see the ONVIF requirement). In both cases the received H.264 SHALL be
handled by a shared, protocol-independent passthrough pipeline that neither decodes nor renders
locally: `… → rtph264depay → h264parse → h264timestamper → capsfilter → mp4mux` (fragmented,
streamable) `→ appsink`. The `h264timestamper` reconstructs the PTS/DTS the camera's RTP buffers lack
(so `mp4mux` does not abort on a missing PTS), and the `capsfilter` forces AVC / `alignment=au` output
so `mp4mux` can negotiate. The muxed fragmented-MP4 buffers SHALL be delivered either to the built-in
HTTP media server or to a file, according to `--out`.

#### Scenario: Serve mode pipeline
- **WHEN** `cameraStream` runs in serve mode (the default, or an `http://` `--out`)
- **THEN** the muxed fragmented-MP4 buffers SHALL be pushed to the built-in HTTP media server, which serves them to a browser that decodes and plays them via Media Source Extensions

#### Scenario: Record mode pipeline
- **WHEN** `cameraStream` is invoked with `--out file://<path>`
- **THEN** the muxed fragmented-MP4 buffers SHALL be written to the file at that path

#### Scenario: GStreamer not available
- **WHEN** GStreamer libraries (with `webrtcbin` or `rtspsrc` as required) are not available at runtime
- **THEN** the command SHALL print an error explaining the requirement and exit gracefully

### Requirement: cameraStream subscribes to Barton events

The `cameraStream` command SHALL subscribe to resource events on the device to receive data
asynchronously, according to the active protocol. For **WebRTC** cameras it SHALL subscribe on
`ep/webrtc` to:
- `remoteSdp` events (for the camera's remote SDP — an answer when the client is the offerer, or an offer when the client is the answerer)
- `remoteIceCandidates` events (for the camera's ICE candidates)
- `webrtcError` events (for asynchronous session termination and errors)

For **ONVIF** cameras it SHALL subscribe on `ep/onvif` to `mediaUrl` (and `snapshotUrl` when taking a
picture). The command SHALL NOT subscribe to any `sessionStatus` resource on `ep/camera`. It SHALL
obtain the active protocol and entry-point URI from the `stream` execute result.

#### Scenario: Remote SDP delivered via event
- **WHEN** the camera provides its remote SDP (an answer when the client offered, or an offer when the client is the answerer)
- **THEN** the reference app SHALL receive it as a `remoteSdp` event and feed it to webrtcbin as the remote description

#### Scenario: Remote ICE candidates delivered via events
- **WHEN** the camera sends ICE candidates
- **THEN** the reference app SHALL receive them as `remoteIceCandidates` events and add each candidate to webrtcbin

#### Scenario: Session error delivered via webrtcError event
- **WHEN** the driver emits a `webrtcError` event with an ended or failed value
- **THEN** the reference app SHALL treat it as a session-terminated signal and begin graceful teardown, reporting the reason from the event metadata

#### Scenario: ONVIF media URL delivered via event
- **WHEN** the active protocol is `onvif` and the driver emits a `mediaUrl` event
- **THEN** the reference app SHALL receive the RTSP URL from that event and use it as the `rtspsrc` location

## ADDED Requirements

### Requirement: cameraStream supports ONVIF/RTSP cameras

When the `stream` result reports protocol `onvif`, the `cameraStream` command SHALL drive the camera
through the `ep/onvif` endpoint using the same command interface as WebRTC: it SHALL read
`authRequired`, ensure credentials are applied, subscribe to `mediaUrl`, execute `getMediaUrl`, and on
receiving the `mediaUrl` event open a GStreamer `rtspsrc` source at that URL (applying credentials
when `authRequired` is true) feeding the shared fragmented-MP4 → serve/record sink. Taking a picture
SHALL use `takePicture` → `getSnapshotUrl` → `snapshotUrl` analogously.

#### Scenario: ONVIF stream played through the same command
- **WHEN** a user runs `cameraStream <onvifDeviceId>` and the driver reports protocol `onvif`
- **THEN** the command SHALL obtain the RTSP URL from a `mediaUrl` event and serve or record it via the same `--out` destinations used for WebRTC

#### Scenario: ONVIF credentials applied to the RTSP source
- **WHEN** `authRequired` reads `"true"` for the camera
- **THEN** the command SHALL apply the configured credentials to the `rtspsrc` connection and SHALL NOT print the credentials

### Requirement: cameraStream accepts ONVIF credentials via interim flags

For ONVIF cameras, the `cameraStream` command SHALL accept the camera credentials through optional
command flags (e.g. `--user`/`--pass`) and write them to the sensitive `username`/`password`
resources on `ep/onvif` before streaming. This flag-based credential entry is an explicitly
**interim** mechanism for this driver version and SHALL be documented as such; it is expected to be
superseded by a configuration-driven credential model. The command's argument contract SHALL be
extended accordingly: beyond `<deviceId> [--out <uri>]` it SHALL also accept the optional
`--user`/`--pass` (and `--snapshot <path>`) flags, and its registered maximum-argument limit SHALL be
raised to admit them.

#### Scenario: Credentials supplied via flags
- **WHEN** a user runs `cameraStream <onvifDeviceId> --user <u> --pass <p>`
- **THEN** the command SHALL write the sensitive `username`/`password` resources before executing `getMediaUrl`

#### Scenario: Missing credentials reported clearly
- **WHEN** an ONVIF camera requires authentication and no credentials have been provided
- **THEN** the command SHALL print a clear error indicating credentials are required and SHALL NOT hang
