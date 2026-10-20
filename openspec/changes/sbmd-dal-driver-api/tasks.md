## 1. Driver API surface (JS contract)

- [ ] 1.1 Add `storage.mergeTransientData` and the `supplements` option for `requestCommand`/`readAttribute` to `scriptCommon/sbmd-result.js` and `scriptCommon/sbmd-script.d.ts`; update `SbmdWireContract.h` (~2h)
- [ ] 1.2 JS-level unit tests (builder validation, wire shape) in `SbmdResultExecutorTest.cpp` (~1h)

## 2. Merge write in the DAL

- [ ] 2.1 Ensure `cJSON_Utils` is available in the build; implement `DataAccessLayer::MergeJson` (design D2) (~2h)
- [ ] 2.2 Unit tests: absent/non-object/object targets, `null` deletes, nested merge, array replacement, TTL, `ttlSecs==0` delete, invalid patch (~2h)

## 3. Result parsing and op execution

- [ ] 3.1 Add `ResultOp::MergeTransientData` parsing in `SbmdResultExecutor.cpp` and execution in `SbmdHandlerInvoker::ExecuteOps` via the DAL; parse `supplements` option on deferred terminals (~3h)
- [ ] 3.2 Unit tests in `SbmdResultExecutorTest.cpp` / `SbmdHandlerInvokerTest.cpp` (~2h)

## 4. Fresh supplements in deferred callbacks

- [ ] 4.1 Add `supplementDecl` to `PendingOperation`; populate in `ExecuteRequestCommand`/`ExecuteReadAttribute` (option override, else originating handler declaration) and on re-arm in `ContinueDeferredChain` (~3h)
- [ ] 4.2 Extend `BuildCommandResponseArgs`/`BuildDeferredErrorArgs` (and readAttribute response args) with supplements; fetch fresh supplements in `HandleDeferredCommandResponse`, `HandleDeferredCommandError`, timeout path, and `ExecuteReadAttribute` (~4h)
- [ ] 4.3 Unit tests in `SbmdDriverTest.cpp`: concurrent DAL write during park is visible in `onResponse`, `onError`, and timeout; override replaces inherited declaration; no supplements when none declared (~4h)

## 5. Documentation

- [ ] 5.1 Update `docs/SBMD.md` §4.12, §5.1, §7.2, §7.3 and `schema/CHANGELOG.md` (~2h)

## 6. Regression (Docker)

- [ ] 6.1 Run full SBMD unit suites and pytest integration suite in Docker (~2h, requires Docker)
