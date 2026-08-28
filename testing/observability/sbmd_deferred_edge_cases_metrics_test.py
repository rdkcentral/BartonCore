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
SBMD observability metrics — deferred operation edge cases tests.

These tests verify the deferred-operation metrics for scenarios beyond
the basic single-round-trip covered by sbmd_deferred_metrics_test.py:

  - rearmToggle   → re-arms once in onResponse   → deferred.depth{sum=1}
  - timeoutToggle → requestCommand with 1 ms TTL → deferred.timeout
  - runawayToggle → always re-arms               → deferred.max_depth + in_flight cleanup

The driver (deferred-command-test.sbmd.js) is a test-only driver loaded from
testing/resources/sbmd-specs/.  The device type is 0x010B.

Key invariant tested in every case:
  deferred.in_flight == 0 after the operation settles.
  A non-zero in_flight after settling means a deferred op leaked.
"""

import time

import pytest

from testing.helpers.sbmd_metrics_helpers import (
    find_datapoint,
    format_histogram,
    get_metrics,
)
from testing.utils.barton_utils import (
    commission_device,
    resource_uri,
)

pytestmark = [pytest.mark.requires_matterjs]

_DRIVER = "deferred-command-test"


def _commission(default_environment, matter_deferred_cmd_test_device):
    return commission_device(
        default_environment, matter_deferred_cmd_test_device, "deferredCmdTest"
    )


def test_deferred_re_arm_records_depth_one(
    default_environment, matter_deferred_cmd_test_device
):
    """
    rearmToggle executes a Toggle command, then re-arms once in onResponse
    to execute a second Toggle command.  The re-arm depth is 1.

    Expected metrics:
      deferred.depth{resource_id="rearmToggle"}: count=1, sum=1
      deferred.in_flight: 0 after both Toggles complete

    The re-arm depth histogram accumulates the re-arm count per top-level
    execute call, not per individual Toggle.  One execute → one observation
    with value 1 (because it re-armed once).
    """
    device = _commission(default_environment, matter_deferred_cmd_test_device)
    client = default_environment.get_client()

    client.execute_resource(resource_uri(device, "rearmToggle", endpoint_id=1), "", "")

    # rearmToggle issues two Toggles, returning isOn to its starting value.  The
    # transient isOn change can be coalesced away by Matter attribute reporting,
    # so waiting on isOn is unreliable; wait on the deferred metrics instead.
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        metrics = get_metrics(client)
        depth_dp = find_datapoint(
            metrics, "sbmd.deferred.depth",
            driver=_DRIVER, op_type="execute", resource_id="rearmToggle",
        )
        in_flight = metrics["sbmd.deferred.in_flight"]["dataPoints"][0]["value"]

        if depth_dp is not None and in_flight == 0:
            break

        time.sleep(0.25)

    print(f"\n  deferred.depth datapoint: {depth_dp}")
    if depth_dp:
        print(format_histogram(depth_dp, unit=" re-arms"))
    print(f"  deferred.in_flight after settle: {in_flight}")

    assert depth_dp is not None, (
        f"No deferred.depth datapoint for {_DRIVER}/execute/rearmToggle. "
        "The re-arm observation was not recorded."
    )
    assert depth_dp["count"] == 1, (
        f"deferred.depth count={depth_dp['count']}, expected 1 (one top-level execute)"
    )
    assert depth_dp["sum"] == 1, (
        f"deferred.depth sum={depth_dp['sum']}, expected 1 (one re-arm)"
    )
    assert in_flight == 0, (
        f"deferred.in_flight is {in_flight} after rearmToggle settled — expected 0. "
        "A deferred operation may have leaked."
    )


def test_deferred_timeout_increments_counter_and_cleans_up(
    default_environment, matter_deferred_cmd_test_device
):
    """
    timeoutToggle issues requestCommand with timeoutMs=1.  The command
    expires before the virtual device can respond, so the deferred
    operation fires the timeout handler and cleans up.

    Expected metrics:
      deferred.timeout{resource_id="timeoutToggle"} >= 1
      deferred.in_flight == 0 after the timeout fires

    This test also confirms that deferred.timeout is a counter (not a
    gauge), so it only ever increases.  A decreasing value would be a bug.
    """
    device = _commission(default_environment, matter_deferred_cmd_test_device)
    client = default_environment.get_client()

    client.execute_resource(
        resource_uri(device, "timeoutToggle", endpoint_id=1), "", ""
    )
    # The timeout fires asynchronously.  Give the runtime time to process it.
    time.sleep(2)

    metrics = get_metrics(client)

    timeout_dp = find_datapoint(
        metrics, "sbmd.deferred.timeout",
        driver=_DRIVER, op_type="execute", resource_id="timeoutToggle",
    )
    in_flight = metrics["sbmd.deferred.in_flight"]["dataPoints"][0]["value"]

    print(f"\n  deferred.timeout datapoint: {timeout_dp}")
    print(f"  deferred.in_flight after settle: {in_flight}")

    assert timeout_dp is not None, (
        "sbmd.deferred.timeout not recorded for timeoutToggle. "
        "The deferred timeout path may not be firing the metric."
    )
    assert timeout_dp["value"] >= 1, (
        f"deferred.timeout value={timeout_dp['value']}, expected >= 1"
    )
    assert in_flight == 0, (
        f"deferred.in_flight is {in_flight} after timeout fired — expected 0. "
        "Timed-out operations must be cleaned up from the in_flight set."
    )


def test_deferred_max_depth_increments_and_cleans_up(
    default_environment, matter_deferred_cmd_test_device
):
    """
    runawayToggle always re-arms in onResponse until MAX_DEFERRAL_DEPTH (10)
    is hit.  At that point SbmdDeferredExecutor should reject the re-arm,
    increment deferred.max_depth, and clean up.

    Expected metrics:
      deferred.max_depth{resource_id="runawayToggle"} >= 1
      deferred.in_flight == 0 after the runaway terminates

    This test deliberately saturates the depth limit.  If the runaway never
    terminates (in_flight != 0 or max_depth stays 0), the circuit-breaker
    logic is broken.
    """
    device = _commission(default_environment, matter_deferred_cmd_test_device)
    client = default_environment.get_client()

    client.execute_resource(
        resource_uri(device, "runawayToggle", endpoint_id=1), "", ""
    )
    # Runaway fires up to 10 re-arms synchronously under the JS mutex.
    # Give the runtime enough time to unwind all of them.
    time.sleep(5)

    metrics = get_metrics(client)

    max_depth_dp = find_datapoint(
        metrics, "sbmd.deferred.max_depth",
        driver=_DRIVER, op_type="execute", resource_id="runawayToggle",
    )
    in_flight = metrics["sbmd.deferred.in_flight"]["dataPoints"][0]["value"]

    print(f"\n  deferred.max_depth datapoint: {max_depth_dp}")
    print(f"  deferred.in_flight after runaway terminated: {in_flight}")

    assert max_depth_dp is not None, (
        "sbmd.deferred.max_depth not recorded for runawayToggle. "
        "The depth-limit circuit-breaker may not be firing the metric."
    )
    assert max_depth_dp["value"] >= 1, (
        f"deferred.max_depth value={max_depth_dp['value']}, expected >= 1"
    )
    assert in_flight == 0, (
        f"deferred.in_flight is {in_flight} after runawayToggle terminated. "
        "Expected 0: once max_depth is hit the operation must be cleaned up. "
        "If non-zero, the depth circuit-breaker is leaking operations."
    )
