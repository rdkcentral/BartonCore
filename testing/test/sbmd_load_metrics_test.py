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
Integration tests for SBMD driver load metrics (tasks 7.9 and 7.14).

These tests verify that SbmdFactory's startup instrumentation records correctly
after Barton starts.  No devices are commissioned; the metrics are populated
entirely by RegisterDriversFromDirectory during the startup sequence.

  - sbmd.driver.load.duration_ms and sbmd.driver.load.heap_bytes must have at
    least one observation (one per successfully loaded driver).
  - sbmd.driver.registered.count gauge must equal the total number of .sbmd.js
    files across the production and test specs directories.

NOTE: task 7.10 (driver load failure counter) cannot be tested here because a
driver load failure causes RegisterDrivers() to return false, which makes the
Matter constructor throw and prevents Barton from starting.  Telemetry is
unreachable from a process that never reached READY_FOR_DEVICE_OPERATION.
"""

import json
from pathlib import Path


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------


def test_driver_load_cost_metrics_populated(default_environment):
    """
    7.9: After startup sbmd.driver.load.duration_ms and
    sbmd.driver.load.heap_bytes each have at least one observation —
    one histogram datapoint per successfully loaded driver.
    """
    client = default_environment.get_client()
    telemetry = json.loads(client.get_telemetry())
    metrics = telemetry.get("metrics", {})

    # duration_ms — each loaded driver contributes one observation
    dur_metric = metrics.get("sbmd.driver.load.duration_ms")
    assert dur_metric is not None, (
        "sbmd.driver.load.duration_ms not found in telemetry after startup"
    )
    dur_dps = dur_metric.get("dataPoints", [])
    assert len(dur_dps) >= 1, "sbmd.driver.load.duration_ms has no datapoints"
    assert sum(dp["count"] for dp in dur_dps) >= 1
    assert sum(dp["sum"] for dp in dur_dps) > 0, (
        "sbmd.driver.load.duration_ms sum should be positive (loading takes non-zero time)"
    )

    # heap_bytes — each loaded driver contributes one observation
    heap_metric = metrics.get("sbmd.driver.load.heap_bytes")
    assert heap_metric is not None, (
        "sbmd.driver.load.heap_bytes not found in telemetry after startup"
    )
    heap_dps = heap_metric.get("dataPoints", [])
    assert len(heap_dps) >= 1, "sbmd.driver.load.heap_bytes has no datapoints"
    assert sum(dp["count"] for dp in heap_dps) >= 1


def test_registered_driver_count_correct(default_environment):
    """
    7.14: sbmd.driver.registered.count gauge equals the total number of
    .sbmd.js files across the production and test specs directories.
    """
    workspace_root = Path(__file__).parent.parent.parent
    production_specs = workspace_root / "core" / "deviceDrivers" / "matter" / "sbmd" / "specs"
    test_specs = workspace_root / "testing" / "resources" / "sbmd-specs"
    expected_count = len(list(production_specs.glob("*.sbmd.js"))) + len(
        list(test_specs.glob("*.sbmd.js"))
    )

    client = default_environment.get_client()
    telemetry = json.loads(client.get_telemetry())
    metrics = telemetry.get("metrics", {})

    count_metric = metrics.get("sbmd.driver.registered.count")
    assert count_metric is not None, (
        "sbmd.driver.registered.count not found in telemetry after startup"
    )
    count_dps = count_metric.get("dataPoints", [])
    assert len(count_dps) == 1, (
        f"Expected 1 datapoint for sbmd.driver.registered.count, got {len(count_dps)}"
    )
    assert count_dps[0]["value"] == expected_count, (
        f"Expected registered driver count {expected_count} "
        f"(= {len(list(production_specs.glob('*.sbmd.js')))} production + "
        f"{len(list(test_specs.glob('*.sbmd.js')))} test drivers), "
        f"got {count_dps[0]['value']}"
    )
