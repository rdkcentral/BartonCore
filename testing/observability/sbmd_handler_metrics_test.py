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
SBMD observability metrics — handler invocation correctness tests.

These tests commission a single light device and execute N write operations
on the isOn resource.  They verify that handler.duration_ms,
handler.heap_delta_bytes, handler.outcome, and heap.used_bytes are recorded
correctly for each invocation.

Each test uses function-scoped fixtures (fresh Barton instance + fresh device)
for a clean metric state.
"""

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

_DRIVER = "light"
_OP_TYPE = "write"
_RESOURCE_ID = "isOn"
_N = 10


def _write_is_on_n_times(client, device, n):
    """Execute n alternating isOn writes, waiting for each update to confirm completion."""
    for i in range(n):
        value = "true" if i % 2 == 0 else "false"
        queue = resource_update_listener(client, _RESOURCE_ID)
        client.write_resource(resource_uri(device, _RESOURCE_ID, endpoint_id=1), value)
        wait_for_resource_value(queue, value, timeout=10)


def test_handler_attributes_are_populated(default_environment, matter_light):
    """
    After read, write, and execute operations, every datapoint in
    handler.duration_ms, handler.heap_delta_bytes, and handler.outcome
    carries non-empty driver, op_type, and resource_id attributes.

    Missing attributes make it impossible to pin a production latency spike
    or failure to a specific driver or resource.
    """
    device = commission_device(default_environment, matter_light, _DRIVER)
    client = default_environment.get_client()

    # One write is enough to generate handler datapoints.
    queue = resource_update_listener(client, _RESOURCE_ID)
    client.write_resource(resource_uri(device, _RESOURCE_ID, endpoint_id=1), "true")
    wait_for_resource_value(queue, "true", timeout=10)

    metrics = get_metrics(client)

    print("\n── Attribute completeness check ─────────────────────────")
    for metric_name in [
        "sbmd.handler.duration_ms",
        "sbmd.handler.heap_delta_bytes",
        "sbmd.handler.outcome",
    ]:
        resource_specific_count = 0
        for dp in metrics.get(metric_name, {}).get("dataPoints", []):
            attrs = dp.get("attributes", {})
            driver_attr   = attrs.get("driver", "")
            op_type_attr  = attrs.get("op_type", "")
            resource_attr = attrs.get("resource_id", "")

            assert driver_attr, (
                f"{metric_name}: datapoint has empty 'driver' attribute: {dp}"
            )
            assert op_type_attr, (
                f"{metric_name}: datapoint has empty 'op_type' attribute: {dp}"
            )

            if not resource_attr:
                # Per spec, resource_id is omitted for attribute/event handler
                # invocations (e.g. Matter subscription attribute reports).
                # These are valid datapoints — skip the resource_id check.
                print(
                    f"  ○  {metric_name}  driver={driver_attr}  "
                    f"op={op_type_attr}  resource=(attribute handler — no resource_id)"
                )
                continue

            resource_specific_count += 1
            print(
                f"  ✓  {metric_name}  driver={driver_attr}  "
                f"op={op_type_attr}  resource={resource_attr}"
            )

        assert resource_specific_count > 0, (
            f"{metric_name}: no resource-specific datapoints found after an isOn write. "
            f"All {len(metrics.get(metric_name, {}).get('dataPoints', []))} datapoint(s) "
            f"appear to be attribute handlers (no resource_id). "
            f"The write handler must emit at least one resource-tagged metric."
        )


def test_handler_outcome_counts_all_invocations(default_environment, matter_light):
    """
    After _N isOn writes, handler.outcome{outcome="success"} equals _N, and
    the sum of ALL outcome labels for the same driver/op_type/resource_id also
    equals _N.

    The second assertion catches the silent-drop case: when
    SbmdResultExecutor::Parse() returns nullopt for a malformed result, no
    outcome is recorded at all — the invocation disappears from the metrics.
    """
    device = commission_device(default_environment, matter_light, _DRIVER)
    client = default_environment.get_client()
    _write_is_on_n_times(client, device, _N)

    metrics = get_metrics(client)

    success_dp = find_datapoint(
        metrics, "sbmd.handler.outcome",
        driver=_DRIVER, op_type=_OP_TYPE, resource_id=_RESOURCE_ID, outcome="success",
    )
    print(f"\n  success outcome datapoint: {success_dp}")
    assert success_dp is not None, (
        f"No handler.outcome{{success}} datapoint for driver={_DRIVER}, "
        f"op_type={_OP_TYPE}, resource_id={_RESOURCE_ID}"
    )
    assert success_dp["value"] == _N, (
        f"Expected outcome success count={_N}, got {success_dp['value']}"
    )

    # Total of ALL outcomes for this driver/resource must also equal _N.
    # Any shortfall means some invocations were silently dropped (no outcome recorded).
    all_outcomes_total = sum(
        dp["value"]
        for dp in metrics["sbmd.handler.outcome"]["dataPoints"]
        if dp.get("attributes", {}).get("driver") == _DRIVER
        and dp.get("attributes", {}).get("op_type") == _OP_TYPE
        and dp.get("attributes", {}).get("resource_id") == _RESOURCE_ID
    )
    print(f"  total outcomes (all labels): {all_outcomes_total}")
    assert all_outcomes_total == _N, (
        f"Sum of all outcome labels is {all_outcomes_total}, expected {_N}. "
        f"Some invocations may have been silently dropped (no outcome recorded)."
    )


def test_handler_duration_in_low_latency_buckets(default_environment, matter_light):
    """
    For _N writes on a simple resource, all measured handler durations fall
    in histogram buckets ≤ 25 ms (indices 0–3).  The worst-case (max) is also
    printed for use as a performance baseline.

    Catches: unexpectedly slow handlers that inflate the p-max toward the
    5000 ms script timeout.

    NOTE: The threshold (≤ 25 ms) is calibrated for dev hardware.  Gateway
    hardware will see higher values; this test is expected to guide (not block)
    on gateways.
    """
    device = commission_device(default_environment, matter_light, _DRIVER)
    client = default_environment.get_client()
    _write_is_on_n_times(client, device, _N)

    metrics = get_metrics(client)
    dur_dp = find_datapoint(
        metrics, "sbmd.handler.duration_ms",
        driver=_DRIVER, op_type=_OP_TYPE, resource_id=_RESOURCE_ID,
    )
    assert dur_dp is not None, (
        f"No handler.duration_ms datapoint for {_DRIVER}/{_OP_TYPE}/{_RESOURCE_ID}"
    )
    assert dur_dp["count"] == _N, (
        f"Expected {_N} duration observations, got {dur_dp['count']}"
    )

    print(f"\n  handler.duration_ms for {_DRIVER}/{_OP_TYPE}/{_RESOURCE_ID}:")
    print(format_histogram(dur_dp, unit="ms"))

    low_count  = bucket_sum(dur_dp, 0, 4)   # buckets covering ≤ 25 ms
    high_count = bucket_sum(dur_dp, 4, 16)  # buckets covering > 25 ms
    print(f"\n  Observations ≤ 25 ms: {low_count}/{_N}")
    print(f"  Observations > 25 ms: {high_count}/{_N}")

    assert low_count == _N, (
        f"{high_count} of {_N} handler.duration_ms observations exceeded 25 ms "
        f"(max={dur_dp.get('max', '?')} ms).  "
        f"Bucket distribution: {dur_dp.get('buckets')}"
    )


def test_handler_heap_delta_bytes_recorded_per_invocation(default_environment, matter_light):
    """
    After _N invocations, handler.heap_delta_bytes has count == _N.

    This confirms JS_GetMemoryUsage() is not silently failing.  The sign of
    the values is not asserted here: positive means net allocation per call,
    negative means GC reclaimed more than was allocated (both are valid).
    """
    device = commission_device(default_environment, matter_light, _DRIVER)
    client = default_environment.get_client()
    _write_is_on_n_times(client, device, _N)

    metrics = get_metrics(client)
    delta_dp = find_datapoint(
        metrics, "sbmd.handler.heap_delta_bytes",
        driver=_DRIVER, op_type=_OP_TYPE, resource_id=_RESOURCE_ID,
    )
    assert delta_dp is not None, (
        f"No handler.heap_delta_bytes datapoint for {_DRIVER}/{_OP_TYPE}/{_RESOURCE_ID}"
    )
    assert delta_dp["count"] == _N, (
        f"Expected {_N} heap_delta observations, got {delta_dp['count']}. "
        f"JS_GetMemoryUsage() may be failing silently."
    )

    print(f"\n  handler.heap_delta_bytes for {_DRIVER}/{_OP_TYPE}/{_RESOURCE_ID}:")
    print(format_histogram(delta_dp, unit=" B"))

    neg_obs = delta_dp.get("buckets", [{}])[0].get("count", 0)
    avg_delta = delta_dp["sum"] / delta_dp["count"]
    print(f"\n  avg delta per call: {avg_delta:+.1f} B")
    print(f"  bucket[0] (≤0 B — GC reclaimed): {neg_obs} observations")
    if neg_obs == 0:
        print(
            "  NOTE: No negative observations.  GC did not run during these calls. "
            "This is not an error, but confirms GC has not been exercised yet."
        )


def test_heap_used_bytes_snapshots_accumulate(default_environment, matter_light):
    """
    After _N invocations, heap.used_bytes has count >= _N + 1.

    The +1 is the initial snapshot taken in MQuickJsRuntime::Initialize().
    Each subsequent handler call records one more snapshot inline
    (MQuickJsRuntime::RecordHeapSnapshot in InvokeHandler).

    Also asserts that no observation exceeds the arena ceiling.
    """
    device = commission_device(default_environment, matter_light, _DRIVER)
    client = default_environment.get_client()
    _write_is_on_n_times(client, device, _N)

    metrics = get_metrics(client)
    arena = metrics["sbmd.js.heap.arena_bytes"]["dataPoints"][0]["value"]
    used_dps = metrics["sbmd.js.heap.used_bytes"]["dataPoints"]
    total_count = sum(dp["count"] for dp in used_dps)

    print(f"\n  used_bytes total observations: {total_count} (expected >= {_N + 1})")
    for dp in used_dps:
        print(format_histogram(dp, unit=" B"))
        assert dp.get("max", 0) <= arena, (
            f"heap.used_bytes max ({dp.get('max')}) exceeds arena ({arena}) — impossible"
        )

    assert total_count >= _N + 1, (
        f"heap.used_bytes has {total_count} observations, expected >= {_N + 1}. "
        f"The inline per-invocation snapshot path may not be executing."
    )
