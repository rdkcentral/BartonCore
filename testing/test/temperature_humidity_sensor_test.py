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


# ================================================================
# Positive tests — commission and read resources
# ================================================================


def test_commission_composite_temperature_humidity_sensor(
    default_environment, matter_temperature_humidity_sensor
):
    """Commission a composite temperature/humidity sensor and verify both resources."""
    device = commission_device(
        default_environment,
        matter_temperature_humidity_sensor,
        "temperatureHumiditySensor",
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

    # Virtual device defaults: temperature = 2550 (25.50°C), humidity = 5000 (50.00%)
    temp_queue = resource_update_listener(client, "temperature")
    hum_queue = resource_update_listener(client, "humidity")

    wait_for_resource_value(temp_queue, "2550")
    wait_for_resource_value(hum_queue, "5000")


# ================================================================
# Dynamic update tests — verify attribute change reports
# ================================================================


def test_composite_sensor_temperature_update(
    default_environment, matter_temperature_humidity_sensor
):
    """Update temperature on composite sensor via side-band and verify Barton receives it."""
    commission_device(
        default_environment,
        matter_temperature_humidity_sensor,
        "temperatureHumiditySensor",
    )
    client = default_environment.get_client()

    temp_queue = resource_update_listener(client, "temperature")

    matter_temperature_humidity_sensor.sideband.send(
        "setTemperature", {"value": 3000}
    )
    wait_for_resource_value(temp_queue, "3000")


def test_composite_sensor_humidity_update(
    default_environment, matter_temperature_humidity_sensor
):
    """Update humidity on composite sensor via side-band and verify Barton receives it."""
    commission_device(
        default_environment,
        matter_temperature_humidity_sensor,
        "temperatureHumiditySensor",
    )
    client = default_environment.get_client()

    hum_queue = resource_update_listener(client, "humidity")

    matter_temperature_humidity_sensor.sideband.send(
        "setHumidity", {"value": 7500}
    )
    wait_for_resource_value(hum_queue, "7500")


# ================================================================
# Negative tests — composite driver priority over standalone
# ================================================================


def test_standalone_temperature_driver_does_not_claim_composite(
    default_environment, matter_temperature_humidity_sensor
):
    """A composite device should be claimed by the composite driver, not the standalone temperature driver."""
    device = commission_device(
        default_environment,
        matter_temperature_humidity_sensor,
        "temperatureHumiditySensor",
    )
    assert device.props.device_class == "temperatureHumiditySensor"


def test_standalone_humidity_driver_does_not_claim_composite(
    default_environment, matter_temperature_humidity_sensor
):
    """A composite device should be claimed by the composite driver, not the standalone humidity driver."""
    device = commission_device(
        default_environment,
        matter_temperature_humidity_sensor,
        "temperatureHumiditySensor",
    )
    assert device.props.device_class == "temperatureHumiditySensor"
