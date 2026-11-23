## 1. Shared infrastructure (testing/conftest.py)

- [x] 1.1 Register the `observability_suite`, `ci_gate`, and `sbmd_report` pytest markers in `pytest_configure`, alongside the existing `requires_matterjs`/`slow` registrations.
- [x] 1.2 Add a `pytest_collection_modifyitems` hook that auto-tags every collected test under `testing/observability/` with the `observability_suite` marker, based on file path.
- [x] 1.3 Add a `pytest_runtest_makereport` hookwrapper that stashes each test's outcome on the item (`item.rep_<when>`), for the `sbmd_report` fixture to read at teardown.
- [x] 1.4 Add the shared `sbmd_report` fixture: reads the `sbmd_report` marker's `scenario`/`metrics`/`client_fixture` kwargs, determines the test's pass/fail outcome from the stashed report, and (only when report emission is enabled for the invocation) calls the existing `collect()`/`emit_report()` with a `context` including `check_status` and `check_message`.
- [x] 1.5 Add a reporting-enabled check (reusing the existing `SBMD_REPORT_DIR`/environment convention already used by `run_observability_suite.py`) that the fixture consults before doing any report I/O.
- [x] 1.6 Unit-test the fixture and hookwrapper in isolation (a small throwaway pytest file exercising a passing and a failing marked test) to confirm report entries are written correctly and skipped correctly when disabled, before wiring it into the real 38 tests.

## 2. Consolidate test file locations

- [x] 2.1 Grep the repo for any reference to `testing/test/sbmd_load_metrics_test.py` and `testing/test/sbmd_deferred_metrics_test.py` outside the files themselves (CI scripts, docs, other tests) to confirm nothing else depends on their current path.
- [x] 2.2 Move `testing/test/sbmd_load_metrics_test.py` into `testing/observability/`; resolve its duplicate `test_driver_load_cost_metrics_populated` against the identically-named test in `sbmd_baseline_metrics_test.py` (merge into one, keeping the stronger assertion of the two).
- [x] 2.3 Move `testing/test/sbmd_deferred_metrics_test.py` into `testing/observability/`, keeping its cross-reference relationship with `sbmd_deferred_edge_cases_metrics_test.py` intact.
- [x] 2.4 Update `scripts/ci/run_integration_tests.sh`'s comment that currently references "pre-existing observability coverage under `testing/test/`" to reflect the new location.

## 3. Annotate existing tests with markers

- [x] 3.1 Mark the ~32 identified build-independent tests (per the metric-family audit: `baseline`, `deferred_edge_cases`, `failure_mode`, most of `handler` and `heap_gc_mutex`, plus the relocated `load`/`deferred` tests) with `ci_gate`.
- [x] 3.2 Explicitly confirm the 2 `DEBUG_GC`-sensitive tests (`test_handler_duration_in_low_latency_buckets`, `test_gc_fires_under_allocation_pressure`) and the 4 heavy scale tests are NOT marked `ci_gate`, with a short comment at each site explaining why.
- [x] 3.3 Add `sbmd_report` markers (scenario name + relevant metric names) to the broad majority of the 38 tests, requesting the `sbmd_report` fixture in each signature.
- [x] 3.4 For the failure-mode tests that deliberately trigger exceptions/timeouts, set the `fault_injection` context flag so their entries reuse the report's existing downgrade mechanism instead of looking like a concern.
- [x] 3.5 Leave the 7 tests in `sbmd_metrics_report_test.py` unmarked with `ci_gate` (no assertions to gate on) — confirm they keep working as pure report-only tests, unaffected by this change.

## 4. Report roll-up

- [x] 4.1 Add a pass/fail summary line (`N/M sbmd_report-tagged tests passed`) to the top of `_render` in `sbmd_report_writer.py`, sourced from the per-scenario `check_status` context fields already being written.
- [x] 4.2 Confirm `sbmd_metrics.json` also carries enough structured data to recompute the same roll-up programmatically (no format change needed beyond what task 1.4 already adds).

## 5. CI wiring

- [x] 5.1 Replace `--ignore="$REPO_ROOT/testing/observability"` in `scripts/ci/run_integration_tests.sh` with `-m "not observability_suite or ci_gate"` on the existing `py_test.sh testing/` invocation.
- [x] 5.2 Update the script's surrounding comment to explain the marker-based selection and point at `testing/observability/README.md` for the decision criteria.

## 6. Documentation

- [x] 6.1 Update `testing/observability/README.md` with the marker decision tree: report-only dump (no markers needed) vs. build-independent correctness check (`ci_gate` + `sbmd_report`) vs. magnitude-sensitive (`sbmd_report` only, with inline comment explaining the `DEBUG_GC` dependency) vs. heavy/scale (`sbmd_report` only, cost noted).
- [x] 6.2 Decide (with the team) whether a dedicated `.github/skills/.../SKILL.md` is warranted in addition to the README; defer if undecided — not a blocker for this change. (Deferred: left as an open question per design.md; README is the committed documentation artifact for this change.)

## 7. Validation

- [x] 7.1 Full build (`cmake --build build --target install`) and full unit test run (`ctest --test-dir build`) — confirm no regressions from the file moves. (425/425 unit tests passed.)
- [x] 7.2 Run the new CI-gated subset (`py_test.sh testing/ -m "not observability_suite or ci_gate"`) under the real CI build/environment, not just locally — specifically confirm the `live_bytes`-bound tests tolerate the `DEBUG_GC`-on constant offset as expected. (116/116 passed, 13 correctly deselected, in 868s.)
- [x] 7.3 Run the full `run_observability_suite.py` end to end — confirm `sbmd_metrics.txt`/`.json` still generate in the same format, with the new pass/fail roll-up line present and populated correctly. (Validated via direct `testing/observability` run — same report-writer code path `run_observability_suite.py` uses, minus the DEBUG_GC off-flip, which is orthogonal to this change: 41/41 passed, `CHECKS: 30/30 passed` rollup line present and correct. Surfaced one interesting pre-existing visibility gap — `sbmd.js.exception` flagged CONCERN in a scenario whose own assertion only checks `handler.outcome` — noted as a possible follow-up, not fixed here.)
- [x] 7.4 Re-run `openspec validate sbmd-observability-test-suite --strict` to confirm the delta spec merges cleanly with no drift. (Both `openspec validate sbmd-observability-ci-gating --strict` and `openspec validate sbmd-observability-test-suite --strict` pass clean.)

## 8. Sanity-ceiling checks for build-independent report metrics

- [x] 8.1 Add a checked-in `_SANITY_CEILING_GC_ROOTS` constant to `testing/observability/sbmd_metrics_report_test.py`, with a comment citing the real baseline it was calibrated from (76 observed as of 2026-11-16) and the multiplier chosen.
- [x] 8.2 Add a hard `assert` in `test_report_handler_invocation_profile` (and/or `test_report_heap_progression_over_ops`) that `sbmd.js.gc_roots` stays under that ceiling.
- [x] 8.3 Add a checked-in `_SANITY_CEILING_DEFERRED_DEPTH_TOGGLE` constant and a hard `assert` in `test_report_deferred_profile`, scoped to the `toggle` resource only — explicitly not `runawayToggle`/`timeoutToggle`, which deliberately exercise the re-arm cap by design.
- [x] 8.4 Write a small, not-run-in-CI helper script that reads a recent `sbmd_metrics.json` and prints current measured values next to the checked-in ceiling constants, to assist a human deciding whether to raise them — the script itself must never write to the constants. (`testing/helpers/sbmd_sanity_baseline_check.py`, imports the constants directly from the test module so they can't drift.)
- [x] 8.5 Update `testing/observability/README.md`'s decision tree with this fifth test shape (loose sanity-ceiling, build-independent counts only), including the explicit exclusion rule for `DEBUG_GC`-dependent/timing metrics and `peak_bytes`.
- [x] 8.6 Decide whether the two new sanity-ceiling asserts should also be marked `ci_gate`. **Decided: no** — both tests call `emit_report()` unconditionally with no `SBMD_REPORT_DISABLE` check, unlike every other `ci_gate` test; marking them would reintroduce report I/O into CI. This also surfaced and fixed a real pre-existing gap: `scripts/ci/run_integration_tests.sh` never actually set `SBMD_REPORT_DISABLE`, so the other 28 `ci_gate` tests were silently doing report I/O in CI already.
- [x] 8.7 Validate: run the two affected report tests directly, confirm both new asserts pass against current real data; re-run `openspec validate sbmd-observability-ci-gating --strict`. (Both tests pass; baseline-check script confirmed against the same run: gc_roots 76/150, deferred.depth{toggle} 0/2; `ci_gate` subset re-collected at 28/41 within `testing/observability` alone, unaffected by the new tests; `SBMD_REPORT_DISABLE=1` confirmed to produce zero report I/O for a real `ci_gate` test post-fix.)


## 9. Strict-review follow-ups

- [x] 9.1 Remove `test_heap_delta_per_call_is_bounded` (and its `_MAX_AVG_DELTA_BYTES` constant) from `sbmd_heap_gc_mutex_metrics_test.py`. Its average-delta bound depends on where the lazy-GC sawtooth falls within the measured calls, which varies with the `DEBUG_GC` state, so it could not meet the `ci_gate` "holds under any build" rule, and the average is not a leak signal (the report writer already grades the per-series max instead). Suite size drops from 41 to 40 tests, and the `ci_gate` subset from 28 to 27.
- [x] 9.2 `run_observability_suite.py` fails fast when `build/CMakeCache.txt` does not have both `BCORE_SBMD_METRICS` and `BCORE_SBMD_GC_INSTRUMENTATION` on, instead of letting every GC-dependent test skip while the run passes.
- [x] 9.3 Concurrent-load tests use daemon worker threads and fail when a worker is still blocked after its join timeout, so a write that never returns (e.g. a JS mutex deadlock) fails the test instead of letting it pass.
- [x] 9.4 `testing/gcflip.sh` installs archives with a temp-file-then-rename copy and serializes flips with `flock` on a lock file next to the shared installed archive, so runs with different `GCFLIP_CACHE_DIR` values also serialize.
- [x] 9.5 The report writer deletes only the CSV sidecars recorded in the previous run's report, never other CSV files in `SBMD_REPORT_DIR`.
- [x] 9.6 Re-collect the suite (`py_test.sh testing/observability --collect-only -q`, then with `-m ci_gate`) and confirm 40 tests and 27 `ci_gate`. (40 collected; 27/40 with `-m ci_gate`.)
- [x] 9.7 Re-run `openspec validate sbmd-observability-ci-gating --strict`. (Strict validation passes; full `run_observability_suite.py` passes under `DEBUG_GC`-off: 38 passed, 2 skipped; report roll-up: 28 passed, 0 failed, 1 skipped. Runner restored `DEBUG_GC`-on.)
- [x] 9.8 The `sbmd_report` fixture records a skipped test as `SKIP` (with the skip reason), and the report's `CHECKS` line counts passed, failed, and skipped separately; a skip is never counted as a failure.
