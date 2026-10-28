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
SBMD observability metrics — scale tests.

These answer the "how big does SBMD need to be?" questions that the
single-device characterization suite physically could not:

  test_retained_memory_vs_device_count
      Commission N devices one at a time and read the post-GC live set
      (sbmd.js.heap.live_bytes) after each, producing the retained-memory curve
      that sizes the 1 MiB arena ("X KB/device", "1 MiB supports ~N devices").

Retained memory is only legible through backlog #1's live_bytes gauge: under the
lazy compacting GC, used_bytes/free_bytes carry transient garbage and cannot
isolate the live set.  live_bytes is heap_used sampled right after a GC, so its
value tracks the retained working set.

DEBUG_GC note: under the dev build (DEBUG_GC on) live_bytes carries a constant
~128 KB dummy-block offset, so ABSOLUTE values are inflated — but the PER-DEVICE
SLOPE (the increment between device counts) is unaffected, since the offset
cancels in the delta.  The slope is the figure that sizes the arena.

Marked slow: spins up many matter.js Node subprocesses.  Skipped when live_bytes
is absent (BCORE_SBMD_GC_INSTRUMENTATION=OFF or no GC fired).
"""

import os
import threading
import time
from pathlib import Path

import pytest

from testing.environment.default_environment_orchestrator import (
    DefaultEnvironmentOrchestrator,
)
from testing.helpers.sbmd_metrics_helpers import get_metrics
from testing.helpers.sbmd_report_writer import collect, emit_report, emit_timeseries
from testing.mocks.devices.matter.matter_deferred_cmd_test_device import (
    MatterDeferredCmdTestDevice,
)
from testing.mocks.devices.matter.matter_door_lock import MatterDoorLock
from testing.mocks.devices.matter.matter_humidity_sensor import MatterHumiditySensor
from testing.mocks.devices.matter.matter_light import MatterLight
from testing.mocks.devices.matter.matter_temperature_sensor import MatterTemperatureSensor
from testing.mocks.devices.matter.matter_thermostat import MatterThermostat
from testing.utils.barton_utils import (
    commission_device,
    resource_uri,
)

pytestmark = [pytest.mark.requires_matterjs, pytest.mark.slow]

# Device count for the sweep.  Overridable so a heavier run can be requested
# without editing the test (e.g. SBMD_SCALE_DEVICES=50).
_N_DEVICES = int(os.environ.get("SBMD_SCALE_DEVICES", "20"))

# Writes each device's worker thread fires during the concurrency stress test.
_CONCURRENT_WRITES_PER_DEVICE = int(os.environ.get("SBMD_CONCURRENT_WRITES", "25"))

# Number of generated stub drivers for the driver-scaling test.
_N_STUB_DRIVERS = int(os.environ.get("SBMD_SCALE_DRIVERS", "100"))

# Matter device-type base for generated stubs.  SbmdLoader parses matter.deviceTypes
# via GetUint16Array(), which narrows every value to uint16_t — a base like
# 0xFFF20000 (used in an earlier version of this test) truncates to just 0x0000
# plus the index, colliding with real production device types in 0x00-0x63 (e.g.
# door-lock=0x000a, contact-sensor=0x0015, air-quality=0x002c, water-leak=0x0043).
# 0xF000 keeps all 100 stubs inside a single 16-bit value, unique among
# themselves and clear of every real device type used in this repo (all <0x0310).
_STUB_DEVICE_TYPE_BASE = 0xF000

# Sanity ceiling on the average retained bytes per commissioned device.  A light
# retains a device object, subscriptions, and cached attributes — a few KB.  A
# value far above this would mean per-device retention is heavier than expected
# and the arena would size poorly; it is a generous guard, not a tight budget.
_MAX_AVG_BYTES_PER_DEVICE = 32 * 1024

# Tight flatness tolerance for the "retained memory does not scale with device
# count" claim.  _MAX_AVG_BYTES_PER_DEVICE above is deliberately generous (a
# catastrophic-growth ceiling); a real, modest linear footprint of up to 32 KiB
# per device would still pass it despite contradicting the no-scaling claim.
# This bound is well above the few-hundred-byte sample jitter seen empirically
# between devices, but far tighter than the generous guard.
_FLAT_SLOPE_TOLERANCE_BYTES = 4 * 1024

# The retained live set must stay well under the arena even at the top of the
# sweep (real occupancy, post-GC).
_LIVE_OOM_THRESHOLD = 0.75


def _live_bytes(client):
    dps = get_metrics(client).get("sbmd.js.heap.live_bytes", {}).get("dataPoints", [])
    return dps[0]["value"] if dps else None


# Real isOn writes driven on a device before sampling.  This is legitimate device
# exercise (not a forced GC or a metric-only nudge): under DEBUG_GC off the
# accumulated allocation lets the arena fill enough for the engine's own lazy GC
# to fire, which passively refreshes live_bytes.  We then read whatever that
# natural GC left — pure observation, no side effect on any other metric.
_SWEEP_LOAD_WRITES = 20


def _drive_load_and_read(client, device):
    """Drive real load on *device*, then read the current post-GC live_bytes."""
    write_errors = []
    for i in range(_SWEEP_LOAD_WRITES):
        try:
            client.write_resource(
                resource_uri(device, "isOn", endpoint_id=1),
                "true" if i % 2 == 0 else "false",
            )
        except Exception as exc:  # noqa: BLE001 - collected, asserted below
            write_errors.append(exc)
    assert not write_errors, (
        f"{len(write_errors)}/{_SWEEP_LOAD_WRITES} isOn writes failed for this device: "
        f"{write_errors[:3]} — a device that silently fails every write would still be "
        f"counted as 'exercised' in the retained-memory curve, invalidating it."
    )
    time.sleep(0.2)
    return _live_bytes(client)


def _gauge(metrics, name):
    dps = metrics.get(name, {}).get("dataPoints", [])
    return dps[0]["value"] if dps else ""


def _gc_count(client):
    dps = get_metrics(client).get("sbmd.js.gc.count", {}).get("dataPoints", [])
    return dps[0].get("value", 0) if dps else None


# Bounded retries for the freshness check below.  Each round drives another
# _SWEEP_LOAD_WRITES batch, so this caps worst-case extra load per device.
_MAX_FRESHNESS_ROUNDS = 5


def _drive_until_fresh_sample(client, device):
    """Drive load on *device* until a NEW GC cycle has actually fired since
    entry (not just re-reading a stale live_bytes value left over from a
    previous device's GC), then return the fresh post-GC live set.

    Under DEBUG_GC off, live_bytes only updates when the engine's own lazy GC
    fires; one _SWEEP_LOAD_WRITES batch usually fills the arena enough to
    trigger it, but if it doesn't, driving more load (rather than silently
    returning whatever value happens to still be sitting in the gauge) is what
    makes the per-device curve trustworthy as a *fresh* sample series, not an
    artifact of under-driving.
    """
    gc_before = _gc_count(client)
    live = None
    for _ in range(_MAX_FRESHNESS_ROUNDS):
        live = _drive_load_and_read(client, device)
        if gc_before is None or _gc_count(client) != gc_before:
            return live
    pytest.fail(
        f"No GC cycle fired after {_MAX_FRESHNESS_ROUNDS} load rounds on this device "
        f"(gc.count stayed at {gc_before}) — live_bytes would be a stale sample from a "
        f"previous device, not a fresh post-commission reading, which would silently "
        f"flatten the retained-memory curve regardless of the true value."
    )


def test_retained_memory_vs_device_count(default_environment):
    """
    Commission _N_DEVICES lights one at a time; after each, record the post-GC
    live set.  Assert the retained set scales sanely (bounded per-device slope)
    and never approaches the arena ceiling, and emit the full curve as a report
    time-series so the "KB/device" / "devices per MiB" sizing can be read off.
    """
    client = default_environment.get_client()

    # Baseline: post-GC live set with zero commissioned devices.  Wait briefly for
    # a natural GC (fired during startup / driver load) to populate the gauge.
    for _ in range(20):
        if _live_bytes(client) is not None:
            break
        time.sleep(0.25)

    baseline = _live_bytes(client)
    if baseline is None:
        pytest.skip(
            "sbmd.js.heap.live_bytes absent — BCORE_SBMD_GC_INSTRUMENTATION=OFF "
            "or no GC fired"
        )

    devices = []
    rows = []
    live_samples = [baseline]  # every present post-GC reading; min = cleanest floor

    metrics0 = get_metrics(client)
    arena = metrics0["sbmd.js.heap.arena_bytes"]["dataPoints"][0]["value"]
    rows.append([
        0,
        baseline,
        _gauge(metrics0, "sbmd.js.heap.free_bytes"),
        _gauge(metrics0, "sbmd.js.heap.peak_bytes"),
        _gauge(metrics0, "sbmd.js.gc_roots"),
        arena,
    ])

    try:
        for i in range(_N_DEVICES):
            light = MatterLight()
            light.start()
            devices.append(light)
            # The orchestrator's device-added flag is sticky (set once, never
            # reset), so wait_for_device_added() would return instantly after
            # the first device without this reset — masking whether THIS
            # device actually finished commissioning before we drive load on it.
            default_environment._commissioned_device = False
            commissioned = commission_device(default_environment, light, "light")

            # Drive real load on this device until a fresh GC cycle confirms
            # live_bytes reflects THIS commission, not a stale prior reading.
            live = _drive_until_fresh_sample(client, commissioned)
            if live is not None:
                live_samples.append(live)
            metrics = get_metrics(client)
            rows.append([
                i + 1,
                live if live is not None else "",
                _gauge(metrics, "sbmd.js.heap.free_bytes"),
                _gauge(metrics, "sbmd.js.heap.peak_bytes"),
                _gauge(metrics, "sbmd.js.gc_roots"),
                arena,
            ])

        present = [r[1] for r in rows if r[1] != ""]
        final_live = present[-1] if present else baseline
        floor = min(live_samples)

        per_device = (final_live - baseline) / _N_DEVICES
        devices_per_arena = arena / per_device if per_device > 0 else None

        print(f"\n  ── Retained memory vs device count ({_N_DEVICES} lights) ──")
        print(f"  {'devices':>7}  {'live_bytes':>12}  {'% arena':>8}")
        for count, live, *_ in rows:
            if live != "":
                print(f"  {count:>7}  {live:>12,}  {live / arena:>7.1%}")
        print(f"\n  baseline (0 devices):     {baseline:,} B")
        print(f"  final ({_N_DEVICES} devices):        {final_live:,} B")
        print(f"  cleanest floor (min):     {floor:,} B ({floor / arena:.1%})")
        print(f"  avg retained per device:  {per_device:,.0f} B")
        if devices_per_arena is not None:
            print(f"  => 1 MiB arena sizes to ~{devices_per_arena:,.0f} devices (retained-only, "
                  f"pre transient headroom)")
        else:
            print("  => per-device slope <= 0: SBMD JS retained memory does not grow with "
                  "device count (per-device state lives in the C++ layer, not the JS arena)")
        print(
            "  NOTE: read under DEBUG_GC off (no dummy-block offset). The floor is the "
            "minimum post-GC live set — the cleanest retained estimate, least transient noise."
        )

        emit_timeseries(
            "device_scale",
            ["devices", "live_bytes", "free_bytes", "peak_bytes", "gc_roots", "arena_bytes"],
            rows,
            payload={
                "context": {
                    "device_type": "light",
                    "devices": _N_DEVICES,
                    "baseline_live_bytes": baseline,
                    "final_live_bytes": final_live,
                    "floor_live_bytes": floor,
                    "avg_retained_bytes_per_device": round(per_device),
                    "estimated_devices_per_arena": (
                        round(devices_per_arena) if devices_per_arena is not None else None
                    ),
                },
                "metrics": collect(get_metrics(client), [
                    "sbmd.js.heap.live_bytes",
                    "sbmd.js.heap.free_bytes",
                    "sbmd.js.heap.peak_bytes",
                    "sbmd.js.gc_roots",
                    "sbmd.js.heap.arena_bytes",
                ]),
            },
        )

        assert final_live < arena * _LIVE_OOM_THRESHOLD, (
            f"Retained live set with {_N_DEVICES} devices ({final_live:,} B, "
            f"{final_live / arena:.1%}) exceeds the {_LIVE_OOM_THRESHOLD:.0%} arena "
            f"ceiling — the arena is undersized for this device count."
        )
        assert per_device <= _MAX_AVG_BYTES_PER_DEVICE, (
            f"Average retained memory per device is {per_device:,.0f} B, above the "
            f"{_MAX_AVG_BYTES_PER_DEVICE:,} B guard. Per-device retention is heavier "
            f"than expected; re-check subscriptions/cached-attribute growth."
        )
        # The guard above is generous enough that a real, modest linear per-device
        # footprint (e.g. up to 32 KiB/device) would still pass it, even though
        # that would contradict "SBMD retained memory does not scale with device
        # count". This tighter bound is the one that actually backs that claim —
        # it is well above the few-hundred-byte sample jitter seen empirically,
        # but far below any device-count-scaling footprint worth flagging.
        assert abs(per_device) <= _FLAT_SLOPE_TOLERANCE_BYTES, (
            f"Average retained memory per device is {per_device:+,.0f} B, outside the "
            f"\u00b1{_FLAT_SLOPE_TOLERANCE_BYTES:,} B flatness tolerance. A genuinely flat "
            f"per-device JS footprint (state lives in the C++ layer, not the JS arena) "
            f"should land well inside this band; a value outside it indicates real "
            f"per-device JS retention worth investigating before loosening this bound."
        )
    finally:
        for light in devices:
            try:
                light._cleanup()
            except Exception:
                pass


# ── Comprehensive runtime metrics under concurrent multi-device load ──────────

# Handler watchdog (BCORE_SBMD_SCRIPT_TIMEOUT_MS); handler/deferred durations must
# stay far below it.
_SCRIPT_TIMEOUT_MS = 5000

# Generous per-GC pause ceiling.  Prod (DEBUG_GC off) GC pauses are sub-ms; this
# only catches a pathological stall.
_MAX_GC_PAUSE_MS = 250


def _hist_agg(metrics, name):
    """Aggregate (sum, count, max) across every datapoint of a histogram metric."""
    total_sum = 0.0
    total_count = 0
    hi = 0.0
    for dp in metrics.get(name, {}).get("dataPoints", []):
        total_sum += dp.get("sum", 0.0)
        total_count += dp.get("count", 0)
        if dp.get("max") is not None:
            hi = max(hi, dp["max"])
    return total_sum, total_count, hi


def _gauge_val(metrics, name, default=0):
    dps = metrics.get(name, {}).get("dataPoints", [])
    return dps[0].get("value", default) if dps else default


# ── per-driver op closures: each returns fn(i) doing one invocation ───────────

# Driver filename stem (telemetry "driver" attribute) for the shared deferred
# worker every scenario drives in addition to its own *driven* closures.
_DEFERRED_DRIVER_STEM = "deferred-command-test"


# Each driven-closure factory returns (fn, attrs, mode):
#   attrs — the (driver, op_type) the op's SUCCESSFUL direct invocation lands in
#   mode  — "exact": a synchronous, non-deferred handler call (exactly one
#           InvokeHandler per client call), so the aggregate success count
#           across every closure sharing (driver, op_type) must equal
#           len(matching closures) * _CONCURRENT_WRITES_PER_DEVICE exactly —
#           "min": the invocation count is not 1:1 with client calls (a
#           sideband push can be coalesced by Matter subscription batching into
#           fewer attribute reports), so only "at least one fired" is asserted.
def _light_op(client, device):
    attrs = {"driver": "light", "op_type": "write"}

    def op(i):
        client.write_resource(
            resource_uri(device, "isOn", endpoint_id=1),
            "true" if i % 2 == 0 else "false",
        )
    return op, attrs, "exact"


def _lock_op(client, device):
    attrs = {"driver": "door-lock", "op_type": "execute"}

    def op(i):
        client.execute_resource(
            resource_uri(device, "lock" if i % 2 == 0 else "unlock", endpoint_id=1), ""
        )
    return op, attrs, "exact"


def _thermostat_op(client, device):
    attrs = {"driver": "thermostat", "op_type": "write"}

    def op(i):
        client.write_resource(
            resource_uri(device, "heatSetpoint", endpoint_id=1), str(1800 + (i % 6) * 50)
        )
    return op, attrs, "exact"


def _sideband_op(instance, operation, base, driver_stem):
    # Sensors are read-only over Matter; pushing a value via the matter.js sideband
    # triggers an attribute report, which invokes the driver's attribute handler.
    # The resulting report count isn't 1:1 with pushes (subscription batching
    # can coalesce them), so this is "min" — at least one must land.
    attrs = {"driver": driver_stem, "op_type": "attribute_report"}

    def op(i):
        instance.sideband.send(operation, {"value": base + (i % 10) * 10})
    return op, attrs, "min"


def _exercise_all_runtime_metrics(client, driven, deferred, scenario, extra_context):
    """
    Drive healthy concurrent load via the *driven* per-driver op closures plus a
    deferred worker, then inject the two deferred circuit-breaker faults, and
    assert EVERY runtime metric family.  *driven* is a list of
    (fn(i), attrs, mode) tuples, one per concurrently-driven driver (a light
    write, a lock execute, a sensor sideband push, ...), so the same helper
    serves both the homogeneous-device and heterogeneous-driver scenarios.

    Assertions are correctness/bounds only (build-independent); magnitudes are
    reported for the prod-representative DEBUG_GC off run.  The js.exception and
    healthy in_flight checks read the phase-1 (pre-fault) snapshot, so the
    deliberate faults in phase 2 don't pollute the healthy-path assertions.
    """
    errors = []
    # +1 for the deferred worker, which fires the same number of ops as every
    # other driven closure but isn't itself one of the *driven* entries.
    total_writes = (len(driven) + 1) * _CONCURRENT_WRITES_PER_DEVICE

    def op_worker(fn):
        for i in range(_CONCURRENT_WRITES_PER_DEVICE):
            try:
                fn(i)
            except Exception as exc:  # noqa: BLE001 - collected, asserted below
                errors.append(exc)

    def deferred_worker(dev):
        for _ in range(_CONCURRENT_WRITES_PER_DEVICE):
            try:
                client.execute_resource(resource_uri(dev, "toggle", endpoint_id=1), "")
            except Exception as exc:  # noqa: BLE001
                errors.append(exc)

    # Phase 1 — healthy concurrent load.
    workers = [threading.Thread(target=op_worker, args=(fn,)) for fn, _, _ in driven]
    workers.append(threading.Thread(target=deferred_worker, args=(deferred,)))
    t0 = time.time()
    for w in workers:
        w.start()
    _JOIN_TIMEOUT_S = 180
    for w in workers:
        w.join(timeout=_JOIN_TIMEOUT_S)
    elapsed = time.time() - t0
    stuck = [w for w in workers if w.is_alive()]
    assert not stuck, (
        f"{len(stuck)}/{len(workers)} worker thread(s) still running after the "
        f"{_JOIN_TIMEOUT_S}s join timeout — phase 1 (healthy load) must fully "
        f"complete before phase 2 (fault injection) starts, or the two overlap "
        f"and neither snapshot is meaningful."
    )
    time.sleep(1.5)
    m = get_metrics(client)

    arena = m["sbmd.js.heap.arena_bytes"]["dataPoints"][0]["value"]
    mutex_sum, mutex_count, mutex_max = _hist_agg(m, "sbmd.js.mutex.wait_ms")
    hdur_sum, hdur_count, hdur_max = _hist_agg(m, "sbmd.handler.duration_ms")
    hdelta_sum, hdelta_count, _ = _hist_agg(m, "sbmd.handler.heap_delta_bytes")
    used_sum, used_count, used_max = _hist_agg(m, "sbmd.js.heap.used_bytes")
    _, _, gcdur_max = _hist_agg(m, "sbmd.js.gc.duration_ms")

    # Phase 1 is pure healthy load (faults are injected later, in phase 2), so
    # EVERY outcome here must be success — across ALL drivers, not just lights.
    # This is the real correctness gate: a failure anywhere fails the test,
    # independent of the report's cosmetic fault-injection downgrade.
    total_success = 0
    phase1_nonsuccess = []
    success_by_key = {}
    for dp in m.get("sbmd.handler.outcome", {}).get("dataPoints", []):
        attrs = dp.get("attributes", {})
        if attrs.get("outcome") == "success":
            total_success += dp.get("value", 0)
            key = (attrs.get("driver"), attrs.get("op_type"))
            success_by_key[key] = success_by_key.get(key, 0) + dp.get("value", 0)
        else:
            phase1_nonsuccess.append((attrs, dp.get("value", 0)))

    # Per-series expected-count check: a single successful call anywhere can
    # make `total_success > 0` true even if every other driven closure's calls
    # silently vanished, so verify each (driver, op_type) series individually.
    expected_exact = {}
    expected_min_keys = set()
    for _, attrs, mode in driven:
        key = (attrs["driver"], attrs["op_type"])
        if mode == "exact":
            expected_exact[key] = expected_exact.get(key, 0) + _CONCURRENT_WRITES_PER_DEVICE
        else:
            expected_min_keys.add(key)
    # The deferred worker's toggle is a deferred (requestCommand) call, not a
    # synchronous one-InvokeHandler-per-call op, so its exact per-call
    # multiplier isn't assumed here — only that every issued toggle shows up
    # at least once.
    expected_min_keys.add((_DEFERRED_DRIVER_STEM, "execute"))
    deferred_floor = {(_DEFERRED_DRIVER_STEM, "execute"): _CONCURRENT_WRITES_PER_DEVICE}

    series_errors = []
    for key, expected_count in expected_exact.items():
        actual = success_by_key.get(key, 0)
        if actual != expected_count:
            series_errors.append(
                f"{key[0]}/{key[1]}: expected exactly {expected_count} successful "
                f"direct invocations, observed {actual}"
            )
    for key in expected_min_keys:
        floor = deferred_floor.get(key, 1)
        actual = success_by_key.get(key, 0)
        if actual < floor:
            series_errors.append(
                f"{key[0]}/{key[1]}: expected at least {floor} successful invocation(s), "
                f"observed {actual}"
            )

    exceptions = sum(
        dp.get("value", 0) for dp in m.get("sbmd.js.exception", {}).get("dataPoints", [])
    )
    gc_count = _gauge_val(m, "sbmd.js.gc.count")
    gc_roots = _gauge_val(m, "sbmd.js.gc_roots")
    in_flight = _gauge_val(m, "sbmd.deferred.in_flight")
    _, _, deferred_dur_max = _hist_agg(m, "sbmd.deferred.duration_ms")
    _, _, deferred_depth_max = _hist_agg(m, "sbmd.deferred.depth")
    live = _gauge_val(m, "sbmd.js.heap.live_bytes", default=None)
    peak = _gauge_val(m, "sbmd.js.heap.peak_bytes")
    throughput = total_writes / elapsed if elapsed else 0

    # Phase 2 — deferred circuit-breaker fault injection.  Sequential: runaway
    # re-arms synchronously under the JS mutex, so keep it out of the concurrency
    # window.  These are the only paths that fire deferred.timeout / max_depth.
    for res, wait in (("timeoutToggle", 2.0), ("runawayToggle", 5.0)):
        try:
            client.execute_resource(resource_uri(deferred, res, endpoint_id=1), "")
        except Exception as exc:  # noqa: BLE001
            errors.append(exc)
        time.sleep(wait)
    m2 = get_metrics(client)
    deferred_timeout = sum(
        dp.get("value", 0) for dp in m2.get("sbmd.deferred.timeout", {}).get("dataPoints", [])
    )
    deferred_max_depth = sum(
        dp.get("value", 0) for dp in m2.get("sbmd.deferred.max_depth", {}).get("dataPoints", [])
    )
    in_flight_after = _gauge_val(m2, "sbmd.deferred.in_flight")

    print(f"\n  ── {scenario}: {len(driven)} driver(s) + deferred, {total_writes} "
          f"concurrent ops in {elapsed:.1f}s ({throughput:.0f} ops/s) ──")
    print(f"  thread errors:        {len(errors)}")
    print(f"  handler.outcome:      {total_success} success / "
          f"{len(phase1_nonsuccess)} non-success (healthy phase)")
    print(f"  js.exception:         {exceptions}")
    print(f"  mutex.wait_ms:        sum {mutex_sum:.2f} / max {mutex_max:.3f} over {mutex_count} waits")
    print(f"  handler.duration_ms:  max {hdur_max:.3f} / avg "
          f"{(hdur_sum / hdur_count if hdur_count else 0):.3f} over {hdur_count}")
    print(f"  handler.heap_delta:   avg "
          f"{(hdelta_sum / hdelta_count if hdelta_count else 0):.0f} B over {hdelta_count}")
    print(f"  heap used_bytes:      max {used_max:,.0f} over {used_count} samples")
    print(f"  gc.count:             {gc_count}   gc.duration_ms max: {gcdur_max:.3f}   gc_roots: {gc_roots}")
    print(f"  deferred:             in_flight {in_flight}->{in_flight_after}  dur_max "
          f"{deferred_dur_max:.1f}  depth_max {deferred_depth_max:.0f}  timeout "
          f"{deferred_timeout}  max_depth {deferred_max_depth}")
    if live is not None:
        print(f"  heap live_bytes:      {live:,} ({live / arena:.1%})   peak: {peak:,} ({peak / arena:.1%})")
    else:
        print(f"  heap live_bytes:      (no post-GC sample)   peak: {peak:,} ({peak / arena:.1%})")

    context = {
        "drivers_driven": len(driven),
        "ops_per_driver": _CONCURRENT_WRITES_PER_DEVICE,
        "total_ops": total_writes,
        "elapsed_s": round(elapsed, 2),
        "throughput_ops_per_s": round(throughput),
        "fault_injection": True,
    }
    context.update(extra_context or {})
    emit_report(scenario, {
        "context": context,
        "metrics": collect(m2, [
            "sbmd.js.mutex.wait_ms",
            "sbmd.handler.duration_ms",
            "sbmd.handler.heap_delta_bytes",
            "sbmd.handler.outcome",
            "sbmd.js.exception",
            "sbmd.js.gc.count",
            "sbmd.js.gc.duration_ms",
            "sbmd.js.gc.alloc_churn_bytes",
            "sbmd.js.gc_roots",
            "sbmd.deferred.in_flight",
            "sbmd.deferred.duration_ms",
            "sbmd.deferred.depth",
            "sbmd.deferred.timeout",
            "sbmd.deferred.max_depth",
            "sbmd.js.heap.used_bytes",
            "sbmd.js.heap.live_bytes",
            "sbmd.js.heap.peak_bytes",
            "sbmd.js.heap.free_bytes",
            "sbmd.js.heap.arena_bytes",
        ]),
    })

    # ── correctness / bounds (build-independent) ──────────────────────────────
    assert not errors, f"{len(errors)} calls raised: {errors[:3]}"
    assert not phase1_nonsuccess, (
        f"non-success handler outcome(s) under healthy load (pre fault-injection): "
        f"{phase1_nonsuccess}"
    )
    assert not series_errors, (
        "per-series invocation count check failed — a driven closure's calls "
        "partially or fully vanished even though the global success total was "
        "positive:\n  " + "\n  ".join(series_errors)
    )
    assert exceptions == 0, f"{exceptions} JS exceptions under healthy concurrent load"
    assert mutex_sum > 0, (
        "mutex.wait_ms sum is 0 under concurrent load — the shared JS mutex should "
        "have serialized at least some overlapping calls."
    )
    assert hdur_max < _SCRIPT_TIMEOUT_MS, (
        f"handler.duration_ms max {hdur_max:.1f} ms approaches the {_SCRIPT_TIMEOUT_MS} ms watchdog."
    )
    assert used_count > 0, "sbmd.js.heap.used_bytes recorded no samples"
    assert 0 < used_max <= arena, f"used_bytes max {used_max} out of range for a {arena:,}-byte arena"
    assert in_flight == 0, f"deferred.in_flight {in_flight} after healthy load — a chain did not settle"
    if deferred_dur_max:
        assert deferred_dur_max < _SCRIPT_TIMEOUT_MS, (
            f"deferred.duration_ms max {deferred_dur_max:.1f} ms approaches the watchdog."
        )
    if "sbmd.js.gc.count" in m:
        assert gc_count > 0, "GC instrumentation present but no GC fired under load"
        assert gcdur_max < _MAX_GC_PAUSE_MS, (
            f"gc.duration_ms max {gcdur_max:.1f} ms exceeds {_MAX_GC_PAUSE_MS} ms — a pathological stall."
        )
    if live is not None:
        assert live < arena * _LIVE_OOM_THRESHOLD, (
            f"Retained live set {live:,} B ({live / arena:.1%}) exceeds "
            f"{_LIVE_OOM_THRESHOLD:.0%} of arena under load."
        )
    # Circuit breakers must have fired and cleaned up under load.
    assert deferred_timeout >= 1, "deferred.timeout did not fire for timeoutToggle under load"
    assert deferred_max_depth >= 1, "deferred.max_depth did not fire for runawayToggle under load"
    assert in_flight_after == 0, (
        f"deferred.in_flight {in_flight_after} after circuit breakers — a faulted chain leaked."
    )


def test_all_runtime_metrics_under_concurrent_device_load(default_environment):
    """
    Commission _N_DEVICES lights + one deferred-command device, then drive
    concurrent load across all of them (one worker thread per light firing
    _CONCURRENT_WRITES_PER_DEVICE isOn writes; a thread exercising the deferred
    device's toggle) and assert on EVERY runtime metric family at scale — the
    multi-device analogue of the single-device suite.

    This exercises the shared-context blast radius the single-device runs could
    not: all drivers share one 1 MiB JS context behind one mutex, so N devices
    firing at once is the real contention test.

    Assertions are correctness/bounds only (build-independent); magnitude numbers
    (mutex/gc/handler latency) are reported for the prod-representative DEBUG_GC
    off run.  gc.* / deferred.* assertions degrade gracefully when the metric is
    absent (GC instrumentation off, or no GC fired).
    """
    devices = []
    try:
        # The orchestrator's device-added flag is sticky (set once, never reset),
        # so wait_for_device_added() returns instantly after the first device.
        # Same-type sweeps mask this; a mixed-type commission does not. Reset it
        # before each commission so the wait genuinely blocks for THIS device.
        def _commission(device, cls):
            default_environment._commissioned_device = False
            return commission_device(default_environment, device, cls)

        lights = []
        for _ in range(_N_DEVICES):
            light = MatterLight()
            light.start()
            devices.append(light)
            lights.append(_commission(light, "light"))

        deferred_dev = MatterDeferredCmdTestDevice()
        deferred_dev.start()
        devices.append(deferred_dev)
        deferred = _commission(deferred_dev, "deferredCmdTest")

        client = default_environment.get_client()
        driven = [_light_op(client, dev) for dev in lights]
        _exercise_all_runtime_metrics(
            client, driven, deferred,
            "concurrent_device_scale", {"devices": _N_DEVICES},
        )
    finally:
        for dev in devices:
            try:
                dev._cleanup()
            except Exception:
                pass


def test_all_runtime_metrics_across_heterogeneous_drivers(default_environment):
    """
    Commission one device of EACH drivable type — light, door-lock, thermostat,
    temperature sensor, humidity sensor — plus a deferred-command device, then
    drive them all concurrently and assert on EVERY runtime metric family.

    Where test_all_runtime_metrics_under_concurrent_device_load stresses ONE
    driver across many devices, this stresses MANY DISTINCT drivers sharing the
    single 1 MiB JS context at once: attribute writes, command executes, and
    sideband-driven attribute reports all contend for the same mutex and heap.
    It is the realistic heterogeneous gateway — each device type binds a
    different SBMD driver with its own handler code.

    matter.js exposes no generic/parameterizable device type (the endpoint's
    device type is baked into each standard device class), so the handful of real
    types here is the ceiling for DISTINCT ACTIVE drivers without patching
    matter.js; 100 *active* matchable stub drivers is not feasible with the
    current harness.  Driver-count scaling to 100 is covered separately by inert
    stubs, which only meaningfully exercise load/registry/footprint metrics.
    """
    env = default_environment
    client = env.get_client()
    devices = []
    try:
        # Sticky device-added flag: reset before each commission so the wait
        # genuinely blocks for THIS device (see the concurrent-device test).
        def _commission(device, cls):
            env._commissioned_device = False
            return commission_device(env, device, cls)

        light = MatterLight()
        light.start()
        devices.append(light)
        light_dev = _commission(light, "light")

        lock = MatterDoorLock()
        lock.start()
        devices.append(lock)
        lock_dev = _commission(lock, "doorLock")

        thermostat = MatterThermostat()
        thermostat.start()
        devices.append(thermostat)
        thermostat_dev = _commission(thermostat, "thermostat")

        temp = MatterTemperatureSensor()
        temp.start()
        devices.append(temp)
        _commission(temp, "environmentalSensor")

        humidity = MatterHumiditySensor()
        humidity.start()
        devices.append(humidity)
        _commission(humidity, "environmentalSensor")

        deferred_dev = MatterDeferredCmdTestDevice()
        deferred_dev.start()
        devices.append(deferred_dev)
        deferred = _commission(deferred_dev, "deferredCmdTest")

        # Five distinct active drivers under concurrent load (+ the deferred
        # driver exercised by the shared helper) across three invocation paths:
        # attribute write, command execute, and sideband-driven attribute report.
        driven = [
            _light_op(client, light_dev),
            _lock_op(client, lock_dev),
            _thermostat_op(client, thermostat_dev),
            _sideband_op(temp, "setTemperature", 2000, "temperature-sensor"),
            _sideband_op(humidity, "setHumidity", 4000, "humidity-sensor"),
        ]
        _exercise_all_runtime_metrics(
            client, driven, deferred,
            "heterogeneous_driver_runtime",
            {"distinct_active_drivers": len(driven) + 1},
        )

        # The point of this scenario is contention between DISTINCT drivers, not
        # just N copies of one — verify telemetry actually reflects that mix,
        # rather than trusting the aggregate success/mutex/duration checks
        # (which a single working driver could satisfy alone).
        final_metrics = get_metrics(client)
        distinct_drivers = {
            dp.get("attributes", {}).get("driver")
            for dp in final_metrics.get("sbmd.handler.outcome", {}).get("dataPoints", [])
            if dp.get("attributes", {}).get("outcome") == "success"
            and dp.get("attributes", {}).get("driver")
        }
        assert len(distinct_drivers) >= len(driven), (
            f"Expected successful handler.outcome telemetry from at least "
            f"{len(driven)} distinct drivers (the point of this scenario), observed "
            f"{len(distinct_drivers)}: {sorted(distinct_drivers)}"
        )
    finally:
        for dev in devices:
            try:
                dev._cleanup()
            except Exception:
                pass


# ── Driver scaling: 100 generated stub drivers ────────────────────────────────

def _stub_spec(index):
    """Return the source of a minimal, schema-valid SBMD stub driver."""
    device_type = _STUB_DEVICE_TYPE_BASE + index
    return (
        "SbmdDriver({\n"
        "    schemaVersion: '5.0',\n"
        "    driverVersion: 1,\n"
        f"    name: 'Scale Stub {index}',\n"
        "    constants: { EP: '1' },\n"
        f"    barton: {{ deviceClass: 'scaleStub{index}', deviceClassVersion: 1 }},\n"
        f"    matter: {{ deviceTypes: [{device_type}] }}\n"
        "});\n"
    )


@pytest.fixture
def hundred_driver_environment(tmp_path):
    """
    Bring up a Barton environment with _N_STUB_DRIVERS extra stub SBMD drivers
    generated into a temp directory and injected via BARTON_EXTRA_SBMD_DIRS.

    The env var is read by BaseEnvironmentOrchestrator.__init__, so it must be
    set before the orchestrator is constructed here.
    """
    stub_dir = tmp_path / "stub-specs"
    stub_dir.mkdir()
    for i in range(_N_STUB_DRIVERS):
        (stub_dir / f"scale-stub-{i:03d}.sbmd.js").write_text(_stub_spec(i))

    prev = os.environ.get("BARTON_EXTRA_SBMD_DIRS")
    os.environ["BARTON_EXTRA_SBMD_DIRS"] = str(stub_dir)
    env = None
    try:
        env = DefaultEnvironmentOrchestrator()
        env.start_client()
        env.wait_for_client_to_be_ready()
        yield env
    finally:
        if env is not None:
            env._cleanup()
        if prev is None:
            os.environ.pop("BARTON_EXTRA_SBMD_DIRS", None)
        else:
            os.environ["BARTON_EXTRA_SBMD_DIRS"] = prev


def _hist_sum(metrics, name):
    return sum(dp.get("sum", 0) for dp in metrics.get(name, {}).get("dataPoints", []))


def test_driver_load_scales_with_stub_drivers(hundred_driver_environment):
    """
    Load _N_STUB_DRIVERS generated stub drivers alongside the real ones and
    characterize what driver count does to startup metrics: registration time,
    per-driver load time, registered count, and heap footprint.

    This answers "what happens at 100 drivers?" — whether load time and memory
    scale linearly and stay inside the 1 MiB arena, or hit a wall.  Stubs are
    minimal (no handlers), so this isolates the discover -> eval -> activate ->
    register pipeline cost per driver.
    """
    client = hundred_driver_environment.get_client()
    metrics = get_metrics(client)

    registered = metrics.get("sbmd.driver.registered.count", {}).get(
        "dataPoints", [{}]
    )[0].get("value", 0)
    reg_total = _hist_sum(metrics, "sbmd.driver.registration.total_ms")
    load_sum = _hist_sum(metrics, "sbmd.driver.load.duration_ms")
    bundle = _hist_sum(metrics, "sbmd.driver.bundle_load_ms")
    load_count = sum(
        dp.get("count", 0)
        for dp in metrics.get("sbmd.driver.load.duration_ms", {}).get("dataPoints", [])
    )
    load_failures = sum(
        dp.get("value", 0)
        for dp in metrics.get("sbmd.driver.load.failure", {}).get("dataPoints", [])
    )

    arena = metrics["sbmd.js.heap.arena_bytes"]["dataPoints"][0]["value"]
    peak = metrics.get("sbmd.js.heap.peak_bytes", {}).get("dataPoints", [{}])[0].get("value", 0)
    live_dps = metrics.get("sbmd.js.heap.live_bytes", {}).get("dataPoints", [])
    live = live_dps[0]["value"] if live_dps else None
    gc_roots = _gauge_val(metrics, "sbmd.js.gc_roots")
    per_driver_load = load_sum / load_count if load_count else 0

    print(f"\n  ── Driver scaling: +{_N_STUB_DRIVERS} stub drivers ──")
    print(f"  registered.count:            {registered}")
    print(f"  registration.total_ms:       {reg_total:,.1f} ms")
    print(f"  load.duration sum:           {load_sum:,.1f} ms over {load_count} drivers")
    print(f"  per-driver load:             {per_driver_load:.2f} ms")
    print(f"  bundle_load_ms (one-time):   {bundle:,.1f} ms")
    print(f"  load.failure:                {load_failures}")
    print(f"  gc_roots:                    {gc_roots}")
    print(f"  heap peak:                   {peak:,} B ({peak / arena:.1%})")
    if live is not None:
        print(f"  heap live (post-GC):         {live:,} B ({live / arena:.1%})")

    emit_report("driver_scale", {
        "context": {
            "stub_drivers": _N_STUB_DRIVERS,
            "registered_count": registered,
            "registration_total_ms": round(reg_total, 1),
            "load_duration_sum_ms": round(load_sum, 1),
            "per_driver_load_ms": round(per_driver_load, 3),
            "bundle_load_ms": round(bundle, 1),
            "gc_roots": gc_roots,
        },
        "metrics": collect(metrics, [
            "sbmd.driver.registered.count",
            "sbmd.driver.registration.total_ms",
            "sbmd.driver.load.duration_ms",
            "sbmd.driver.bundle_load_ms",
            "sbmd.driver.load.failure",
            "sbmd.js.gc_roots",
            "sbmd.js.heap.peak_bytes",
            "sbmd.js.heap.live_bytes",
            "sbmd.js.heap.free_bytes",
            "sbmd.js.heap.arena_bytes",
        ]),
    })

    assert load_failures == 0, f"{load_failures} driver(s) failed to load"
    # Baseline = production + test .sbmd.js files (same glob pattern used by
    # test_registered_driver_count_correct). Exact equality, not a lower bound:
    # ">= _N_STUB_DRIVERS" alone would pass even if every production/test driver
    # were missing, since the stub count alone already clears that bar.
    workspace_root = Path(__file__).parent.parent.parent
    production_specs = workspace_root / "core" / "deviceDrivers" / "matter" / "sbmd" / "specs"
    test_specs = workspace_root / "testing" / "resources" / "sbmd-specs"
    baseline_count = len(list(production_specs.glob("*.sbmd.js"))) + len(
        list(test_specs.glob("*.sbmd.js"))
    )
    expected_total = baseline_count + _N_STUB_DRIVERS
    assert registered == expected_total, (
        f"Expected exactly {expected_total} registered drivers "
        f"({baseline_count} baseline production+test drivers + {_N_STUB_DRIVERS} stubs), "
        f"got {registered}. A lower-bound check here would pass even if every "
        f"baseline driver silently failed to register, as long as the stubs alone "
        f"cleared {_N_STUB_DRIVERS}."
    )
    # gc_roots must not scale with driver count — a registry that rooted per-driver
    # objects would leak.  It is recorded only during handler runs, so 0 = not-yet-
    # sampled (fine); any positive value must stay small/bounded, not ~driver-count.
    assert gc_roots < 200, (
        f"gc_roots is {gc_roots} with {registered} drivers registered — the root set "
        f"appears to scale with driver count (expected small + flat)."
    )
    if live is not None:
        assert live < arena * _LIVE_OOM_THRESHOLD, (
            f"Retained live set with {_N_STUB_DRIVERS} extra drivers "
            f"({live:,} B, {live / arena:.1%}) exceeds the {_LIVE_OOM_THRESHOLD:.0%} "
            f"arena ceiling — driver count is pressuring the JS arena."
        )
