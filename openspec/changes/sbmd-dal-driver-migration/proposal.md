## Why

Jira: **BARTON-425** (epic **BARTON-420** — "SBMD Data Access Layer: safe, consistent driver data access").

**Depends on:** every other change in the epic and the standalone BARTON-412 fix: BARTON-412 (`sbmd-deferred-failure-callbacks`), BARTON-423 (`sbmd-transaction-orchestrator`), BARTON-421 (`sbmd-dal-core`), BARTON-430 (`sbmd-dal-source-plugins`), BARTON-422 (`sbmd-dal-driver-api`), BARTON-424 (`sbmd-transaction-failure-handler`). Delivery order (epic D13): 412 fix → 423 → 421 → 430 → 422 → 424 → 425; this change lands last. The epic architecture and glossary are in `openspec/changes/sbmd-dal-core/design.md`.

**Terminology:** An **SBMD Transaction** is the driver-level execution of one request, from the initial handler through every parked device operation and deferred callback to the terminal result (and, on failure, the driver's `onTransactionFailure`). A **DAL Transaction** is the data-access unit (routed reads, staged writes, commit or abort) owned by it.

The DAL and orchestrator provide the mechanisms, but the reported defects stay reproducible until drivers use them. `camera.sbmd.js` is the only spec in `core/deviceDrivers/matter/sbmd/specs/` that uses `setTransientData` and `requestCommand`. It stores all sessions as one JSON blob (`TD_SESSIONS`) and threads a park-time `sessions` snapshot through `context` into `handleVideoStreamAllocateResponse`, `handleAllocateForSolicit`, `handleSolicitOfferResponse`, and `handleProvideOfferResponse`, writing it back whole (BARTON-411). On failure, its per-step `onError` handlers (`handleVideoStreamAllocateError`, `handleSolicitOfferError`, `handleProvideOfferError`) only emit a `webrtcError` resource update and return `success`; nothing ever sends `VideoStreamDeallocate` (the driver has no such constant), so a stream allocated before a later failure leaks on the device, and pre-send failures never reach these handlers at all (BARTON-412). There are no tests that reproduce either scenario end to end, and nothing exercises multi-step compensation such as a door lock user/credential/schedule sequence.

## What Changes

- **Migrate `camera.sbmd.js`**:
  - Replace whole-map `setTransientData(TD_SESSIONS, JSON.stringify(sessions), …)` writes with `storage.mergeTransientData(TD_SESSIONS, {[sessionId]: {...}}, ONE_HOUR_SECS)` per-session patches (delete via `{[sessionId]: null}`) in `executeCreateSession`, `executeStream`, `executeDestroySession`, `executeLocalSdp`, `executeLocalIceCandidates`, `allocateThenSolicitOffer`, `allocateThenProvideOffer`, and all deferred handlers.
  - Deferred handlers read sessions from `args.supplements.transientData[TD_SESSIONS]` (fetched fresh at each step, BARTON-422). `context` carries IDs only (`sessionId`), never session data.
  - Allocate session ids with a sequence supplement (`supplements: { sequence: TD_NEXT_SESSION_ID }`, epic D11, BARTON-422) so concurrent `createSession` calls always get distinct ids; the driver no longer writes the counter.
  - Per-step `onError` handlers return `error(...)` so the SBMD Transaction fails (epic D12: returning `success()` would count as recovery and suppress `onTransactionFailure`). Stream release and the `webrtcError` notification move into a single driver-level `onTransactionFailure(args)` (BARTON-424), which finds a successful `VideoStreamAllocate` in `args.operations`, sends `VideoStreamDeallocate` for its `videoStreamID`, resets the session member, and emits `webrtcError`.
  - Drop the "reset corrupt sessions then `error`" writes; under discard-on-error they would never take effect.
  - Bump `driverVersion` to 2 (content marker).
- **Audit all other SBMD specs** for data writes preceding an `error` terminal (discard-on-error, BARTON-423/422) and for unconditional read-compute-write of shared keys (epic D11 limitation) and for deferred flows; record findings.
- **Door-lock compensation scenario**: a test driver/fixture exercising SetUser → SetCredential → SetWeekDaySchedule where the third step fails and `onTransactionFailure` clears the credential and user using the operation log. The production `door-lock.sbmd.js` only implements `LockDoor`/`UnlockDoor` today, so this is a test-driver scenario, not a production driver change.
- **Tests**:
  - C++ runtime reproductions in `core/test/src/SbmdCameraWebrtcTest.cpp` for BARTON-411 (concurrent `createSession` / `destroySession` during a parked `stream`, and two interleaved `createSession` calls receiving distinct ids) and BARTON-412 (pre-send failure and timeout after allocate release the stream exactly once and leave no `streaming` session).
  - C++ door-lock compensation test with a test SBMD driver and fake device.
  - pytest integration tests (Docker) with a matter.js camera mock reproducing the BARTON-411/412 scenarios end to end, if matter.js camera cluster support is feasible.
- **Docs**: update `docs/SBMD.md` camera example(s) and add a "Migrating drivers to the DAL" checklist.

### Non-goals

- New camera features or resource changes; the `camera-session-lifecycle` client contract is unchanged apart from the guarantees added here.
- Adding user, credential, or schedule management to the production door-lock driver.
- Changing non-camera driver behavior beyond the audit.
- Runtime changes of any kind (BARTON-412, 421, 430, 423, 422, 424).

## Capabilities

### New Capabilities

- `sbmd-dal-regression-tests`: Unit and integration tests that reproduce BARTON-411 and BARTON-412 scenarios, exercise driver-level compensation, and guard the epic's guarantees.

### Modified Capabilities

- `camera-session-lifecycle`: Adds requirements that concurrent session operations never lose or resurrect sessions and that failed `stream` operations release device stream allocations exactly once and never leave a session `streaming`.

## Impact

- **Affected layers**: SBMD drivers, tests (C++ unit, Python integration, matter.js mocks), docs. No runtime, core service, subsystem, or public API changes.
- **Affected files**: `core/deviceDrivers/matter/sbmd/specs/camera.sbmd.js`; other `specs/*.sbmd.js` only if the audit finds issues; `core/test/src/SbmdCameraWebrtcTest.cpp`; new `core/test/src/SbmdTransactionCompensationTest.cpp` (door-lock compensation test driver as an inline raw-string source, following `SbmdDriverTest.cpp`); new `testing/mocks/devices/matterjs/src/CameraDevice.js`, `testing/mocks/devices/matter/matter_camera.py`, `testing/test/sbmd_camera_concurrency_test.py`; `testing/conftest.py` (fixture registration); `docs/SBMD.md`.
- **CMake flags**: `BCORE_MATTER`; camera/WebRTC features as currently gated by the build (no new flags).
- **Backward compatibility**: Client-visible camera resources are unchanged. Failed `stream` operations now report failure on the execute call (previously some per-step errors were swallowed into `success` plus a `webrtcError` event); `webrtcError` is still emitted. Transient data is in-memory, so no data migration is needed.
- **Hardware**: Not required; a physical Matter camera is recommended for a final manual smoke test.
