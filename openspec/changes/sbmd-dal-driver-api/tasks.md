## 0. Prerequisites

- [ ] 0.1 Confirm BARTON-412, BARTON-423 (`sbmd-transaction-orchestrator`), BARTON-421 (`sbmd-dal-core`), and BARTON-426 (`sbmd-dal-source-plugins`) are merged, and `DalSbmdDataAccess` is the active seam implementation

## 1. Driver API surface (JS contract)

- [ ] 1.1 Add `storage.mergeTransientData`, `storage.setTransientDataIf`, the `supplements` option for `requestCommand`/`readAttribute`, and `sequence` supplements to `scriptCommon/sbmd-result.js`, `scriptCommon/sbmd-script.d.ts`, the JSON schema, and `SbmdWireContract.h` (~3h)
- [ ] 1.2 Unit tests: builder validation, wire shape, schema accepts `sequence` string/array (~2h)

## 2. Seam and DAL write extensions

- [ ] 2.1 Add `SbmdWrite::mergePatchJson` and DAL `Write::mergePatchJson`; map in `DalSbmdDataAccess` (~2h)
- [ ] 2.2 Implement in-tree RFC 7396 `ApplyJsonMergePatch` (no `cjson_utils` dependency) and merge resolution in the transient plugin commit (~3h)
- [ ] 2.3 Unit tests: RFC 7396 Appendix A vectors, absent/non-object targets, merge-after-set, merge-after-merge, read-your-own-staged merge, `ttlSecs==0` (~3h)

## 3. Result parsing and staging

- [ ] 3.1 Add `ResultOp::MergeTransientData` and `ResultOp::SetTransientDataIf` parsing; extend `SbmdHandlerInvoker::ToSbmdWrites` to emit merge writes and conditional writes in op order (~3h)
- [ ] 3.2 Unit tests in `SbmdResultExecutorTest.cpp` / `SbmdHandlerInvokerTest.cpp` (~2h)
- [ ] 3.3 Runtime tests: `setTransientDataIf` mismatch fails with `commitFailed`/`conditionFailed`, discards other staged writes, invokes the failure hand-off; match commits (~3h)

## 4. Supplements

- [ ] 4.1 Add `sequences` to `SbmdSupplements`, loader parsing, `PlanFetch` and `ToFetchedSupplements` mapping to `args.supplements.sequence` (~3h)
- [ ] 4.2 Add `supplementDecl` to the armed hop, set from override or inherited declaration; use it in `SupplementsForStep` (~2h)
- [ ] 4.3 Runtime tests: concurrent change visible in `onResponse`/`onError`/timeout; override replaces inherited; concurrent transactions get distinct sequence values; sequence not rolled back on abort (~4h)

## 5. Documentation

- [ ] 5.1 Update `docs/SBMD.md` §4.12, §5.1, §7.2, §7.3 and add "Interleaving safety" guidance; update `schema/CHANGELOG.md` (~2h)

## 6. Regression (Docker)

- [ ] 6.1 Run full SBMD unit suites and pytest integration suite in Docker (~2h, requires Docker)
