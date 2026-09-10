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
SBMD observability metrics — baseline / smoke tests.

These tests verify the startup state of all SBMD metrics before any device
is commissioned.  No virtual devices are needed; the default_environment
fixture is sufficient.

What is checked:
  - All expected metric names are present with the correct type.
  - Static gauge values (arena size, registered driver count, in-flight ops)
    match known good values.
  - Startup heap headroom is sufficient (≥ 20 % of the arena).
  - Per-driver load cost metrics are populated and individually bounded.

NOTE: sbmd.driver.load.failure cannot be tested here because a driver load
failure causes RegisterDriversFromDirectory() to return false, which prevents
the Matter subsystem from initialising.  That counter is covered by the
SbmdObservabilityTest C++ unit tests.
"""

from pathlib import Path

import pytest

from testing.helpers.sbmd_metrics_helpers import (
    format_heap_snapshot,
    format_histogram,
    get_metrics,
)

pytestmark = [pytest.mark.requires_matterjs]

# Expected metric names and types.  GC metrics are guarded separately because
# they are absent when BCORE_SBMD_GC_INSTRUMENTATION=OFF.
_EXPECTED_METRICS = [
    ("sbmd.js.heap.used_bytes",       "histogram"),
    ("sbmd.js.heap.arena_bytes",      "gauge"),
    ("sbmd.js.heap.free_bytes",       "gauge"),
    ("sbmd.js.heap.peak_bytes",       "gauge"),
    ("sbmd.js.mutex.wait_ms",         "histogram"),
    ("sbmd.js.exception",             "counter"),
    ("sbmd.handler.duration_ms",      "histogram"),
    ("sbmd.handler.heap_delta_bytes", "histogram"),
    ("sbmd.handler.outcome",          "counter"),
    ("sbmd.driver.load.duration_ms",  "histogram"),
    ("sbmd.driver.registration.total_ms", "histogram"),
    ("sbmd.driver.bundle_load_ms",    "histogram"),
    ("sbmd.driver.load.failure",      "counter"),
    ("sbmd.driver.registered.count",  "gauge"),
    ("sbmd.deferred.in_flight",       "gauge"),
    ("sbmd.deferred.duration_ms",     "histogram"),
    ("sbmd.deferred.depth",           "histogram"),
    ("sbmd.deferred.timeout",         "counter"),
    ("sbmd.deferred.max_depth",       "counter"),
]

_EXPECTED_GC_METRICS = [
    ("sbmd.js.gc.count",       "counter"),
    ("sbmd.js.gc.duration_ms", "histogram"),
    ("sbmd.js.gc_roots",       "gauge"),
]

# Configured arena size for the dev build.  Gateways may differ; the test
# will still pass because it reads the arena value from the metric itself and
# only asserts it is non-zero and positive.
_DEV_ARENA_BYTES = 1_048_576

# Minimum free heap fraction required at startup (after all drivers load).
_MIN_FREE_FRACTION = 0.20


def test_all_metric_names_present(default_environment):
    """
    Every expected SBMD metric is present in the telemetry JSON with the
    correct type field after a clean startup.

    Catches: metric renames, type changes, or metrics silently compiled out.
    """
    client = default_environment.get_client()
    metrics = get_metrics(client)

    print("\n── All metrics present ──────────────────────────────────")
    for name, expected_type in _EXPECTED_METRICS:
        assert name in metrics, f"Missing metric: {name}"
        actual_type = metrics[name].get("type")
        assert actual_type == expected_type, (
            f"{name}: expected type '{expected_type}', got '{actual_type}'"
        )
        print(f"  ✓  {name}  ({actual_type})")

    # GC metrics: present only when BCORE_SBMD_GC_INSTRUMENTATION=ON (default).
    # Assert type correctness for whichever are present; do not fail if absent.
    for name, expected_type in _EXPECTED_GC_METRICS:
        if name in metrics:
            actual_type = metrics[name].get("type")
            assert actual_type == expected_type, (
                f"{name}: expected type '{expected_type}', got '{actual_type}'"
            )
            print(f"  ✓  {name}  ({actual_type})")
        else:
            print(f"  –  {name}  (absent — BCORE_SBMD_GC_INSTRUMENTATION may be OFF)")


def test_heap_arena_size(default_environment):
    """
    sbmd.js.heap.arena_bytes has exactly one datapoint with a positive value.
    On the dev build the value equals BCORE_MQUICKJS_MEMSIZE_BYTES (1 MiB).

    This metric is set once in MQuickJsRuntime::Initialize() and never changes.
    If the value is 0, the metric was not recorded.  If it differs from the
    expected dev constant, a non-standard build config is in use.
    """
    client = default_environment.get_client()
    metrics = get_metrics(client)

    arena_dps = metrics["sbmd.js.heap.arena_bytes"]["dataPoints"]
    assert len(arena_dps) == 1, (
        f"Expected 1 datapoint for arena_bytes, got {len(arena_dps)}"
    )
    arena = arena_dps[0]["value"]
    assert arena > 0, "sbmd.js.heap.arena_bytes is 0 — metric was not recorded"
    print(f"\n  arena_bytes = {arena:,}")
    if arena != _DEV_ARENA_BYTES:
        print(
            f"  NOTE: arena ({arena:,}) differs from dev constant ({_DEV_ARENA_BYTES:,}). "
            f"Non-default BCORE_MQUICKJS_MEMSIZE_BYTES build."
        )


def test_startup_heap_headroom(default_environment):
    """
    After all drivers load, at least _MIN_FREE_FRACTION of the arena is free.

    Catches: a new or modified driver that consumes too much heap at load time,
    leaving insufficient headroom for runtime operations.
    """
    client = default_environment.get_client()
    metrics = get_metrics(client)

    arena = metrics["sbmd.js.heap.arena_bytes"]["dataPoints"][0]["value"]
    free  = metrics["sbmd.js.heap.free_bytes"]["dataPoints"][0]["value"]

    print()
    format_heap_snapshot("Startup heap state:", metrics)
    print(f"\n  Required minimum free: {arena * _MIN_FREE_FRACTION:,.0f} bytes ({_MIN_FREE_FRACTION:.0%})")

    assert free >= arena * _MIN_FREE_FRACTION, (
        f"Startup heap headroom too low: {free:,} bytes free "
        f"({free / arena:.1%} of {arena:,}-byte arena). "
        f"Required: >= {_MIN_FREE_FRACTION:.0%}."
    )


def test_heap_used_bytes_has_initial_snapshot(default_environment):
    """
    sbmd.js.heap.used_bytes has at least one histogram observation after
    startup.  This snapshot is taken at the end of MQuickJsRuntime::Initialize()
    before any handler has been invoked.

    If count is 0, the Initialize() recording path is broken.
    """
    client = default_environment.get_client()
    metrics = get_metrics(client)

    used_dps = metrics["sbmd.js.heap.used_bytes"]["dataPoints"]
    total_count = sum(dp["count"] for dp in used_dps)
    assert total_count >= 1, (
        "sbmd.js.heap.used_bytes has no observations after startup. "
        "The Initialize() heap snapshot was not recorded."
    )
    print(f"\n  used_bytes observations at startup: {total_count}")
    for dp in used_dps:
        print(format_histogram(dp, unit=" B"))


def test_deferred_in_flight_zero_at_startup(default_environment):
    """
    sbmd.deferred.in_flight gauge reflects zero in-flight deferred ops at
    startup.  The gauge is only recorded when ExecuteRequestCommand runs, so
    its dataPoints list is empty before any deferred operation has been
    started.  An empty list is as good as 0: no op has ever been queued.

    A non-zero value would mean a deferred operation was parked during driver
    initialisation — a code bug.
    """
    client = default_environment.get_client()
    metrics = get_metrics(client)

    in_flight_dps = metrics["sbmd.deferred.in_flight"]["dataPoints"]
    # Gauge is recorded lazily — no datapoints means never set, which is fine.
    in_flight = in_flight_dps[0]["value"] if in_flight_dps else 0
    assert in_flight == 0, (
        f"deferred.in_flight is {in_flight} at startup — expected 0"
    )
    print(f"\n  deferred.in_flight at startup: {in_flight} "
          f"({'recorded' if in_flight_dps else 'gauge never set — no deferred ops yet'})")


def test_driver_registered_count_matches_filesystem(default_environment):
    """
    sbmd.driver.registered.count equals the number of .sbmd.js files on
    disk across both the production and test specs directories.

    A count lower than the file count means at least one driver silently
    failed to load (even though Barton started successfully).
    """
    workspace_root = Path(__file__).parent.parent.parent
    production_specs = workspace_root / "core" / "deviceDrivers" / "matter" / "sbmd" / "specs"
    test_specs       = workspace_root / "testing" / "resources" / "sbmd-specs"
    expected = (
        len(list(production_specs.glob("*.sbmd.js")))
        + len(list(test_specs.glob("*.sbmd.js")))
    )

    client = default_environment.get_client()
    metrics = get_metrics(client)

    count_dps = metrics["sbmd.driver.registered.count"]["dataPoints"]
    assert len(count_dps) == 1
    actual = count_dps[0]["value"]
    print(
        f"\n  registered.count = {actual} "
        f"(production: {len(list(production_specs.glob('*.sbmd.js')))}, "
        f"test: {len(list(test_specs.glob('*.sbmd.js')))})"
    )
    assert actual == expected, (
        f"Expected {expected} registered drivers ({len(list(production_specs.glob('*.sbmd.js')))} "
        f"production + {len(list(test_specs.glob('*.sbmd.js')))} test), got {actual}. "
        f"At least one driver may have failed to load silently."
    )


def test_driver_load_cost_metrics_populated(default_environment):
    """
    sbmd.driver.load.duration_ms has one datapoint per successfully loaded
    driver (count=1, sum>0).

    Also prints the driver load time table sorted by duration so that slow
    drivers are immediately visible in the test output.
    """
    client = default_environment.get_client()
    metrics = get_metrics(client)

    dur_dps = metrics["sbmd.driver.load.duration_ms"]["dataPoints"]

    assert len(dur_dps) >= 1, "sbmd.driver.load.duration_ms has no datapoints"

    print(f"\n  {'Driver':<38} {'ms':>10}")
    print(f"  {'-' * 38} {'-' * 10}")

    for dp in sorted(dur_dps, key=lambda d: -d["sum"]):
        drv = dp.get("attributes", {}).get("driver", "?")
        ms  = dp["sum"]

        assert dp["count"] == 1, (
            f"Driver '{drv}': load.duration_ms count={dp['count']}, expected 1. "
            f"Each driver should be loaded exactly once per startup."
        )
        assert ms > 0, f"Driver '{drv}': load time is 0 ms — metric was not recorded"

        print(f"  {drv:<38} {ms:>10.1f}")

    total_ms = sum(dp["sum"] for dp in dur_dps)
    print(f"  {'-' * 38} {'-' * 10}")
    print(f"  {'TOTAL':<38} {total_ms:>10.1f}")
