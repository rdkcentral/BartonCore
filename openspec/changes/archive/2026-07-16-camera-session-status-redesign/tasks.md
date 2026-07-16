## 0. SBMD runtime — volatile (non-cached) resource mode

- [x] 0.1 Add `volatile` to the modes enum in `core/deviceDrivers/matter/sbmd/schema/sbmd-spec-schema-v4.0.json` with a description
- [x] 0.2 Accept `volatile` in `ConvertModesToBitmask` (no mode bit) in `SpecBasedMatterDeviceDriver.cpp`
- [x] 0.3 In `DoRegisterDriverResources`, set `CACHING_POLICY_NEVER` when a resource has a read handler OR declares the `volatile` mode
- [x] 0.4 Validate the `volatile` mode: camera-device registration accepts `modes: ['volatile']` (the `webrtc` endpoint + `webrtcError` resource register with no "Unsupported resource mode" error), confirmed via E2E commissioning. No standalone caching-policy unit test was added — there is no existing device-registration/cachingPolicy test harness and it is disproportionate for the 3-line derivation change; the unconditional-emit behavior is exercised by the E2E path.

## 1. Camera SBMD driver — remove sessionStatus

- [x] 1.1 Remove the `sessionStatus` resource declaration from the `ep/camera` endpoint in `camera.sbmd.js`
- [x] 1.2 Change `executeStream` to return `{ protocol, entryPoint }` as its success result and delete its `sessionStatus`/`STATUS_SETUP` `updateResource` call and `nextAction` metadata
- [x] 1.3 Remove the now-unused `STATUS_SETUP`/`STATUS_DONE`/`STATUS_ERROR` constants and update the header comment block to describe the entry-point-return contract (no traffic-controller language)

## 2. Camera SBMD driver — add webrtcError event

- [x] 2.1 Declare a `webrtcError` event-only resource on the `ep/webrtc` endpoint with `modes: ['volatile']` so `updateResource` bypasses value-change suppression
- [x] 2.2 Add a shared helper that emits `webrtcError` with a value + `{ reason, detail }` metadata
- [x] 2.3 Rewrite `handleIncomingEnd` to emit `webrtcError` (ended) instead of `sessionStatus` error, preserving session cleanup
- [x] 2.4 Emit `webrtcError` (failed) from `handleVideoStreamAllocateError` with the allocate error detail
- [x] 2.5 Emit `webrtcError` (failed) from `handleProvideOfferError` with the provide-offer error detail
- [x] 2.6 Emit `webrtcError` (failed) from the offer-flow `requestCommand` timeout path (allocate + provide-offer `timeoutMs` continuations) with a timeout reason

## 3. Reference app — consume webrtcError and drop sessionStatus

- [x] 3.1 In `cameraDeviceSession.c`, remove the `sessionStatus` URI subscription/handling and subscribe to `ep/webrtc/r/webrtcError`, mapping ended/failed to the existing `onError` teardown with the metadata reason
- [x] 3.2 Parse the `stream` execute result for `{ protocol, entryPoint }` and use `entryPoint` instead of a hard-coded webrtc URI where the flow begins
- [x] 3.3 Update `cameraCategory.c` progress output to report `protocol`/`entryPoint` and to surface the `webrtcError` failure reason on teardown

## 4. Reference app — connectivity/ICE timeout

- [x] 4.1 In `cameraWebrtcClient.c`, watch `webrtcbin` `ice-connection-state` (and/or `connection-state`); on `failed`, request teardown via the existing `onWebrtcClosed` cross-thread path with a failure reason
- [x] 4.2 Start a bounded connectivity timer when signaling completes; if not `connected`/`completed` before it expires, request teardown via `onWebrtcClosed`; reset/cancel the timer on first media
- [x] 4.3 Expose the timeout window as a named constant with margin over the observed worst-case first-media latency

## 5. Tests and validation

- [x] 5.1 Update `SbmdCameraWebrtcTest.cpp`: replace the `sessionStatus` `updateResource` assertion with a `webrtcError` assertion, add coverage for the allocate/provide-offer/timeout failure emissions, and assert the `stream` result shape `{ protocol, entryPoint }`
- [x] 5.2 Run `validate-sbmd` on `camera.sbmd.js` and the C++ unit suite; confirm green
- [x] 5.3 Manual end-to-end: happy path streams (found + fixed an `entryPoint` URI bug — driver emitted `/devices/<id>/...` but Barton URIs are `/<id>/...`); connection reaches CONNECTED, watchdog cancels with no false timeout, no spurious `webrtcError`. Pre-connection ICE-failure watchdog and `webrtcError` emissions (End/allocate/provide-offer/timeout) are covered by unit tests; post-connection media loss is out of scope (unchanged pre-existing behavior).
- [x] 5.4 `clang-format` all touched C/C++ files from the repo root and confirm formatting/hooks pass
