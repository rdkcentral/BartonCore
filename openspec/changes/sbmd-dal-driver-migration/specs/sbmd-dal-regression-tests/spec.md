## ADDED Requirements

### Requirement: BARTON-411 lost-update reproductions
The test suite SHALL include deterministic C++ runtime tests (in `core/test/src/SbmdCameraWebrtcTest.cpp`) and, where a matter.js camera mock is feasible, Docker-based pytest integration tests (in `testing/test/sbmd_camera_concurrency_test.py`) that park a camera `stream` SBMD Transaction, perform a concurrent `createSession` or `destroySession`, release the parked response, and assert the concurrent change is preserved. The C++ suite SHALL also run two `createSession` SBMD Transactions whose supplement fetches interleave before either commits and assert distinct session ids; this test SHALL pass (it is not marked expected-fail).

#### Scenario: Interleaved createSession calls get distinct ids
- **WHEN** the test interleaves two `createSession` SBMD Transactions so both fetch supplements before either commits
- **THEN** the test SHALL assert the two returned session ids differ and both sessions exist

#### Scenario: Runtime test controls response timing
- **WHEN** the C++ test fake delays `VideoStreamAllocateResponse` and the test issues `createSession` before releasing it
- **THEN** the test SHALL assert the new session exists after the `stream` SBMD Transaction completes

#### Scenario: Integration test with matter.js camera mock
- **WHEN** the matter.js camera mock is instructed via side-band to delay `VideoStreamAllocate`
- **THEN** the pytest test SHALL issue a concurrent session operation and assert the session map via camera execute resources

---

### Requirement: BARTON-412 failure-path reproductions
The test suite SHALL include C++ runtime tests and, where feasible, Docker-based integration tests that force a pre-send failure and a timeout after a successful `VideoStreamAllocate`, and assert that the driver's `onTransactionFailure` runs exactly once, `VideoStreamDeallocate` is sent exactly once, and no session remains `streaming`.

#### Scenario: Pre-send failure test
- **WHEN** the test fake fails `SendCommandWithCallbacks` for `SolicitOffer`
- **THEN** the test SHALL assert exactly one `onTransactionFailure` invocation, exactly one `VideoStreamDeallocate`, and no `streaming` session

#### Scenario: Timeout test
- **WHEN** the fake or mock never answers `SolicitOffer`
- **THEN** after the timeout the test SHALL assert exactly one `VideoStreamDeallocate` and no `streaming` session

---

### Requirement: Driver-level compensation reproduction
The test suite SHALL include a C++ runtime test (in `core/test/src/SbmdTransactionCompensationTest.cpp`) with a test SBMD driver that performs a multi-step Door Lock (cluster `0x0101`) sequence `SetUser` → `SetCredential` → `SetWeekDaySchedule` in one SBMD Transaction and declares `onTransactionFailure`. The handler SHALL use `args.operations` to identify succeeded steps and compensate them in reverse order.

#### Scenario: Schedule failure clears credential and user
- **WHEN** `SetUser` and `SetCredential` succeed and `SetWeekDaySchedule` fails
- **THEN** `onTransactionFailure` SHALL be invoked exactly once and the fake SHALL receive `ClearCredential` followed by `ClearUser`, each exactly once

#### Scenario: Credential failure clears only the user
- **WHEN** `SetUser` succeeds and `SetCredential` fails
- **THEN** the fake SHALL receive `ClearUser` exactly once and no `ClearCredential`

#### Scenario: Compensation failure does not recurse
- **WHEN** the compensating `ClearUser` command also fails
- **THEN** the failure SHALL be logged and reported and `onTransactionFailure` SHALL NOT be invoked again
