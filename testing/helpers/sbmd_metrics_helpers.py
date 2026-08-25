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
Shared helpers for SBMD observability metrics integration tests.

Provides:
  - get_metrics(client)               — parse telemetry JSON into the metrics dict
  - find_datapoint(metrics, name, **) — locate a datapoint by metric name and
                                        attribute key/value filters
  - bucket_sum(dp, lo, hi)            — sum a slice of histogram bucket counts
  - format_histogram(dp, ...)         — human-readable ASCII bar chart of a
                                        histogram datapoint
  - format_heap_snapshot(label, m)    — formatted heap health summary block

Histogram bucket reference (15 bounds from observabilityMemory.c → 16 buckets):

  Index  Covers
  -----  ------
    0    ≤ 0      (negatives land here — heap_delta_bytes can be negative)
    1    (0,  5]
    2    (5, 10]
    3    (10, 25]
    4    (25, 50]
    5    (50, 75]
    6    (75, 100]
    7    (100, 250]
    8    (250, 500]
    9    (500, 750]
   10    (750, 1k]
   11    (1k, 2.5k]
   12    (2.5k, 5k]
   13    (5k, 7.5k]
   14    (7.5k, 10k]
   15    > 10k     (overflow)
"""

import json

# Upper bounds for each bucket as defined in observabilityMemory.c.
# These are shared by every histogram in the system (duration_ms, heap_delta_bytes, etc.)
HISTOGRAM_BOUNDS = [0, 5, 10, 25, 50, 75, 100, 250, 500, 750, 1000, 2500, 5000, 7500, 10000]
HISTOGRAM_LABELS = [
    "≤0", "≤5", "≤10", "≤25", "≤50", "≤75", "≤100",
    "≤250", "≤500", "≤750", "≤1k", "≤2.5k", "≤5k", "≤7.5k", "≤10k", ">10k",
]


def get_metrics(client):
    """Return the 'metrics' dict from a b_core_client_get_telemetry() JSON dump."""
    return json.loads(client.get_telemetry()).get("metrics", {})


def find_datapoint(metrics, metric_name, **attrs):
    """
    Return the first datapoint in *metric_name* whose attributes dict contains
    every key=value pair supplied in *attrs*.  Returns None when the metric is
    absent or no matching datapoint exists.

    Example:
        dp = find_datapoint(metrics, "sbmd.handler.outcome",
                            driver="light", op_type="write",
                            resource_id="isOn", outcome="success")
    """
    metric = metrics.get(metric_name)
    if metric is None:
        return None
    for dp in metric.get("dataPoints", []):
        dp_attrs = dp.get("attributes", {})
        if all(dp_attrs.get(k) == v for k, v in attrs.items()):
            return dp
    return None


def bucket_sum(dp, lo_index, hi_index_exclusive):
    """
    Sum histogram bucket counts from *lo_index* up to (but not including)
    *hi_index_exclusive*.  Treats missing bucket entries as 0.

    Each element of dp["buckets"] is {"le": <bound>, "count": <n>} as
    produced by observabilityDumpJson().

    Example — count observations ≤ 25 ms (buckets 0-3):
        bucket_sum(dp, 0, 4)
    """
    buckets = dp.get("buckets", [])
    return sum(
        buckets[i].get("count", 0)
        for i in range(lo_index, hi_index_exclusive)
        if i < len(buckets)
    )


def format_histogram(dp, unit="", bar_width=28):
    """
    Render a histogram datapoint as a human-readable ASCII bar chart.
    Returns a multi-line string; pass it directly to print().

    Only non-zero buckets are rendered.

    Args:
        dp:        histogram datapoint dict (from metrics JSON)
        unit:      suffix appended to count/sum/avg labels (e.g. "ms", " B")
        bar_width: max width of the ASCII bar in characters
    """
    count = dp.get("count", 0)
    if count == 0:
        return "  (no observations)"

    lines = [
        f"  count={count}  "
        f"sum={dp['sum']:.2f}{unit}  "
        f"avg={dp['sum'] / count:.2f}{unit}  "
        f"min={dp.get('min', '?')}{unit}  "
        f"max={dp.get('max', '?')}{unit}"
    ]

    # Each element is {"le": <bound>, "count": <n>} per observabilityDumpJson().
    buckets = dp.get("buckets", [])
    b_counts = [b.get("count", 0) for b in buckets]
    peak_bucket = max(b_counts) if b_counts else 1

    for label, b_count in zip(HISTOGRAM_LABELS, b_counts):
        if b_count == 0:
            continue
        bar = "█" * max(1, int(bar_width * b_count / max(peak_bucket, 1)))
        pct = 100.0 * b_count / count
        lines.append(f"  {label:>6}{unit}: {b_count:4d} ({pct:4.1f}%) |{bar}")

    return "\n".join(lines)


def format_heap_snapshot(label, metrics):
    """
    Print a labelled heap health summary block.

    Reads sbmd.js.heap.{arena,free,peak,used}_bytes and sbmd.js.gc_roots
    from *metrics* and prints a compact formatted block to stdout.

    NOTE: heap.peak_bytes is an all-time-high gauge (never decreases across the
    process lifetime).  It reflects the worst-case heap usage ever observed,
    not the current usage.
    """
    arena_dps = metrics.get("sbmd.js.heap.arena_bytes", {}).get("dataPoints", [])
    free_dps  = metrics.get("sbmd.js.heap.free_bytes",  {}).get("dataPoints", [])
    peak_dps  = metrics.get("sbmd.js.heap.peak_bytes",  {}).get("dataPoints", [])
    roots_dps = metrics.get("sbmd.js.gc_roots",         {}).get("dataPoints", [])
    used_dps  = metrics.get("sbmd.js.heap.used_bytes",  {}).get("dataPoints", [])

    arena = arena_dps[0]["value"] if arena_dps else 0
    free  = free_dps[0]["value"]  if free_dps  else 0
    peak  = peak_dps[0]["value"]  if peak_dps  else 0
    roots = roots_dps[0]["value"] if roots_dps else "n/a"

    total_count = sum(dp["count"] for dp in used_dps)
    total_sum   = sum(dp["sum"]   for dp in used_dps)
    avg_used    = total_sum / total_count if total_count else 0

    pct = lambda v: f"{v / arena:.1%}" if arena else "n/a"

    print(label)
    print(f"  arena:    {arena:>12,} bytes")
    print(f"  free:     {free:>12,} bytes  ({pct(free)})")
    print(f"  peak:     {peak:>12,} bytes  ({pct(peak)})  ← all-time high, never decreases")
    print(f"  avg used: {avg_used:>12,.0f} bytes  ({pct(avg_used)})")
    if roots != "n/a":
        print(f"  gc_roots: {roots}")
