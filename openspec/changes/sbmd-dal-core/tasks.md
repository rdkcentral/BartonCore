## 1. Types and Interfaces (internal API)

- [ ] 1.1 Add `core/deviceDrivers/matter/sbmd/dal/DataKey.h` (`Namespace`, `DataKey`, `DataKeyHash`, `ToString`) and `DalTypes.h` (`DalTransactionId`, `DataValue`, `ReadResult`, `Write` with optional `expected`, `DalFailureType` incl. `NotAtomic`, `CommitFailureReason`, `DalError`) with Apache-2.0 headers (≤2h)
- [ ] 1.2 Unit tests `core/test/src/SbmdDalDataKeyTest.cpp`: equality, ordering, hashing, namespace distinctness (≤1h)
- [ ] 1.3 Add `DataSourcePlugin.h` (`Executor`, `PluginScope`, `PluginCaps` incl. `atomic`, `PluginBinding` incl. default `AllocateSequence`, `DataSourcePlugin`) and reusable `FakePlugin` / `FakeAtomicPlugin` test doubles under `core/test/src/sbmdDalFakes/` (≤2h)
- [ ] 1.4 Add the DAL sources to the SBMD CMake source list and `core/test/CMakeLists.txt` (no new flags; DAL core does not depend on the Matter SDK) (≤1h)

## 2. Executor

- [ ] 2.1 Add public `ThreadHop::PostMainLoopAsync` forwarder to 423's `core/deviceDrivers/matter/sbmd/txn/ThreadHop.h` (no behavior change for existing callers) and implement `DalExecutor` as an adapter over the shared `ThreadHop` (affinity mapping via `OnMainLoop`/`OnMatter`, inline when already on the target executor, logged fail-fast instead of assert for main-loop waits from the Matter thread, `PostMainLoop`, ms budget → absolute `ThreadHop::Deadline`) (≤3h)
- [ ] 2.2 Unit tests `SbmdDalExecutorTest.cpp` using 423's `ThreadHop` fake and `DefaultThreadHop` with a real `GMainLoop` thread and a simulated Matter thread: affinity mapping, inline, nested-hop deadline propagation, fail-fast without blocking, non-blocking post (≤3h)

## 3. Registry

- [ ] 3.1 Implement `DalPluginRegistry` (marshaled `Register`/`Unregister`, overlap rejection, most-specific `Resolve`, bound-transaction tracking, abort-on-unregister, idempotent unregister, non-blocking unregister from the Matter thread) (≤4h)
- [ ] 3.2 Unit tests `SbmdDalRegistryTest.cpp`: resolution, overlap, abort-on-unregister of bound-only transactions, single `Release`, idempotence, cross-thread registration (≤3h)

## 4. DalTransaction

- [ ] 4.1 Implement `DalTransaction` lazy binding, batched routed `Read` (staged → overlay → plugin), `Stage` (writable check, last-write-wins, TTL passthrough), `SetOverlay`, `Abort` (≤4h)
- [ ] 4.2 Unit tests `SbmdDalTransactionTest.cpp`: one `Read` per plugin per batch, lazy single bind, read-your-own-staged-writes, overlay, `NoPlugin`, `NotWritable`, abort discards, ended-transaction errors (≤3h)
- [ ] 4.3 Implement `DalTransaction::Commit` per D8 (prior-value capture, deterministic order, reverse rollback including deletes, `commitFailed`, rollback-failure logging) (≤3h)
- [ ] 4.4 Unit tests `SbmdDalCommitTest.cpp`: success across plugins, mid-commit failure rollback, absent-prior delete, rollback failure, ordering determinism (≤3h)

## 5. Atomic Primitives (D11)

- [ ] 5.1 Implement `DalTransaction::AllocateSequence` (Atomic check, immediate, not rolled back) and `StageConditional` (Atomic check, staged `expected`, read-your-own-staged-writes); propagate `ConditionFailed` from plugin `Commit` through D8 rollback as `CommitFailed`/`ConditionFailed` (≤4h)
- [ ] 5.2 Unit tests `SbmdDalAtomicTest.cpp`: allocation from absent, distinct values across interleaved transactions, survival across abort and failed commit, `NotAtomic`; conditional match, mismatch, expected-absent, nothing applied in the failing plugin, rollback of earlier plugins; interleaved camera `createSession` pattern (≤3h)

## 6. Façade

- [ ] 6.1 Implement `Dal` façade, constructed with the orchestrator's shared `ThreadHop` (`Begin`, `Read`, `AllocateSequences`, `Stage`, `SetOverlay`, `Commit`, `Abort`, `Registry`) marshaling to the main loop with deadlines and inline on main loop (≤3h)
- [ ] 6.2 Unit tests `SbmdDalFacadeTest.cpp`: calls from a worker thread, from the main loop (inline), from the Matter thread (fail-fast), Matter-affinity fake plugin hop end to end, unregister while a transaction is mid-read (≤3h)

## 7. `DalSbmdDataAccess` (implements the BARTON-423 `SbmdDataAccess` seam)

- [ ] 7.1 Implement `DalSbmdDataAccess::Begin` / `DalSbmdDataSession::Fetch`: batched supplement fetch (attribute, resource, persistent, transient keys + sequence allocation in one step) and mapping of results back to the supplement shape (≤4h)
- [ ] 7.2 Unit tests for supplement fetch mapping and one-hop-per-step behavior against fake plugins (≤3h)
- [ ] 7.3 Implement `DalSbmdDataSession::Stage` (`updateResource`, `setMetadata`, `setPersistentData`, `setTransientData`), `StageConditional`, `RecordAttributeWrite`, `Commit`, `Abort`, `SetAbortListener`, and failure mapping (`commitFailed` with `pluginError`/`conditionFailed`/`rollbackFailed`, `aborted`) (≤4h)
- [ ] 7.4 Unit tests for op staging, overlay, commit/abort, and failure mapping; run the shared `SbmdDataAccess` contract test suite (owned by BARTON-423, also run against `LegacySbmdDataAccess`) against `DalSbmdDataAccess` with fake plugins (≤3h)

## 8. Documentation

- [ ] 8.1 Update `docs/SBMD.md` with the DAL architecture subsection: glossary (SBMD Transaction vs DAL Transaction), orchestrator diagram, wait-direction rule, plugin model, commit semantics, D11 atomic primitives and the read-compute-write limitation, D12 recovery rule, D13 delivery order (≤2h)

## 9. Verification

- [ ] 9.1 Build and run the full unit test suite in the Docker build container (SBMD sources build only with `BCORE_MATTER`); confirm no driver behavior change (orchestrator still uses `LegacySbmdDataAccess`; existing SBMD tests pass unchanged) (≤2h, requires Docker)
