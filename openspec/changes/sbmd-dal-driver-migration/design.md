# Design: Migrate SBMD drivers to the DAL and reproduce BARTON-411/412 (BARTON-425, epic BARTON-420)

## Context

Depends on every other change in the epic. Delivery order (epic D13): 412 fix → 423 → 421 → 430 → 422 → 424 → **425**. The epic architecture and glossary are in `openspec/changes/sbmd-dal-core/design.md`. This change is drivers, tests, and docs only.

Terminology (epic glossary D1): an **SBMD Transaction** is the driver-level execution of one request — initial handler, every parked device operation and deferred callback, the terminal result, and on failure the driver-level `onTransactionFailure`. A **DAL Transaction** is the data-access unit owned by it: routed reads, staged writes, commit or abort.

Contract this change relies on:

- Every step receives `args.supplements` fetched by the `SbmdTransactionOrchestrator` at that step's time; deferred callbacks inherit the originating handler's declaration (BARTON-422).
- `storage.mergeTransientData(name, patch, ttl)` applies an RFC 7396 merge patch at commit (BARTON-422, resolved by the transient data plugin from BARTON-430).
- Writes are staged and discarded if the SBMD Transaction fails, including writes before an `error` terminal; `log` still runs (BARTON-423, extended by BARTON-422).
- `supplements: { sequence: KEY }` atomically allocates a counter value during the step's fetch, delivered in `args.supplements.sequence[KEY]`, never rolled back; `storage.setTransientDataIf` fails the commit with `commitFailed`/`conditionFailed` on mismatch (BARTON-422, epic D11).
- Recovery rule (epic D12): if a hop's `onError` returns a non-`error` terminal that the runtime honors, the SBMD Transaction recovers and `onTransactionFailure` does not run. For `timeout` and depth-limit failures the recovery terminal is ignored (BARTON-412/423).
- A driver may declare one `onTransactionFailure`, either a function or `{ supplements, handler }`. It runs exactly once per failed SBMD Transaction, after the failing step's `onError` and after staged writes are discarded, with `args.operations` (ordered log of every requested operation with `kind`, `endpoint`, `cluster`, `command`/`attribute`, `status` `succeeded`/`failed`/`notSent`, `request`, `response` as base64 TLV, `error`), `args.failure` (`{step, type, message, matterCode?}`), `args.context`, and fresh `args.supplements`. Its supplements come from its own declaration. A returned chain runs as a new SBMD Transaction with failure handling disabled (BARTON-424).

Current camera driver (`core/deviceDrivers/matter/sbmd/specs/camera.sbmd.js`, `driverVersion: 1`): all sessions live in transient key `TD_SESSIONS` (`'sessions'`), the next id in `TD_NEXT_SESSION_ID`. The `stream` path is `executeStream` → `allocateThenSolicitOffer` (or `allocateThenProvideOffer`) → `requestCommand(VideoStreamAllocate)` → `handleAllocateForSolicit` (or `handleVideoStreamAllocateResponse`) → `requestCommand(SolicitOffer | ProvideOffer)` → `handleSolicitOfferResponse` (or `handleProvideOfferResponse`). Each step passes `{sessionId, sessions}` in `context` and writes `JSON.stringify(ctx.sessions)` back. The error handlers `handleVideoStreamAllocateError`, `handleSolicitOfferError`, and `handleProvideOfferError` emit `webrtcError` via `updateResource` and return `success()`. The driver defines no `VideoStreamDeallocate` constant and never releases an allocated stream. Three execute handlers write `setTransientData(TD_SESSIONS, '', 0)` immediately before `error('Corrupt session data…')`.

Door lock grounding: `core/deviceDrivers/matter/sbmd/specs/door-lock.sbmd.js` implements only `LockDoor`/`UnlockDoor` (`sendCommand`) plus attribute/event mappers; it has no user, credential, or schedule flows and no deferred operations. The matter.js mock `testing/mocks/devices/matterjs/src/DoorLockDevice.js` handles lock/unlock, alarms, and lock-operation events; its `getState` returns empty `users`/`pinCodes` arrays and it implements no `SetUser`/`SetCredential`/`SetWeekDaySchedule` handlers, although `openspec/specs/matterjs-door-lock-device/spec.md` has a "User and PIN code management via Matter" requirement. The compensation scenario is therefore a C++ test-driver scenario.

No matter.js camera mock or camera pytest exists; runtime-level camera coverage is `core/test/src/SbmdCameraWebrtcTest.cpp`. Test drivers in `SbmdDriverTest.cpp` are inline raw-string sources loaded with `SbmdLoader::LoadDriver`.

No `docs/dev/architecture.md` exists in the project.

## References

- `openspec/changes/sbmd-dal-core/design.md` — epic architecture, glossary, delivery order.
- `openspec/changes/sbmd-transaction-orchestrator/design.md` — `SbmdTransaction`, operation log, per-step supplement fetch.
- `openspec/changes/sbmd-dal-driver-api/design.md` — fresh supplements, `mergeTransientData`, discard on error.
- `openspec/changes/sbmd-transaction-failure-handler/design.md` — `onTransactionFailure(args)` contract.
- `openspec/specs/camera-session-lifecycle/spec.md` — client contract preserved by this migration.
- `openspec/specs/matterjs-door-lock-device/spec.md` — door lock mock capabilities.
- Matter Application Cluster Specification, Camera AV Stream Management (0x0551) and Door Lock (0x0101) clusters — command IDs and fields used below; verify IDs against the SDK's generated cluster definitions before implementation.
- [matter.js](https://github.com/project-chip/matter.js) — used by `testing/mocks/devices/matterjs/`; camera cluster support must be verified before writing the mock.

## Goals / Non-Goals

**Goals:**

- `camera.sbmd.js` uses per-session merges, fresh supplements, ID-only `context`, and a single `onTransactionFailure` that releases allocated streams; no park-time snapshots are written back.
- Deterministic reproductions of BARTON-411 and BARTON-412 that fail before the epic and pass after.
- A compensation test proving `onTransactionFailure` can undo earlier successful steps using the operation log.

**Non-Goals:**

- Camera feature changes; runtime changes; production door-lock user management.

## Architecture

```text
SBMD Transaction "stream A"                                      other SBMD Transactions
---------------------------                                      -----------------------
step 0 executeStream(args{supplements.sessions@T0})
  merge {A: {state:'allocating'}}  (staged)
  requestCommand(VideoStreamAllocate, context {sessionId:A})  -- park --
                                                                 createSession B: merge {B:{...}} commit
                                                                 destroySession C: merge {C:null}  commit
step 1 handleAllocateForSolicit(args{supplements.sessions@T1 fresh: has B, no C})
  if !sessions[A] -> error('session destroyed')
  merge {A: {videoStreamID}}  (staged)
  requestCommand(SolicitOffer, context {sessionId:A})          -- park --
step 2 handleSolicitOfferResponse(args{supplements fresh})
  merge {A: {webRTCSessionID, state:'streaming'}}; success -> COMMIT (patches applied to current map)

failure at any step (device error, timeout, pre-send, aborted, commitFailed)
  -> step onError returns error(...)          (no writes needed; would be discarded)
  -> DAL Transaction aborted (staged merges discarded)
  -> onTransactionFailure(args{operations, failure, context, supplements fresh})
       find succeeded VideoStreamAllocate in args.operations -> videoStreamID
       returns chain (new SBMD Transaction, no failure handling):
         sendCommand(VideoStreamDeallocate{videoStreamID})
         merge {A: {state:'created', videoStreamID:null, webRTCSessionID:null}} (if A still exists)
         updateResource(webrtc/webrtcError, FAILED)
```

## Decisions

### D1: Per-session merge writes and ID-only context

**Decision:** Every session mutation is a merge patch on that session's member; deletes use `null`. `context` carries only `sessionId`. Deferred handlers read sessions from fresh supplements; a session missing at a later step means it was destroyed concurrently, and the step fails. Example:

```js
// core/deviceDrivers/matter/sbmd/specs/camera.sbmd.js
function sessionPatch(sessionId, fields) {
    var patch = {};
    patch[sessionId] = fields;
    return patch;
}

function allocateThenSolicitOffer(args, sessions, sessionId) {
    var featureMap = args.clusterFeatureMaps[CL_CAMERA_AV_STREAM_MGMT] || 0;
    var allocPayload = buildVideoStreamAllocatePayload(featureMap);

    return Sbmd.result()
        .storage.mergeTransientData(TD_SESSIONS, sessionPatch(sessionId, {state: 'allocating'}), ONE_HOUR_SECS)
        .device.requestCommand(CL_CAMERA_AV_STREAM_MGMT, CMD_VIDEO_STREAM_ALLOCATE, allocPayload, {
            responseCommandId: CMD_VIDEO_STREAM_ALLOCATE_RESP,
            onResponse: handleAllocateForSolicit,
            onError: handleVideoStreamAllocateError,
            context: {sessionId: sessionId},
            timeoutMs: 5000
        });
}

function handleAllocateForSolicit(args) {
    var sessionId = args.handlerContext.sessionId;
    var sessions = parseSessions(args.supplements.transientData[TD_SESSIONS]) || {};
    var videoStreamID = Sbmd.Tlv.decode(args.response.data)[0];

    if (!sessions[sessionId]) {
        // Destroyed while VideoStreamAllocate was in flight; onTransactionFailure releases the stream.
        return Sbmd.result().error('Session ' + sessionId + ' was destroyed during stream setup');
    }

    var solicitPayload = Sbmd.Tlv.encodeStruct(
        {streamUsage: STREAM_USAGE_LIVE_VIEW, originatingEndpointID: WEBRTC_REQUESTOR_ENDPOINT_ID, videoStreamID: videoStreamID},
        {streamUsage: {tag: 0, type: 'enum8'}, originatingEndpointID: {tag: 1, type: 'uint16'}, videoStreamID: {tag: 2, type: 'uint16'}}
    );

    return Sbmd.result()
        .storage.mergeTransientData(TD_SESSIONS, sessionPatch(sessionId, {videoStreamID: videoStreamID}), ONE_HOUR_SECS)
        .device.requestCommand(CL_WEBRTC_TRANSPORT_PROVIDER, CMD_SOLICIT_OFFER, solicitPayload, {
            responseCommandId: CMD_SOLICIT_OFFER_RESP,
            onResponse: handleSolicitOfferResponse,
            onError: handleSolicitOfferError,
            context: {sessionId: sessionId},
            timeoutMs: 10000
        });
}

function handleSolicitOfferResponse(args) {
    var sessionId = args.handlerContext.sessionId;
    var sessions = parseSessions(args.supplements.transientData[TD_SESSIONS]) || {};
    var webRTCSessionID = Sbmd.Tlv.decode(args.response.data)[0];

    if (!sessions[sessionId]) {
        return Sbmd.result().error('Session ' + sessionId + ' was destroyed during stream setup');
    }

    return Sbmd.result()
        .storage.mergeTransientData(
            TD_SESSIONS,
            sessionPatch(sessionId, {webRTCSessionID: webRTCSessionID, state: 'streaming'}),
            ONE_HOUR_SECS)
        .success();
}

function executeDestroySession(args) {
    // ... validation unchanged; reads fresh sessions from args.supplements ...
    var result = Sbmd.result().storage.mergeTransientData(TD_SESSIONS, sessionPatch(sessionId, null), ONE_HOUR_SECS);
    // ... EndSession sendCommand when wasStreaming, unchanged ...
}
```

The ProvideOffer path (`allocateThenProvideOffer`, `handleVideoStreamAllocateResponse`, `handleProvideOfferResponse`) and `executeCreateSession`, `executeStream`, `executeLocalSdp`, `executeLocalIceCandidates` follow the same pattern. Supplement declarations on `stream`, `createSession`, `destroySession`, `localSdp`, and `localIceCandidates` already include `TD_SESSIONS`; deferred callbacks inherit them.

**Alternative considered:** One transient key per session. Rejected — supplements require static key names; per-member merge achieves the same isolation.

**Trade-offs:** Driver code no longer has a full post-write session map in hand; any step needing it reads fresh supplements, which include the SBMD Transaction's own staged merges.

---

### D2: Stream release and error notification in `onTransactionFailure`

**Decision:** Per epic D12, an `onError` that returns `success()` or a continuation means the driver recovered, and `onTransactionFailure` does not run. The camera's per-step error handlers represent real failures, so each returns `error(...)`; the SBMD Transaction fails and the `stream` execute reports failure. A single driver-level `onTransactionFailure` handles every failure type (device error, timeout, pre-send failure, depth limit, `aborted`, `commitFailed`) for every SBMD Transaction of the driver. It releases a stream only if the operation log shows a succeeded `VideoStreamAllocate`, so release happens exactly once and never for a stream that was not allocated. It emits `webrtcError` (preserving today's client notification) and resets the session member if the session still exists. Its chain runs as a new SBMD Transaction, so these writes commit even though the failed transaction's writes were discarded.

```js
// core/deviceDrivers/matter/sbmd/specs/camera.sbmd.js
SbmdDriver({
    schemaVersion: '5.0',
    driverVersion: 2,
    name: 'Camera',
    // ...
    onTransactionFailure: {
        supplements: {transientData: [TD_SESSIONS]},
        handler: handleTransactionFailure
    },
    // ...
});

// constants: VideoStreamDeallocate (Camera AV Stream Management 0x0551). Verify ID against the SDK cluster definitions.
//     CMD_VIDEO_STREAM_DEALLOCATE: 0x06,

function handleVideoStreamAllocateError(args) {
    return Sbmd.result().error('VideoStreamAllocate failed: ' + (args.error ? args.error.message : 'unknown'));
}

function handleSolicitOfferError(args) {
    return Sbmd.result().error('SolicitOffer failed: ' + (args.error ? args.error.message : 'unknown'));
}

function handleProvideOfferError(args) {
    return Sbmd.result().error('ProvideOffer failed: ' + (args.error ? args.error.message : 'unknown'));
}

function findAllocatedVideoStreamId(operations) {
    for (var i = 0; i < operations.length; i++) {
        var op = operations[i];
        if (op.kind === 'command' && op.cluster === CL_CAMERA_AV_STREAM_MGMT &&
            op.command === CMD_VIDEO_STREAM_ALLOCATE && op.status === 'succeeded' && op.response) {
            return Sbmd.Tlv.decode(op.response)[0]; // response is base64 TLV
        }
    }
    return undefined;
}

function handleTransactionFailure(args) {
    var ctx = args.context || {};
    var sessionId = ctx.sessionId;
    var result = Sbmd.result().log('Camera transaction failed at step ' + args.failure.step + ': ' +
                                   args.failure.type + ' ' + args.failure.message);

    var videoStreamID = findAllocatedVideoStreamId(args.operations);
    if (sessionId === undefined && videoStreamID === undefined) {
        return result.success(); // not a stream transaction; nothing to compensate
    }

    if (videoStreamID !== undefined) {
        result = result.device.sendCommand(CL_CAMERA_AV_STREAM_MGMT, CMD_VIDEO_STREAM_DEALLOCATE,
            Sbmd.Tlv.encodeStruct({videoStreamID: videoStreamID}, {videoStreamID: {tag: 0, type: 'uint16'}}));
    }

    var sessions = parseSessions(args.supplements && args.supplements.transientData[TD_SESSIONS]) || {};
    if (sessionId !== undefined && sessions[sessionId]) {
        result = result.storage.mergeTransientData(TD_SESSIONS,
            sessionPatch(sessionId, {state: 'created', videoStreamID: null, webRTCSessionID: null}), ONE_HOUR_SECS);
    }

    return result.dataModel.updateResource(EP_WEBRTC, 'webrtcError', WEBRTC_ERROR_FAILED, {
        sessionId: sessionId !== undefined ? sessionId : 'unknown',
        reason: args.failure.type,
        detail: args.failure.message
    }).success();
}
```

`args.context` is the `context` supplied on the transaction's first deferred request (BARTON-424); every camera deferred request carries `{sessionId}`, so it identifies the session. `createSession`, `destroySession`, and other transactions without a deferred request get `null` and only log. Field names (`kind`, `cluster`, `command`, `response` as base64 TLV) follow BARTON-424.

**Alternative considered:** Deallocate in each step's `onError`. Rejected — duplicates logic, misses pre-send failures before BARTON-412, and cannot reliably tell whether a stream was allocated.

**Alternative considered:** Keep `onError` returning `success()` with a `webrtcError` event. Rejected — under D12 that is a recovery: the SBMD Transaction succeeds, `onTransactionFailure` never runs, the stream leaks, and the client sees a successful `stream` execute for a failed stream.

**Trade-offs:** `stream` execute now fails where it previously succeeded with an asynchronous `webrtcError`; clients already handle both. The existing comment that "a bare `.error()` here has no client channel" is addressed by keeping `webrtcError` in the failure handler.

---

### D3: Corrupt-session handling and session-id allocation

**Decision:** Remove `.storage.setTransientData(TD_SESSIONS, '', 0)` before `error('Corrupt session data…')` in `executeCreateSession`, `executeStream`, and `executeDestroySession`; under discard-on-error it would never take effect. `parseSessions` returning `null` is treated as an error without a write; `executeCreateSession` instead treats unparsable data as `{}` and overwrites it with `setTransientData(TD_SESSIONS, JSON.stringify({[id]: …}))`, which is the only remaining whole-value session write and is correct because the old value is garbage.

`executeCreateSession` allocates the session id with a sequence supplement (epic D11, BARTON-422) and adds its session with a merge. Because allocation is atomic in the transient plugin during the step's fetch, two concurrent `createSession` SBMD Transactions always receive distinct ids, closing the duplicate-id case. The driver no longer writes `TD_NEXT_SESSION_ID`. Transient data is in-memory, so the counter starts fresh after restart (first id `1`, as today). Gaps from failed transactions are harmless.

```js
// core/deviceDrivers/matter/sbmd/specs/camera.sbmd.js
// resources.createSession.execute:
//     supplements: {transientData: [TD_SESSIONS], sequence: TD_NEXT_SESSION_ID},
//     handler: executeCreateSession

function executeCreateSession(args) {
    var sessionId = String(args.supplements.sequence[TD_NEXT_SESSION_ID]);
    var sessions = parseSessions(args.supplements.transientData[TD_SESSIONS]);
    var entry = {state: 'created', protocol: PROTO_WEBRTC};

    if (sessions === null) {
        // Corrupt map: replace it outright; the old value is unusable.
        var fresh = {};
        fresh[sessionId] = entry;
        return Sbmd.result()
            .storage.setTransientData(TD_SESSIONS, JSON.stringify(fresh), ONE_HOUR_SECS)
            .success(sessionId);
    }

    return Sbmd.result()
        .storage.mergeTransientData(TD_SESSIONS, sessionPatch(sessionId, entry), ONE_HOUR_SECS)
        .success(sessionId);
}
```

**Alternative considered:** `setTransientDataIf` on the counter. Rejected — a concurrent create would fail with `conditionFailed` instead of succeeding; the sequence never fails.

**Alternative considered:** Derive the id from the current map maximum. Rejected — racy and reuses ids of destroyed sessions.

---

### D4: Audit of other drivers

**Decision:** A preliminary audit of `core/deviceDrivers/matter/sbmd/specs/*.sbmd.js` found that only `camera.sbmd.js` uses `setTransientData`, `requestCommand`, or `readAttribute` terminals, and only `camera.sbmd.js` stages data writes before an `error` terminal. `air-quality-sensor`, `contact-sensor`, `door-lock`, `humidity-sensor`, and other specs return bare `error(...)` on decode failure. Task 3.1 repeats the audit with chained-variable patterns (`var result = …; … result.error(`) and records the findings in the PR.

---

### D5: Door-lock compensation as a test-driver scenario

**Decision:** Because neither the production door-lock driver nor the matter.js door-lock mock implements user/credential/schedule commands, the compensation scenario is a C++ runtime test with an inline test driver and the existing SBMD fake device. The test driver's `executeAddUser` resource handler runs SetUser → SetCredential → SetWeekDaySchedule as chained `requestCommand` steps (Door Lock cluster `0x0101`: `SetWeekDaySchedule` 0x0B, `SetUser` 0x1A, `SetCredential` 0x22 with response `SetCredentialResponse` 0x23, `ClearCredential` 0x26, `ClearUser` 0x1D; verify against the SDK cluster definitions). The fake fails `SetWeekDaySchedule`. `onTransactionFailure` reads `userIndex` from the logged `SetUser` request and the credential from the logged `SetCredential` request, and returns `ClearCredential` then `ClearUser`.

```js
// core/test/src/SbmdTransactionCompensationTest.cpp (inline test driver source, excerpt)
function handleTransactionFailure(args) {
    var result = Sbmd.result().log('add-user failed: ' + args.failure.type);
    var user = null, cred = null;
    for (var i = 0; i < args.operations.length; i++) {
        var op = args.operations[i];
        if (op.status !== 'succeeded' || op.cluster !== CL_DOOR_LOCK) continue;
        if (op.command === CMD_SET_USER) user = args.context.userIndex;
        if (op.command === CMD_SET_CREDENTIAL) cred = {credentialType: CRED_PIN, credentialIndex: args.context.credentialIndex};
    }
    if (cred) {
        result = result.device.sendCommand(CL_DOOR_LOCK, CMD_CLEAR_CREDENTIAL,
            Sbmd.Tlv.encodeStruct({credential: cred}, CLEAR_CREDENTIAL_SCHEMA));
    }
    if (user !== null) {
        result = result.device.sendCommand(CL_DOOR_LOCK, CMD_CLEAR_USER,
            Sbmd.Tlv.encodeStruct({userIndex: user}, {userIndex: {tag: 0, type: 'uint16'}}));
    }
    return result.success();
}
```

Door Lock user/credential commands require timed invoke on real devices; the test fake does not, so the test driver omits it. Extending the matter.js door-lock mock to support these commands (closing the gap with its own spec) is noted as follow-up, not part of this change.

**Alternative considered:** Add user management to `door-lock.sbmd.js`. Rejected — new product feature, out of scope.

---

### D6: Reproduction tests at two levels

**Decision:** Primary deterministic reproductions are C++ runtime tests in `SbmdCameraWebrtcTest.cpp`, which control response timing via the test fake. Integration tests in pytest exercise the full stack with a matter.js camera mock whose side-band can delay or fail `VideoStreamAllocate` / `SolicitOffer` responses. If matter.js lacks Camera AV Stream Management / WebRTC Transport Provider server support, the mock implements minimum command handlers as custom clusters; if that is infeasible the integration tier is reduced and recorded as a risk (epic open question).

Scenarios:

1. **BARTON-411 erase**: `createSession` A, `stream` A (delay allocate response), `createSession` B during park, release → B exists.
2. **BARTON-411 resurrect**: sessions A and B, `stream` A (delay), `destroySession` B during park, release → B absent.
3. **Destroy own session during park**: `stream` A (delay), `destroySession` A, release → `stream` fails, A absent, `VideoStreamDeallocate` sent exactly once.
4. **BARTON-412 pre-send failure**: session A, fail `SendCommandWithCallbacks` for SolicitOffer → `stream` fails, A not `streaming`, deallocate sent exactly once, `webrtcError` emitted.
5. **BARTON-412 timeout**: SolicitOffer never answered → after timeout A not `streaming`, deallocate sent exactly once.
6. **Allocate itself fails**: VideoStreamAllocate returns failure → no deallocate sent, A not `streaming`, `webrtcError` emitted.
7. **Door-lock compensation**: SetUser ok, SetCredential ok, SetWeekDaySchedule fails → `ClearCredential` then `ClearUser` sent exactly once each, in that order; `onTransactionFailure` invoked exactly once.
8. **Concurrent createSession (BARTON-411, epic D11)**: two `createSession` calls whose fetches interleave before either commits → distinct session ids, both sessions present. Passing test (no longer expected-fail).
9. **Recovery rule (epic D12)**: each camera `onError` returns `error`, so a SolicitOffer `commandFailed` fails the `stream` execute and runs `onTransactionFailure` once.

## Thread Safety

No runtime changes. C++ tests release delayed responses via the fake on the Matter thread; pytest uses the mock side-band.

## Interfaces

No resource changes; camera resources per `camera-session-lifecycle` are unchanged. A failed `stream` execute now returns failure in cases that previously returned success followed by `webrtcError`. No public API/GObject changes.

## Migrations

`driverVersion` 1 → 2 (content marker; no `profileVersion` change since no resources change). Transient data is in-memory; no stored data migration.

## Testing Philosophy

### Runtime reproductions

Each camera scenario first fails against the pre-epic runtime/driver (documented in the PR) and passes after. Assertions read transient `sessions` through the DAL and count commands sent by the fake device.

### Compensation

`SbmdTransactionCompensationTest.cpp` asserts command order and counts on the fake, that the failure handler ran once, that its chain did not trigger a second failure handler when a clear command is also failed, and that staged writes from the failed transaction were discarded.

### Integration

`testing/test/sbmd_camera_concurrency_test.py` runs scenarios 1, 2, 4, 5 end to end in Docker against the matter.js camera mock, asserting via camera execute results and mock-recorded commands.

### Regression

Existing `SbmdCameraWebrtcTest.cpp` coverage passes (with expectations updated where `stream` now fails instead of succeeding), full SBMD unit suites and pytest suite in Docker, and `validate-sbmd` schema check of `camera.sbmd.js`.

## Documentation Plan

### `docs/SBMD.md`

**Audience:** Driver authors

Replace camera-style read-modify-write examples with merge patches and an `onTransactionFailure` example. Add a "Migrating drivers to the DAL" checklist: no data snapshots in `context` (IDs only), read fresh supplements at every step, per-member merges for shared JSON, no writes before `error` (they are discarded), let `onError` fail and put compensation in `onTransactionFailure` using `args.operations`.

## Risks / Trade-offs

### Remaining read-compute-write in the camera driver

**Risk:** Epic D11 notes that an unconditional read-compute-write of the same key can still lose an update. The corrupt-map reset in `executeCreateSession` is the only whole-value session write left.

**Mitigation:** It runs only when the stored value is unparsable, so there is nothing valid to lose. All other session writes are per-session merges and ids come from a sequence. Task 3.1 checks for any remaining unconditional read-compute-write.

### matter.js camera support

**Risk:** matter.js may not support the camera clusters needed for the mock.

**Mitigation:** Custom minimal cluster handlers; C++ reproductions remain the authoritative tests.

### Behavior drift in camera flows

**Risk:** Refactoring all handlers and turning recovered errors into failures may regress WebRTC negotiation or client expectations.

**Mitigation:** Existing `SbmdCameraWebrtcTest.cpp` coverage must pass; `webrtcError` emission is preserved; manual smoke test on a physical camera before release (hardware).

### Compensation command failure

**Risk:** `VideoStreamDeallocate`, `ClearCredential`, or `ClearUser` can itself fail, leaving device state allocated.

**Mitigation:** By design (BARTON-424) the compensating SBMD Transaction is logged and reported, not retried. A leaked stream persists until the device releases it or the session is destroyed; this is no worse than today, where no deallocation is ever sent.
