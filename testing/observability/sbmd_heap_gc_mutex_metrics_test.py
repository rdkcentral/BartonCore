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
SBMD observability metrics — heap, GC, and mutex tests.

Tests in this file focus on the long-running health signals:

  Heap:
    - peak_bytes stays well below the OOM ceiling
    - avg heap_delta per call is bounded (no per-call leak)
    - gc_roots count is stable across repeated identical calls (no GC root leak)

  GC:
    - GC fires at least once under explicit allocation pressure
    - GC durations land in low-latency buckets (GC is incremental)

  Mutex:
    - Uncontended path has near-zero wait time
    - Contended path records measurable (non-zero) cumulative wait

GC metric tests are skipped when sbmd.js.gc_roots is absent from the telemetry
JSON (i.e. BCORE_SBMD_GC_INSTRUMENTATION=OFF build).

All heap/GC tests use a light device (simple, stable handler) and the
deferred-command-test device (gcPressure resource) for GC pressure.
"""

import threading
import time

import pytest

from testing.helpers.sbmd_metrics_helpers import (
    bucket_sum,
    find_datapoint,
    format_histogram,
    get_metrics,
)
from testing.utils.barton_utils import (
    commission_device,
    resource_update_listener,
    resource_uri,
    wait_for_resource_value,
)

pytestmark = [pytest.mark.requires_matterjs]

_LIGHT_DRIVER  = "light"
_DEFERRED_DRIVER = "deferred-command-test"

# OOM safety threshold: peak heap must not exceed this fraction of the arena.
_PEAK_OOM_THRESHOLD = 0.75

# Average heap delta bound: per-call net allocation must be below this limit.
# Exceeding this over many calls would indicate a per-call heap leak.
_MAX_AVG_DELTA_BYTES = 1024

# Number of isOn writes for the sustained-load tests.
_N_SUSTAINED = 200

# Number of gcPressure executes (each allocates 500k array elements).
_N_GC_PRESSURE = 20

# Warmup writes before sampling the gc_roots baseline.  gc_roots is only recorded
# during handler invocations, so its pre-handler value is an unmeasured 0;
# sampling after a warmup captures the true steady-state root count.
_WARMUP_CALLS = 10


def _write_is_on_n_times(client, device, n):
    """Execute n alternating isOn writes; wait for each to confirm completion."""
    for i in range(n):
        value = "true" if i % 2 == 0 else "false"
        queue = resource_update_listener(client, "isOn")
        client.write_resource(resource_uri(device, "isOn", endpoint_id=1), value)
        wait_for_resource_value(queue, value, timeout=10)


# ── GC root stability ─────────────────────────────────────────────────────────

def test_gc_roots_stable_across_identical_calls(default_environment, matter_light):
    """
    gc_roots count does not grow by more than 10 across 200 identical writes
    on the same resource.

    A continuously increasing gc_roots count under repeated identical calls
    means the driver or runtime is adding GC-tracked objects per call without
    freeing them — a root leak that will eventually exhaust the heap.

    NOTE: Skipped when BCORE_SBMD_GC_INSTRUMENTATION=OFF (gc_roots absent).
    """
    device = commission_device(default_environment, matter_light, _LIGHT_DRIVER)
    client = default_environment.get_client()

    if "sbmd.js.gc_roots" not in get_metrics(client):
        pytest.skip("sbmd.js.gc_roots not present — BCORE_SBMD_GC_INSTRUMENTATION=OFF")

    # gc_roots is only recorded during handler invocations, so its value before
    # any handler runs is an unmeasured 0.  Warm up first so the baseline is the
    # steady-state root count; otherwise the 0→steady-state jump reads as a leak.
    _write_is_on_n_times(client, device, _WARMUP_CALLS)
    time.sleep(1)
    roots_before = get_metrics(client)["sbmd.js.gc_roots"]["dataPoints"][0]["value"]

    _write_is_on_n_times(client, device, _N_SUSTAINED)
    time.sleep(2)

    roots_after = get_metrics(client)["sbmd.js.gc_roots"]["dataPoints"][0]["value"]
    growth = roots_after - roots_before

    print(f"\n  gc_roots before: {roots_before}")
    print(f"  gc_roots after {_N_SUSTAINED} writes: {roots_after}")
    print(f"  growth: {growth}")
    print(
        "  NOTE: Monotonic gc_root growth across identical calls indicates a "
        "GC root leak in the driver or the SBMD runtime."
    )

    # After warmup the root count should be flat across identical calls: a real
    # leak shows up as unbounded, monotonic growth.  The bound is a small noise
    # tolerance (ideal growth is 0) — anything at/above it is treated as a leak.
    assert growth < 10, (
        f"gc_roots grew by {growth} across {_N_SUSTAINED} identical writes. "
        f"Expected stable (< 10 growth). Before={roots_before}, after={roots_after}."
    )


# ── Peak heap ceiling ─────────────────────────────────────────────────────────

def test_heap_peak_stays_below_oom_threshold(default_environment, matter_light):
    """
    After 200 isOn writes, sbmd.js.heap.peak_bytes is below 75 % of the arena.

    peak_bytes is an all-time high-water mark that never decreases.  Staying
    below 75 % means there is enough headroom for GC to operate and for burst
    allocations during concurrent handlers.

    NOTE: peak_bytes is a monotonic high-water mark: it can only increase, never
    decrease, even after GC.
    """
    device = commission_device(default_environment, matter_light, _LIGHT_DRIVER)
    client = default_environment.get_client()
    _write_is_on_n_times(client, device, _N_SUSTAINED)
    time.sleep(1)

    metrics = get_metrics(client)
    arena = metrics["sbmd.js.heap.arena_bytes"]["dataPoints"][0]["value"]
    peak  = metrics["sbmd.js.heap.peak_bytes"]["dataPoints"][0]["value"]

    print(f"\n  arena:       {arena:,} bytes")
    print(f"  peak:        {peak:,} bytes  ({peak / arena:.1%})")
    print(f"  threshold:   {arena * _PEAK_OOM_THRESHOLD:,.0f} bytes  ({_PEAK_OOM_THRESHOLD:.0%})")
    print(f"  remaining:   {arena - peak:,} bytes")

    assert peak < arena * _PEAK_OOM_THRESHOLD, (
        f"Peak heap ({peak:,} bytes, {peak / arena:.1%}) is above the "
        f"{_PEAK_OOM_THRESHOLD:.0%} threshold. "
        f"Arena={arena:,} bytes.  Risk of OOM under concurrent load."
    )


# ── Per-call heap delta bound ─────────────────────────────────────────────────

def test_heap_delta_per_call_is_bounded(default_environment, matter_light):
    """
    Average heap_delta per call (sum / count) is < _MAX_AVG_DELTA_BYTES.

    A per-call average of 0 B or below means the JS engine's GC runs regularly
    and the driver is not leaking heap per invocation.  A growing positive
    average over many calls would predict eventual heap exhaustion.
    """
    device = commission_device(default_environment, matter_light, _LIGHT_DRIVER)
    client = default_environment.get_client()
    _write_is_on_n_times(client, device, _N_SUSTAINED)

    metrics = get_metrics(client)
    delta_dp = find_datapoint(
        metrics, "sbmd.handler.heap_delta_bytes",
        driver=_LIGHT_DRIVER, op_type="write", resource_id="isOn",
    )
    assert delta_dp is not None, (
        f"No handler.heap_delta_bytes for {_LIGHT_DRIVER}/write/isOn"
    )
    assert delta_dp["count"] >= 1

    avg_delta = delta_dp["sum"] / delta_dp["count"]
    min_obs   = delta_dp.get("min", 0)
    # bucket[0] covers values ≤ 0 (GC reclaimed more than was allocated).
    # Each bucket is {"le": ..., "count": ...} per observabilityDumpJson().
    neg_obs   = delta_dp.get("buckets", [{}])[0].get("count", 0)

    print(f"\n  handler.heap_delta_bytes histogram:")
    print(format_histogram(delta_dp, unit=" B"))
    print(f"\n  avg delta per call: {avg_delta:+.1f} B")
    print(f"  min observed:       {min_obs} B")
    print(f"  bucket[0] (≤0 B):   {neg_obs} observations")
    if min_obs >= 0:
        print(
            "  NOTE: min ≥ 0 means GC never reclaimed more than was allocated "
            "during any single call.  Not an error; just confirms GC is lazy."
        )

    assert avg_delta < _MAX_AVG_DELTA_BYTES, (
        f"Average heap delta per call is {avg_delta:+.1f} B, "
        f"exceeds {_MAX_AVG_DELTA_BYTES} B threshold. "
        f"This may indicate a per-call allocation leak in the 'isOn' handler."
    )


# ── GC fires under pressure ───────────────────────────────────────────────────

def test_gc_fires_under_allocation_pressure(
    default_environment, matter_deferred_cmd_test_device
):
    """
    _N_GC_PRESSURE calls to gcPressure (each allocates 500 × 1000-element
    arrays) trigger at least one GC cycle.

    Expected metrics:
      sbmd.js.gc.count:       increased after the pressure calls
      sbmd.js.gc.duration_ms: has >= 1 observation
      All gc.duration_ms observations land in the ≤ 25 ms buckets (GC is fast)

    NOTE: Skipped when BCORE_SBMD_GC_INSTRUMENTATION=OFF.
    """
    device = commission_device(
        default_environment, matter_deferred_cmd_test_device, "deferredCmdTest"
    )
    client = default_environment.get_client()

    metrics_before = get_metrics(client)
    if "sbmd.js.gc_roots" not in metrics_before:
        pytest.skip("sbmd.js.gc_roots not present — BCORE_SBMD_GC_INSTRUMENTATION=OFF")

    gc_count_before = (
        metrics_before.get("sbmd.js.gc.count", {})
        .get("dataPoints", [{}])[0]
        .get("value", 0)
    )

    for _ in range(_N_GC_PRESSURE):
        client.execute_resource(
            resource_uri(device, "gcPressure", endpoint_id=1), "", ""
        )
    time.sleep(2)

    metrics_after = get_metrics(client)
    gc_count_after = (
        metrics_after.get("sbmd.js.gc.count", {})
        .get("dataPoints", [{}])[0]
        .get("value", 0)
    )
    gc_dur_dps = metrics_after.get("sbmd.js.gc.duration_ms", {}).get("dataPoints", [])
    gc_obs = sum(dp["count"] for dp in gc_dur_dps)

    print(f"\n  gc.count before: {gc_count_before}")
    print(f"  gc.count after:  {gc_count_after}")
    print(f"  gc.duration_ms observations: {gc_obs}")
    if gc_dur_dps:
        for dp in gc_dur_dps:
            print(format_histogram(dp, unit="ms"))

    assert gc_count_after > gc_count_before, (
        f"GC count did not increase after {_N_GC_PRESSURE} high-pressure calls. "
        f"Before={gc_count_before}, after={gc_count_after}. "
        f"Either GC instrumentation is not firing, or the pressure was insufficient."
    )
    assert gc_obs >= 1, "sbmd.js.gc.duration_ms has no observations despite GC firing"

    # All GC cycles should complete quickly (incremental GC).
    for dp in gc_dur_dps:
        fast_count = bucket_sum(dp, 0, 4)  # ≤ 25 ms
        print(f"\n  GC observations ≤ 25 ms: {fast_count}/{dp['count']}")
        assert fast_count == dp["count"], (
            f"Some GC cycles exceeded 25 ms.  "
            f"Bucket distribution: {dp.get('buckets')}, max={dp.get('max')} ms"
        )


# ── Mutex — uncontended ───────────────────────────────────────────────────────

def test_mutex_wait_near_zero_when_uncontended(default_environment, matter_light):
    """
    30 sequential writes on a single thread produce an average mutex.wait_ms
    < 2.0 ms.

    Sequential calls never queue behind another holder, so the mutex should
    be grabbed immediately.  Elevated wait times here indicate OS scheduler
    or system load issues, not a SBMD bug.
    """
    device = commission_device(default_environment, matter_light, _LIGHT_DRIVER)
    client = default_environment.get_client()

    for i in range(30):
        client.write_resource(
            resource_uri(device, "isOn", endpoint_id=1),
            "true" if i % 2 == 0 else "false",
        )
    time.sleep(0.5)

    metrics = get_metrics(client)
    wait_dps = metrics.get("sbmd.js.mutex.wait_ms", {}).get("dataPoints", [])
    assert wait_dps, "sbmd.js.mutex.wait_ms has no datapoints — metric not recorded"

    print("\n── mutex.wait_ms (sequential, uncontended) ──────────────")
    for dp in wait_dps:
        print(format_histogram(dp, unit="ms"))
        if dp["count"]:
            avg = dp["sum"] / dp["count"]
            print(f"  avg={avg:.4f} ms  max={dp.get('max', '?')} ms")
            assert avg < 2.0, (
                f"Uncontended mutex avg wait {avg:.4f} ms exceeds 2.0 ms. "
                f"Expected near-zero.  max={dp.get('max')} ms."
            )


# ── Mutex — contended ─────────────────────────────────────────────────────────

def test_mutex_wait_nonzero_under_concurrent_load(default_environment, matter_light):
    """
    5 threads × 10 writes each (50 concurrent calls) produce a measurable
    cumulative mutex.wait_ms > 0.

    Under concurrency, threads queue behind the current JS-runtime holder.
    The sum of wait times must be positive — confirming the mutex contention
    path records observations.  The distribution shape is printed for visual
    inspection.
    """
    device = commission_device(default_environment, matter_light, _LIGHT_DRIVER)
    client = default_environment.get_client()

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
        t.join(timeout=60)

    if errors:
        print(f"\n  WARNING: {len(errors)} concurrent calls raised exceptions: {errors}")

    time.sleep(0.5)
    metrics = get_metrics(client)
    wait_dps = metrics.get("sbmd.js.mutex.wait_ms", {}).get("dataPoints", [])
    assert wait_dps, "sbmd.js.mutex.wait_ms has no datapoints — metric not recorded"

    print("\n── mutex.wait_ms (5 threads × 10 calls, contended) ──────")
    total_sum   = 0.0
    total_count = 0
    for dp in wait_dps:
        print(format_histogram(dp, unit="ms"))
        total_sum   += dp["sum"]
        total_count += dp["count"]
        if dp["count"]:
            print(
                f"  avg={dp['sum'] / dp['count']:.4f} ms  "
                f"max={dp.get('max', '?')} ms  "
                f"count={dp['count']}"
            )

    print(f"\n  cumulative wait across all datapoints: {total_sum:.4f} ms")
    assert total_sum > 0, (
        "mutex.wait_ms cumulative sum is 0 under 5-thread concurrent load. "
        "Expected non-zero: at least some threads should have waited."
    )
