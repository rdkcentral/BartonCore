## Why

The `webrtc` endpoint's `negotiationRole` resource currently reports the role the *client* must take (`offerer`/`answerer`). But `ep/webrtc` is the camera's Matter data model — a resource on the camera's endpoint should describe the *camera*, not the client consuming it. Reporting the client's role inverts the perspective, leaks a client concern into the device model, and forces every reader to know that the value is "the opposite of what the device does."

## What Changes

- **BREAKING**: `negotiationRole` (on `ep/webrtc`) SHALL report the **camera's** WebRTC negotiation role — `offerer` when the camera generates the SDP offer, `answerer` when the camera answers the client's offer — instead of the client's role.
- Invert the driver's mapping from the camera's advertised `AcceptedCommandList` to the reported role: camera accepts `SolicitOffer` → camera is the `offerer`; camera accepts `ProvideOffer` → camera is the `answerer`. The default (SolicitOffer flow) therefore reports `offerer`.
- Decouple the internal SolicitOffer/ProvideOffer flow selection in `localSdp` from the public role value so the Matter signaling sequence is byte-for-byte unchanged; only the reported role string and its interpretation change.
- Update the reference app to derive its own role by inverting the camera's reported role: act as the **answerer** when the camera role is `offerer`, and as the **offerer** when the camera role is `answerer`.
- Update the affected specs, driver comments, and tests to the camera-perspective contract.

## Capabilities

### New Capabilities

<!-- none -->

### Modified Capabilities

- `webrtc-signaling-endpoint`: the `negotiationRole` requirement changes so the read reports the camera's role (not the client's); the `AcceptedCommandList` → role mapping is inverted (`SolicitOffer` → `offerer`, `ProvideOffer` → `answerer`, default `offerer`).
- `camera-stream-reference-command`: the reference app's orchestration requirement changes so it reads the camera's role and inverts it to select its own WebRTC role.

## Non-goals

- No change to the underlying Matter signaling flow: the `VideoStreamAllocate` → `SolicitOffer`/`ProvideOffer` → `ProvideAnswer`/`ProvideICECandidates` sequence, the incoming `Offer`/`Answer`/`ICECandidates`/`End` handlers, and the `remoteSdp`/`remoteIceCandidates`/`webrtcError` event resources are all unchanged.
- No change to the `negotiationRole` resource's type or modes (it remains a `read` `string` on `ep/webrtc`).
- No new streaming protocols, camera features, or session-lifecycle changes.

## Impact

- **Core driver** (`core/deviceDrivers/matter/sbmd/specs/camera.sbmd.js`): `pickNegotiationRole` / `readNegotiationRole` and the `localSdp` flow-selection logic; the `ROLE_OFFERER`/`ROLE_ANSWERER` usage and the endpoint/flow comments.
- **Reference app** (`reference/src/cameraCategory.c`, `cameraDeviceSession.c` / `.h`, `cameraWebrtcClient.*` comments): the role interpretation, which becomes `answerer = (cameraRole == "offerer")`.
- **Specs**: `openspec/specs/webrtc-signaling-endpoint/spec.md` and `openspec/specs/camera-stream-reference-command/spec.md`.
- **Tests**: SBMD camera unit test(s) that assert `negotiationRole`, and any Python integration test asserting the role value.
- **CMake flags**: the reference-app portion is gated by `BCORE_REFERENCE_CAMERA_SUPPORT`; the driver portion is always built.
- **Compatibility**: the only in-tree consumer of `negotiationRole` is the reference app, which is updated in lockstep, so no runtime mismatch ships. The contract inversion is nonetheless breaking for any independent client that assumed the old client-perspective semantics.
