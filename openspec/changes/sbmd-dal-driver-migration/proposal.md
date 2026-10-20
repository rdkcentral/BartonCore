## Why

Jira: **BARTON-425** (epic **BARTON-420** — "SBMD Data Access Layer: safe, consistent driver data access").

**Depends on:** BARTON-421 (`sbmd-dal-core-store`), BARTON-422 (`sbmd-dal-driver-api`), BARTON-423 (`sbmd-dal-optimistic-transactions`), BARTON-424 (`sbmd-dal-cleanup-hooks`), and the standalone BARTON-412 fix. The epic architecture is in `openspec/changes/sbmd-dal-core-store/design.md`.

The DAL provides the mechanisms, but the reported defects stay reproducible until drivers use them. `camera.sbmd.js` is the only spec in `core/deviceDrivers/matter/sbmd/specs/` that uses `setTransientData` and `requestCommand`; it stores all sessions as one JSON blob (`TD_SESSIONS`) and threads a park-time `sessions` snapshot through `context` into `handleVideoStreamAllocateResponse`, `handleAllocateForSolicit`, `handleSolicitOfferResponse`, and the error handlers, writing it back whole (BARTON-411). Its stream deallocation on failure is duplicated across per-hop `onError` handlers and was not reached on pre-send failures (BARTON-412). There are also no tests that reproduce either scenario end to end.

## What Changes

- **Migrate `camera.sbmd.js`**:
  - Replace whole-map `setTransientData(TD_SESSIONS, JSON.stringify(sessions), …)` writes with `storage.mergeTransientData(TD_SESSIONS, {[sessionId]: {...}}, ONE_HOUR_SECS)` member patches (delete via `{[sessionId]: null}`), in `executeCreateSession`, `executeStream`, `executeDestroySession`, `executeLocalSdp`, `allocateThenSolicitOffer`, `allocateThenProvideOffer`, and all deferred handlers.
  - Stop passing `sessions` through `context`; deferred handlers read `args.supplements.transientData[TD_SESSIONS]` (fresh, BARTON-422) and pass only `sessionId` (and IDs) in `context`.
  - After `VideoStreamAllocate` succeeds, register `onFailure(handleDeallocateStreamCleanup, {sessionId, videoStreamID})` (BARTON-424) instead of duplicating deallocation in `handleSolicitOfferError` / `handleProvideOfferError`.
  - Keep `TD_NEXT_SESSION_ID` allocation correct under concurrency (pre-park validation retries it; BARTON-423).
  - Bump `driverVersion` to 2 (content marker).
- **Audit all other SBMD specs** for reliance on writes preceding an `error` terminal (semantic change in BARTON-423) and adjust if found.
- **Tests**:
  - C++ runtime-level reproductions in `core/test/src/SbmdCameraWebrtcTest.cpp`: BARTON-411 (concurrent `createSession` / `destroySession` during a parked `stream`) and BARTON-412 (pre-send failure and timeout leave no `streaming` session and deallocate the stream).
  - pytest integration tests (Docker) with a matter.js camera mock reproducing the same scenarios end to end.
- **Docs**: update `docs/SBMD.md` camera example(s) and the migration guide section.

### Non-goals

- New camera features or resource changes; the `camera-session-lifecycle` client contract is unchanged.
- Changing non-camera driver behavior beyond the error-terminal audit.
- The BARTON-412 runtime fix itself.

## Capabilities

### New Capabilities

- `sbmd-dal-regression-tests`: Unit and integration tests that reproduce BARTON-411 and BARTON-412 scenarios and guard the DAL guarantees.

### Modified Capabilities

- `camera-session-lifecycle`: Adds requirements that concurrent session operations never lose or resurrect sessions and that failed `stream` operations release device stream allocations and never leave a session `streaming`.

## Impact

- **Affected layers**: SBMD drivers, tests (C++ unit, Python integration, matter.js mocks), docs. No runtime, core service, subsystem, or public API changes.
- **Affected files**: `core/deviceDrivers/matter/sbmd/specs/camera.sbmd.js`; possibly other `specs/*.sbmd.js` per audit; `core/test/src/SbmdCameraWebrtcTest.cpp`; new `testing/mocks/devices/matterjs/src/CameraDevice.js`, `testing/mocks/devices/matter/matter_camera.py`, `testing/test/sbmd_camera_concurrency_test.py`; `testing/conftest.py` (fixture registration); `docs/SBMD.md`.
- **CMake flags**: `BCORE_MATTER`; camera/WebRTC features as currently gated by the build (no new flags).
- **Backward compatibility**: Client-visible camera behavior (resources, execute responses) is unchanged except that the reported defects no longer occur. Transient data is in-memory, so no data migration is needed.
- **Hardware**: Not required; a physical Matter camera is recommended for a final manual smoke test.
