# Design: Migrate SBMD drivers to the DAL and reproduce BARTON-411/412 (BARTON-425, epic BARTON-420)

## Context

Depends on BARTON-421–424 and BARTON-412; architecture in `openspec/changes/sbmd-dal-core-store/design.md`. `camera.sbmd.js` keeps all sessions in transient key `TD_SESSIONS` (`'sessions'`) and the next id in `TD_NEXT_SESSION_ID`. Its `stream` path is `executeStream` → `allocateThenSolicitOffer` (or `allocateThenProvideOffer`) → `requestCommand(VideoStreamAllocate)` → `handleAllocateForSolicit` → `requestCommand(SolicitOffer)` → `handleSolicitOfferResponse`. Each hop passes `{sessionId, sessions}` in `context` and writes `JSON.stringify(ctx.sessions)` back. No matter.js camera mock or camera pytest exists today; runtime-level camera coverage lives in `core/test/src/SbmdCameraWebrtcTest.cpp`.

No `docs/dev/architecture.md` exists in the project.

## References

- `openspec/changes/sbmd-dal-core-store/design.md`, `sbmd-dal-driver-api/design.md`, `sbmd-dal-optimistic-transactions/design.md`, `sbmd-dal-cleanup-hooks/design.md`.
- `openspec/specs/camera-session-lifecycle/spec.md` — client contract preserved by this migration.
- [matter.js](https://github.com/project-chip/matter.js) — used by `testing/mocks/devices/matterjs/`; camera cluster support must be verified before writing the mock.

## Goals / Non-Goals

**Goals:**

- `camera.sbmd.js` uses member merges, fresh supplements, and cleanup hooks; no park-time snapshots are written back.
- Deterministic reproductions of BARTON-411 and BARTON-412 that fail before the epic and pass after.

**Non-Goals:**

- Camera feature changes; runtime changes.

## Architecture

```text
executeStream(args{supplements.sessions@T0})
  sessionId chosen; mergeTransientData(sessions, {[id]: {state:'allocating'}})
  requestCommand(VideoStreamAllocate, context {sessionId})              -- park --
      | meanwhile: createSession / destroySession merge their own members
handleAllocateForSolicit(args{supplements.sessions@T1 fresh})
  if session gone -> error('session destroyed')  (hook below deallocates)
  onFailure(handleDeallocateStreamCleanup, {sessionId, videoStreamID})
  mergeTransientData(sessions, {[id]: {videoStreamID}})
  requestCommand(SolicitOffer, context {sessionId, videoStreamID})      -- park --
handleSolicitOfferResponse(args{supplements.sessions fresh})
  mergeTransientData(sessions, {[id]: {webRTCSessionID, state:'streaming'}}); success
failure anywhere after allocate -> onError (hop) -> handleDeallocateStreamCleanup (once)
  sendCommand(VideoStreamDeallocate); mergeTransientData(sessions, {[id]: {state:'created', videoStreamID:null}})
```

## Decisions

### D1: Member-level session writes

**Decision:** Every session mutation becomes a merge patch on that session's member; deletes use `null`. Example for `handleAllocateForSolicit`:

```js
// core/deviceDrivers/matter/sbmd/specs/camera.sbmd.js
function handleAllocateForSolicit(args) {
    var ctx = args.handlerContext;
    var sessions = parseSessions(args.supplements.transientData[TD_SESSIONS]);
    var videoStreamID = Sbmd.Tlv.decode(args.response.data)[0];

    var cleanupCtx = {sessionId: ctx.sessionId, videoStreamID: videoStreamID};
    var result = Sbmd.result().onFailure(handleDeallocateStreamCleanup, cleanupCtx);

    if (!sessions[ctx.sessionId]) {
        // Destroyed while VideoStreamAllocate was in flight; the hook releases the stream.
        return result.error('Session ' + ctx.sessionId + ' was destroyed during stream setup');
    }

    var patch = {};
    patch[ctx.sessionId] = {videoStreamID: videoStreamID};

    var solicitPayload = Sbmd.Tlv.encodeStruct(
        {streamUsage: STREAM_USAGE_LIVE_VIEW, originatingEndpointID: WEBRTC_REQUESTOR_ENDPOINT_ID, videoStreamID: videoStreamID},
        {streamUsage: {tag: 0, type: 'enum8'}, originatingEndpointID: {tag: 1, type: 'uint16'}, videoStreamID: {tag: 2, type: 'uint16'}}
    );

    return result
        .storage.mergeTransientData(TD_SESSIONS, patch, ONE_HOUR_SECS)
        .device.requestCommand(CL_WEBRTC_TRANSPORT_PROVIDER, CMD_SOLICIT_OFFER, solicitPayload, {
            responseCommandId: CMD_SOLICIT_OFFER_RESP,
            onResponse: handleSolicitOfferResponse,
            onError: handleSolicitOfferError,
            context: {sessionId: ctx.sessionId, videoStreamID: videoStreamID},
            timeoutMs: 10000
        });
}

function handleDeallocateStreamCleanup(args) {
    var ctx = args.handlerContext;
    var patch = {};
    patch[ctx.sessionId] = {state: 'created', videoStreamID: null, webRTCSessionID: null};
    var sessions = parseSessions(args.supplements.transientData[TD_SESSIONS]);
    var result = Sbmd.result().log('Releasing video stream ' + ctx.videoStreamID + ' (' + args.failure.type + ')');
    if (sessions[ctx.sessionId]) {
        result = result.storage.mergeTransientData(TD_SESSIONS, patch, ONE_HOUR_SECS);
    }
    return result.device.sendCommand(CL_CAMERA_AV_STREAM_MGMT, CMD_VIDEO_STREAM_DEALLOCATE,
        Sbmd.Tlv.encodeStruct({videoStreamID: ctx.videoStreamID}, {videoStreamID: {tag: 0, type: 'uint16'}}));
}
```

`CMD_VIDEO_STREAM_DEALLOCATE` is added to constants if not already present. Supplement declarations on `stream`, `createSession`, `destroySession`, and `localSdp` already include `TD_SESSIONS`; deferred callbacks inherit them (BARTON-422).

**Alternative considered:** Split sessions into one transient key per session. Rejected — requires dynamic key discovery in supplements (not supported); member merge achieves the same isolation.

---

### D2: Session-id allocation

**Decision:** `executeCreateSession` keeps reading `TD_NEXT_SESSION_ID` and writing `id+1` as a whole value. Because `createSession` has no device side effect, BARTON-423 pre-park validation retries it on conflict, so two concurrent creates get distinct ids.

---

### D3: Error-terminal audit

**Decision:** Grep every `specs/*.sbmd.js` for results that combine data write ops with `.error(`; move any such intent to `onError` or `onFailure`. Current inspection shows `camera.sbmd.js` writes `setTransientData(TD_SESSIONS, '', 0)` before `error` on unparsable sessions; that becomes a cleanup in `onError`-equivalent or is dropped (the corrupt value is replaced on next create).

---

### D4: Reproduction tests at two levels

**Decision:** Primary deterministic reproductions are C++ runtime tests in `SbmdCameraWebrtcTest.cpp`, which control response timing exactly via the test fake. Integration tests in pytest exercise the full stack with a matter.js camera mock whose side-band can delay or fail `VideoStreamAllocate` / `SolicitOffer` responses. If matter.js lacks Camera AV Stream Management / WebRTC Transport Provider server support, the mock implements the minimum command handlers as custom clusters; if that is infeasible the integration tier is reduced and recorded as a risk.

Scenarios:

1. **BARTON-411 erase**: `createSession` A, `stream` A (delay allocate response), `createSession` B during park, release response → B exists.
2. **BARTON-411 resurrect**: sessions A and B, `stream` A (delay), `destroySession` B during park, release → B absent.
3. **Destroy own session during park**: `stream` A (delay), `destroySession` A, release → stream deallocated once, A absent, `stream` fails.
4. **BARTON-412 pre-send failure**: session A, make `SendCommandWithCallbacks` fail for SolicitOffer → A not `streaming`, deallocate sent once.
5. **BARTON-412 timeout**: SolicitOffer never answered → after timeout A not `streaming`, deallocate sent once.

## Thread Safety

No runtime changes. Tests that release delayed responses must do so via the Matter thread in the C++ fake, and via the mock side-band in pytest.

## Interfaces

No client-facing changes; camera resources and execute responses per `camera-session-lifecycle` are unchanged. No public API/GObject changes.

## Migrations

`driverVersion` 1 → 2 (content marker; no `profileVersion` change since no resources change). Transient data is in-memory; no stored data migration.

## Testing Philosophy

### Runtime reproductions

Each scenario first fails against the pre-epic runtime/driver (documented in the PR) and passes after. Assertions read transient `sessions` via the DAL and count commands sent by the fake device.

### Integration

`testing/test/sbmd_camera_concurrency_test.py` runs scenarios 1, 2, 4, 5 end to end in Docker against the matter.js camera mock, asserting via `createSession`/`destroySession`/`stream` execute results and mock-recorded commands.

### Regression

Full SBMD unit suites and pytest suite in Docker; `validate-sbmd` schema check of `camera.sbmd.js`.

## Documentation Plan

### `docs/SBMD.md`

**Audience:** Driver authors

Replace camera-style read-modify-write examples with merge patches and `onFailure`; add a short "Migrating drivers to the DAL" checklist (no park-time snapshots in `context`, use fresh supplements, member merges, cleanup hooks for side effects, no writes before `error`).

## Risks / Trade-offs

### matter.js camera support

**Risk:** matter.js may not support the camera clusters needed for the mock.

**Mitigation:** Custom minimal cluster handlers; C++ reproductions remain the authoritative tests.

### Behavior drift in camera flows

**Risk:** Refactoring all handlers risks regressions in WebRTC negotiation.

**Mitigation:** Existing `SbmdCameraWebrtcTest.cpp` coverage must pass unchanged; manual smoke test on a physical camera before release (hardware).
