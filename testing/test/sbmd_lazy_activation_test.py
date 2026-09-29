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
Integration tests for lazy SBMD driver activation.

At startup SBMD drivers are loaded inactive (claim-stub only). A driver is
activated when a device first binds to it. These tests verify that behavior
through the activation observability metrics:

  - sbmd.driver.active.count : set-on-change gauge of currently-active drivers
  - sbmd.driver.activation   : counter incremented on each driver activation
"""

import json
import time

import pytest
from testing.utils.barton_utils import commission_device

pytestmark = [
    pytest.mark.requires_matterjs,
]


def _gauge_sum(metrics, name):
    metric = metrics.get(name)

    if metric is None:
        return None

    return sum(dp["value"] for dp in metric.get("dataPoints", []))


def _metrics(client):
    return json.loads(client.get_telemetry()).get("metrics", {})


def test_drivers_inactive_at_startup(default_environment):
    """
    With no devices commissioned, no driver is activated: the active-driver
    gauge is either absent (never recorded) or reports zero.
    """
    client = default_environment.get_client()
    telemetry = json.loads(client.get_telemetry())
    metrics = telemetry.get("metrics", {})

    active = _gauge_sum(metrics, "sbmd.driver.active.count")
    assert active in (None, 0), (
        f"Expected no active SBMD drivers at startup, gauge reported {active}"
    )


def test_commissioning_activates_driver(
    default_environment, matter_temperature_sensor
):
    """
    Commissioning a device that claims an SBMD driver activates that driver:
    the activation counter increments and the active-driver gauge rises above
    zero.
    """
    commission_device(
        default_environment,
        matter_temperature_sensor,
        "environmentalSensor",
    )
    client = default_environment.get_client()
    telemetry = json.loads(client.get_telemetry())
    metrics = telemetry.get("metrics", {})

    activation = _gauge_sum(metrics, "sbmd.driver.activation")
    assert activation is not None and activation >= 1, (
        "sbmd.driver.activation counter should be >= 1 after commissioning"
    )

    active = _gauge_sum(metrics, "sbmd.driver.active.count")
    assert active is not None and active >= 1, (
        "sbmd.driver.active.count gauge should be >= 1 after commissioning"
    )


def test_removal_deactivates_driver(
    default_environment, matter_temperature_sensor
):
    """
    Removing the last device bound to a driver deactivates it: the deactivation
    counter increments and the active-driver gauge returns to zero.
    """
    device = commission_device(
        default_environment,
        matter_temperature_sensor,
        "environmentalSensor",
    )
    client = default_environment.get_client()

    assert (_gauge_sum(_metrics(client), "sbmd.driver.active.count") or 0) >= 1

    assert client.remove_device(device.props.uuid), "remove_device failed"

    # Deactivation runs asynchronously on the Matter thread after fabric teardown.
    active = None
    deactivation = None
    deadline = time.time() + 20

    while time.time() < deadline:
        metrics = _metrics(client)
        active = _gauge_sum(metrics, "sbmd.driver.active.count")
        deactivation = _gauge_sum(metrics, "sbmd.driver.deactivation")

        if (deactivation or 0) >= 1 and (active or 0) == 0:
            break

        time.sleep(0.5)

    assert (deactivation or 0) >= 1, (
        "sbmd.driver.deactivation counter should be >= 1 after last device removed"
    )
    assert (active or 0) == 0, (
        "sbmd.driver.active.count gauge should return to 0 after last device removed"
    )
