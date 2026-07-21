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
Integration tests for SBMD deferred operation metrics.

A test-only SBMD driver (deferred-command-test.sbmd.js) drives a virtual
Dimmable Plug-in Unit device (DeferredCmdTestDevice.js, device type 0x010B).
That driver's "toggle" execute resource uses requestCommand, which exercises the
deferred code path in SpecBasedMatterDeviceDriver. These tests verify that
sbmd.deferred.duration_ms, sbmd.deferred.depth, and sbmd.deferred.in_flight are
populated correctly in the JSON returned by b_core_client_get_telemetry().

NOTE: A dedicated test-only driver is used here rather than modifying an
an existing production driver. Changing sendCommand to requestCommand in a driver
introduces new synchronization on the Matter event loop thread and two competing
timeout deadlines, which is a significant enough change in behavior to make it not
worthwhile just to test deferred metrics. See deferred-command-test.sbmd.js for details.

TODO: A future production SBMD driver that naturally uses requestCommand can
replace this test-only driver for the deferred-metrics integration tests.

The default_environment and matter_deferred_cmd_test_device fixtures are both
function-scoped, so each test starts with a fresh Barton instance and a clean
metric state.
"""

import json

import pytest
from testing.utils.barton_utils import (
    commission_device,
    resource_update_listener,
    resource_uri,
    wait_for_resource_value,
)

pytestmark = [
    pytest.mark.requires_matterjs,
]

_DRIVER_NAME = "Deferred Command Test"
_OP_TYPE = "execute"


def _commission_test_device(default_environment, matter_deferred_cmd_test_device):
    return commission_device(
        default_environment, matter_deferred_cmd_test_device, "deferredCmdTest"
    )


def _find_datapoint(metrics, metric_name, **attrs):
    """
    Return the first histogram or gauge datapoint in *metric_name* whose
    attributes dict contains all key=value pairs in *attrs*.  Returns None if
    the metric is absent or no matching datapoint exists.
    """
    metric = metrics.get(metric_name)

    if metric is None:
        return None

    for dp in metric.get("dataPoints", []):
        dp_attrs = dp.get("attributes", {})

        if all(dp_attrs.get(k) == v for k, v in attrs.items()):
            return dp

    return None


def test_deferred_duration_and_in_flight_metrics(
    default_environment, matter_deferred_cmd_test_device
):
    """
    Execute a toggle command via the requestCommand path and verify:
    - sbmd.deferred.duration_ms records one observation with a positive value
    - sbmd.deferred.in_flight gauge is 0 after the operation completes

    execute_resource is synchronous and blocks until the deferred chain
    finishes, so in_flight is already 0 by the time get_telemetry() is called.
    The gauge's prior value of 1 (while the command was in flight) is proven
    implicitly by the duration_ms observation: a deferred op ran, so in_flight
    was transiently nonzero.
    """

    device = _commission_test_device(
        default_environment, matter_deferred_cmd_test_device
    )
    client = default_environment.get_client()

    # The virtual device starts with isOn=false.  Toggling produces an OnOff
    # attribute report that confirms end-to-end completion.
    resource_updated_queue = resource_update_listener(client, "isOn")
    client.execute_resource(resource_uri(device, "toggle", endpoint_id=1), "", "")
    wait_for_resource_value(resource_updated_queue, "true", timeout=10)

    telemetry = json.loads(client.get_telemetry())
    metrics = telemetry.get("metrics", {})

    # sbmd.deferred.duration_ms — one observation for the toggle execute
    dp = _find_datapoint(
        metrics,
        "sbmd.deferred.duration_ms",
        driver=_DRIVER_NAME,
        op_type=_OP_TYPE,
        resource_id="toggle",
    )
    assert (
        dp is not None
    ), "sbmd.deferred.duration_ms datapoint for Deferred Command Test/execute/toggle not found in telemetry"
    assert (
        dp["count"] == 1
    ), f"Expected 1 duration observation for deferred toggle, got {dp['count']}"
    assert (
        dp["sum"] > 0
    ), f"Expected positive duration for deferred toggle, got {dp['sum']}"

    # sbmd.deferred.in_flight — gauge should be 0 now that the operation completed
    in_flight_metric = metrics.get("sbmd.deferred.in_flight")
    assert in_flight_metric is not None, (
        "sbmd.deferred.in_flight not found in telemetry after deferred operation"
    )
    in_flight_dps = in_flight_metric.get("dataPoints", [])
    assert len(in_flight_dps) == 1, (
        f"Expected 1 in_flight datapoint, got {len(in_flight_dps)}"
    )
    assert in_flight_dps[0]["value"] == 0, (
        f"Expected in_flight=0 after operation completed, got {in_flight_dps[0]['value']}"
    )


def test_deferred_depth_zero_for_single_round_trip(
    default_environment, matter_deferred_cmd_test_device
):
    """
    Execute a toggle command and verify sbmd.deferred.depth records 0.

    Depth 0 means the deferred operation resolved after exactly one
    requestCommand round-trip with no re-arming.  The toggle handler issues a
    single requestCommand; handleToggleResponse returns success() immediately
    without issuing a further deferred terminal.
    """

    device = _commission_test_device(
        default_environment, matter_deferred_cmd_test_device
    )
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "isOn")
    client.execute_resource(resource_uri(device, "toggle", endpoint_id=1), "", "")
    wait_for_resource_value(resource_updated_queue, "true", timeout=10)

    telemetry = json.loads(client.get_telemetry())
    metrics = telemetry.get("metrics", {})

    dp = _find_datapoint(
        metrics,
        "sbmd.deferred.depth",
        driver=_DRIVER_NAME,
        op_type=_OP_TYPE,
        resource_id="toggle",
    )
    assert (
        dp is not None
    ), "sbmd.deferred.depth datapoint for Deferred Command Test/execute/toggle not found in telemetry"
    assert (
        dp["count"] == 1
    ), f"Expected 1 depth observation for single-round-trip toggle, got {dp['count']}"
    assert dp["sum"] == 0, (
        f"Expected depth=0 (single round-trip, no re-arming), got sum={dp['sum']}"
    )
