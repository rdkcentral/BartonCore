## Why

The SBMD observability test suite (`testing/observability/`) has 38 tests, of which ~32 are cheap, deterministic, build-independent correctness checks (metric presence, exact counts, outcome taxonomy, invariants like `in_flight == 0`). Today all 38 are blanket-excluded from CI via `scripts/ci/run_integration_tests.sh --ignore="testing/observability"`, so none of them gate merges — a regression in any of them can only be caught by someone manually running the full observability suite. Separately, most of these same tests never contribute their result to the consolidated metrics report (`sbmd_metrics.txt`/`.json`): only `sbmd_scale_metrics_test.py` and the dedicated `sbmd_metrics_report_test.py` call into `sbmd_report_writer`, so a human reading the report (the artifact teams actually look at day to day) has no visibility into whether those ~32 correctness checks passed, even though pytest itself does catch their failures. Two related test files living outside the directory (`testing/test/sbmd_load_metrics_test.py`, `testing/test/sbmd_deferred_metrics_test.py`) duplicate coverage already present inside it, a historical artifact of being written before `testing/observability/` existed.

This change makes both gaps closable without restructuring every test body: a shared pytest marker + fixture lets any test opt into report-contribution, and a second marker lets build-independent tests opt into CI gating, selected via a marker expression instead of a blanket directory exclusion.

## What Changes

- Add an `sbmd_report` pytest marker + a shared fixture (hookwrapper-based, reading the test's own pass/fail outcome at teardown) so a test can contribute its specific check's result — not just a re-sampled metric value — to the consolidated report, with no change to the test's existing body or assertions.
- Add a `ci_gate` pytest marker for tests whose assertions are independent of the mquickjs `DEBUG_GC` compile-time flag (i.e., not timing/latency-bucket assertions that only hold under the production-representative build) and cheap enough to run on every CI invocation.
- Add a path-based `observability_suite` auto-tag via a `pytest_collection_modifyitems` hook in `testing/conftest.py`, so test selection for CI can be expressed as a marker expression.
- Replace `scripts/ci/run_integration_tests.sh`'s `--ignore="testing/observability"` with a marker-expression selection (`-m "not observability_suite or ci_gate"`) run as a single `py_test.sh testing/` invocation, so CI output shape is unchanged other than more tests appearing in the existing summary.
- Relocate `testing/test/sbmd_load_metrics_test.py` and `testing/test/sbmd_deferred_metrics_test.py` into `testing/observability/`, consolidating their coverage with the overlapping tests already there (e.g. the duplicate `test_driver_load_cost_metrics_populated`).
- Mark the ~32 identified build-independent tests with `ci_gate`; mark the large majority of observability tests with `sbmd_report` so their individual check result (not a generic re-sampled value) appears in `sbmd_metrics.txt`/`.json`.
- Add a pass/fail roll-up summary line to the top of `sbmd_metrics.txt`, sourced from the same per-test outcome data the new fixture already captures.
- Document the marker decision procedure (when a new test should get `ci_gate`, `sbmd_report`, both, or neither) in `testing/observability/README.md`.
- Add explicit, checked-in "sanity ceiling" assertions to two previously-assertion-free report-only tests, for the two metrics confirmed build-independent of the `DEBUG_GC` state: `sbmd.js.gc_roots` (a plain object count) and `sbmd.deferred.depth` on the non-runaway `toggle` resource. Each ceiling is a named constant documented with the real baseline it was calibrated from, updated only via reviewed code change — never computed or cached from a live CI run.

## Capabilities

### New Capabilities
(none — this extends the existing observability test suite capability rather than introducing a new one)

### Modified Capabilities
- `sbmd-observability-test-suite`: test categorization scheme (new markers), CI selection mechanism (marker expression instead of path `--ignore`), per-test report contribution requirement, file organization (the two relocated files), and a new checked-in sanity-ceiling requirement for the two confirmed build-independent report metrics.

## Impact

- `testing/conftest.py`: new `sbmd_report` fixture + `pytest_runtest_makereport` hookwrapper, new `observability_suite`/`ci_gate`/`sbmd_report` marker registrations, new `pytest_collection_modifyitems` path-based auto-tagging hook.
- `testing/observability/*.py` (7 files) and the 2 relocated files: marker annotations added to ~38 existing test functions; no assertion logic changes.
- `testing/helpers/sbmd_report_writer.py`: no changes required — existing `collect()`/`emit_report()` reused as-is.
- `scripts/ci/run_integration_tests.sh`: selection mechanism changes from `--ignore` to a marker expression.
- `testing/observability/README.md`: decision-tree documentation added, including the sanity-ceiling test shape.
- `testing/observability/sbmd_metrics_report_test.py`: two new hard asserts added (`gc_roots`, `deferred.depth` on `toggle`) — the only assertion logic changes in this whole change; everything else is markers/wiring.
- A new, not-CI-run helper script to assist a human comparing current measured values against the checked-in ceiling constants before deciding to update them.
- No changes to C/C++ code, CMake feature flags, or the public API — this is Python/pytest test infrastructure only.

## Non-Goals

- Does not add a hard pass/fail bound for `sbmd.js.heap.peak_bytes`. Unlike `gc_roots`/`deferred.depth`, `peak_bytes` carries a `DEBUG_GC`-dependent byte offset (the dev-build's ~128 KB dummy relocating block), so a sanity ceiling for it needs separate per-build calibration not designed here. Still a real, known gap, intentionally tracked as follow-up work.
- Does not add sanity ceilings for any timing/duration metric (driver load, handler duration, GC pause, mutex wait) — all are `DEBUG_GC`-magnitude-sensitive (50–130x inflation under the dev-default build), and CI has no on-demand way to flip to the production-representative build. Scoped here strictly to the two confirmed build-independent count metrics; extending this treatment to timing metrics (and `peak_bytes`) once a per-`DEBUG_GC`-build baseline mechanism exists is intentionally deferred, not ruled out.
- Does not solve the `DEBUG_GC`-in-CI problem for the 2 tests whose assertions require the production-representative mquickjs build (`gc.duration_ms` and `handler.duration_ms`'s "is it fast" claims) — they remain observability-suite-only.
- Does not add CI gating for the 4 heavy scale tests (`sbmd_scale_metrics_test.py`) — excluded on cost/runtime grounds, not correctness, and not revisited here.
- Does not commit to a dedicated agent-skill file (`.github/skills/.../SKILL.md`) for this pattern — the README is the committed documentation artifact for this change; a skill file is left as a team decision for later.
- Does not build any CI-runner-side caching or auto-updating mechanism for the sanity-ceiling constants — deliberately a plain, human-reviewed source constant instead (see design.md).
