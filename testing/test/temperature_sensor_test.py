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

#
# Created by Kevin Funderburg on 04/13/2026
#


import logging

import pytest
from testing.utils.barton_utils import (
    assert_device_has_common_resources,
    commission_device,
    resource_update_listener,
    wait_for_resource_value,
)

logger = logging.getLogger(__name__)

pytestmark = pytest.mark.requires_matterjs


def test_commission_standalone_temperature_sensor(
    default_environment, matter_temperature_sensor
):
    """Commission a standalone temperature sensor and verify the temperature resource."""
    device = commission_device(
        default_environment,
        matter_temperature_sensor,
        "temperatureSensor",
    )
    client = default_environment.get_client()

    assert_device_has_common_resources(
        client,
        device,
        [
            "firmwareVersionString",
            "macAddress",
            "networkType",
            "serialNumber",
        ],
    )

    # Virtual device defaults: temperature = 2550 (25.50°C)
    temp_queue = resource_update_listener(client, "temperature")
    wait_for_resource_value(temp_queue, "2550")


def test_composite_driver_rejects_temperature_only(
    default_environment, matter_temperature_sensor
):
    """A standalone temperature device should NOT be claimed by the composite driver."""
    device = commission_device(
        default_environment,
        matter_temperature_sensor,
        "temperatureSensor",
    )
    # If it were claimed by the composite driver, the device class would be
    # "temperatureHumiditySensor" — verify it is "temperatureSensor" instead
    assert device.props.device_class == "temperatureSensor"


def test_standalone_temperature_sensor_update(
    default_environment, matter_temperature_sensor
):
    """Update temperature on standalone sensor via side-band and verify Barton receives it."""
    commission_device(
        default_environment,
        matter_temperature_sensor,
        "temperatureSensor",
    )
    client = default_environment.get_client()

    temp_queue = resource_update_listener(client, "temperature")

    matter_temperature_sensor.sideband.send(
        "setTemperature", {"value": 3000}
    )
    wait_for_resource_value(temp_queue, "3000")
