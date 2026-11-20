## Context

`testing/observability/` holds 38 pytest tests characterizing the SBMD (mquickjs) JS runtime: heap/GC behavior, handler invocation, deferred-operation edge cases, failure-mode outcome taxonomy, mutex contention, and device/driver scale. `scripts/ci/run_integration_tests.sh` excludes the entire directory via `--ignore`, on the stated rationale that it is "heavy and produces a report rather than gating merges." That rationale is true for a minority of the suite (4 scale tests spinning up many matter.js subprocesses, and 7 tests in `sbmd_metrics_report_test.py` that have zero assertions by design) but not for the other 27, which are cheap, deterministic, hard `assert`-based correctness checks with no dependency on heavy fixtures.

Separately, only 11 of the 38 tests (the 4 scale tests + the 7 report-only tests) call into `testing/helpers/sbmd_report_writer.py`. The other 27 never contribute to `sbmd_metrics.txt`/`.json` — their results are only visible in raw pytest/console output, which is not what people read day to day (the report is). The report-only tests partially mask this: they independently re-sample most of the same metric families with their own load pattern, so metric *values* broadly appear in the report, but the 27 hard tests' *specific checks and pass/fail outcomes* do not.

Two files outside the directory (`testing/test/sbmd_load_metrics_test.py`, `testing/test/sbmd_deferred_metrics_test.py`) already run in CI and duplicate coverage inside the directory (e.g. an identically-named `test_driver_load_cost_metrics_populated` exists in both `sbmd_load_metrics_test.py` and `sbmd_baseline_metrics_test.py`) — a historical artifact of predating the directory's creation.

Constraint: 2 tests' assertions (`gc.duration_ms` and `handler.duration_ms`'s "is it fast" claims) depend on the mquickjs `DEBUG_GC` compile-time flag staying off; CI's build leaves it at its dev-default (`on`), which inflates these timings 50–130×. There is no CI-side mechanism to flip this today (`testing/gcflip.sh`'s cache is always cold on an ephemeral CI runner and requires a live network clone), so these 2 tests must stay out of any CI-gated set until that is solved separately.

## Goals / Non-Goals

**Goals:**
- Let any observability test opt into contributing its own check's pass/fail result (not just a re-sampled value) to the consolidated report, without restructuring the test body.
- Let build-independent, cheap tests opt into CI gating, selected via pytest markers rather than a blanket directory exclusion.
- Make the CI wiring change invisible to developers other than "more tests run" — same script shape, same single consolidated pytest summary.
- Eliminate the duplicate coverage between `testing/test/` and `testing/observability/` by consolidating into one location.

**Non-Goals:**
- Closing the `peak_bytes` coverage gap (no hard assertion exists for it in either suite today) — tracked as explicit, not-yet-scheduled follow-up.
- Solving the `DEBUG_GC`-in-CI problem for the 2 magnitude-blocked tests — they remain observability-suite-only.
- Adding CI gating for the 4 heavy scale tests — cost tradeoff, not revisited here.
- Any change to C/C++ runtime code, CMake flags, or the public API — this is pytest test infrastructure only; the "GObject/thread-safety/public API backward compatibility" review angles from the design template don't apply to this change.

## Decisions

### 1. Shared fixture + hookwrapper, not a per-test core/wrapper split

**Chosen:** A single `sbmd_report` pytest fixture, backed by a `pytest_runtest_makereport` hookwrapper (stashing the test outcome on the item, a standard pytest idiom), lets a test opt into report contribution by adding one marker (`@pytest.mark.sbmd_report(scenario=..., metrics=[...])`) and one fixture parameter. The fixture's teardown reads the test's own pass/fail outcome and, if reporting is enabled, calls the existing `collect()`/`emit_report()` functions unchanged.

**Alternative considered:** Split each test into a pure-assert "core" function plus a thin "wrapper" that adds printing/report calls (the originally discussed approach for the 3 scale-test call sites that already mix the two). Rejected as the general solution because it requires 38 new wrapper functions (one per test) — the fixture approach requires zero new functions, since the existing test body is already the "core."

### 2. CI selection via marker expression in a single invocation, not a second `py_test.sh` call

**Chosen:** Auto-tag every test under `testing/observability/` with an `observability_suite` marker via a path-based `pytest_collection_modifyitems` hook (no per-test annotation needed for this one). Explicitly mark the ~32 build-independent tests with `ci_gate`. Change `run_integration_tests.sh`'s single `py_test.sh testing/` call to add `-m "not observability_suite or ci_gate"` instead of `--ignore="testing/observability"`.

**Alternative considered:** Keep the directory-level `--ignore` and add a *second* `py_test.sh testing/observability -m ci_gate` invocation. Rejected: two invocations means two separate pytest summary blocks and two exit codes to combine in the shell script — a visible shape change in CI output, contrary to the goal that developers "shouldn't notice anything changed other than more tests running." The marker-expression approach keeps one invocation, one summary, and reduces the script diff to a single flag.

### 3. Relocate the two `testing/test/` files into `testing/observability/`

**Chosen:** Move `sbmd_load_metrics_test.py` and `sbmd_deferred_metrics_test.py` into `testing/observability/`, consolidating their coverage with the overlapping tests already there, and apply the same marker scheme uniformly.

**Alternative considered:** Leave them in place and just add markers/fixture usage from where they are. Rejected given the confirmed duplication (identical test name in two files, and `sbmd_deferred_edge_cases_metrics_test.py`'s own docstring already treats the outside file as part of the same family) — consolidating removes the duplication rather than papering over it with markers.

### 4. No change to `sbmd_report_writer.py`

**Chosen:** Reuse `collect()`/`emit_report()` exactly as they exist. The `context` dict is already rendered generically in `_render_scenario()` (`body.append(f"  context: {context}")`), so adding `check_status`/`check_message` keys requires no rendering changes. Metric grading reuses the existing `_assess()` dispatch table unchanged.

**Alternative considered:** Extend `sbmd_report_writer.py` with a dedicated "test outcome" artifact section. Rejected as unnecessary — the generic context rendering already accommodates this without any new code in that module.

### 5. Checked-in constants, not CI-runner caching, for the sanity-ceiling baselines

**Chosen:** Two of the seven report-only tests (`sbmd_metrics_report_test.py`) gain hard, generous ceiling assertions for the two metrics confirmed independent of `DEBUG_GC` magnitude: `sbmd.js.gc_roots` (a plain root-object count; 76 observed in the current baseline run) and `sbmd.deferred.depth` on the `toggle` resource specifically (0 observed; the `runawayToggle`/`timeoutToggle` resources deliberately trip the re-arm cap by design and are excluded from the ceiling). Each ceiling is a named Python constant with a comment citing the real measurement it was derived from and the multiplier chosen (e.g. "baseline 76, 2x margin, measured 2026-11-16"), changed only via a reviewed code change.

**Alternative considered:** Cache the baseline value in the CI runner (e.g. via `actions/cache`) so it self-updates from live CI runs. Rejected: hosted CI runners are ephemeral (the same reason `gcflip.sh`'s cache doesn't help in CI, see Risk below), and more importantly, a runner-maintained cache that updates itself from CI runs removes the human-review step that makes a ceiling meaningful — it would silently become "whatever the last run measured" rather than a deliberately chosen bound. A plain source constant needs no caching infrastructure at all: it's part of the checked-out tree on every clone, including CI's.

**Alternative considered:** A single flat multiplier (e.g. "2x") applied inline against whatever the current run measures. Rejected as a logical error, not just a design preference: "assert measured <= 2x measured-in-this-same-run" is a tautology, always true. The multiplier has to apply to a frozen prior baseline, which is exactly the checked-in-constant approach above.

**Removed rather than gated:** `test_heap_delta_per_call_is_bounded` asserted an average `sbmd.handler.heap_delta_bytes` below 1024 B. That average depends on where the lazy-GC sawtooth falls within the measured calls, so it changes with the `DEBUG_GC` state and cannot meet the `ci_gate` rule; it is also not a leak signal. The test was deleted. `sbmd.handler.heap_delta_bytes` is still recorded and graded in the report by per-series maximum, and retained-memory growth is covered by the `live_bytes` tests.

**Explicitly deferred from this treatment, not rejected:** every timing/duration metric (`driver.load.duration_ms`, `handler.duration_ms`, `gc.duration_ms`, `mutex.wait_ms`) and `sbmd.js.heap.peak_bytes` (which carries a `DEBUG_GC`-dependent byte offset, not just a timing one). All of these would need a baseline calibrated per-`DEBUG_GC`-state, which CI cannot produce on demand today — see the existing `gcflip.sh` CI-cache risk already documented in this file. This is tracked as a follow-up once that per-build baseline mechanism exists, not a closed decision.

## Risks / Trade-offs

- **[Risk]** The `sbmd_report` fixture's teardown needs a fixture to resolve the client (`request.getfixturevalue(...)`), which couples it to each test's fixture naming convention (`default_environment` vs others). → **Mitigation:** default to `default_environment`, override via `client_fixture=` marker kwarg for the few tests using a different fixture name (e.g. `matter_light`-based tests already call `default_environment.get_client()` directly in their bodies, so the convention already holds for nearly all 38 tests).
- **[Risk]** Relocating the two `testing/test/` files changes their import paths and could break any other reference to them (CI scripts, other tests importing helpers from them). → **Mitigation:** grep the whole repo for references to both file paths before moving; update `scripts/ci/run_integration_tests.sh`'s comment that currently calls out "pre-existing observability coverage under `testing/test/`."
- **[Risk]** A test marked `ci_gate` could later be modified by someone to introduce a `DEBUG_GC`-sensitive assertion, silently reintroducing CI flakiness without anyone re-auditing the marker. → **Mitigation:** document the exact decision criteria in `testing/observability/README.md` (and point to it from a short comment near the marker registration in `conftest.py`) so the criteria are visible at the point of decision, not just in this design doc.
- **[Risk]** The `driver.registration.total_ms`-style wall-clock assertions could be newly exposed to CI host contention once gated, producing rare flakes unrelated to real regressions. → **Mitigation:** accepted as a known, pre-existing risk (already flagged in the original characterization change's design doc), not new to this change; monitor after rollout rather than pre-solve.
- **[Risk]** A checked-in sanity-ceiling constant can rot in either direction — too loose to catch a real regression if the system naturally grows, or a source of recurring false alarms if it was calibrated too tightly against one noisy sample. → **Mitigation:** require the calibrating comment to cite the actual measurement and date it was taken (not just the final number), and provide a small, not-CI-run helper script that prints "current measured vs. checked-in ceiling" so a human has the data needed to decide whether to raise it, without the script itself being allowed to change the constant.

## Migration Plan

1. Add marker registrations (`observability_suite`, `ci_gate`, `sbmd_report`) and the `pytest_collection_modifyitems` auto-tagging hook in `testing/conftest.py`.
2. Add the `sbmd_report` fixture + `pytest_runtest_makereport` hookwrapper in `testing/conftest.py`.
3. Relocate the two `testing/test/` files into `testing/observability/`; resolve the duplicate test name during consolidation.
4. Annotate the ~32 build-independent tests with `ci_gate`; annotate the broad majority of tests with `sbmd_report` (scenario name + metrics list per test).
5. Add the pass/fail roll-up line to `sbmd_metrics.txt` rendering.
6. Update `scripts/ci/run_integration_tests.sh`'s selection mechanism and its explanatory comment.
7. Update `testing/observability/README.md` with the marker decision tree, including the new sanity-ceiling test shape.
8. Add the `gc_roots` and `deferred.depth` (`toggle`-only) sanity-ceiling constants and asserts to `sbmd_metrics_report_test.py`, with the calibrating comment; add the companion not-CI-run baseline-comparison helper script.
9. Validate: full build, unit tests, the newly CI-gated subset under the actual CI build (confirming the "medium confidence" `live_bytes`-bound tests tolerate `DEBUG_GC`-on), a full `run_observability_suite.py` run confirming the report still generates in the same format with the new roll-up line present, and the two new sanity-ceiling asserts passing against real current data.

Rollback is a plain revert — no data migration, no persistent state, no schema change to the report artifacts (additive fields only).

## Open Questions

- Should a dedicated `.github/skills/.../SKILL.md` be added for this pattern, or is the `testing/observability/README.md` update sufficient documentation? Left as a follow-up decision, not blocking this change.
- Should the `peak_bytes` gap be addressed in a follow-up change now that the marker/fixture infrastructure exists, or tracked separately without a committed timeline? Explicitly deferred per Non-Goals, but intentionally not forgotten.
- ~~Should the two new sanity-ceiling asserts (`gc_roots`, `deferred.depth`) also be marked `ci_gate`?~~ **Resolved: no.** `test_report_handler_invocation_profile` and `test_report_deferred_profile` call `emit_report()` directly in their own body (unlike every `sbmd_report`-fixture-based `ci_gate` test), with no `SBMD_REPORT_DISABLE` check at all. Marking them `ci_gate` would reintroduce unconditional report I/O into CI for exactly these two tests, undermining the reason `SBMD_REPORT_DISABLE` exists. (This also surfaced a real gap: `scripts/ci/run_integration_tests.sh` never actually set `SBMD_REPORT_DISABLE`, so the other 28 `ci_gate` tests were silently doing report I/O in CI already — fixed alongside this.)
- Once a per-`DEBUG_GC`-build baseline mechanism exists (tracked separately, not designed here), should the timing-based report metrics and `peak_bytes` get the same sanity-ceiling treatment, calibrated per build state?
