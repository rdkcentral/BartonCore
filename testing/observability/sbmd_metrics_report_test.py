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
SBMD observability metrics — reporting tests.

These tests have no hard assertions.  Their sole purpose is to print
human-readable metric summaries so that engineers can see actual numbers
on every run.  Run with pytest -s to see the output.

  ./testing/py_test.sh -s -k sbmd_metrics_report

They are most useful when run:
  - On a fresh dev environment to establish baselines.
  - On gateway hardware to compare against dev baselines.
  - After adding or modifying SBMD drivers to see how startup cost changed.
"""

import threading
import time

import pytest

from testing.helpers.sbmd_metrics_helpers import (
    find_datapoint,
    format_heap_snapshot,
    format_histogram,
    get_metrics,
)
from testing.utils.barton_utils import (
    commission_device,
    resource_update_listener,
    resource_uri,
    wait_for_resource_value,
)
from testing.helpers.sbmd_report_writer import (
    collect,
    emit_report,
    emit_timeseries,
)

pytestmark = [pytest.mark.requires_matterjs]

_N_HANDLER_CALLS = 50
_N_HEAP_CALLS = 200
_HEAP_SNAPSHOT_INTERVAL = 50
_N_GC_PRESSURE = 20


def _heap_row(ops, metrics):
    """Extract one heap time-series row from a metrics snapshot."""
    def gauge(name):
        dps = metrics.get(name, {}).get("dataPoints", [])
        return dps[0]["value"] if dps else ""

    used = metrics.get("sbmd.js.heap.used_bytes", {}).get("dataPoints", [])
    used_count = sum(dp["count"] for dp in used)
    used_avg = sum(dp["sum"] for dp in used) / used_count if used_count else ""

    return [
        ops,
        gauge("sbmd.js.heap.peak_bytes"),
        gauge("sbmd.js.heap.free_bytes"),
        used_avg,
        gauge("sbmd.js.gc_roots"),
        gauge("sbmd.js.heap.arena_bytes"),
    ]


def test_report_startup_metrics_snapshot(default_environment):
    """
    Print all SBMD metrics at startup before any device is commissioned.
    This is the zero-load baseline: driver load costs, heap state, and
    any counters that have already fired during initialization.
    """
    client = default_environment.get_client()
    metrics = get_metrics(client)

    print("\n" + "=" * 60)
    print("SBMD METRICS — STARTUP SNAPSHOT")
    print("=" * 60)

    # ── Heap ─────────────────────────────────────────────────────────────────
    print("\n── JS Heap ──────────────────────────────────────────────")
    format_heap_snapshot("  State at startup:", metrics)
    used_dps = metrics.get("sbmd.js.heap.used_bytes", {}).get("dataPoints", [])
    if used_dps:
        print("  used_bytes histogram (Initialize() snapshot):")
        for dp in used_dps:
            print(format_histogram(dp, unit=" B"))

    # ── Driver load ───────────────────────────────────────────────────────────
    print("\n── Driver Load Times ────────────────────────────────────")
    dur_dps = metrics.get("sbmd.driver.load.duration_ms", {}).get("dataPoints", [])

    print(f"  {'Driver':<38} {'ms':>8}")
    print(f"  {'-' * 38} {'-' * 8}")
    total_ms = 0.0
    for dp in sorted(dur_dps, key=lambda d: -d["sum"]):
        drv = dp.get("attributes", {}).get("driver", "?")
        ms  = dp["sum"]
        total_ms += ms
        print(f"  {drv:<38} {ms:>8.1f}")

    print(f"  {'-' * 38} {'-' * 8}")
    print(f"  {'TOTAL':<38} {total_ms:>8.1f}")

    reg_dps = metrics.get("sbmd.driver.registered.count", {}).get("dataPoints", [])
    registered = reg_dps[0].get("value", "?") if reg_dps else "?"
    print(f"\n  Registered drivers: {registered}")

    # ── Deferred & exceptions ─────────────────────────────────────────────────
    print("\n── Deferred & Exceptions ────────────────────────────────")
    in_flight_dps = metrics.get("sbmd.deferred.in_flight", {}).get("dataPoints", [])
    in_flight = in_flight_dps[0].get("value", "?") if in_flight_dps else "?"
    print(f"  deferred.in_flight: {in_flight}")
    exc_dps = metrics.get("sbmd.js.exception", {}).get("dataPoints", [])
    if exc_dps:
        for dp in exc_dps:
            print(f"  js.exception {dp.get('attributes', {})}: {dp['value']}")
    else:
        print("  js.exception: (no observations — expected at startup)")

    emit_report("startup_snapshot", {
        "context": {"commissioned_devices": 0},
        "metrics": collect(metrics, [
            "sbmd.js.heap.arena_bytes",
            "sbmd.js.heap.free_bytes",
            "sbmd.js.heap.peak_bytes",
            "sbmd.js.heap.used_bytes",
            "sbmd.js.gc_roots",
            "sbmd.driver.load.duration_ms",
            "sbmd.driver.load.failure",
            "sbmd.driver.registered.count",
            "sbmd.deferred.in_flight",
            "sbmd.js.exception",
        ]),
    })

    print("\n" + "=" * 60)


def test_report_driver_startup_costs(default_environment):
    """
    Print a table of per-driver load times, sorted by duration.

    This is the primary diagnostic for the driver startup budget question.
    On dev hardware, run this first; then run on a gateway and compare the two
    tables to see the SoC flash/CPU cost difference.
    """
    client = default_environment.get_client()
    metrics = get_metrics(client)

    dur_dps = metrics.get("sbmd.driver.load.duration_ms", {}).get("dataPoints", [])

    print(f"\n{'=' * 66}")
    print("SBMD DRIVER LOAD TIMES  (sorted by duration, slowest first)")
    print(f"{'=' * 66}")
    print(f"  {'Driver':<38} {'Duration (ms)':>14}")
    print(f"  {'-' * 38} {'-' * 14}")

    total_ms = 0.0
    for dp in sorted(dur_dps, key=lambda d: -d["sum"]):
        drv = dp.get("attributes", {}).get("driver", "?")
        ms  = dp["sum"]
        total_ms += ms
        print(f"  {drv:<38} {ms:>14.2f}")

    print(f"  {'-' * 38} {'-' * 14}")
    print(f"  {'TOTAL (eval+activate)':<38} {total_ms:>14.2f}")

    def _hsum(name):
        return sum(dp["sum"] for dp in metrics.get(name, {}).get("dataPoints", []))

    reg_total = _hsum("sbmd.driver.registration.total_ms")
    bundle = _hsum("sbmd.driver.bundle_load_ms")
    print(f"  {'bundle load (one-time)':<38} {bundle:>14.2f}")
    print(f"  {'registration total (discover->reg)':<38} {reg_total:>14.2f}")
    print(f"  {'unaccounted (file I/O + register)':<38} {reg_total - bundle - total_ms:>14.2f}")

    emit_report("driver_startup_costs", {
        "metrics": collect(metrics, [
            "sbmd.driver.load.duration_ms",
            "sbmd.driver.registration.total_ms",
            "sbmd.driver.bundle_load_ms",
            "sbmd.driver.registered.count",
            "sbmd.driver.load.failure",
        ]),
    })
    print(f"{'=' * 66}")


def test_report_handler_invocation_profile(default_environment, matter_light):
    """
    Commission a light, execute _N_HANDLER_CALLS writes on the isOn resource,
    and print the full handler metric distributions:
      - handler.duration_ms distribution (latency profile)
      - handler.heap_delta_bytes distribution (per-call allocation profile)
      - handler.outcome breakdown
      - heap state after the load
    """
    device = commission_device(default_environment, matter_light, "light")
    client = default_environment.get_client()

    # Alternate writes so the virtual device actually changes state each time.
    for i in range(_N_HANDLER_CALLS):
        value = "true" if i % 2 == 0 else "false"
        queue = resource_update_listener(client, "isOn")
        client.write_resource(resource_uri(device, "isOn", endpoint_id=1), value)
        wait_for_resource_value(queue, value, timeout=10)

    metrics = get_metrics(client)

    print(f"\n{'=' * 60}")
    print(f"SBMD HANDLER PROFILE — {_N_HANDLER_CALLS} isOn writes on 'light' driver")
    print(f"{'=' * 60}")

    # duration_ms
    print("\n── handler.duration_ms (write / isOn) ───────────────────")
    dur_dp = find_datapoint(
        metrics, "sbmd.handler.duration_ms",
        driver="light", op_type="write", resource_id="isOn",
    )
    if dur_dp:
        print(format_histogram(dur_dp, unit="ms"))
    else:
        print("  (no datapoint found)")

    # heap_delta_bytes
    print("\n── handler.heap_delta_bytes (write / isOn) ──────────────")
    delta_dp = find_datapoint(
        metrics, "sbmd.handler.heap_delta_bytes",
        driver="light", op_type="write", resource_id="isOn",
    )
    if delta_dp:
        print(format_histogram(delta_dp, unit=" B"))
        neg = delta_dp.get("buckets", [{}])[0].get("count", 0)
        print(
            f"  bucket[0] (≤0 B, i.e. GC reclaimed): "
            f"{neg} observations"
        )
        if delta_dp["count"]:
            avg = delta_dp["sum"] / delta_dp["count"]
            print(f"  net avg delta per call: {avg:+.1f} B")
    else:
        print("  (no datapoint found)")

    # outcome
    print("\n── handler.outcome ──────────────────────────────────────")
    for dp in metrics.get("sbmd.handler.outcome", {}).get("dataPoints", []):
        attrs = dp.get("attributes", {})
        if attrs.get("driver") == "light" and attrs.get("resource_id") == "isOn":
            print(
                f"  op={attrs.get('op_type')}  outcome={attrs.get('outcome')}: "
                f"{dp['value']}"
            )

    # heap state
    print()
    format_heap_snapshot(
        f"Heap after {_N_HANDLER_CALLS} handler calls:", metrics
    )

    emit_report("handler_profile", {
        "context": {
            "driver": "light", "op_type": "write",
            "resource_id": "isOn", "calls": _N_HANDLER_CALLS,
        },
        "metrics": collect(metrics, [
            "sbmd.handler.duration_ms",
            "sbmd.handler.heap_delta_bytes",
            "sbmd.handler.outcome",
            "sbmd.js.heap.peak_bytes",
            "sbmd.js.heap.free_bytes",
            "sbmd.js.gc_roots",
        ]),
    })
    print(f"{'=' * 60}")


def test_report_heap_progression_over_ops(default_environment, matter_light):
    """
    Execute _N_HEAP_CALLS isOn writes, printing a heap snapshot every
    _HEAP_SNAPSHOT_INTERVAL calls.  Shows whether memory is stable, growing,
    or oscillating under sustained load.
    """
    device = commission_device(default_environment, matter_light, "light")
    client = default_environment.get_client()

    print(f"\n{'=' * 60}")
    print(f"SBMD HEAP PROGRESSION — {_N_HEAP_CALLS} isOn writes")
    print(f"{'=' * 60}")

    format_heap_snapshot("Heap at start (pre-ops):", get_metrics(client))

    rows = []
    start_metrics = get_metrics(client)
    rows.append(_heap_row(0, start_metrics))

    for i in range(_N_HEAP_CALLS):
        value = "true" if i % 2 == 0 else "false"
        client.write_resource(resource_uri(device, "isOn", endpoint_id=1), value)

        if (i + 1) % _HEAP_SNAPSHOT_INTERVAL == 0:
            time.sleep(0.3)
            snapshot = get_metrics(client)
            format_heap_snapshot(f"Heap after {i + 1} ops:", snapshot)
            rows.append(_heap_row(i + 1, snapshot))

    time.sleep(1)
    metrics = get_metrics(client)

    # Summary signals
    peak  = metrics.get("sbmd.js.heap.peak_bytes",  {}).get("dataPoints", [{}])[0].get("value", 0)
    arena = metrics.get("sbmd.js.heap.arena_bytes",  {}).get("dataPoints", [{}])[0].get("value", 1)
    free  = metrics.get("sbmd.js.heap.free_bytes",   {}).get("dataPoints", [{}])[0].get("value", 0)
    roots = metrics.get("sbmd.js.gc_roots",          {}).get("dataPoints", [{}])[0].get("value", "n/a")
    delta_dp = find_datapoint(
        metrics, "sbmd.handler.heap_delta_bytes",
        driver="light", op_type="write", resource_id="isOn",
    )
    avg_delta = (
        delta_dp["sum"] / delta_dp["count"]
        if delta_dp and delta_dp["count"]
        else None
    )

    print(f"\n── Key signals after {_N_HEAP_CALLS} calls ──────────────────────────")
    print(f"  peak / arena:            {peak / arena:.1%}  ({peak:,} / {arena:,} bytes)")
    print(f"  remaining free:          {free:,} bytes")
    print(f"  gc_roots (final):        {roots}")
    if avg_delta is not None:
        print(f"  avg heap_delta per call: {avg_delta:+.1f} B")
        print(
            f"  → If avg_delta > 0 and rising: heap is accumulating allocations per call\n"
            f"  → If gc_roots rises monotonically: GC root leak in driver or runtime"
        )

    emit_timeseries(
        "heap_progression",
        ["ops", "peak_bytes", "free_bytes", "used_avg_bytes", "gc_roots", "arena_bytes"],
        rows,
        payload={
            "context": {
                "driver": "light", "op_type": "write",
                "resource_id": "isOn", "calls": _N_HEAP_CALLS,
            },
            "metrics": collect(metrics, [
                "sbmd.js.heap.peak_bytes",
                "sbmd.js.heap.free_bytes",
                "sbmd.js.gc_roots",
                "sbmd.handler.heap_delta_bytes",
            ]),
        },
    )
    print(f"{'=' * 60}")


def test_report_mutex_characterization(default_environment, matter_light):
    """
    Print mutex.wait_ms distributions for both uncontended (sequential)
    and contended (concurrent threads) scenarios.

    The two distributions together show:
      - Uncontended: baseline overhead of acquiring a free mutex
      - Contended:   real serialization cost under concurrent load

    On dev hardware these numbers are small.  On a gateway SoC the contended
    tail can be 10-100× higher, revealing the JS serialization bottleneck.
    """
    device = commission_device(default_environment, matter_light, "light")
    client = default_environment.get_client()

    # ── Sequential baseline ───────────────────────────────────────────────────
    print(f"\n{'=' * 60}")
    print("SBMD MUTEX CHARACTERIZATION")
    print(f"{'=' * 60}")
    print("\n── Sequential (uncontended, 30 calls) ───────────────────")
    for i in range(30):
        client.write_resource(resource_uri(device, "isOn", endpoint_id=1),
                              "true" if i % 2 == 0 else "false")
    time.sleep(0.5)
    seq_metrics = get_metrics(client)
    for dp in seq_metrics.get("sbmd.js.mutex.wait_ms", {}).get("dataPoints", []):
        print(format_histogram(dp, unit="ms"))

    # ── Concurrent load ───────────────────────────────────────────────────────
    print("\n── Concurrent (5 threads × 10 calls each) ───────────────")

    errors = []

    def worker():
        for i in range(10):
            try:
                client.write_resource(
                    resource_uri(device, "isOn", endpoint_id=1),
                    "true" if i % 2 == 0 else "false",
                )
            except Exception as exc:
                errors.append(exc)

    threads = [threading.Thread(target=worker) for _ in range(5)]
    for t in threads:
        t.start()
    for t in threads:
        t.join(timeout=30)

    if errors:
        print(f"  WARNING: {len(errors)} concurrent calls raised exceptions: {errors}")

    time.sleep(0.5)
    conc_metrics = get_metrics(client)
    for dp in conc_metrics.get("sbmd.js.mutex.wait_ms", {}).get("dataPoints", []):
        print(format_histogram(dp, unit="ms"))
        if dp["count"]:
            print(
                f"  avg={dp['sum'] / dp['count']:.3f}ms  "
                f"max={dp.get('max', '?')}ms"
            )

    emit_report("mutex_characterization", {
        "sequential": collect(seq_metrics, ["sbmd.js.mutex.wait_ms"]),
        "concurrent": collect(conc_metrics, ["sbmd.js.mutex.wait_ms"]),
    })
    print(f"{'=' * 60}")


def test_report_gc_pressure(default_environment, matter_deferred_cmd_test_device):
    """
    Drive allocation pressure via gcPressure executes and dump the GC metric
    family (gc.count, gc.duration_ms) plus the resulting heap state.
    """
    device = commission_device(
        default_environment, matter_deferred_cmd_test_device, "deferredCmdTest"
    )
    client = default_environment.get_client()

    gc_present = "sbmd.js.gc.count" in get_metrics(client)

    for _ in range(_N_GC_PRESSURE):
        client.execute_resource(
            resource_uri(device, "gcPressure", endpoint_id=1), "", ""
        )

    time.sleep(2)
    metrics = get_metrics(client)

    emit_report("gc_pressure", {
        "context": {
            "driver": "deferred-command-test",
            "gcPressure_calls": _N_GC_PRESSURE,
            "gc_instrumentation": gc_present,
        },
        "metrics": collect(metrics, [
            "sbmd.js.gc.count",
            "sbmd.js.gc.duration_ms",
            "sbmd.js.heap.peak_bytes",
            "sbmd.js.heap.free_bytes",
            "sbmd.js.gc_roots",
        ]),
    })


def test_report_deferred_profile(default_environment, matter_deferred_cmd_test_device):
    """
    Drive the full deferred lifecycle (normal, re-arm, timeout, runaway) and dump
    the deferred.* metric family.
    """
    device = commission_device(
        default_environment, matter_deferred_cmd_test_device, "deferredCmdTest"
    )
    client = default_environment.get_client()

    for resource in ("toggle", "rearmToggle", "timeoutToggle", "runawayToggle"):
        client.execute_resource(resource_uri(device, resource, endpoint_id=1), "", "")

    # runawayToggle unwinds up to the depth limit asynchronously; let it settle.
    time.sleep(5)
    metrics = get_metrics(client)

    emit_report("deferred_profile", {
        "context": {
            "driver": "deferred-command-test",
            "resources": ["toggle", "rearmToggle", "timeoutToggle", "runawayToggle"],
        },
        "metrics": collect(metrics, [
            "sbmd.deferred.duration_ms",
            "sbmd.deferred.depth",
            "sbmd.deferred.timeout",
            "sbmd.deferred.max_depth",
            "sbmd.deferred.in_flight",
        ]),
    })
