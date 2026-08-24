## 1. Commit 0 — Builder-image dependency (for the mock RTSP server)

- [x] 1.1 Confirm the driver's own runtime needs (libcurl, libxml2, glib `g_checksum`/`g_base64`) are already satisfied by the builder image — no `docker/Dockerfile` change is needed for the driver itself
- [x] 1.2 Add the one builder-image dependency the mock's live RTSP server requires — `gir1.2-gst-rtsp-server-1.0` — to `docker/Dockerfile` (folded into the mock commit; requires a Docker image rebuild/publish)

## 2. Commit 1 — Camera model definitions (API/headers)

- [x] 2.1 Add ONVIF constants to `commonDeviceDefs.h`: `ONVIF_PROFILE` (`"onvif"`), `ep/onvif` resource names (`getMediaUrl`, `mediaUrl`, `getSnapshotUrl`, `snapshotUrl`, `authRequired`), and credential resource names (`username`, `password`)
- [x] 2.2 Verify the header compiles into an existing consumer and that names follow the file's conventions (no behavior; compile-only verification)

## 3. Commit 2 — ONVIF SOAP client (standalone C++)

- [x] 3.1 Implement an `OnvifSoapClient` unit (no `DeviceDriver` coupling) issuing `GetDeviceInformation`, `GetProfiles`, `GetStreamUri`, `GetSnapshotUri` over libcurl, parsing responses with libxml2
- [x] 3.2 Implement WS-UsernameToken digest `Base64(SHA1(nonce + created + password))` using glib `g_checksum`/`g_base64`
- [x] 3.3 Unit test (Google Test) the SOAP client and UsernameToken against canned XML fixtures, including credential-free URL parsing

## 4. Commit 3 — WS-Discovery probe (standalone C++)

- [x] 4.1 Implement a WS-Discovery unit that sends a `Probe` (UDP multicast `239.255.255.250:3702`) and parses `ProbeMatch`, extracting the `urn:uuid` endpoint reference and ONVIF service address
- [x] 4.2 Unit test (Google Test) `ProbeMatch` parsing and `urn:uuid` → device-uuid derivation against canned payloads

## 5. Commit 4 — ONVIF device driver + build wiring

- [x] 5.1 Add the `BCORE_ONVIF` CMake option (default OFF) → `BARTON_CONFIG_ONVIF`, wire `core/deviceDrivers/onvif/` sources and library links in `core/CMakeLists.txt`
- [x] 5.2 Implement the driver skeleton: C++ class behind `extern "C"` thunks with `this` in `callbackContext`, self-registered via `__attribute__((constructor))`, declaring device class `"camera"`
- [x] 5.3 Implement `discoverDevices` (returns immediately; runs WS-Discovery on a worker thread via commit 3) and `GetDeviceInformation`, then `deviceServiceDeviceFound`
- [x] 5.4 Implement `configureDevice` to create `ep/camera` (four lifecycle executes) and `ep/onvif` (media/snapshot/auth/credential resources) with **no** authenticated ONVIF calls; credentials as `RESOURCE_MODE_SENSITIVE`
- [x] 5.5 Implement `executeResource` for `createSession`/`stream`/`takePicture`/`destroySession` as stateless springboards (ignore `sessionId`)
- [x] 5.6 Implement `getMediaUrl`/`getSnapshotUrl` to call the SOAP client (commit 2) on-demand and emit `mediaUrl`/`snapshotUrl` events marshalled onto the GLib main loop via `g_main_context_invoke`; guard driver state with a `pthread` mutex
- [x] 5.7 Prominently document in the driver module/header that the auth model is primitive/interim (static per-device credentials; no rotation/tokens/config-driven provisioning) and likely to be reworked with future configuration support
- [x] 5.8 Unit test (Google Test + FFF) driver behavior: registration/wiring contract, springboard results, and clean `destroyDriver` (ASan-clean `delete` of the C++ instance)

## 6. Commit 5 — Mock ONVIF device (test asset)

- [x] 6.1 Implement a Python stdlib WS-Discovery responder (UDP) that answers `Probe` with a `ProbeMatch` carrying a `urn:uuid` and service address
- [x] 6.2 Implement a Python stdlib `http.server` responder serving canned `GetDeviceInformation`/`GetProfiles`/`GetStreamUri`/`GetSnapshotUri` SOAP responses with credential-free URIs
- [x] 6.3 Add a pytest fixture that starts/stops the mock; unit-test the mock's responses in isolation (no driver)

## 7. Commit 6 — Integration test (requires Docker)

- [x] 7.1 Add a pytest integration test that, via the public `BCoreClient` API against the mock, discovers the camera, writes credentials, executes `stream` and observes a `mediaUrl` event, then executes `takePicture` and observes a `snapshotUrl` event
- [x] 7.2 Handle WS-Discovery reachability in CI (bind the mock responder to the controlled interface; add a unicast-to-mock fallback seam if multicast is blocked)

## 8. Commit 7 — Reference-app ONVIF/RTSP support (same `cs` command)

- [x] 8.1 Branch the `cameraStream` (`cs`) command on the `stream` result `protocol`, preserving the existing WebRTC path unchanged and routing `onvif` to a new ONVIF flow
- [x] 8.2 Implement the ONVIF flow: read `authRequired`, write credentials from optional `--user`/`--pass` flags, subscribe to `mediaUrl`, execute `getMediaUrl`, and open a GStreamer `rtspsrc` source feeding the shared `mp4mux → appsink → media-server/file` sink
- [x] 8.3 Wire `takePicture` → `getSnapshotUrl` → `snapshotUrl` for the ONVIF path (via `--snapshot`, fetched over HTTP with libcurl); document the `--user`/`--pass` flags as an interim credential mechanism
- [x] 8.4 Manual/verification: the mock now runs a live GStreamer RTSP server (dummy H.264 test pattern) and serves a canned snapshot JPEG, so the ONVIF media plane is validated end-to-end in CI — `test_onvif_stream_media_flows` opens the driver-reported RTSP URL and asserts buffers flow, and `test_onvif_snapshot_fetch_returns_jpeg` fetches the snapshot URL. (Spawning the reference `cs` binary for a WebRTC+ONVIF coexistence run remains an optional follow-up; the shared rtspsrc→mux chain is the same one exercised here.)

## 9. Finalize

- [x] 9.1 Apply clang-format (diff-only) and Apache-2.0 headers to all new C/C++ files
- [x] 9.2 Run unit tests (`ctest`) with `BCORE_ONVIF=ON` and the ONVIF integration test; confirm green (416 unit tests + 4 ONVIF integration tests pass)
- [x] 9.3 Validate the change (`openspec validate` — valid); the prerequisite `execute_resource` out-param commit builds independently, and the remaining ONVIF work forms the follow-on stack commit(s)

## 10. Post-review refinements

- [x] 10.1 Refactor the reference-app `cs` command into a `CameraStreamBackend` strategy: `cameraCategory` is now technology-agnostic orchestration that selects a backend by the `stream` protocol; shared plumbing lives in `CameraStreamContext` and each protocol (WebRTC, ONVIF/RTSP) owns its own state — removing the `CameraStreamState` grab-bag. Behavior-preserving (validated live).
- [x] 10.2 Delegate `OnvifXml` find/text/attribute helpers to bartoncommon's `xhXmlHelper` (`findChildNode`/`getXmlNodeContentsAsString`/`getXmlNodeAttributeAsString`), keeping only the collect-all-by-local-name helper (no bartoncommon equivalent) and the `std::string` surface. All ONVIF unit tests still pass.

