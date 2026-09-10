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

# Matter device-type base for generated stubs.  Well outside real device types
# and the existing test drivers (command-echo uses 0xfff10000), so the 100 stubs
# neither collide with each other nor match any real device.
_STUB_DEVICE_TYPE_BASE = 0xFFF20000

# Sanity ceiling on the average retained bytes per commissioned device.  A light
# retains a device object, subscriptions, and cached attributes — a few KB.  A
# value far above this would mean per-device retention is heavier than expected
# and the arena would size poorly; it is a generous guard, not a tight budget.
_MAX_AVG_BYTES_PER_DEVICE = 32 * 1024

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
    for i in range(_SWEEP_LOAD_WRITES):
        try:
            client.write_resource(
                resource_uri(device, "isOn", endpoint_id=1),
                "true" if i % 2 == 0 else "false",
            )
        except Exception:
            pass
    time.sleep(0.2)
    return _live_bytes(client)


def _gauge(metrics, name):
    dps = metrics.get(name, {}).get("dataPoints", [])
    return dps[0]["value"] if dps else ""


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
            commissioned = commission_device(default_environment, light, "light")

            # Drive real load on this device so the engine's own lazy GC fires
            # (under off) and refreshes live_bytes, then read the post-GC live set.
            live = _drive_load_and_read(client, commissioned)
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

def _light_op(client, device):
    def op(i):
        client.write_resource(
            resource_uri(device, "isOn", endpoint_id=1),
            "true" if i % 2 == 0 else "false",
        )
    return op


def _lock_op(client, device):
    def op(i):
        client.execute_resource(
            resource_uri(device, "lock" if i % 2 == 0 else "unlock", endpoint_id=1), "", ""
        )
    return op


def _thermostat_op(client, device):
    def op(i):
        client.write_resource(
            resource_uri(device, "heatSetpoint", endpoint_id=1), str(1800 + (i % 6) * 50)
        )
    return op


def _sideband_op(instance, operation, base):
    # Sensors are read-only over Matter; pushing a value via the matter.js sideband
    # triggers an attribute report, which invokes the driver's attribute handler.
    def op(i):
        instance.sideband.send(operation, {"value": base + (i % 10) * 10})
    return op


def _exercise_all_runtime_metrics(client, driven, deferred, scenario, extra_context):
    """
    Drive healthy concurrent load via the *driven* per-driver op closures plus a
    deferred worker, then inject the two deferred circuit-breaker faults, and
    assert EVERY runtime metric family.  *driven* is a list of fn(i) callables,
    one per concurrently-driven driver (a light write, a lock execute, a sensor
    sideband push, ...), so the same helper serves both the homogeneous-device
    and heterogeneous-driver scenarios.

    Assertions are correctness/bounds only (build-independent); magnitudes are
    reported for the prod-representative DEBUG_GC off run.  The js.exception and
    healthy in_flight checks read the phase-1 (pre-fault) snapshot, so the
    deliberate faults in phase 2 don't pollute the healthy-path assertions.
    """
    errors = []
    total_writes = len(driven) * _CONCURRENT_WRITES_PER_DEVICE

    def op_worker(fn):
        for i in range(_CONCURRENT_WRITES_PER_DEVICE):
            try:
                fn(i)
            except Exception as exc:  # noqa: BLE001 - collected, asserted below
                errors.append(exc)

    def deferred_worker(dev):
        for _ in range(_CONCURRENT_WRITES_PER_DEVICE):
            try:
                client.execute_resource(resource_uri(dev, "toggle", endpoint_id=1), "", "")
            except Exception as exc:  # noqa: BLE001
                errors.append(exc)

    # Phase 1 — healthy concurrent load.
    workers = [threading.Thread(target=op_worker, args=(fn,)) for fn in driven]
    workers.append(threading.Thread(target=deferred_worker, args=(deferred,)))
    t0 = time.time()
    for w in workers:
        w.start()
    for w in workers:
        w.join(timeout=180)
    elapsed = time.time() - t0
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
    for dp in m.get("sbmd.handler.outcome", {}).get("dataPoints", []):
        attrs = dp.get("attributes", {})
        if attrs.get("outcome") == "success":
            total_success += dp.get("value", 0)
        else:
            phase1_nonsuccess.append((attrs, dp.get("value", 0)))

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
            client.execute_resource(resource_uri(deferred, res, endpoint_id=1), "", "")
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
        ]),
    })

    # ── correctness / bounds (build-independent) ──────────────────────────────
    assert not errors, f"{len(errors)} calls raised: {errors[:3]}"
    assert not phase1_nonsuccess, (
        f"non-success handler outcome(s) under healthy load (pre fault-injection): "
        f"{phase1_nonsuccess}"
    )
    assert total_success > 0, "no successful handler invocations recorded"
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
            _sideband_op(temp, "setTemperature", 2000),
            _sideband_op(humidity, "setHumidity", 4000),
        ]
        _exercise_all_runtime_metrics(
            client, driven, deferred,
            "heterogeneous_driver_runtime",
            {"distinct_active_drivers": len(driven) + 1},
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
        ]),
    })

    assert load_failures == 0, f"{load_failures} driver(s) failed to load"
    assert registered >= _N_STUB_DRIVERS, (
        f"Only {registered} drivers registered; expected at least the "
        f"{_N_STUB_DRIVERS} stubs (plus real drivers). Some stubs failed to register."
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
