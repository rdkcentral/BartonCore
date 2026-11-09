## Why

Jira: **BARTON-422** (epic **BARTON-420** — "SBMD Data Access Layer: safe, consistent driver data access").

**Depends on:** BARTON-423 (`openspec/changes/sbmd-transaction-orchestrator/`), BARTON-421 (`openspec/changes/sbmd-dal-core/`), and BARTON-430 (`openspec/changes/sbmd-dal-source-plugins/`). Epic delivery order: 412 fix → 423 → 421 → 430 → **422** → 424 → 425. The epic-wide architecture, glossary, and decisions (D1–D13) live in `openspec/changes/sbmd-dal-core/design.md` and are not repeated here.

**Terminology:** An **SBMD Transaction** is the driver-level execution of one request, from the initial handler invocation through every parked device operation and deferred callback, to the terminal result (and, on failure, the failure handler). A **DAL Transaction** is the caller-agnostic data-access unit (per-key reads routed to source plugins, staged writes, commit or abort). The orchestrator reaches data only through the BARTON-423 seam `SbmdDataAccess` / `SbmdDataSession` (`Fetch`, `Stage`, `StageConditional`, `RecordAttributeWrite`, `Commit`, `Abort`). When this change lands, the seam is implemented by `DalSbmdDataAccess` (BARTON-421/430), whose session owns one DAL Transaction.

BARTON-411 has driver-visible root causes that the runtime alone cannot fix:

1. **Deferred callbacks received no fresh data.** Before BARTON-423 the only state a deferred callback saw was the `context` captured at park time, so `camera.sbmd.js` passed `{sessionId, sessions}` through `context` and wrote the stale map back. BARTON-423 fetches supplements per step, but the driver contract (inheritance, override, what each step sees) is not yet specified.
2. **Transient writes are whole-value only.** `storage.setTransientData` replaces the entire value, so two interleaved SBMD Transactions editing different members of one JSON object clobber each other even with fresh reads.
3. **Interleaving before any park (epic D11).** Under the orchestrator model a step's fetch, JS run, and commit are separate thread hops, so two concurrent SBMD Transactions can both read a counter such as `TD_NEXT_SESSION_ID` and allocate the same value. BARTON-423 and BARTON-421 provide sequence allocation and conditional writes in the seam and the DAL; drivers have no way to request them yet.

## What Changes

- **Fresh supplements at every step**: the initial handler and every deferred `onResponse`/`onError` (including timeout, pre-send failure, and depth-limit errors) receive `args.supplements` fetched through `SbmdDataSession::Fetch` at that step's time. Deferred callbacks inherit the declaration effective for the step that returned the terminal; a new optional `supplements` option on `requestCommand`/`readAttribute` overrides it. Each step sees its own SBMD Transaction's staged writes. The failure handler's supplements are declared on `onTransactionFailure` itself (BARTON-424) and fetched under the same rules.
- **Sequence supplements (epic D11)**: a handler may declare `supplements: { sequence: "<transientKey>" }` (or an array of keys). During the step's batched `Fetch`, each key is atomically incremented and the new value is delivered in `args.supplements.sequence["<key>"]`. Allocation is immediate and is not rolled back on abort or failed commit (gaps are acceptable).
- **Conditional write (epic D11)**: new non-terminal op `storage.setTransientDataIf(name, expected, value, ttlSecs)` staged with `SbmdDataSession::StageConditional`. At commit, a mismatch fails the commit; the SBMD Transaction fails with `args.failure.type = "commitFailed"` (reason `conditionFailed`), all staged writes are discarded, and the failure handler runs. `expected === null` means "key must be absent".
- **Per-member transient writes**: new non-terminal op `storage.mergeTransientData(name, patch, ttlSecs)` applying an RFC 7396 JSON Merge Patch to the value of `name` current at commit (a `null` member deletes it). Requires extending `SbmdWrite` and the DAL `Write` with a merge-patch form resolved by the transient data plugin.
- **Discard on error (confirmed, behavior change from BARTON-423)**: writes staged by any step of a failed SBMD Transaction, including writes preceding an `error` terminal, are discarded; `log` ops still run. This change extends it to the new ops.
- **Authoring guidance**: document the limitation that an unconditional read-compute-write of the same key can still lose an update, and direct authors to sequences, merges, or conditional writes.
- **Runtime plumbing**: `ResultOp::MergeTransientData` and `ResultOp::SetTransientDataIf` in `SbmdResultExecutor`; `SbmdHandlerInvoker::ToSbmdWrites` and `PlanFetch` extended; supplement override parsed on deferred terminals; `sbmd-result.js` and `sbmd-script.d.ts` gain the builder methods and option; JSON schema accepts `sequence` in supplements.
- **Schema/contract**: `docs/SBMD.md` §4.12, §5.1, §7.2, §7.3 updated; `core/deviceDrivers/matter/sbmd/schema/CHANGELOG.md` records the change.

### Non-goals

- The orchestrator, `SbmdTransaction`, operation log, seam, and threading — BARTON-423.
- DAL core, `AllocateSequence`/`StageConditional` implementation, commit rollback — BARTON-421; plugin implementations (including the transient plugin's `Atomic` capability) — BARTON-430.
- `onTransactionFailure` semantics — BARTON-424.
- Migrating `camera.sbmd.js` — BARTON-425.
- Sequences or conditional writes on resources, metadata, or persistent data (transient keys only in this change).
- Automatic retry on `conditionFailed`.
- Exposing transaction IDs or other DAL internals to JS.

## Capabilities

### New Capabilities

- `sbmd-driver-data-api`: Driver-facing data access semantics — fresh supplements at every step, supplement override, sequence supplements, `storage.setTransientDataIf`, `storage.mergeTransientData`, discard on failure, and the interleaving limitation.

### Modified Capabilities

- `sbmd-v4-runtime`: "Supplements pre-loading" adds transient and sequence supplements fetched per step; "Result chain execution" adds `mergeTransientData` and `setTransientDataIf`; "Deferred operations" adds supplement inheritance and the `supplements` override. Archive order is 423 → 422 → 424; this delta restates each requirement as BARTON-423 leaves it plus these additions.

## Impact

- **Affected layers**: SBMD runtime (`core/deviceDrivers/matter/sbmd/`), the internal data seam and DAL write type (additive field), the transient data plugin (merge resolution), SBMD script common library, SBMD schema, documentation. No core service, subsystem, or public API changes.
- **Affected files**: `mquickjs/SbmdResultExecutor.{h,cpp}`, `mquickjs/SbmdHandlerInvoker.{h,cpp}`, `mquickjs/SbmdWireContract.h`, `txn/SbmdDataAccess.h` (`SbmdWrite::mergePatchJson`), `txn/DalSbmdDataAccess.cpp`, `dal/DalTypes.h` (`Write::mergePatchJson`), the transient data plugin, `SbmdTransactionOrchestrator.cpp` (override inheritance), `scriptCommon/sbmd-result.js`, `scriptCommon/sbmd-script.d.ts`, `schema/` JSON schema and `CHANGELOG.md`, `docs/SBMD.md`; tests in `core/test/src/SbmdResultExecutorTest.cpp`, `SbmdHandlerInvokerTest.cpp`, `SbmdDriverTest.cpp`, transient plugin tests.
- **CMake flags**: `BCORE_MATTER`. No new flags. cJSON_Utils is not linked anywhere in the repo; the design uses an in-tree RFC 7396 implementation (see design D4).
- **Backward compatibility**: All JS additions are additive; `context` is unchanged; drivers that declare no supplements receive none. Discard-on-error was introduced by BARTON-423 and is unchanged here.
- **Public API / GObject**: None.
