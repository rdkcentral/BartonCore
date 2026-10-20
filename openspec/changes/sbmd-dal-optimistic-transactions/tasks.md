## 0. Prerequisite

- [ ] 0.1 Confirm BARTON-412 (exactly-once `onError` on all failure paths) is merged before starting section 4

## 1. Transaction types (internal API)

- [ ] 1.1 Add `dal/Transaction.h` and `dal/TransactionManager.h` (design D1) (~2h)
- [ ] 1.2 Compile-only/unit scaffolding in `core/test/src/SbmdDalTransactionTest.cpp` (~1h)

## 2. TransactionManager implementation

- [ ] 2.1 Implement `Begin`, `Read` (read-your-writes, read set), `Stage`, `Abort`, `ResetForRetry` (~3h)
- [ ] 2.2 Unit tests for 2.1 (~2h)
- [ ] 2.3 Implement `Validate`, `Commit`, `PrepareSideEffect`, `TryMerge`, compensation commit (D2–D5) (~4h)
- [ ] 2.4 Unit tests: conflicts (changed/deleted/created), blind writes, merge-patch rebase, unmergeable Replace, compensation, retry bound (~4h)

## 3. Staging in op execution

- [ ] 3.1 Route supplement fetchers through `TransactionManager::Read` and `ExecuteOps` writes through `Stage` (keep `log` immediate) (~3h)
- [ ] 3.2 Unit tests in `SbmdHandlerInvokerTest.cpp` (~2h)

## 4. Runtime lifecycle integration

- [ ] 4.1 Begin/commit/abort in `HandleResourceOp` and `ExecuteTerminal` for synchronous terminals; pre-park/synchronous retry loop (~4h)
- [ ] 4.2 Unit tests: synchronous success/error, pre-park retry, retry exhaustion → `conflict` (~3h)
- [ ] 4.3 Carry `txId` on `PendingOperation`; `PrepareSideEffect` before `SendCommandWithCallbacks`/`writeAttribute`/`sendCommand`; commit in `CompletePendingOperation(id, true)`, abort on false; compensation tx for `onError` ops; `conflict` failure path (~4h)
- [ ] 4.4 Unit tests in `SbmdDriverTest.cpp`: post-park merge, post-park unmergeable → single `onError(conflict)`, command sent once (~4h)
- [ ] 4.5 Transactions for `DispatchToHandlers` and `InvokeSeedHandler` (retry then commit-with-warning) (~2h)
- [ ] 4.6 Unit tests for 4.5 (~2h)

## 5. Metrics

- [ ] 5.1 Add commit/abort/retry/merge/conflict counters per driver; unit test counters (~2h)

## 6. Documentation

- [ ] 6.1 `docs/SBMD.md` "Consistency guarantees" section; update error-terminal wording; `schema/CHANGELOG.md` (~2h)

## 7. Regression (Docker)

- [ ] 7.1 Run SBMD unit suites and full pytest integration suite in Docker; triage drivers affected by the error-terminal semantic change (~3h, requires Docker)
