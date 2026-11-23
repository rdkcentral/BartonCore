# SBMD observability test suite

Characterization tests for the SBMD JavaScript runtime (mquickjs). They exercise
the runtime under a range of workloads — startup, single-handler profiling,
GC pressure, deferred-execution edge cases, and multi-device / multi-driver
scale — and most of them also emit a consolidated **metrics report** instead of
just passing or failing. Use them to answer "how does the SBMD runtime behave?"

A build-independent, cheap subset of these tests (marked `ci_gate`) also runs as
part of the default CI integration suite, so they DO gate merges. The rest —
anything `DEBUG_GC`-magnitude-sensitive, or heavy/multi-subprocess scale
scenarios, or the pure-reporting tests with no assertions at all — stays
observability-suite-only. See "Adding a new test" below for the exact decision
criteria.

Tests live in this directory (`testing/observability/`). There is no checked-in
findings writeup — run the suite (see below) to generate current results; the
report is regenerated fresh every run, so a stale committed snapshot would only
mislead.

---

## Adding a new test: which marker(s) does it need?

Every test here is auto-tagged `observability_suite` by path (no action needed).
Beyond that, decide per test:

| Test shape | `ci_gate`? | `sbmd_report`? |
|---|---|---|
| Pure reporting dump, no `assert` at all (like `sbmd_metrics_report_test.py`) | No | No — it already calls `emit_report()`/`collect()` directly in its own body; the fixture would be redundant |
| Correctness check whose assertions hold under *any* build (presence, exact counts, outcome taxonomy, generous/ratio bounds) | **Yes** | **Yes** |
| Correctness check that asserts a tight timing/latency-bucket claim (e.g. "GC pauses are fast", "handler calls are fast") | No — `DEBUG_GC`'s dev-default inflates these 50–130x and CI cannot flip it on demand today | **Yes** — still valuable in the observability-suite report |
| Heavy/scale scenario spinning up many matter.js subprocesses or generated drivers | No — cost, not correctness; a CI-gating decision for these is a separate tradeoff, not made here | Usually not needed — the existing scale tests already call `emit_report`/`emit_timeseries` directly |
| Loose "did this go completely off the rails" sanity ceiling on a report-only test's metric, where the metric is a plain **count** confirmed independent of `DEBUG_GC` (e.g. `sbmd.js.gc_roots`, `sbmd.deferred.depth` on a non-runaway resource) | Worth considering — it's a build-independent hard assert once added | N/A — the test already self-reports; add a plain `assert` against a checked-in ceiling constant instead |

### Sanity-ceiling methodology (the fifth shape above)

This is **not** the same as a regression gate — it's a generous bound meant to catch "something ran away," not "did this get slightly slower." Two examples exist today in `sbmd_metrics_report_test.py`: `_SANITY_CEILING_GC_ROOTS` and `_SANITY_CEILING_DEFERRED_DEPTH_TOGGLE`.

- **The ceiling must be a named, checked-in constant** with a comment citing the real baseline measurement it was derived from (value, date, which report it came from) and the multiplier chosen. Never compute it from the same run that checks it — "assert measured ≤ 2× measured-in-this-run" is a tautology, always true.
- **Only apply this to metrics confirmed independent of `DEBUG_GC`** — plain object/event counts, not timing. Every duration/timing histogram (driver load, handler duration, GC pause, mutex wait) and `sbmd.js.heap.peak_bytes` (which carries its own `DEBUG_GC` byte offset) is explicitly deferred until a per-`DEBUG_GC`-build baseline mechanism exists — see `openspec/changes/sbmd-observability-ci-gating/design.md` Decision 5.
- **Watch for deliberately-triggered events.** `sbmd_deferred_profile`'s `runawayToggle`/`timeoutToggle` resources are *designed* to re-arm to the cap — the ceiling on `deferred.depth` applies only to the plain `toggle` resource, not the whole metric.
- **Updating a ceiling always goes through a reviewed code change**, never an automated or CI-side update. `testing/helpers/sbmd_sanity_baseline_check.py` is a standalone, not-CI-run helper that prints current measured values next to the checked-in constants, to give a human the data needed to decide — it never edits the constants itself.

To opt a test into report contribution without changing its assertions, add:

```python
@pytest.mark.ci_gate                 # omit this line if it doesn't qualify per the table above
@pytest.mark.sbmd_report(scenario="my_scenario_name", metrics=["sbmd.some.metric"])
def test_my_check(default_environment, sbmd_report):
    ...  # body is unchanged; sbmd_report is a fixture, not a function to call
```

If the test deliberately triggers a failure path (a timeout, a thrown exception,
a circuit breaker) as its *expected, passing* behavior, set
`sbmd_report["fault_injection"] = True` early in the test body — this reuses the
report's existing downgrade so a deliberately-induced non-success outcome isn't
graded as a concern.

---

## Running the full suite

One command does everything — build, prerequisites, run, and report:

```bash
python3 testing/run_observability_suite.py
```

That will:
1. `cmake --build build --target install` (so the `BCore` typelib is current),
2. start the D-Bus session bus and `npm ci` the matter.js virtual devices,
3. run every test in `testing/observability/` under the **DEBUG_GC off** build
   (see below), and
4. write the consolidated report and print its path.

### Common options

| Command | Effect |
|---|---|
| `python3 testing/run_observability_suite.py` | full run under DEBUG_GC off (default) |
| `… --no-build` | skip the cmake build + install (build already current) |
| `… --no-deps` | skip the D-Bus start + `npm ci` (already set up) |
| `… --debug-gc on` | run under the dev build and leave it on |
| `… --debug-gc both` | run under on **and** off, into separate report dirs |
| `… -- -k heap -v` | pass anything after `--` straight to pytest |

> **These tests are normally run under DEBUG_GC `off`, and that is the default — you
> normally pass no `--debug-gc` flag at all.** Every scenario in this suite, and
> every number in the findings doc, assumes the `off` build. The `on` and `both`
> options exist only for the occasional GC-behavior cross-check; they are not the
> standard way to run the suite. (Why `off` is the standard is explained just
> below.)

Example — quick re-run when the build and deps are already in place, filtered to
one scenario:

```bash
python3 testing/run_observability_suite.py --no-build --no-deps -- -k heterogeneous
```

### A note on DEBUG_GC

`DEBUG_GC` is a compile-time mquickjs flag that forces a garbage collection
before every allocation. It is the **dev default (resting state = on)** because
it surfaces use-after-move bugs, but it inflates timing and GC magnitudes ~50–130×,
so it is **not** representative for magnitude measurements. The runner therefore
defaults to the **off** build: it flips the flag off via
[`testing/gcflip.sh`](../gcflip.sh) (cached — only the first flip of each variant
rebuilds), runs the suite, and **always restores `on`** when finished. You never
toggle it by hand.

Counting/correctness metrics (exceptions, outcomes, load failures, deferred
depth/timeout) are GC-magnitude-independent and read the same either way.

### Running the tests directly (without the runner)

The suite is ordinary pytest, so you can run it through the ASAN wrapper:

```bash
./testing/py_test.sh testing/observability/ -v
```

Caveats if you do this: it runs under whatever DEBUG_GC build is currently
installed (the runner is what manages the flip), and you must have already built
+ installed BartonCore, started D-Bus, and `npm ci`'d the matter.js devices. The
tests are marked `slow` and `requires_matterjs`.

---

## Where the results go

Everything lands in **`testing/.metrics-reports/`**:

| File | What it is |
|---|---|
| `sbmd_metrics.txt` | human-readable report — verdict banner, per-scenario sections, per-metric status. **Start here.** |
| `sbmd_metrics.json` | the same data, machine-parseable (for scripting / diffing). |
| `<scenario>.csv` | per-scenario time series where one exists (e.g. `heap_progression.csv`, `device_scale.csv`). |
| `observability_run.log` | full console output of the run. |

`--debug-gc both` writes two parallel sets under
`.metrics-reports-debug_gc_on/` and `.metrics-reports-debug_gc_off/`.

The report is written by [`testing/helpers/sbmd_report_writer.py`](../helpers/sbmd_report_writer.py).
Each run **truncates** the previous report (it is keyed by a per-run id), so the
files always reflect the most recent run. Set `SBMD_REPORT_DIR` to route the
report somewhere else.

---

## Reading the report

Open `sbmd_metrics.txt`. It starts with a verdict banner:

```
SBMD METRICS REPORT
  run_id:    1787777621
  generated: 2026-08-26 20:55:03 +0000
  VERDICT:   ✅ OK
  Legend: ✅ ok   ⚠️ watch   🚫 concern   ℹ️ info
```

The **verdict** is the worst per-metric status across every scenario. Statuses:

- **✅ ok** — graded and within bounds.
- **⚠️ watch** — graded and approaching a threshold; look, don't panic.
- **🚫 concern** — graded and over a hard limit; investigate.
- **ℹ️ info** — reported, not graded (context or a high-water/instantaneous value
  that is not a capacity signal on its own).

Then one section per **scenario** (`startup_snapshot`, `handler_profile`,
`gc_pressure`, `deferred_profile`, `device_scale`, `concurrent_device_scale`,
`heterogeneous_driver_runtime`, `driver_scale`, …), each with its own badge and a
`context: {…}` line describing the workload (device count, ops, etc.).

### Metrics worth knowing

| Metric | Read it as |
|---|---|
| `sbmd.js.heap.live_bytes` | **true retained memory** (post-GC live set). The real occupancy signal. |
| `sbmd.js.heap.peak_bytes` / `free_bytes` / `used_bytes` | high-water / instantaneous — swing with reclaimable garbage; `info`, not capacity. |
| `sbmd.handler.duration_ms` | per-handler execution time vs the 5000 ms watchdog. |
| `sbmd.handler.heap_delta_bytes` | transient allocation per call; graded per `(driver, op_type)` series max against an arena-scaled ceiling. |
| `sbmd.js.mutex.wait_ms` | contention on the single shared JS mutex. |
| `sbmd.js.gc.count` / `gc.duration_ms` | GC frequency and pause length (needs the GC-instrumentation build). |
| `sbmd.driver.registration.total_ms` / `load.duration_ms` / `bundle_load_ms` | driver load timing at startup. |
| `sbmd.deferred.timeout` / `max_depth` | circuit breakers — should be **0** in the field (tests trip them on purpose). |
| `sbmd.js.exception`, `sbmd.handler.outcome` (error) | should be **0** in normal operation. |

Histograms print as `count / sum / avg / min / max` plus bucket counts; gauges
print their current value; counters print their total.

### Build options for the metrics

The SBMD runtime metrics need `BCORE_SBMD_METRICS` (on by default). Five
GC-derived metrics also need `BCORE_SBMD_GC_INSTRUMENTATION`: `sbmd.js.gc.count`,
`sbmd.js.gc.duration_ms`, `sbmd.js.gc_roots`, `sbmd.js.heap.live_bytes`, and
`sbmd.js.gc.alloc_churn_bytes`. That option is **off by default**, because its GC
callback adds work to every collection in shipping builds. The dev platform
profile (`config/cmake/platforms/dev/linux.cmake`, used by `./build.sh`) turns it
on; any other build that wants these metrics must pass
`-DBCORE_SBMD_GC_INSTRUMENTATION=ON`. `run_observability_suite.py` refuses to run
against a build directory that has it off.

`sbmd.driver.load.heap_bytes` is no longer recorded. Under the compacting GC, a
heap reading before and after a driver load does not measure that driver's
cost. Use `sbmd.js.heap.live_bytes` for retained memory instead.

### Time series (CSV)

Scenarios that sample a metric over a sweep also drop a `<scenario>.csv` — e.g.
`heap_progression.csv` (heap over N operations) and `device_scale.csv`
(`live_bytes` per commissioned device). Open these to see trends rather than a
single snapshot (a flat `live_bytes` column across the sweep = no leak / no
per-device growth).

### Parsing programmatically

`sbmd_metrics.json` mirrors the text report as structured data — use it to diff
runs or pull specific series. Shape:

```jsonc
{
  "run_id": "1787777621",
  "generated": "2026-08-26 20:55:03 +0000",
  "scenarios": {
    "heterogeneous_driver_runtime": {
      "context": { "drivers_driven": 5, "total_ops": 125, "...": "..." },
      "metrics": {
        "sbmd.handler.heap_delta_bytes": {
          "type": "histogram", "unit": "By", "description": "...",
          "dataPoints": [ { "attributes": {"driver": "thermostat", "op_type": "write"},
                            "count": 25, "sum": 65800, "min": 2632, "max": 2632 } ]
        }
      },
      "timeseries": { "columns": ["..."], "rows": [["..."]], "csv": "…path.csv" }
    }
  }
}
```

`timeseries` is present only on scenarios that sweep a metric (it backs the
`<scenario>.csv`). Note the report JSON is **not** the same as the live gateway
`getTelemetry` dump: the dump is a flat `{ "metrics": { … } }` snapshot, while
this report groups metrics by scenario and adds `context` / `timeseries`.
