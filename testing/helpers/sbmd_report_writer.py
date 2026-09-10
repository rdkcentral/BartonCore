# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2026 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#
# ------------------------------ tabstop = 4 ----------------------------------

"""
Consolidated SBMD metrics report writer.

Reporting ("data-gathering") tests call emit_report() / emit_timeseries() to
record structured metric data.  Every scenario from a single pytest invocation
is merged into ONE consolidated set of artifacts under testing/.metrics-reports/:

  sbmd_metrics.json  — machine-parseable; every scenario, every metric
  sbmd_metrics.txt   — human-readable rendering of the same data
  <scenario>.csv     — time-series rows for scenarios that emit them

The report data is written to dedicated files, entirely separate from Barton C
library logs and pytest output, so it can be parsed without sifting through logs.

Because each test runs in its own subprocess (see conftest.py), artifacts are
keyed by SBMD_REPORT_RUN_ID — set once by the parent pytest process and inherited
by every child through the environment.  The first scenario of a run truncates
any stale file; the rest append.  A run with no reporting tests never touches the
artifacts, so a previous run's data is preserved.
"""

import csv
import json
import os
import time
from pathlib import Path

from testing.helpers.sbmd_metrics_helpers import HISTOGRAM_LABELS

REPORT_DIR = Path(
    os.environ.get(
        "SBMD_REPORT_DIR",
        str(Path(__file__).resolve().parent.parent / ".metrics-reports"),
    )
)
JSON_PATH = REPORT_DIR / "sbmd_metrics.json"
TXT_PATH = REPORT_DIR / "sbmd_metrics.txt"


def _run_id():
    return os.environ.get("SBMD_REPORT_RUN_ID", "manual")


def _fresh():
    return {
        "run_id": _run_id(),
        "generated": time.strftime("%Y-%m-%d %H:%M:%S %z"),
        "scenarios": {},
    }


def _load():
    """Load the current run's data, or start fresh if the file is from an old run."""
    if JSON_PATH.exists():
        try:
            data = json.loads(JSON_PATH.read_text())

            if data.get("run_id") == _run_id():
                return data
        except (ValueError, OSError):
            pass

    return _fresh()


def _write(data):
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    JSON_PATH.write_text(json.dumps(data, indent=2, default=str))
    TXT_PATH.write_text(_render(data))


def summarize_metric(metrics, name):
    """Return a clean, JSON-serializable summary of one metric and its datapoints."""
    metric = metrics.get(name)

    if metric is None:
        return {"present": False}

    return {
        "type": metric.get("type"),
        "unit": metric.get("unit"),
        "description": metric.get("description"),
        "dataPoints": metric.get("dataPoints", []),
    }


def collect(metrics, names):
    """Summarize a list of metric names into a {name: summary} dict."""
    return {name: summarize_metric(metrics, name) for name in names}


def emit_report(scenario, payload):
    """Record one scenario's structured data into the consolidated artifacts."""
    data = _load()
    data["scenarios"][scenario] = payload
    _write(data)


def emit_timeseries(scenario, columns, rows, payload=None):
    """Record a time-series scenario: writes a CSV sidecar and folds the rows
    into the consolidated JSON/txt so all three formats stay in sync."""
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    csv_path = REPORT_DIR / f"{scenario}.csv"

    with csv_path.open("w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(columns)
        writer.writerows(rows)

    entry = dict(payload or {})
    entry["timeseries"] = {"columns": columns, "rows": rows, "csv": csv_path.name}

    data = _load()
    data["scenarios"][scenario] = entry
    _write(data)


# ── assessment (status flags) ─────────────────────────────────────────────────
#
# The .json / .csv artifacts stay pure data.  Only the .txt is annotated with a
# verdict banner and a per-metric status so results are readable at a glance.
#
# Thresholds reuse the suite's existing pass/fail limits where they exist and add
# a hard "whoa that's bad" ceiling everywhere; see the constants below.

ARENA_BYTES = 1_048_576          # configured mquickjs arena (BCORE_MQUICKJS_MEMSIZE_BYTES)
SCRIPT_TIMEOUT_MS = 5000         # BCORE_SBMD_SCRIPT_TIMEOUT_MS (handler watchdog)
MAX_DEFERRAL_DEPTH = 10          # SbmdDeferredExecutor re-arm circuit-breaker

# Per-call transient allocation is graded per (driver, op_type) series against an
# absolute ceiling scaled to the arena — NOT a scenario-wide average, which
# conflates test composition with per-handler cost and hides a single hot series.
HEAP_DELTA_WATCH_FRAC = 0.008  # ~8 KB @ 1 MiB — above every known-good handler
HEAP_DELTA_CONCERN_FRAC = 0.03  # ~32 KB @ 1 MiB — a genuinely pathological call

_MARK = {"ok": "✅", "watch": "⚠️", "concern": "🚫", "devonly": "🔬", "info": "ℹ️"}
_SEV = {"concern": 3, "watch": 2, "devonly": 1, "info": 0, "ok": 0}
_BADGE = {
    "concern": "🚫 CONCERN",
    "watch": "⚠️  WATCH",
    "devonly": "🔬 dev-only",
    "info": "ℹ️  info",
    "ok": "✅ OK",
}


def _worse(a, b):
    return a if _SEV[a] >= _SEV[b] else b


def _hist_totals(summary):
    count = sum(dp.get("count", 0) for dp in summary.get("dataPoints", []))
    total = sum(dp.get("sum", 0) for dp in summary.get("dataPoints", []))
    return count, total


def _hist_avg(summary):
    count, total = _hist_totals(summary)
    return (total / count) if count else None


def _hist_max(summary):
    maxes = [dp.get("max") for dp in summary.get("dataPoints", []) if dp.get("max") is not None]
    return max(maxes) if maxes else None


def _counter_total(summary):
    return sum(dp.get("value", 0) for dp in summary.get("dataPoints", []))


def _gauge(summary):
    dps = summary.get("dataPoints", [])
    return dps[0].get("value") if dps else None


def _assess(name, summary):
    """Return (status, note) for one metric summary.  status ∈ ok/watch/concern/
    devonly/info."""
    # ── hard-fault / logical metrics (graded in every mode) ───────────────────
    if name == "sbmd.js.exception":
        n = _counter_total(summary)
        return ("concern" if n else "ok", f"{n} exception(s)")

    if name == "sbmd.driver.load.failure":
        n = _counter_total(summary)
        return ("concern" if n else "ok", f"{n} load failure(s)")

    if name == "sbmd.handler.outcome":
        bad = sum(
            dp.get("value", 0)
            for dp in summary.get("dataPoints", [])
            if dp.get("attributes", {}).get("outcome") not in (None, "success")
        )
        return ("concern" if bad else "ok", f"{bad} non-success outcome(s)")

    if name == "sbmd.deferred.in_flight":
        v = _gauge(summary) or 0
        return ("concern" if v > 0 else "ok", f"in_flight={v} at rest")

    if name == "sbmd.handler.heap_delta_bytes":
        dps = summary.get("dataPoints", [])
        if not dps:
            return "info", "no observations"
        # Grade the WORST per-(driver, op_type) series' max against an absolute
        # per-call ceiling, not the scenario average: the average moves with the
        # mix of ops a test happens to fire and would dilute a single hot series.
        # TODO(regression): the complementary half — flag a series that DRIFTS
        # from its own baseline even while under the ceiling — needs a persisted
        # per-series history to compare against. Not wired yet, so today we only
        # enforce the absolute ceiling.
        watch_ceil = ARENA_BYTES * HEAP_DELTA_WATCH_FRAC
        concern_ceil = ARENA_BYTES * HEAP_DELTA_CONCERN_FRAC
        worst, worst_attrs = None, {}
        for dp in dps:
            mx = dp.get("max")
            if mx is not None and (worst is None or mx > worst):
                worst, worst_attrs = mx, dp.get("attributes", {})
        if worst is None:
            return "info", "no observations"
        who = (
            " ".join(
                f"{k}={worst_attrs[k]}"
                for k in ("driver", "op_type", "resource_id")
                if worst_attrs.get(k)
            )
            or "(unlabeled)"
        )
        avg = _hist_avg(summary) or 0
        if worst >= concern_ceil:
            return "concern", (
                f"peak {worst:+,.0f} B/call on {who} "
                f"(≥{HEAP_DELTA_CONCERN_FRAC:.0%} arena) — likely per-call leak"
            )
        if worst >= watch_ceil:
            return "watch", (
                f"peak {worst:+,.0f} B/call on {who} (≥{HEAP_DELTA_WATCH_FRAC:.1%} arena)"
            )
        return "ok", (
            f"peak {worst:+,.0f} B/call on {who}, avg {avg:+.0f} (bounded/reclaimed)"
        )

    if name == "sbmd.deferred.depth":
        mx = _hist_max(summary)
        if mx is None:
            return "info", "no observations"
        # Driven on purpose by the deferred profile (runawayToggle saturates the
        # cap), so report rather than grade.  In the field, max == the cap
        # (MAX_DEFERRAL_DEPTH) means a runaway re-arm chain.
        return "info", f"max re-arm depth {mx} (cap is {MAX_DEFERRAL_DEPTH}; hitting it in the field = runaway)"

    if name == "sbmd.deferred.timeout":
        return "info", f"{_counter_total(summary)} deadline miss(es) (expected in profile; nonzero in field = concern)"

    if name == "sbmd.deferred.max_depth":
        return "info", f"{_counter_total(summary)} runaway-cap hit(s) (expected in profile)"

    if name == "sbmd.driver.registered.count":
        return "info", f"{_gauge(summary)} drivers registered"

    if name == "sbmd.js.heap.arena_bytes":
        return "info", f"{_gauge(summary):,} B configured arena"

    if name == "sbmd.js.gc_roots":
        v = _gauge(summary)
        if not v:
            return "info", "0 = unmeasured baseline (recorded only during handler calls)"
        return "info", f"{v} live roots (leak signal is growth over time — see heap_progression)"

    if name == "sbmd.js.heap.live_bytes":
        # Post-GC (compacted) live set — the one heap metric that is TRUE
        # occupancy and therefore graded.  Sampled at GC end, so it excludes the
        # transient garbage that inflates used_bytes/peak_bytes.  A rising floor
        # here (see heap_progression time-series) is a genuine retained leak.
        v = _gauge(summary)
        if not v:
            return "info", "no post-GC sample yet (recorded only when a GC fires)"
        frac = v / ARENA_BYTES
        if frac >= 0.75:
            return (
                "concern",
                f"live set {frac:.1%} of arena — genuine occupancy near OOM",
            )
        if frac >= 0.50:
            return "watch", f"live set {frac:.1%} of arena (retained, post-GC)"
        return "ok", f"live set {v:,} B ({frac:.1%} of arena, retained post-GC)"

    if name == "sbmd.js.heap.peak_bytes":
        # All-time high-water of bytes outstanding *before* a GC ran.  Under
        # mquickjs's lazy compacting GC this rides near 100% and is NOT current
        # occupancy, so it is reported, never graded.  (An OOM would surface as a
        # JS exception / allocation failure, which sbmd.js.exception grades.)
        frac = (_gauge(summary) or 0) / ARENA_BYTES
        return "info", f"peak {frac:.1%} of arena (all-time high-water, not live occupancy)"

    if name == "sbmd.js.heap.free_bytes":
        # Instantaneous free space.  Dips are expected under lazy GC (garbage is
        # reclaimed on demand), so this is reported, never graded.
        frac = (_gauge(summary) or 0) / ARENA_BYTES
        return "info", f"{frac:.1%} free at sample time (uncollected garbage is reclaimable)"

    if name == "sbmd.js.heap.used_bytes":
        # arena - free at sample time; under load this includes uncollected
        # garbage, so it is reported, never graded.
        mx = _hist_max(summary)
        return "info", (f"max used {mx:,} B" if mx is not None else "no data")

    if name in ("sbmd.handler.duration_ms", "sbmd.deferred.duration_ms"):
        mx = _hist_max(summary)
        if mx is None:
            return "info", "no observations"
        frac = mx / SCRIPT_TIMEOUT_MS
        if frac >= 0.50:
            return "concern", f"max {mx:.1f} ms — >50% of {SCRIPT_TIMEOUT_MS} ms timeout"
        if frac >= 0.10:
            return "watch", f"max {mx:.1f} ms"
        return "ok", f"max {mx:.1f} ms (well under {SCRIPT_TIMEOUT_MS} ms timeout)"

    if name == "sbmd.js.gc.duration_ms":
        mx = _hist_max(summary)
        if mx is None:
            return "info", "no observations"
        if mx >= 100:
            return "concern", f"max GC pause {mx:.1f} ms — user-visible stall"
        if mx >= 25:
            return "watch", f"max GC pause {mx:.1f} ms"
        return "ok", f"max GC pause {mx:.1f} ms"

    if name == "sbmd.js.gc.count":
        return "info", f"{_gauge(summary):,} GC cycles"

    if name == "sbmd.js.mutex.wait_ms":
        avg = _hist_avg(summary)
        mx = _hist_max(summary)
        if avg is None:
            return "ok", "no contention observed"
        if avg >= 10 or (mx is not None and mx >= 100):
            return "concern", f"avg {avg:.2f} ms / max {mx} ms — serialization bottleneck"
        if avg >= 2:
            return "watch", f"avg {avg:.2f} ms"
        return "ok", f"avg {avg:.3f} ms (negligible)"

    if name == "sbmd.driver.load.duration_ms":
        mx = _hist_max(summary)
        if mx is None:
            return "info", "no data"
        if mx >= 1000:
            return "concern", f"slowest driver {mx:.0f} ms"
        if mx >= 250:
            return "watch", f"slowest driver {mx:.0f} ms"
        return "ok", f"slowest driver {mx:.0f} ms"

    if name in ("sbmd.driver.registration.total_ms", "sbmd.driver.bundle_load_ms"):
        total = sum(dp.get("sum", 0) for dp in summary.get("dataPoints", []))
        label = "registration total" if name.endswith("total_ms") else "bundle load"
        return "info", f"{label} {total:.0f} ms"

    return "info", ""


# ── pretty (.txt) rendering ───────────────────────────────────────────────────

def _hist_line(dp):
    count = dp.get("count", 0)

    if not count:
        return "histogram: (no observations)"

    avg = dp.get("sum", 0) / count

    return (
        f"histogram: count={count} sum={dp.get('sum', 0):.2f} "
        f"avg={avg:.2f} min={dp.get('min', '?')} max={dp.get('max', '?')}"
    )


def _looks_like_histogram(value):
    return isinstance(value, dict) and "buckets" in value and "count" in value


def _render_datapoint(dp, indent):
    pad = " " * indent
    out = []
    attrs = dp.get("attributes", {})
    label = "  ".join(f"{k}={v}" for k, v in attrs.items()) or "(no attributes)"

    if _looks_like_histogram(dp):
        out.append(f"{pad}{label}")
        out.append(f"{pad}  {_hist_line(dp)}")
        counts = [b.get("count", 0) for b in dp.get("buckets", [])]

        for name, count in zip(HISTOGRAM_LABELS, counts):
            if count:
                out.append(f"{pad}    {name:>6}: {count}")
    else:
        out.append(f"{pad}{label}: {dp.get('value')}")

    return out


def _render_metric(name, summary, indent, fault_injection=False):
    pad = " " * indent
    status, note = _assess(name, summary)
    # In a fault-injection scenario the deliberate timeout/runaway trips record a
    # non-success handler.outcome; that is expected, not a real fault, so it must
    # not drive the verdict. The test itself is the real correctness gate.
    if fault_injection and name == "sbmd.handler.outcome" and status == "concern":
        status = "info"
        note = (
            f"{note} (expected — fault-injection scenario)"
            if note
            else "expected — fault-injection scenario"
        )
    tail = f" — {note}" if note else ""
    out = [f"{pad}{_MARK[status]} {name}{tail}"]

    if summary.get("present") is False:
        out.append(f"{pad}     (absent)")
        return out, status

    out.append(f"{pad}     {summary.get('type')} [{summary.get('unit')}]")

    for dp in summary.get("dataPoints", []):
        out.extend(_render_datapoint(dp, indent + 5))

    return out, status


def _render_metrics_map(metrics, indent, fault_injection=False):
    out = []
    worst = "ok"

    for name, summary in metrics.items():
        lines, status = _render_metric(name, summary, indent, fault_injection)
        out.extend(lines)
        worst = _worse(worst, status)

    return out, worst


def _fmt_cell(value):
    if isinstance(value, bool):
        return str(value)
    if isinstance(value, int):
        return f"{value:,}"
    if isinstance(value, float):
        return f"{value:,.0f}" if value == int(value) else f"{value:,.1f}"
    return str(value)


def _render_timeseries(ts, indent):
    pad = " " * indent
    cols = ts.get("columns", [])
    out = [f"{pad}time-series → {ts.get('csv')}"]
    out.append(pad + "  " + "".join(f"{c:>14}" for c in cols))

    for row in ts.get("rows", []):
        out.append(pad + "  " + "".join(f"{_fmt_cell(v):>14}" for v in row))

    return out


def _render_scenario(name, payload):
    body = []
    worst = "ok"

    context = payload.get("context")
    fault_injection = (
        bool(context.get("fault_injection")) if isinstance(context, dict) else False
    )
    if context:
        body.append(f"  context: {context}")

    for key in ("metrics", "sequential", "concurrent"):
        section = payload.get(key)

        if isinstance(section, dict):
            if key != "metrics":
                body.append(f"  [{key}]")

            lines, section_worst = _render_metrics_map(
                section,
                indent=2 if key == "metrics" else 4,
                fault_injection=fault_injection,
            )
            body.extend(lines)
            worst = _worse(worst, section_worst)

    timeseries = payload.get("timeseries")
    if timeseries:
        body.extend(_render_timeseries(timeseries, indent=2))

    head = [
        "",
        "─" * 72,
        f"▶ {name}   [{_BADGE[worst]}]",
        "─" * 72,
    ]

    return head + body, worst


def _render(data):
    scenario_lines = []
    overall = "ok"

    for name, payload in data.get("scenarios", {}).items():
        lines, worst = _render_scenario(name, payload)
        scenario_lines.extend(lines)
        overall = _worse(overall, worst)

    banner = [
        "=" * 72,
        "SBMD METRICS REPORT",
        f"  run_id:    {data.get('run_id')}",
        f"  generated: {data.get('generated')}",
        f"  VERDICT:   {_BADGE[overall]}",
        "=" * 72,
        "  Legend: ✅ ok   ⚠️ watch   🚫 concern   ℹ️ info",
    ]

    return "\n".join(banner + scenario_lines) + "\n"
