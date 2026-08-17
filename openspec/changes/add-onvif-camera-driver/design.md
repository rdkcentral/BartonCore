## Context

Barton's camera data model is two-layered: an abstract `ep/camera` endpoint that owns a
protocol-agnostic session lifecycle (`createSession`, `stream`, `takePicture`,
`destroySession`), plus a protocol-specific endpoint that carries the technology details. Today
the only implementation is the Matter WebRTC SBMD driver, and the abstract contract exists only in
`camera.sbmd.js` (server) and `reference/src/cameraDeviceSession.c` (client). No native driver
implements the contract, so "protocol-agnostic" is unproven.

ONVIF is an IP-camera standard whose streaming maps to the archived design's "direct / immediate
URL" shape: no SDP/ICE negotiation — a couple of SOAP calls yield an RTSP URI. This makes it an
ideal second technology to validate the model. Native drivers in Barton are the norm (all Zigbee
drivers, Philips Hue) and plug into `deviceService` through the C `DeviceDriver` struct of function
pointers; the Matter driver is the only C++ one. Discovery flows through the public API
(`b_core_client_discover_start(["camera"], …)` → `DeviceDriver.discoverDevices` → `deviceServiceDeviceFound`).

The resource model already provides `RESOURCE_MODE_SENSITIVE` (encrypted at rest, redacted in logs)
and `RESOURCE_TYPE_PASSWORD`, so camera credentials need no new infrastructure.

```
                        public discovery API
  b_core_client_discover_start(["camera"], filters, timeout)
                              │
                              ▼
                deviceService → DeviceDriver.discoverDevices(ctx,"camera")
                              │  (OnvifDriver: WS-Discovery Probe on worker thread)
                              ▼
                deviceServiceDeviceFound(DeviceFoundDetails{uuid=urn:uuid,…})
                              │
                              ▼   configureDevice (no auth calls)
   ┌─────────────────────────────────────────────────────────────────┐
   │ Camera device (deviceClass "camera")                             │
   │                                                                   │
   │  ep/camera  (profile "camera")  ── abstract springboard          │
   │    createSession  → sessionId                                     │
   │    stream         → {protocol:"onvif", entryPoint:.../ep/onvif/r/getMediaUrl}
   │    takePicture    → {protocol:"onvif", entryPoint:.../ep/onvif/r/getSnapshotUrl}
   │    destroySession → local cleanup                                 │
   │                                                                   │
   │  ep/onvif   (profile "onvif")   ── protocol delivery + creds      │
   │    getMediaUrl   [execute] → mediaUrl   [event] = rtsp://…        │
   │    getSnapshotUrl[execute] → snapshotUrl[event] = http://…jpg     │
   │    authRequired  [read]    = "true"      (non-secret signal)      │
   │    username      [write, SENSITIVE]                               │
   │    password      [write, SENSITIVE, PASSWORD]                     │
   └─────────────────────────────────────────────────────────────────┘
        getMediaUrl / getSnapshotUrl → on-demand ONVIF SOAP
        (GetStreamUri / GetSnapshotUri) with WS-UsernameToken using stored creds
```

## Goals / Non-Goals

**Goals:**
- Prove the abstract camera contract is implementable by a native (non-SBMD, non-Matter) driver.
- Establish the reusable "native C++ driver" pattern (C++ class behind `extern "C"` thunks).
- Drive the *existing* reference-app camera client unchanged against an ONVIF camera.
- Resolve the model's deferred edges: credential onboarding, snapshot delivery, URL secrecy.
- Keep the change additive and off-by-default (`BCORE_ONVIF`).

**Non-Goals:**
- IP-camera subsystem, keep-alive, liveness, comm-fail monitoring.
- Any media handling (RTSP/RTP open/decode/proxy/teardown) — the driver returns URLs only.
- Media-profile selection, PTZ, two-way audio, ONVIF events, firmware/RMA.
- Productization (multi-subnet discovery, credential rotation). Real-hardware validation is deferred.

## Decisions

### D1: Native C++ driver behind `extern "C"` thunks
The `DeviceDriver` is a C struct of function pointers with an opaque `callbackContext`. The ONVIF
driver is a C++ class whose instance pointer is stored in `callbackContext`; each `DeviceDriver`
callback is a static `extern "C"` thunk that casts `ctx` back to the object. Self-registration uses
`__attribute__((constructor))` like Philips Hue. **Why not C:** the SOAP/XML/session logic benefits
from RAII and STL containers; the Matter driver already sets the C++ precedent. **Why not extend an
existing base (à la `zigbeeDriverCommon`):** there is no IP-camera common layer and building one is
out of scope (KISS).

### D2: First-class `ep/onvif` endpoint, protocol id `"onvif"`
ONVIF gets its own protocol endpoint and identifier rather than reusing a generic `"direct"`
profile. **Why:** ONVIF has real, non-generic semantics (snapshot, auth) and making it first-class
makes the proof more convincing that the abstract layer is truly a thin springboard. Profiles are
just strings passed to `createEndpoint`, so this costs nothing structurally.

### D3: `stream` and `takePicture` are pure springboards; `sessionId` is ignored
Mirroring the Matter `executeStream`, ONVIF's `stream` validates nothing about sessions and returns
`{protocol, entryPoint}`. `takePicture` behaves symmetrically, returning a springboard to
`getSnapshotUrl`. The mandatory `sessionId` argument may be anything (including null); the driver
does not track sessions because ONVIF URLs are stateless. **Why symmetric springboards:** keeps the
abstract endpoint uniformly "return where to go next," and routes *all* ONVIF-specific delivery
through `ep/onvif`. **Alternative considered:** `takePicture` returns the JPEG URL directly — simpler
but breaks the symmetry and bypasses the protocol endpoint; rejected for consistency.

### D4: Lazy, credential-free configuration
`configureDevice` creates the endpoints and the (empty) sensitive credential resources but makes
**no authenticated ONVIF calls**. The first authenticated SOAP call happens on-demand at
`getMediaUrl` / `getSnapshotUrl`. **Why:** ONVIF has no reliable factory-default credential and the
public discovery API carries no credential channel, so credentials arrive *after* the device exists
(client writes the sensitive resources). Lazy config sidesteps the chicken-and-egg with no onboarding
state machine. If credentials are missing/invalid at call time, the execute returns an error.

### D5: Credential-free URLs + non-secret `authRequired` signal
`GetStreamUri`/`GetSnapshotUri` return credential-free URLs (per ONVIF spec); the driver emits them
verbatim in `mediaUrl`/`snapshotUrl` events. RTSP/HTTP auth uses the **same persistent device
credentials** the ONVIF calls use; the client already holds them (it wrote them). The model surfaces
only a non-secret `authRequired` resource so the client knows to apply those credentials. **Why:**
never place secrets in a resource value, event, or URL (OWASP sensitive-data-exposure). Secrets live
only in `SENSITIVE` resources.

### D5a: The auth model is primitive and interim (v1)
This version's authentication is deliberately minimal and **must be understood as a stopgap**. The
credential model is a single static per-device `username`/`password` pair, written out-of-band by
the client into `SENSITIVE` resources, with **no** per-stream tokens, credential rotation, expiry,
separate media accounts, TLS/certificate handling, or configuration-driven credential provisioning.
There is no onboarding state machine — credentials simply exist or do not at call time. **Why accept
this:** it is the smallest thing that proves the data model end-to-end without inventing a
configuration subsystem that does not yet exist. **Consequence / follow-up:** when Barton gains a
formal device-configuration/onboarding mechanism, this auth model will very likely need rethinking
(e.g. structured credential provisioning, rotation, per-camera media users). This limitation SHALL be
called out prominently in the driver source (header/module doc comment) so future maintainers do not
mistake it for a finished design. See Open Questions.

### D5b: Reference-app coexistence — one command, protocol branch
The reference app's existing `cameraStream` (`cs`) command SHALL drive both WebRTC and ONVIF cameras
through the **same command interface**. After the shared `createSession` + `stream` springboard, the
command branches on the returned `protocol`:
- `"webrtc"` → the existing negotiationRole/SDP/ICE flow over `ep/webrtc` using GStreamer `webrtcbin`
  (unchanged).
- `"onvif"` → read `authRequired`, apply credentials (supplied via optional command flags for this
  primitive version), subscribe to `mediaUrl` on `ep/onvif`, execute `getMediaUrl`, and build a
  GStreamer `rtspsrc → rtph264depay → h264parse → … → mp4mux → appsink` pipeline that feeds the **same**
  media-server / file sink the WebRTC path already uses. `takePicture` → `snapshotUrl` is handled
  analogously.
**Why:** only the media *source* differs (webrtcbin vs rtspsrc); the downstream mux/serve/record
plumbing and the session lifecycle are shared. This keeps a single user-facing command and reuses
nearly all existing reference-app code. **Alternative considered:** a separate `onvifStream` command
— rejected because it duplicates the lifecycle/output plumbing and splits the user interface.

### D6: WS-UsernameToken via glib crypto; SOAP via libcurl + libxml2
WS-UsernameToken digest = Base64(SHA1(nonce + created + password)). Implemented with glib
`g_checksum` (SHA1) and `g_base64_encode`, avoiding a new direct OpenSSL dependency. SOAP requests go
over libcurl; responses (and WS-Discovery ProbeMatch) are parsed with libxml2. All three libraries
are already present. **Why glib crypto:** fewer direct dependencies and glib is already core.

### D7: WS-Discovery on a worker thread; events marshalled to the GLib main loop
`discoverDevices` returns immediately and runs a WS-Discovery Probe (UDP multicast
`239.255.255.250:3702`) on a background thread, parsing ProbeMatch for the `urn:uuid` endpoint
reference (→ device `uuid`) and issuing an anonymous `GetDeviceInformation` for manufacturer/model/
firmware, then calls `deviceServiceDeviceFound`. All `updateResource`/event emissions (e.g.
`mediaUrl`) are marshalled onto the GLib main loop via `g_main_context_invoke`, following the Matter
subsystem pattern. Session-free driver state (discovered-device map, credential cache if any) is
guarded by a `pthread` mutex.

### D8: Mock-first testing with a Python ONVIF mock
A test-only mock answers WS-Discovery probes (UDP) and serves canned SOAP responses over
`http.server` using only Python stdlib — matching the repo's existing mock convention
(`http_fixture_server.py`). To validate the media plane end-to-end, the mock also runs a live
GStreamer RTSP server publishing a dummy H.264 test pattern and serves a canned JPEG for snapshots.
The pytest integration test drives discover → stream → `getMediaUrl` → `takePicture` through the
public client API, then opens the reported RTSP URL (buffers must flow) and fetches the snapshot URL
(JPEG bytes). **Why:** the driver returns URLs only, so a live source is the only way to prove the
RTSP/snapshot wiring; the GStreamer RTSP server reuses bindings already in the image and costs one
builder-image package (`gir1.2-gst-rtsp-server-1.0`) rather than an off-the-shelf simulator.

### D9: Independent, mutually-exclusive commit stack
The work lands as a `gh stack` of commits, ordered so each compiles/tests standalone where possible:
```
(prereq)    fix(api)!: annotate execute_resource `response` as an (out) param   [BREAKING]
(commit 0)  docker/Dockerfile        ← add gir1.2-gst-rtsp-server-1.0 (live RTSP mock)
 1  camera model defs (commonDeviceDefs.h: ep/onvif + credential resource names)
 2  ONVIF SOAP client (standalone C++ + unit test)
 3  WS-Discovery probe (standalone C++ + unit test)
 4  ONVIF DeviceDriver C++ + build wiring (BCORE_ONVIF/BARTON_CONFIG_ONVIF)   [deps 1,2,3]
 5  mock ONVIF device (Python stdlib + GStreamer RTSP server)
 6  integration test (pytest via public client)                              [deps 4,5]
 7  reference-app ONVIF/RTSP branch in the `cs` command (rtspsrc)             [deps 1,4]
```
The **prerequisite** commit fixes a latent GObject-Introspection bug: `b_core_client_execute_resource`'s
`response` had no direction annotation, so bindings treated it as input — a handler that writes a
response (the camera `stream`/`takePicture` springboard) could not have it retrieved from Python and
overflowed the caller's buffer. Annotating it `(out)` is a **breaking** binding-signature change, so it
is a `!` conventional commit; cocogitto bumps the major version and the derived GIR namespace
(`BCore-${MAJOR}.0` → next major), leaving existing-version consumers on the old contract. C callers
(which pass `&response`/NULL) are unaffected. This unblocks the Python integration test from validating
the abstract springboard result. Commits 1–3 and 5 touch disjoint files; 4, 6, and 7 are the
assembly/verification points. Commit 7 depends only on the endpoint/resource names (1) and the running
driver (4), so it can proceed in parallel with the mock/integration commits.

## Risks / Trade-offs

- **[WS-Discovery multicast may not traverse the dev-container/mock network]** → Mitigation: the mock
  binds the discovery responder to the loopback/bridge the test controls; if multicast is blocked in
  CI, fall back to a unicast Probe to a configured mock address (test-only seam).
- **[No off-the-shelf, permissively-licensed ONVIF+WS-Discovery simulator may exist]** → Mitigation:
  the stdlib mock (D8) removes the dependency entirely; confirmed the mock scope is small.
- **[Lazy config hides credential errors until stream time]** → Mitigation: `getMediaUrl`/
  `getSnapshotUrl` return clear, distinct errors for "no credentials" vs "auth rejected"; documented
  in the `onvif-camera-driver` spec scenarios.
- **[Device-class `"camera"` claimed by two drivers]** → Mitigation: ownership is determined by which
  driver discovers the device; ONVIF only reports devices found via WS-Discovery, Matter only via
  commissioning — no overlap.
- **[Symmetric snapshot springboard adds two resources vs. a direct return]** → Trade-off accepted for
  a uniform abstract endpoint and single delivery surface (`ep/onvif`).
- **[IP changes / device liveness unmodeled]** → Trade-off accepted (Non-goal); a LAN camera keeps its
  DHCP lease in practice, and re-discovery re-resolves it. Revisit only if it bites during integration.

## Migration Plan

Purely additive and off-by-default (`BCORE_ONVIF=OFF`). No migration of existing devices, no public
API/GIR changes (endpoints and the `authRequired`/credential resources surface through the existing
`BCoreEndpoint`/`BCoreResource` types — no new signals or properties). The `camera-session-lifecycle`
spec is generalized without removing any WebRTC requirement, so existing Matter camera behavior is
unchanged. Rollback = revert the stack or leave the flag off; no persisted-state implications.

## Open Questions

- The primitive auth model (D5a) is an explicit interim: what does the eventual configuration-driven
  credential model look like (structured provisioning, rotation, per-camera media users, TLS)? To be
  designed when Barton's device-configuration/onboarding mechanism exists.
- Exact WS-Discovery fallback seam for CI (unicast-to-mock) — finalize during the mock/test commits.
- Whether `authRequired` is better expressed as a boolean resource or an `authScheme` string
  (e.g. `"digest"`); starting with a boolean, revisit if a second scheme appears.
- Whether a permissively-licensed ONVIF mock exists worth adopting instead of the stdlib mock —
  quick check during commit 5; default is the stdlib mock.
