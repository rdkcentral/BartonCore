## Context

`ep/webrtc` is the camera's Matter data model, surfaced through the SBMD driver `core/deviceDrivers/matter/sbmd/specs/camera.sbmd.js`. Its `negotiationRole` read resource currently answers the question *"what role must the client take?"* (`offerer`/`answerer`). That is the wrong subject: a resource on the camera's endpoint should describe the camera. The reference app (the only in-tree consumer) reads the value and maps `answerer = (role == "answerer")` directly onto its `webrtcbin` configuration.

Internally the driver overloads the same value twice:
- `pickNegotiationRole()` maps the camera's advertised `WebRTCTransportProvider.AcceptedCommandList` to a client-role string.
- `readNegotiationRole()` returns that string to the client.
- `executeLocalSdp()` *also* calls `pickNegotiationRole()` and branches on `role === ROLE_ANSWERER` to choose the SolicitOffer vs ProvideOffer Matter flow.

So the client-role string is entangled with the internal flow selection. Inverting the public contract must not disturb the Matter signaling sequence.

## Goals / Non-Goals

**Goals:**
- `negotiationRole` reports the **camera's** role: `offerer` when the camera generates the SDP offer (SolicitOffer flow), `answerer` when the camera answers (ProvideOffer flow), defaulting to `offerer`.
- Keep the Matter signaling flow (`VideoStreamAllocate` → `SolicitOffer`/`ProvideOffer` → `ProvideAnswer` → ICE → events) byte-for-byte unchanged.
- The reference app derives its own role by inverting the camera's role and keeps working end-to-end.
- Specs, driver comments, and tests reflect the camera-perspective contract.

**Non-Goals:**
- No change to the Matter command sequence, video-stream allocation, event resources, or session lifecycle.
- No change to the resource's type/modes (`read` `string`).
- No new public C/GObject/GIR API.

## Decisions

### Decision 1: Express the flow selection as an intrinsic camera predicate, not a client role

Introduce an internal helper that answers the camera-centric question directly — e.g. `cameraIsOfferer(args)` → `true` when the camera accepts `SolicitOffer` (or when the accepted-command list is unavailable, i.e. the default SolicitOffer flow), `false` when it accepts only `ProvideOffer`. Both consumers derive from it:

- `readNegotiationRole()` → `cameraIsOfferer ? ROLE_OFFERER : ROLE_ANSWERER`.
- `executeLocalSdp()` branches on `cameraIsOfferer` (true → SolicitOffer flow; false → ProvideOffer flow) — the *same* branch it takes today, just keyed off the intrinsic predicate instead of a client-role string.

*Alternative considered*: simply invert `pickNegotiationRole()` to return the camera role and flip `executeLocalSdp`'s comparison to `role === ROLE_OFFERER`. Rejected because it keeps overloading a role string for flow control, which is exactly the confusion this change removes. The predicate makes the SolicitOffer↔`offerer` correspondence explicit and self-documenting.

### Decision 2: Reference app inverts at the single read site

`cameraCategory.c` reads the role once (`cameraDeviceSessionGetRole`) and computes `answerer`. Change that one mapping from `answerer = (role == "answerer")` to `answerer = (role == "offerer")` — the client answers when the camera offers. `cameraDeviceSession.*` and `cameraWebrtcClient.*` keep operating on the client-side `answerer` boolean, so only the interpretation at the read site changes; the rest of the reference-app pipeline is untouched. Comments that describe the role (`cameraCategory.c`, `cameraDeviceSession.*`, `cameraWebrtcClient.*`) are updated to the camera perspective.

### Decision 3: Spec deltas as MODIFIED + RENAMED

The `webrtc-signaling-endpoint` requirement "negotiationRole read reports the client's role" is renamed to "...the camera's role" and its body/scenarios inverted; the resource table row description is updated. `camera-stream-reference-command`'s orchestration requirement gains the invert-at-read-site behavior and a scenario asserting it.

### Data flow

```
 Matter camera                  SBMD driver (camera.sbmd.js)                 reference app
 (WebRTCTransportProvider)      ep/webrtc                                    (cameraCategory.c)
 ┌───────────────────┐          ┌───────────────────────────────┐           ┌────────────────────┐
 │ AcceptedCommandList│  attr    │ cameraIsOfferer(args):        │           │ role = GetRole()   │
 │  - SolicitOffer   │ ───────► │   SolicitOffer / (unavailable)│           │ answerer =         │
 │  - ProvideOffer   │ supplmt  │     → true  (camera offerer)  │  read     │   (role=="offerer")│
 └───────────────────┘          │   ProvideOffer only           │ ───────►  │ SetAnswerer(webrtc)│
                                 │     → false (camera answerer) │  "offerer"│                    │
                                 │ readNegotiationRole:          │   or      │ client role =      │
                                 │   true→'offerer' false→'answerer'│ "answerer"│  opposite of camera│
                                 │ executeLocalSdp: same branch  │           └────────────────────┘
                                 │   keyed on cameraIsOfferer    │
                                 └───────────────────────────────┘
```

## Risks / Trade-offs

- **[Breaking contract inversion]** Any independent client relying on the old client-perspective value would break. → Mitigation: the only in-tree consumer (reference app) is updated in the same change; the value is validated end-to-end against a real camera before merge; the change is documented as **BREAKING** and lands on `feature/cameras` before any external client depends on it.
- **[Silent flow regression]** If the flow branch is mis-keyed during the predicate refactor, signaling could pick the wrong Matter command and hang. → Mitigation: the predicate maps 1:1 to today's branch (SolicitOffer↔true); covered by the SBMD camera unit test and an end-to-end reference-app run.
- **[Stale role wording]** Comments/spec prose could keep the old perspective. → Mitigation: audit every `negotiationRole`/`offerer`/`answerer` mention in the driver, reference app, and both specs as part of the tasks.

## Migration Plan

No data or schema migration. Coordinated code change: driver + reference app + specs in one change. Rollback = revert the change commit; there is no persisted state keyed on the role value.

## Backward compatibility / API implications

- No public C API, GObject signal/property, or GIR surface changes — `negotiationRole` is an SBMD data-model resource string, not part of the GObject API.
- Thread safety: unchanged. Driver handlers run on the existing SBMD execution context; the reference app reads the role on its command thread. No new threads, locks, or main-loop interactions.

## Open Questions

- None. (Default when `AcceptedCommandList` is unavailable stays the SolicitOffer flow, now reported as `offerer`.)
