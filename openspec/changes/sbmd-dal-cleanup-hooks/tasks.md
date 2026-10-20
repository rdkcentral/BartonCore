## 0. Prerequisites

- [ ] 0.1 Confirm BARTON-412 and BARTON-423 are merged

## 1. Driver API surface

- [ ] 1.1 Add `onFailure(handler, context)` to `scriptCommon/sbmd-result.js` and `sbmd-script.d.ts`; add cleanup args to wire contract (`SbmdWireContract.h`) (~2h)
- [ ] 1.2 Unit tests for builder validation and parsing in `SbmdResultExecutorTest.cpp` (~1h)

## 2. CleanupRegistry

- [ ] 2.1 Implement `dal/CleanupRegistry.{h,cpp}` (design D2) (~2h)
- [ ] 2.2 Unit tests `SbmdDalCleanupTest.cpp`: LIFO, idempotent `Take`, `Clear` (~2h)

## 3. Registration and invocation

- [ ] 3.1 Parse `ResultOp::OnFailure` and register under the JS mutex during `ExecuteOps`; add `BuildCleanupArgs` (~3h)
- [ ] 3.2 Unit tests in `SbmdHandlerInvokerTest.cpp` (~2h)
- [ ] 3.3 Implement `RunCleanupHooks` and `ExecuteCleanupTerminal`; call from `CompletePendingOperation(id, false)` after `onError`, from synchronous failure in `ExecuteTerminal`, and clear on success (~4h)
- [ ] 3.4 Unit tests: each failure type runs hooks once, never on success, ordering vs `onError`, compensation writes commit, fire-and-forget `sendCommand`, throwing hook (~4h)
- [ ] 3.5 Abort sources: device removal, driver deactivation, depth exceeded, unmergeable conflict → `FailureType::Aborted`/`Conflict` (~3h)
- [ ] 3.6 Unit tests for 3.5 (~2h)

## 4. Metrics and docs

- [ ] 4.1 Cleanup run/failure counters per driver; unit test (~1h)
- [ ] 4.2 `docs/SBMD.md` cleanup hooks section; `schema/CHANGELOG.md` (~2h)

## 5. Regression (Docker)

- [ ] 5.1 Run SBMD unit suites and pytest integration suite in Docker (~2h, requires Docker)
