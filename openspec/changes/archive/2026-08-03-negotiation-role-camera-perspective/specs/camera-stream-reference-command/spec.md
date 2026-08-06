## MODIFIED Requirements

### Requirement: cameraStream orchestrates full session lifecycle

The `cameraStream` command SHALL orchestrate the complete camera streaming flow through Barton's resource API:
1. Execute `createSession` on the device's `ep/camera` endpoint
2. Execute `stream` and obtain the active protocol and entry-point URI from its `{ protocol, entryPoint }` result
3. Read the `negotiationRole` resource on `ep/webrtc` — which reports the **camera's** role — and adopt the opposite role for itself: act as the `answerer` when the camera is the `offerer`, or as the `offerer` when the camera is the `answerer`
4. Create a local GStreamer `webrtcbin` peer connection using host candidates only (no STUN/TURN), configured for the client's derived role
5. Perform the SDP exchange for the client's derived role via the `localSdp` resource on `ep/webrtc`:
   - **Client is the offerer** (camera reported `answerer`): generate a local SDP offer from webrtcbin, execute `localSdp` with it, then wait for a `remoteSdp` event (the camera's answer) and set it as the remote description
   - **Client is the answerer** (camera reported `offerer`): execute `localSdp` with empty input to open the flow, wait for a `remoteSdp` event (the camera's offer), set it as the remote description, generate a local SDP answer, and execute `localSdp` with the answer
6. Exchange ICE candidates (local → `localIceCandidates`, remote ← `remoteIceCandidates` events)
7. Wait for the peer connection to reach the connected state, subject to a bounded connectivity timeout
8. Route the received media to the destination selected by `--out`: serve it over the built-in HTTP server or record it to a file
9. On user interrupt (Ctrl+C) or a `webrtcError` event: execute `destroySession` and tear down the pipeline

#### Scenario: Client adopts the opposite of the camera's role
- **WHEN** the `negotiationRole` read returns `offerer` (the camera is the offerer)
- **THEN** the reference app SHALL configure `webrtcbin` as the `answerer` and answer the camera's offer
- **WHEN** the `negotiationRole` read returns `answerer` (the camera is the answerer)
- **THEN** the reference app SHALL configure `webrtcbin` as the `offerer` and generate the SDP offer

#### Scenario: Successful camera stream served to a browser
- **WHEN** a user executes `cameraStream <deviceId>` (or with an `http://` `--out`) and the camera responds to signaling
- **THEN** the reference app SHALL serve the live stream over its built-in HTTP server for a browser to play and print status messages for each step, without opening a local display window

#### Scenario: Successful camera stream to file
- **WHEN** a user executes `cameraStream <deviceId> --out file://recording.mp4`
- **THEN** the reference app SHALL record the video stream to the specified file path

#### Scenario: User stops the stream
- **WHEN** a user presses Ctrl+C during an active stream
- **THEN** the reference app SHALL execute `destroySession`, stop the GStreamer pipeline, and return to the command prompt
