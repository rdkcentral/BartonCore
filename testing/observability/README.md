# SBMD observability test suite

Characterization tests for the SBMD JavaScript runtime (mquickjs). They exercise
the runtime under a range of workloads — startup, single-handler profiling,
GC pressure, deferred-execution edge cases, and multi-device / multi-driver
scale — and emit a consolidated **metrics report** instead of just passing or
failing. Use them to answer "how does the SBMD runtime behave?", not to gate
merges (they are excluded from the CI integration run).

Tests live in this directory (`testing/observability/`). The narrative writeup of
what the current run found is in
[`testing/sbmd_observability_findings.md`](../sbmd_observability_findings.md).

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
