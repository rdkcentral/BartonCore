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


def test_commission_standalone_humidity_sensor(
    default_environment, matter_humidity_sensor
):
    """Commission a standalone humidity sensor and verify the humidity resource."""
    device = commission_device(
        default_environment,
        matter_humidity_sensor,
        "humiditySensor",
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

    # Virtual device defaults: humidity = 5000 (50.00%)
    hum_queue = resource_update_listener(client, "humidity")
    wait_for_resource_value(hum_queue, "5000")


def test_composite_driver_rejects_humidity_only(
    default_environment, matter_humidity_sensor
):
    """A standalone humidity device should NOT be claimed by the composite driver."""
    device = commission_device(
        default_environment,
        matter_humidity_sensor,
        "humiditySensor",
    )
    assert device.props.device_class == "humiditySensor"


def test_standalone_humidity_sensor_update(
    default_environment, matter_humidity_sensor
):
    """Update humidity on standalone sensor via side-band and verify Barton receives it."""
    commission_device(
        default_environment,
        matter_humidity_sensor,
        "humiditySensor",
    )
    client = default_environment.get_client()

    hum_queue = resource_update_listener(client, "humidity")

    matter_humidity_sensor.sideband.send(
        "setHumidity", {"value": 7500}
    )
    wait_for_resource_value(hum_queue, "7500")
