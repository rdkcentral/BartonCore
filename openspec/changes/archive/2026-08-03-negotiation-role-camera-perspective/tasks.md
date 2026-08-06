## 1. Driver contract inversion (camera.sbmd.js)

- [x] 1.1 Add an intrinsic `cameraIsOfferer(args)` predicate deriving from `providerAcceptedCommands`: `true` when `SolicitOffer` is accepted OR the accepted-command list is unavailable (default SolicitOffer flow), `false` when only `ProvideOffer` is accepted
- [x] 1.2 Update `readNegotiationRole` to report the camera's role via the predicate: `cameraIsOfferer ? ROLE_OFFERER : ROLE_ANSWERER`
- [x] 1.3 Rekey `executeLocalSdp`'s SolicitOffer/ProvideOffer branch on `cameraIsOfferer` and confirm the emitted Matter command sequence is identical to today's (SolicitOffer flow when the camera is the offerer)
- [x] 1.4 Update driver docblocks and `ROLE_OFFERER`/`ROLE_ANSWERER` comments (client-flow section, endpoint description) to the camera-perspective wording
- [x] 1.5 Validate the SBMD spec file (`validate-sbmd`/build-time validation) — no schema regressions

## 2. Driver tests (unit)

- [x] 2.1 Update the SBMD camera unit test's `negotiationRole` expectations: `SolicitOffer` → `offerer`, `ProvideOffer`-only → `answerer`, unavailable list → `offerer`
- [x] 2.2 Add/adjust a test asserting `executeLocalSdp` still selects the SolicitOffer flow when the camera is the offerer and ProvideOffer when it is the answerer (flow unchanged)
- [x] 2.3 Run the camera SBMD unit tests via `ctest` and confirm green

## 3. Reference app interpretation (gated by BCORE_REFERENCE_CAMERA_SUPPORT)

- [x] 3.1 In `reference/src/cameraCategory.c`, invert the single mapping to `answerer = (g_strcmp0(role, "offerer") == 0)` and update the adjacent comment (client answers when the camera offers)
- [x] 3.2 Update role-describing comments/docs in `reference/src/cameraDeviceSession.c` / `.h` and `reference/src/cameraWebrtcClient.c` / `.h` to the camera perspective (the resource reports the camera's role; the client adopts the opposite)
- [x] 3.3 Build the reference app with `-DBCORE_REFERENCE_CAMERA_SUPPORT=ON` and confirm it compiles and formats clean

## 4. Spec + docs sync

- [x] 4.1 Confirm the `webrtc-signaling-endpoint` and `camera-stream-reference-command` delta specs in this change match the implemented behavior; run `openspec validate negotiation-role-camera-perspective --strict`
- [x] 4.2 Grep the driver, reference app, and specs for any remaining "client's role" / stale `offerer`/`answerer` wording and reconcile (canonical `specs/` are corrected by this change's delta on archive; `camera-stream` remoteSdp wording legitimately describes the client's own role)

## 5. End-to-end verification

- [x] 5.1 Confirm all camera unit tests pass (must precede integration/manual checks)
- [x] 5.2 Update any Python integration test that asserts the `negotiationRole` value to the camera perspective, and run it (requires Docker) — no integration test asserts `negotiationRole`, so none to update
- [x] 5.3 Manual end-to-end run against a camera (requires Docker + a camera source, e.g. `chip-camera-app` on `/dev/video0`): commission, run `cs <deviceId>`, and confirm the role reads `offerer`, negotiation completes, and media flows to the HTTP server / recording
