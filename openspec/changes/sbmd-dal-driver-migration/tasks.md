## 0. Prerequisites

- [ ] 0.1 Confirm BARTON-412, BARTON-423, BARTON-421, BARTON-426, BARTON-422, and BARTON-424 are merged (delivery order 412 → 423 → 421 → 426 → 422 → 424 → 425)

## 1. Reproduction tests first (C++ unit)

- [ ] 1.1 Add BARTON-411 scenarios 1–3 (design D6) to `core/test/src/SbmdCameraWebrtcTest.cpp` with delayed fake responses; confirm they fail against the unmigrated driver (~4h)
- [ ] 1.2 Add BARTON-412 scenarios 4–6 (pre-send failure, timeout, allocate failure) asserting no `streaming` session, deallocate count, and `webrtcError` (~3h)
- [ ] 1.3 Add concurrent `createSession` distinct-id test (design D6 scenario 8); confirm it fails against the unmigrated driver (~1h)

## 2. camera.sbmd.js migration

- [ ] 2.1 Convert session writes to `mergeTransientData` per-session patches in execute handlers (`executeCreateSession`, `executeStream`, `executeDestroySession`, `executeLocalSdp`, `executeLocalIceCandidates`); remove writes before `error` terminals; switch `createSession` to a `sequence` supplement for `TD_NEXT_SESSION_ID` (design D1, D3) (~3h)
- [ ] 2.2 Unit tests: existing `SbmdCameraWebrtcTest.cpp` passes; new assertions on per-session writes; scenario 8 (concurrent `createSession`) now passes (~2h)
- [ ] 2.3 Convert deferred handlers (`handleVideoStreamAllocateResponse`, `handleAllocateForSolicit`, `handleSolicitOfferResponse`, `handleProvideOfferResponse`) to fresh supplements and ID-only `context`; fail when the session disappeared during the park (~4h)
- [ ] 2.4 Unit tests for 2.3; scenarios 1–3 now pass (~2h)
- [ ] 2.5 Add `CMD_VIDEO_STREAM_DEALLOCATE` (verify ID), make per-step `onError` handlers return `error` (epic D12), add driver-level `onTransactionFailure: {supplements, handler}` (design D2); bump `driverVersion` to 2; validate against schema (~3h)
- [ ] 2.6 Unit tests: scenarios 4–6 and 9 pass; deallocate sent exactly once or not at all as specified; update existing tests whose `stream` now fails (~3h)

## 3. Other drivers

- [ ] 3.1 Repeat the audit of all `core/deviceDrivers/matter/sbmd/specs/*.sbmd.js` for writes before `error` terminals (including chained-variable patterns) and deferred flows; record findings in the PR; fix any found (~2h)
- [ ] 3.2 Unit tests for any changed driver (~2h)

## 4. Door-lock compensation (C++ unit)

- [ ] 4.1 Add `core/test/src/SbmdTransactionCompensationTest.cpp` with an inline door-lock test driver (SetUser → SetCredential → SetWeekDaySchedule, `onTransactionFailure` issuing ClearCredential then ClearUser) and register it in `core/test/CMakeLists.txt` (~4h)
- [ ] 4.2 Test cases: schedule failure compensates in order exactly once; failure at SetCredential clears only the user; compensating command failure is logged and does not re-invoke the failure handler (~3h)

## 5. Integration tests (Docker)

- [ ] 5.1 Verify matter.js camera cluster support; create `testing/mocks/devices/matterjs/src/CameraDevice.js` with side-band delay/fail controls and command recording (~4h, requires Docker)
- [ ] 5.2 Add `testing/mocks/devices/matter/matter_camera.py` fixture and register in `testing/conftest.py` (~2h, requires Docker)
- [ ] 5.3 Add `testing/test/sbmd_camera_concurrency_test.py` covering scenarios 1, 2, 4, 5 (~4h, requires Docker)

## 6. Documentation and verification

- [ ] 6.1 Update `docs/SBMD.md` camera examples and add "Migrating drivers to the DAL" checklist (~2h)
- [ ] 6.2 Full SBMD unit + pytest suite in Docker (~2h, requires Docker)
- [ ] 6.3 Manual smoke test of create/stream/destroy and a forced stream failure on a physical Matter camera (~2h, requires hardware)
