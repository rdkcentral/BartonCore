## Why

Barton's abstract camera data model (the `camera` endpoint session lifecycle plus a protocol-specific endpoint) currently has exactly one implementation — the Matter WebRTC SBMD driver — so the "protocol-agnostic" claim is unproven. Adding a native driver for a second, unrelated technology (ONVIF/RTSP IP cameras) validates that the model is genuinely technology-independent and establishes the pattern for future non-Matter camera integrations.

## What Changes

- Add a native **C++** ONVIF camera device driver (behind a new `BCORE_ONVIF` / `BARTON_CONFIG_ONVIF` build flag) that manages ONVIF/RTSP IP cameras on the local network.
- The driver discovers cameras via **ONVIF WS-Discovery** through the existing public discovery API (`b_core_client_discover_start(["camera"], …)`), reporting found devices with `deviceServiceDeviceFound` exactly like existing native drivers.
- Each managed camera exposes the existing abstract `ep/camera` endpoint (`createSession`, `stream`, `takePicture`, `destroySession`) plus a new first-class **`ep/onvif`** protocol endpoint (`getMediaUrl` → `mediaUrl`, snapshot delivery, and a non-secret `authRequired` signal).
- `stream` acts as a pure springboard: it ignores `sessionId` and returns `{ "protocol": "onvif", "entryPoint": "/<deviceId>/ep/onvif/r/getMediaUrl" }`. Executing `getMediaUrl` performs an on-demand ONVIF `GetStreamUri` SOAP call and emits the resulting credential-free RTSP URL as a `mediaUrl` event.
- `takePicture` performs an on-demand ONVIF `GetSnapshotUri` SOAP call and delivers the JPEG URL.
- Camera **credentials** are stored as `RESOURCE_TYPE_PASSWORD` resources with the existing `RESOURCE_MODE_SENSITIVE` bit (encrypted at rest, redacted in logs). Configuration is **lazy**: `configureDevice` creates endpoints and credential resources but makes **no** authenticated ONVIF calls; authentication (WS-UsernameToken) happens on-demand at `getMediaUrl`/`takePicture` time.
- The auth model in this driver version is deliberately **primitive** (static per-device username/password written out-of-band; no per-stream tokens, rotation, expiry, or configuration-driven management) and is explicitly documented as **interim** — it will likely need rethinking once device configuration/onboarding is formalized.
- Extend the reference app's existing `cameraStream` (`cs`) command so ONVIF/RTSP cameras are driven through the **same command interface** as WebRTC: after the `stream` springboard, the command branches on the returned `protocol`, follows the ONVIF entry point to obtain the RTSP URL from a `mediaUrl` event, and plays/records it via a GStreamer `rtspsrc` source reusing the existing fragmented-MP4 → media-server/file plumbing. WebRTC support is unchanged and coexists.
- Add a lightweight **mock ONVIF device** (Python stdlib: UDP WS-Discovery responder + canned SOAP responses) and a pytest integration test that drives discover → stream → `getMediaUrl` → `takePicture` through the public client API.
- Generalize the abstract camera-session-lifecycle spec so its requirements explicitly admit native (non-SBMD) drivers and non-WebRTC protocols.

## Capabilities

### New Capabilities
- `onvif-camera-driver`: The native ONVIF camera driver — WS-Discovery-based discovery via the public API, lazy credentialed configuration, the `ep/onvif` protocol endpoint, RTSP media-URL retrieval (`GetStreamUri`), snapshot retrieval (`GetSnapshotUri`), and WS-UsernameToken authentication.
- `onvif-mock-device`: A test-only ONVIF device mock that answers WS-Discovery probes and serves canned ONVIF SOAP responses (`GetDeviceInformation`, `GetProfiles`, `GetStreamUri`, `GetSnapshotUri`) for integration tests without real hardware.

### Modified Capabilities
- `camera-session-lifecycle`: Generalize the abstract-endpoint requirements from "the camera SBMD driver / Matter camera" wording to any camera driver, and add scenarios for a non-WebRTC (`onvif`) protocol whose `stream` result points at `ep/onvif`. No behavior is removed; the WebRTC scenarios remain.
- `camera-stream-reference-command`: Extend the `cameraStream` (`cs`) command to branch on the active protocol so it drives both WebRTC and ONVIF/RTSP cameras through the same command interface. WebRTC orchestration is scoped to the `webrtc` protocol; a new ONVIF/RTSP path (credentials, `getMediaUrl`/`mediaUrl`, `rtspsrc`) is added. Existing WebRTC behavior is preserved.

## Impact

- **Affected layers**: Device drivers (new `core/deviceDrivers/onvif/` C++ driver); API/core headers (`commonDeviceDefs.h` gains `ep/onvif` + credential resource names); build system (`BCORE_ONVIF` option, `BARTON_CONFIG_ONVIF` define); reference app (`reference/src/camera*` — ONVIF/RTSP branch in the `cs` command, reusing the GStreamer/media-server plumbing); test infrastructure (mock device + integration test).
- **Dependencies**: The driver itself needs nothing new — `libcurl`, `libxml2`, and OpenSSL/glib crypto are already in the builder image, and it returns URLs only (no media stack). The mock's WS-Discovery and SOAP responders use Python stdlib only; its live RTSP server (for end-to-end media validation) uses the GStreamer GObject-Introspection bindings, which adds one builder-image package (`gir1.2-gst-rtsp-server-1.0`) as the stack's commit 0.
- **Coexistence**: Both the Matter camera SBMD driver and the ONVIF driver declare device class `camera`; ownership is decided by which driver discovers the device (Matter commissioning vs. ONVIF WS-Discovery), so they coexist without conflict.
- **CMake feature flags**: New `BCORE_ONVIF` (off by default), gating `BARTON_CONFIG_ONVIF`.

## Non-goals

- No IP-camera subsystem, keep-alive, liveness probing, or comm-fail monitoring (punted until proven necessary during integration).
- No media handling: the driver never opens, decodes, proxies, or tears down RTSP/RTP streams — it only hands the client a URL.
- No media-profile selection UI/parameterization; a single profile is hardcoded for now.
- No PTZ, two-way audio, ONVIF events, or firmware/RMA support.
- No productization hardening (multi-subnet discovery, credential rotation, cloud onboarding); real-hardware validation on a different network is deferred to manual testing.
- No advanced auth: the credential model is intentionally primitive/interim for this version (static per-device secret set out-of-band); a configuration-driven credential model is out of scope and expected to replace it in a later iteration.
- Credentials are never embedded in returned URLs or emitted in events.
