## Why

Investigation of the camera driver's `sessionStatus` resource showed it is effectively an appendage: its `setup`/`nextAction` metadata is ignored by the only client (the reference app hard-codes protocol knowledge), its `done` value is never emitted, and its lone live use — signaling that the camera ended the session — is a protocol-specific event misplaced on the abstract endpoint. Worse, a spec-path audit of the WebRTC/Matter flow found that **almost every asynchronous failure is silently dropped**: `requestCommand` `onError` results (VideoStreamAllocate rejected, ProvideOffer rejected) and `requestCommand` timeouts return `.error()` that has no delivery channel to the client, and `sendCommand` (outbound ICE, EndSession) is fire-and-forget with no error path at all. The result is that malformed SDP, a camera that goes unreachable during ICE, or a client that sends no ICE candidates all manifest as an unbounded black-screen hang with no error surfaced.

## What Changes

- **BREAKING** (internal signaling contract): Remove the `sessionStatus` resource from the abstract `ep/camera` endpoint. Protocol and entry-point discovery move onto the return value of the `stream` execute (`{ protocol, entryPoint }`), which the client already calls — eliminating the server-pushed "traffic controller" the client never used.
- Add a new declarable SBMD resource mode `volatile` that maps a resource to `CACHING_POLICY_NEVER`, then add a single asynchronous error/termination event resource on the protocol endpoint (`ep/webrtc`, `webrtcError`) declared `modes: ['volatile']` so it emits **unconditionally**, never subject to the device-service no-change suppression that silently dropped repeated status writes. The device-service change-detection logic itself is left untouched.
- Feed that event from **every** currently-dropped failure path: `handleVideoStreamAllocateError`, `handleProvideOfferError`, the `requestCommand` overall-deadline timeout, and the incoming camera `End` command — subsuming the old `sessionStatus="error"` behavior and closing the silent-failure gaps.
- The reference app consumes the new `ep/webrtc` state event (in place of `sessionStatus`) and adds a client-side connectivity/ICE timeout driven by `webrtcbin`'s `ice-connection-state`/`connection-state`, surfacing failures through the existing graceful-teardown path instead of hanging on a black window.

## Capabilities

### New Capabilities
- `camera-session-lifecycle`: The protocol-agnostic abstract camera endpoint contract (`createSession`, `stream`, `takePicture`, `destroySession`) with **no** `sessionStatus` resource. The `stream` execute returns the active protocol and its entry-point URI; all in-session state, error, and teardown signaling lives on the protocol endpoint, not the abstract one.

### Modified Capabilities
- `sbmd-v4-runtime`: Add a declarable resource mode `volatile` that registers a resource with `CACHING_POLICY_NEVER`, so its `updateResource` calls emit events unconditionally (bypassing no-change suppression) without a read handler.
- `webrtc-signaling-endpoint`: Replace the "Incoming End command emits `sessionStatus` error" requirement with an `ep/webrtc` asynchronous error/termination event (`webrtcError`); require that all async signaling failures (stream-allocate error, provide-offer error, command timeout, camera `End`) emit that event; require the event resource to be declared `volatile` so no emission is suppressed.
- `camera-stream-reference-command`: The reference command consumes the `ep/webrtc` state event rather than `sessionStatus`, and enforces a connectivity/ICE timeout so a failed exchange results in a bounded, reported teardown instead of an indefinite hang. This capability's spec is also reconciled with the as-built reference command: an `--out <uri>` destination (record to `file://` or serve over HTTP), a decode-free passthrough pipeline (`rtph264depay → h264parse → mp4mux → appsink`) that serves fragmented MP4 to a browser via Media Source Extensions instead of a local display window, host-only ICE, and the `gstreamer-app-1.0`/`gio-2.0` build dependencies.

## Impact

- **SBMD runtime**: `core/deviceDrivers/matter/sbmd/schema/sbmd-spec-schema-v4.0.json` (add `volatile` to the modes enum) and `core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.cpp` (accept `volatile` in `ConvertModesToBitmask`; treat `read || volatile` as `CACHING_POLICY_NEVER` in `DoRegisterDriverResources`).
- **Drivers**: `core/deviceDrivers/matter/sbmd/specs/camera.sbmd.js` — remove `sessionStatus` resource + its two write sites; change `executeStream` to return `{protocol, entryPoint}`; add the `ep/webrtc` `webrtcError` (`volatile`) resource and emit it from the four failure paths.
- **Reference app**: `reference/src/cameraDeviceSession.c` (stop subscribing to `sessionStatus`, subscribe to the new `ep/webrtc` state event), `reference/src/cameraWebrtcClient.c/.h` (watch `ice-connection-state`/`connection-state`, drive `onWebrtcClosed` on failure/timeout), `reference/src/cameraCategory.c` (surface the failure reason).
- **Tests**: `core/test/src/SbmdCameraWebrtcTest.cpp` — the assertion expecting an `updateResource` for `sessionStatus` (and the End→sessionStatus test) must move to the new `ep/webrtc` state event.
- **Specs**: new `camera-session-lifecycle` spec; delta modifications to `webrtc-signaling-endpoint` and `camera-stream-reference-command`.
- **No public GObject API change**; `BCoreClient` resource/event surface is unchanged (only resource URIs on the camera device change).
- **CMake flags**: `BCORE_REFERENCE_CAMERA_SUPPORT` (reference command) unchanged.
