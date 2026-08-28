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

# OOM safety threshold: the retained live set (post-GC occupancy) must not
# exceed this fraction of the arena.  This is graded against live_bytes, NOT
# peak_bytes: under mquickjs's lazy compacting GC, peak_bytes is an all-time
# high-water mark of transient (already-reclaimed) eval garbage and routinely
# pins at ~99.9 % even with a tiny live set, so it is a churn indicator, not a
# capacity gauge.  live_bytes (heap_used sampled right after a GC compaction) is
# the true occupancy signal an OOM gate must watch.
_LIVE_OOM_THRESHOLD = 0.75

# Average heap delta bound: per-call net allocation must be below this limit.
# Exceeding this over many calls would indicate a per-call heap leak.
_MAX_AVG_DELTA_BYTES = 1024

# Number of isOn writes for the sustained-load tests.
_N_SUSTAINED = 200

# Number of gcPressure executes (each allocates 500k array elements).
_N_GC_PRESSURE = 20

# Soak parameters: sustained writes with periodic post-GC live-set sampling to
# prove the retained floor does not creep upward (a slow leak).  Kept modest so
# the test stays under a few minutes; a real multi-hour soak scales _N_SOAK up.
_N_SOAK = 900
_SOAK_SAMPLE_INTERVAL = 100

# Allowed upward drift of the post-GC live-set floor from the first sampling
# window to the last.  A genuine leak shows unbounded, monotonic floor growth;
# this tolerance absorbs the small one-time rise as the steady-state working set
# settles plus GC-timing jitter in the opportunistic samples.
_SOAK_FLOOR_DRIFT_TOLERANCE_BYTES = 65536

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


# ── Retained live-set ceiling (true OOM gate) ─────────────────────────────────


def test_live_set_stays_below_oom_threshold(default_environment, matter_light):
    """
    After 200 isOn writes, the retained live set (sbmd.js.heap.live_bytes) is
    below 75 % of the arena.

    live_bytes is heap_used sampled immediately after a GC compaction, so it
    reflects only memory that actually survived collection — the true occupancy.
    This is the sound OOM gate: unlike peak_bytes (a monotonic high-water mark of
    already-reclaimed transient garbage that pins near 100 % even at idle), a high
    live_bytes genuinely means the arena is filling with retained state.

    peak_bytes/free_bytes are reported for context only, not graded.

    NOTE: Skipped when live_bytes is absent — either
    BCORE_SBMD_GC_INSTRUMENTATION=OFF (no GC callback compiled in) or no GC
    happened to fire during the run (live_bytes is sampled opportunistically on
    engine-driven collections, never forced).
    """

    device = commission_device(default_environment, matter_light, _LIGHT_DRIVER)
    client = default_environment.get_client()
    _write_is_on_n_times(client, device, _N_SUSTAINED)
    time.sleep(1)

    metrics = get_metrics(client)

    if "sbmd.js.heap.live_bytes" not in metrics:
        pytest.skip(
            "sbmd.js.heap.live_bytes absent — BCORE_SBMD_GC_INSTRUMENTATION=OFF "
            "or no GC fired during the run"
        )

    arena = metrics["sbmd.js.heap.arena_bytes"]["dataPoints"][0]["value"]
    live = metrics["sbmd.js.heap.live_bytes"]["dataPoints"][0]["value"]
    peak  = metrics["sbmd.js.heap.peak_bytes"]["dataPoints"][0]["value"]

    print(f"\n  arena:       {arena:,} bytes")
    print(f"  live set:    {live:,} bytes  ({live / arena:.1%})  <- graded")
    print(
        f"  peak:        {peak:,} bytes  ({peak / arena:.1%})  (reported, not graded)"
    )
    print(
        f"  threshold:   {arena * _LIVE_OOM_THRESHOLD:,.0f} bytes  ({_LIVE_OOM_THRESHOLD:.0%})"
    )

    assert live < arena * _LIVE_OOM_THRESHOLD, (
        f"Retained live set ({live:,} bytes, {live / arena:.1%}) is above the "
        f"{_LIVE_OOM_THRESHOLD:.0%} threshold. Arena={arena:,} bytes. "
        f"This is real occupancy (post-GC), not transient high-water — genuine OOM risk."
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


# ── Post-GC live-set gauge ────────────────────────────────────────────────────


def test_live_bytes_recorded_after_gc(
    default_environment, matter_deferred_cmd_test_device
):
    """
    After GC pressure forces collections, sbmd.js.heap.live_bytes is recorded and
    reflects the compacted live set: positive, below the arena, and never above
    peak_bytes.

    live_bytes is sampled at GC end (post-compaction), so it can only ever be a
    subset of the arena and can never exceed the transient high-water (peak_bytes).
    Confirming live_bytes <= peak proves the gauge captures the reclaimed floor
    rather than the churn high-water — the whole point of the metric.

    NOTE: Skipped when BCORE_SBMD_GC_INSTRUMENTATION=OFF (no GC callback compiled
    in, so the gauge is never written).
    """
    device = commission_device(
        default_environment, matter_deferred_cmd_test_device, "deferredCmdTest"
    )
    client = default_environment.get_client()

    if "sbmd.js.gc_roots" not in get_metrics(client):
        pytest.skip("sbmd.js.gc_roots not present — BCORE_SBMD_GC_INSTRUMENTATION=OFF")

    for _ in range(_N_GC_PRESSURE):
        client.execute_resource(
            resource_uri(device, "gcPressure", endpoint_id=1), "", ""
        )
    time.sleep(2)

    metrics = get_metrics(client)

    assert "sbmd.js.heap.live_bytes" in metrics, (
        "sbmd.js.heap.live_bytes was not recorded even though GC instrumentation "
        "is on and GC pressure was applied — the GC-end sampling path is not firing."
    )

    arena = metrics["sbmd.js.heap.arena_bytes"]["dataPoints"][0]["value"]
    live = metrics["sbmd.js.heap.live_bytes"]["dataPoints"][0]["value"]
    peak = metrics["sbmd.js.heap.peak_bytes"]["dataPoints"][0]["value"]

    print(f"\n  arena:     {arena:,} bytes")
    print(f"  live set:  {live:,} bytes  ({live / arena:.1%})")
    print(f"  peak:      {peak:,} bytes  ({peak / arena:.1%})")
    print(
        "  NOTE: live_bytes is the post-GC (compacted) live set; it must be a "
        "subset of both the arena and the transient high-water peak_bytes."
    )

    assert (
        0 < live <= arena
    ), f"live_bytes ({live:,}) is out of range for a {arena:,}-byte arena."
    assert live <= peak, (
        f"live_bytes ({live:,}) exceeds peak_bytes ({peak:,}); the post-GC live "
        f"set cannot be larger than the all-time transient high-water mark."
    )


# ── Retained-allocation guardrail ─────────────────────────────────────────────


def test_heavy_transient_allocation_is_not_retained(
    default_environment, matter_deferred_cmd_test_device
):
    """
    After _N_GC_PRESSURE gcPressure calls (each allocating 500k array elements
    that go out of scope when the handler returns), the post-GC live set
    (live_bytes) returns to its pre-pressure level.

    This is the guardrail that live_bytes (#1) uniquely enables: a driver that
    *retains* a large allocation (vs. the transient churn gcPressure produces)
    would leave live_bytes elevated after the pressure stops.  peak_bytes/
    used_bytes cannot distinguish retained from transient; live_bytes can.  All
    drivers share one 1 MiB context, so a retained blow-up would starve the rest.

    NOTE: Skipped when live_bytes is absent (BCORE_SBMD_GC_INSTRUMENTATION=OFF or
    no GC fired).
    """
    device = commission_device(
        default_environment, matter_deferred_cmd_test_device, "deferredCmdTest"
    )
    client = default_environment.get_client()

    def _pressure_and_read():
        # A gcPressure call allocates then releases, and (under any build) drives
        # a GC — so live_bytes is refreshed to the post-GC live set afterwards.
        client.execute_resource(
            resource_uri(device, "gcPressure", endpoint_id=1), "", ""
        )
        time.sleep(0.3)
        m = get_metrics(client)
        dps = m.get("sbmd.js.heap.live_bytes", {}).get("dataPoints", [])
        return dps[0]["value"] if dps else None

    baseline = _pressure_and_read()
    if baseline is None:
        pytest.skip(
            "sbmd.js.heap.live_bytes absent — BCORE_SBMD_GC_INSTRUMENTATION=OFF "
            "or no GC fired"
        )

    for _ in range(_N_GC_PRESSURE):
        client.execute_resource(
            resource_uri(device, "gcPressure", endpoint_id=1), "", ""
        )
    time.sleep(1)

    after = _pressure_and_read()
    assert after is not None, "live_bytes disappeared after GC pressure"

    arena = get_metrics(client)["sbmd.js.heap.arena_bytes"]["dataPoints"][0]["value"]
    growth = after - baseline

    print(f"\n  live set before pressure: {baseline:,} B ({baseline / arena:.1%})")
    print(f"  live set after pressure:  {after:,} B ({after / arena:.1%})")
    print(f"  retained growth:          {growth:+,} B")
    print(
        "  NOTE: gcPressure allocates multi-MB of transient arrays; a flat live "
        "set proves they were reclaimed, not retained."
    )

    # The transient arrays must be fully reclaimed: the retained floor should not
    # climb.  Tolerance absorbs GC-timing jitter in the opportunistic samples.
    assert growth <= _SOAK_FLOOR_DRIFT_TOLERANCE_BYTES, (
        f"Live set grew by {growth:,} B after transient GC pressure "
        f"(before {baseline:,} -> after {after:,}). The gcPressure allocations "
        f"appear to be RETAINED, not transient — a driver holding a large live "
        f"object would starve the shared 1 MiB context."
    )


# ── Soak: retained live-set floor stays flat ──────────────────────────────────


@pytest.mark.slow
def test_live_set_floor_flat_under_sustained_load(default_environment, matter_light):
    """
    Over _N_SOAK sustained isOn writes, the post-GC live-set floor
    (min sbmd.js.heap.live_bytes) does not creep upward beyond a small tolerance.

    This is the leak proof that #1 (live_bytes) unlocks.  used_bytes/free_bytes
    swing with the lazy-GC sawtooth, so a slow retained leak is indistinguishable
    from healthy churn in them.  live_bytes is the compacted live set, so its
    FLOOR is the retained working set — if that floor climbs steadily across the
    soak, memory is genuinely accumulating; if it stays flat, the sawtooth is
    just transient garbage.  We compare the floor of the first sampling window to
    the floor of the last and require the drift to stay within tolerance.

    NOTE: A production-representative soak runs for hours; this scaled-down
    version proves the mechanism and catches gross leaks in CI-adjacent time.
    Skipped when live_bytes is absent (BCORE_SBMD_GC_INSTRUMENTATION=OFF or no GC
    fired).
    """
    device = commission_device(default_environment, matter_light, _LIGHT_DRIVER)
    client = default_environment.get_client()

    def _live_now():
        m = get_metrics(client)
        dps = m.get("sbmd.js.heap.live_bytes", {}).get("dataPoints", [])
        return dps[0]["value"] if dps else None

    if _live_now() is None:
        # Nudge a few writes in case no GC has fired at idle yet.
        _write_is_on_n_times(client, device, 10)
        time.sleep(0.5)
        if _live_now() is None:
            pytest.skip(
                "sbmd.js.heap.live_bytes absent — BCORE_SBMD_GC_INSTRUMENTATION=OFF "
                "or no GC fired"
            )

    samples = []  # (ops, live_bytes)
    for i in range(_N_SOAK):
        value = "true" if i % 2 == 0 else "false"
        client.write_resource(resource_uri(device, "isOn", endpoint_id=1), value)

        if (i + 1) % _SOAK_SAMPLE_INTERVAL == 0:
            time.sleep(0.3)
            live = _live_now()
            if live is not None:
                samples.append((i + 1, live))

    time.sleep(1)
    final = _live_now()
    if final is not None:
        samples.append((_N_SOAK, final))

    assert (
        len(samples) >= 4
    ), f"Too few post-GC live-set samples ({len(samples)}) to assess drift."

    arena = get_metrics(client)["sbmd.js.heap.arena_bytes"]["dataPoints"][0]["value"]

    # Compare the floor (min) of the first third of samples to the last third.
    third = max(1, len(samples) // 3)
    early_floor = min(v for _, v in samples[:third])
    late_floor = min(v for _, v in samples[-third:])
    drift = late_floor - early_floor

    print(f"\n  live-set samples (ops -> bytes):")
    for ops, v in samples:
        print(f"    {ops:>5} ops: {v:>10,} B  ({v / arena:.1%})")
    print(f"  early-window floor: {early_floor:,} B")
    print(f"  late-window floor:  {late_floor:,} B")
    print(
        f"  drift:              {drift:+,} B  (tolerance {_SOAK_FLOOR_DRIFT_TOLERANCE_BYTES:,} B)"
    )
    print(
        "  NOTE: a steadily climbing floor across the soak indicates a genuine "
        "retained leak; a flat floor means the sawtooth is transient garbage."
    )

    assert drift <= _SOAK_FLOOR_DRIFT_TOLERANCE_BYTES, (
        f"Post-GC live-set floor drifted up by {drift:,} B over {_N_SOAK} writes "
        f"(early {early_floor:,} -> late {late_floor:,}), exceeding the "
        f"{_SOAK_FLOOR_DRIFT_TOLERANCE_BYTES:,} B tolerance. This is a candidate "
        f"retained-memory leak — the live set is accumulating, not just churning."
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
