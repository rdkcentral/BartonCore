## ADDED Requirements

### Requirement: BARTON-411 lost-update reproductions
The test suite SHALL include deterministic C++ runtime tests (in `core/test/src/SbmdCameraWebrtcTest.cpp`) and Docker-based pytest integration tests (in `testing/test/sbmd_camera_concurrency_test.py`) that park a camera `stream` operation, perform a concurrent `createSession` or `destroySession`, release the parked response, and assert the concurrent change is preserved.

#### Scenario: Runtime test controls response timing
- **WHEN** the C++ test fake delays `VideoStreamAllocateResponse` and the test issues `createSession` before releasing it
- **THEN** the test SHALL assert the new session exists after the `stream` operation completes

#### Scenario: Integration test with matter.js camera mock
- **WHEN** the matter.js camera mock is instructed via side-band to delay `VideoStreamAllocate`
- **THEN** the pytest test SHALL issue a concurrent session operation and assert the session map via camera execute resources

---

### Requirement: BARTON-412 failure-path reproductions
The test suite SHALL include C++ runtime tests and Docker-based integration tests that force a pre-send failure and a timeout after a successful `VideoStreamAllocate`, and assert that the driver failure path runs exactly once, `VideoStreamDeallocate` is sent exactly once, and no session remains `streaming`.

#### Scenario: Pre-send failure test
- **WHEN** the test fake fails `SendCommandWithCallbacks` for `SolicitOffer`
- **THEN** the test SHALL assert exactly one `VideoStreamDeallocate` and no `streaming` session

#### Scenario: Timeout test
- **WHEN** the mock never answers `SolicitOffer`
- **THEN** after the timeout the test SHALL assert exactly one `VideoStreamDeallocate` and no `streaming` session
