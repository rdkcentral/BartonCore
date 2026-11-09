## 1. Registration, schema, and typings (driver-facing API)

- [ ] 1.1 Add `std::optional<SbmdHandler> onTransactionFailure` to `SbmdRegistration` (`core/deviceDrivers/matter/sbmd/SbmdRegistration.h`) and include it in `SbmdDriver::VisitHandlers` (`SbmdDriver.h`) so it is rooted on activate and released on deactivate (≤2 h)
- [ ] 1.2 Extract `onTransactionFailure` in `SbmdLoader` (`mquickjs/SbmdLoader.cpp`) via `ExtractResourceHandler` (function or `{ handler, supplements }`); reject invalid declarations (≤2 h)
- [ ] 1.3 Add `onTransactionFailure` to `schema/sbmd-spec-schema.json`; add an entry to `schema/CHANGELOG.md` (version per design Q2) (≤2 h)
- [ ] 1.4 Add `onTransactionFailure`, `SbmdOperationRecord`, `SbmdTransactionFailure`, `SbmdTransactionFailureArgs` to `scriptCommon/sbmd-script.d.ts` (≤1 h)
- [ ] 1.5 Unit tests in `core/test/src/SbmdDriverTest.cpp`: bare-function form, object form with supplements, invalid declaration rejected, rooting/release across activate/deactivate; schema validation with and without the field (≤3 h)

## 2. Invocation (implementation)

- [ ] 2.1 Add `TransactionFailureSnapshot` (`sbmd/txn/TransactionFailureSnapshot.h`) and `SbmdHandlerInvoker::BuildTransactionFailureArgs` (`operations`, `failure`, `context`, `resource`, common fields) (≤4 h)
- [ ] 2.2 Unit tests for argument shape: operation entries for command/readAttribute/writeAttribute, succeeded/failed/notSent, `endpoint` null, `response` payloads, `context` present/null, `resource` by origin (≤3 h)
- [ ] 2.3 Implement `SpecBasedMatterDeviceDriver::OnTransactionFailed` body: skip when failure handling disabled (log + metric), skip when no handler, build compensation transaction (`MakeCompensationTransaction`: origin `Compensation`, new `DalTransaction`, `failureHandlingEnabled = false`, fresh deadline), post to `SbmdWorker` (≤4 h)
- [ ] 2.4 Compensation session acquisition on `SbmdWorker` for device terminals (reuse BARTON-423 `ConnectAndOrchestrate` connection path) (≤3 h)
- [ ] 2.5 Unit tests in `core/test/src/SbmdTransactionFailureHandlerTest.cpp`: exactly-once for each failure type (`timeout`, `commandFailed`, `readFailed`, `transport`, `internal`, `aborted` × device removed / plugin unregistered, `commitFailed`), not invoked on commit, not invoked without a handler (≤4 h)
- [ ] 2.5a Unit tests for the recovery rule (epic D12): honored `onError` `success()` or successful continuation → no invocation; continuation that later fails → one invocation; `success()` after `timeout` → one invocation; `commitFailed` with reason `conditionFailed` → one invocation (≤3 h)
- [ ] 2.6 Unit tests for ordering: `onError` → data-session abort → post → caller unblocked; handler supplements exclude discarded writes and include concurrently committed values (≤3 h)
- [ ] 2.7 Unit tests for compensation: data writes commit on success; `sendCommand` compensation; failing compensation logged and metered with no recursion; throwing handler → `internal` (≤3 h)
- [ ] 2.8 `aborted` at driver deactivation/shutdown: invoke only if the worker accepts and the driver is still activated; otherwise log and meter the skip; unit tests (≤3 h)

## 3. Metrics

- [ ] 3.1 Add `sbmd.txn.failure_handler_invoked`, `failure_handler_skipped`, `compensation_committed`, `compensation_failed`; extend `SbmdObservabilityTest.cpp` (≤2 h)

## 4. Door-lock scenario

- [ ] 4.1 Add a test-only door-lock SBMD driver implementing the design's worked example (status-only steps simulated with fake response commands until design Q1 is resolved) (≤3 h)
- [ ] 4.2 Unit test: SetUser ok → SetCredential ok → SetWeekDaySchedule fails → exactly one `ClearUser` with the logged `UserIndex` is sent in a compensation transaction; compensation failure path logged (≤3 h)

## 5. Documentation

- [ ] 5.1 `docs/SBMD.md`: add `onTransactionFailure` to §3 skeleton and §4.1 table; update §5.1 trigger table and §5.2 handler summary; point §6.2 to the new section (≤2 h)
- [ ] 5.2 `docs/SBMD.md`: new "Transaction failure handler" section — SBMD Transaction definition, when it runs, ordering, `args.operations`/`args.failure`/`args.context` tables, fresh supplements, compensation semantics (new transaction, no recursion, caller not waiting), `aborted` caveats, door-lock example (≤4 h)
