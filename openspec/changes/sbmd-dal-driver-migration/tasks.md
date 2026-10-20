## 0. Prerequisites

- [ ] 0.1 Confirm BARTON-412, BARTON-421, BARTON-422, BARTON-423, BARTON-424 are merged

## 1. Reproduction tests first (C++ unit)

- [ ] 1.1 Add BARTON-411 scenarios 1–3 (design D4) to `core/test/src/SbmdCameraWebrtcTest.cpp` with delayed fake responses; confirm they fail against the unmigrated driver (~4h)
- [ ] 1.2 Add BARTON-412 scenarios 4–5 (pre-send failure, timeout) asserting no `streaming` session and single deallocate (~3h)

## 2. camera.sbmd.js migration

- [ ] 2.1 Convert session writes to `mergeTransientData` member patches in execute handlers (`executeCreateSession`, `executeStream`, `executeDestroySession`, `executeLocalSdp`, `executeLocalIceCandidates`) (~3h)
- [ ] 2.2 Unit tests: existing `SbmdCameraWebrtcTest.cpp` passes; new assertions on per-member writes (~2h)
- [ ] 2.3 Convert deferred handlers (`handleVideoStreamAllocateResponse`, `handleAllocateForSolicit`, `handleSolicitOfferResponse`, `handleProvideOfferResponse`, error handlers) to fresh supplements and id-only `context` (~4h)
- [ ] 2.4 Unit tests for 2.3; scenarios 1–3 now pass (~2h)
- [ ] 2.5 Add `handleDeallocateStreamCleanup` via `onFailure`; remove duplicated deallocation from per-hop `onError`; bump `driverVersion` to 2; validate against schema (~3h)
- [ ] 2.6 Unit tests: scenarios 4–5 pass; deallocate sent exactly once (~2h)

## 3. Other drivers

- [ ] 3.1 Audit all `core/deviceDrivers/matter/sbmd/specs/*.sbmd.js` for writes before `error` terminals; fix any found (~2h)
- [ ] 3.2 Unit tests for any changed driver (~2h)

## 4. Integration tests (Docker)

- [ ] 4.1 Verify matter.js camera cluster support; create `testing/mocks/devices/matterjs/src/CameraDevice.js` with side-band delay/fail controls and command recording (~4h, requires Docker)
- [ ] 4.2 Add `testing/mocks/devices/matter/matter_camera.py` fixture and register in `testing/conftest.py` (~2h, requires Docker)
- [ ] 4.3 Add `testing/test/sbmd_camera_concurrency_test.py` covering scenarios 1, 2, 4, 5 (~4h, requires Docker)

## 5. Documentation and verification

- [ ] 5.1 Update `docs/SBMD.md` camera examples and add "Migrating drivers to the DAL" checklist (~2h)
- [ ] 5.2 Full SBMD unit + pytest suite in Docker (~2h, requires Docker)
- [ ] 5.3 Manual smoke test of create/stream/destroy on a physical Matter camera (~2h, requires hardware)
