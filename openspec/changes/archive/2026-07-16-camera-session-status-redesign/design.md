## Context

The camera SBMD driver (`core/deviceDrivers/matter/sbmd/specs/camera.sbmd.js`) exposes an abstract session endpoint (`ep/camera`) plus a protocol endpoint (`ep/webrtc`). The abstract endpoint was originally designed around a `sessionStatus` event resource acting as a protocol-agnostic "traffic controller" that would emit `setup`/`done`/`error` with `nextAction` metadata to walk a protocol-blind client through the flow (see archived `2026-05-04-camera-architecture-redesign`).

That vision did not survive implementation, and testing exposed two independent problems:

1. **`sessionStatus` is vestigial.** The reference client ignores `nextAction` and hard-codes the WebRTC flow; `done` is never emitted; only `error` (camera `End`) has a live consumer. The abstraction is also leaky — a client must carry protocol-specific media code (webrtcbin for WebRTC, a URL player for direct), so it inevitably knows the signaling sequence and gains nothing from server-pushed next-steps.

2. **Asynchronous failures are silently dropped.** The SBMD command plumbing has three delivery channels with very different reliability:
   - Synchronous execute `.error()` → surfaces to the client as a failed `executeResource` (reliable).
   - `requestCommand` `onError`/timeout continuations → run and return `.error()`, but the originating execute already returned optimistically, so the result terminal has **no client channel** and is logged/dropped.
   - `sendCommand` (fire-and-forget) → **no `onError` at all**; async failures are invisible.

   Because `sessionStatus="error"` is only written on an incoming camera `End`, every other async failure (malformed SDP rejected by camera, VideoStreamAllocate/ProvideOffer rejected, camera unreachable during ICE, no ICE candidates) produces an unbounded black-screen hang.

Additionally, the device service suppresses `resourceUpdated` events when a cached resource's value is unchanged (`core/src/deviceService.c`), and `sessionStatus` is registered cached — so even the paths that do write it can be silently coalesced (e.g. a persisted `"setup"` produces zero events on a re-stream).

```
  Current                                   Proposed
  ─────────────────────────────────         ─────────────────────────────────
  ep/camera                                 ep/camera
    r/createSession   (execute)               r/createSession   (execute)
    r/stream          (execute) ── emits ──►   r/stream          (execute) ── returns {protocol, entryPoint}
    r/sessionStatus   (event) ◄─ traffic       r/takePicture     (execute)
    r/takePicture     (execute)   controller    r/destroySession  (execute)
    r/destroySession  (execute)              ep/webrtc
  ep/webrtc                                    r/offerSdp          (execute)
    r/offerSdp        (execute)                r/remoteSdp         (event)
    r/remoteSdp       (event)                  r/offerIceCandidates(execute)
    r/offerIceCandidates(execute)              r/remoteIceCandidates(event)
    r/remoteIceCandidates(event)               r/webrtcError       (event, NEVER-cached) ◄─ all async
                                                                                            failures + End
```

## Goals / Non-Goals

**Goals:**
- Remove `sessionStatus` from `ep/camera` and relocate its one real job (async teardown/error signaling) to the protocol endpoint.
- Give every asynchronous signaling failure a reliable delivery channel to the client.
- Make the failure/teardown event immune to no-change event suppression.
- Give the reference client a bounded, reported outcome for a failed ICE/connectivity exchange.
- Keep the abstract-endpoint contract protocol-agnostic so a future non-WebRTC protocol reuses it unchanged.

**Non-Goals:**
- Adding a WebRTC media stack to Barton core (Barton remains signaling-only).
- Implementing a second protocol endpoint (`ep/direct`, `ep/openhome`) — the design must *accommodate* one, not build it.
- Changing the `BCoreClient` public GObject API or the resource/event signal surface.
- Reworking the device-service change-detection logic itself (we opt out per-resource via caching policy rather than changing core behavior).
- SolicitOffer / camera-initiated-offer flows.

## Decisions

### D1: `stream` execute returns `{protocol, entryPoint}` instead of emitting `sessionStatus(setup)`

**Decision**: `executeStream` returns a JSON object `{ "protocol": "webrtc", "entryPoint": "/<id>/ep/webrtc/r/offerSdp" }` as its synchronous execute result. No `setup` event is emitted.

**Rationale**: The client already invokes `stream`; the "what protocol / where next" answer belongs in that call's return value, delivered on the reliable synchronous channel. This preserves the protocol-agnostic property (a client can discover the protocol without hard-coding it) while deleting the unused event machinery. A future `ep/direct` driver returns `{protocol:"direct", entryPoint:".../getMediaUrl"}` identically.

**Alternatives considered**:
- Keep `sessionStatus(setup)` as an event — rejected: it is the machinery we found unused and suppression-prone.
- Put discovery on `createSession` — rejected: protocol is a property of *streaming*, and `stream` is where a session commits to a protocol.

### D2: Single `webrtcError` async event on `ep/webrtc`, non-cached via a new `volatile` SBMD mode

**Decision**: Add one event-only resource `webrtcError` to `ep/webrtc` carrying a small value (`ended` / `failed`) plus metadata `{ reason, detail }`. To make it emit unconditionally, add a new declarable resource mode `volatile` to the SBMD runtime that maps the resource to `CACHING_POLICY_NEVER`; declare `webrtcError` with `modes: ['volatile']`.

**Rationale**: Colocating in-session termination/error signaling with the protocol endpoint keeps protocol concerns off the abstract endpoint. `CACHING_POLICY_NEVER` makes `updateResource` bypass the `strcmp` change-check, so a value that equals the stored one (e.g. a second session's `failed` after the previous session's `failed` persisted) still emits — the exact footgun that silently dropped repeated `sessionStatus` writes.

SBMD v4 does not currently expose caching policy: `SpecBasedMatterDeviceDriver::DoRegisterDriverResources` derives it solely from `resource.read.has_value()` (`CACHING_POLICY_NEVER` for resources with a read handler, else `CACHING_POLICY_ALWAYS`). Rather than overload a read handler as a caching side-channel, we add a first-class, declarative opt-out: a `volatile` mode. The change is small and additive — extend the schema enum, accept `volatile` in `ConvertModesToBitmask` (no mode bit), and treat `read || volatile` as `CACHING_POLICY_NEVER`. The device-service change-detection logic is untouched. `RESOURCE_MODE_EMIT_EVENTS` is already on by default (only `noEvents` opts out).

**Alternatives considered**:
- Register `NEVER` with no core change — rejected: not expressible; caching policy has no declarable field in SBMD v4.
- Give `webrtcError` a `read` handler to force `NEVER` — rejected: obscure (read handler purely as a caching side-channel) and would be the first shipped spec to use `read`.
- Reset `webrtcError` to a neutral value each session so the terminal value always differs — rejected: relies on discipline and still suppresses a duplicate terminal value within a session; a declarative mode is more robust and reusable for future protocol endpoints.
- Encode a nonce in the value — rejected: value-format hack the client must parse.
- Change `deviceService.c` to always emit — rejected: broad blast radius; per-resource opt-out is safer.

### D3: Fan every async failure path into `webrtcError`

**Decision**: Emit `webrtcError` from `handleVideoStreamAllocateError`, `handleProvideOfferError`, the `requestCommand` overall-deadline timeout continuation, and `handleIncomingEnd`, each with a distinguishing `reason`.

**Rationale**: These are precisely the paths whose `.error()` results are currently dropped (the async `requestCommand` continuations) or that previously used `sessionStatus` (`End`). Emitting a resource event is the one continuation action that *does* reach the client, because it flows through the normal event pipeline rather than the (absent) execute return.

**Known limitation**: `sendCommand`-based outbound failures (ProvideICECandidates, EndSession) still have no JS-visible error callback in the runtime. This design does not add one; the client-side connectivity timeout (D4) is the backstop that catches an ICE exchange that never completes for any reason, including a dropped outbound candidate.

### D4: Client-side connectivity/ICE timeout in `cameraWebrtcClient`

**Decision**: The reference WebRTC client watches `webrtcbin`'s `ice-connection-state` (and/or `connection-state`) and starts a bounded timer once signaling completes. If the connection does not reach `connected`/`completed` within the window (or transitions to `failed`), it invokes the existing `onWebrtcClosed` teardown with a failure reason.

**Rationale**: Actual media/ICE connectivity is observable only by the WebRTC peer, never by the Matter signaling relay. The driver fundamentally cannot detect "ICE never connected," so the timeout must live in the client. Routing it through the existing `onWebrtcClosed` path reuses the graceful teardown (EndSession to the camera) already proven for window-close/Ctrl+C.

**Alternatives considered**:
- Rely solely on `webrtcError` from the driver — rejected: the driver can't see connectivity failures where the camera never sends `End`.
- A fixed `g_usleep` guard — rejected: state-driven detection is both faster on success and correct on failure.

### D5: Reference client consumes `webrtcError`; drop `sessionStatus` subscription

**Decision**: `cameraDeviceSession` subscribes to `ep/webrtc/r/webrtcError` and maps `ended`/`failed` to its existing `onError` callback (teardown + reported reason). The `sessionStatus` URI handling is removed. `stream`'s return value is parsed for `protocol`/`entryPoint`.

**Rationale**: One switch arm moves from the abstract endpoint URI to the protocol endpoint URI; the client's teardown behavior is unchanged. Reading `entryPoint` from the `stream` result (rather than assuming the webrtc URI) keeps the client honest about protocol-agnosticism even while WebRTC is the only implementation.

## Risks / Trade-offs

- **[`sendCommand` failures remain invisible to the driver]** → Mitigation: the D4 client connectivity timeout is the catch-all; any exchange that fails to establish — including a lost outbound ICE candidate — resolves to a bounded, reported teardown.
- **[Removing `sessionStatus` breaks any out-of-tree client that read it]** → Mitigation: we own the clients; the only in-tree consumers are the reference app and one unit test, both updated here. This is an internal signaling contract, not the public GObject API.
- **[`stream` return value becomes a semi-structured contract]** → Mitigation: specify the JSON shape (`protocol`, `entryPoint`) in the `camera-session-lifecycle` spec so future protocol drivers conform.
- **[Timeout window tuning]** → A too-short window aborts slow-but-valid connections (the camera's 4s keyframe interval already pushes first media to ~10s); too long delays failure reporting. Mitigation: make the window a named constant with margin over observed worst-case, and reset it on first media.
- **[Thread safety]** → `webrtcError` is emitted from SBMD handler continuations, which run under `MQuickJsRuntime::GetMutex()` and marshal to the device-service event pipeline exactly like `remoteSdp`/`remoteIceCandidates` today — no new synchronization. The client timeout fires on the GStreamer/`webrtcbin` thread and must request teardown via the same cross-thread `onWebrtcClosed` mechanism already used for EOS/error, not act directly.

## Migration Plan

1. Land the SBMD spec changes (resource set + handlers) and validate with the SBMD schema (`validate-sbmd`).
2. Update `SbmdCameraWebrtcTest.cpp` assertions (`sessionStatus` → `webrtcError`; `stream` return shape) and confirm the C++ unit suite is green.
3. Update the reference app (`cameraDeviceSession`, `cameraWebrtcClient`, `cameraCategory`).
4. Manually verify: happy path still streams; force each failure (bad SDP, kill camera mid-ICE, suppress ICE) and confirm a bounded teardown with a reported reason.
5. Rollback is a straight revert — no persisted schema/data migration; transient session data keys are unchanged.

## Open Questions

1. Exact `webrtcError` value vocabulary — minimal (`ended`/`failed`) with reason in metadata, versus a richer enum. Leaning minimal + `reason` metadata for protocol-agnosticism.
2. Connectivity timeout duration and whether it should be overridable via a `cameraStream` flag for high-latency networks.
3. Whether `takePicture` (currently unimplemented) should also return an entry-point/`{protocol}` shape for consistency, or stay a direct execute.
