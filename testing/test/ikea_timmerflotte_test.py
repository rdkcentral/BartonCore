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
    resource_uri,
    wait_for_resource_value,
)

# The matter_ikea_timmerflotte *fixture* is available globally via conftest,
# but the negative test constructs the class directly with custom VID/PID args.
from testing.mocks.devices.matter.matter_ikea_timmerflotte import (
    MatterIkeaTimmerflotte,
)

logger = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.requires_matterjs,
]


# ================================================================
# Positive tests — commission and read resources
# ================================================================


def test_commission_timmerflotte(
    default_environment, matter_ikea_timmerflotte
):
    """Commission an IKEA TIMMERFLOTTE sensor and verify both resources."""
    client = default_environment.get_client()

    # Register listeners before commissioning to catch initial subscription values
    temp_queue = resource_update_listener(client, "temperature")
    hum_queue = resource_update_listener(client, "humidity")

    device = commission_device(
        default_environment,
        matter_ikea_timmerflotte,
        "environmentalSensor",
    )

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

    # Virtual device defaults: temperature = 2550 (25.50°C), humidity = 5000 (50.00%) -> 50 whole percent
    wait_for_resource_value(temp_queue, "2550")
    wait_for_resource_value(hum_queue, "50")


# ================================================================
# Dynamic update tests — verify attribute change reports
# ================================================================


def test_timmerflotte_dynamic_updates(
    default_environment, matter_ikea_timmerflotte
):
    """Update temperature and humidity via side-band and verify Barton receives both."""
    commission_device(
        default_environment,
        matter_ikea_timmerflotte,
        "environmentalSensor",
    )
    client = default_environment.get_client()

    temp_queue = resource_update_listener(client, "temperature")
    hum_queue = resource_update_listener(client, "humidity")

    matter_ikea_timmerflotte.sideband.send(
        "setTemperature", {"value": 3000}
    )
    wait_for_resource_value(temp_queue, "3000")

    matter_ikea_timmerflotte.sideband.send(
        "setHumidity", {"value": 7500}
    )
    wait_for_resource_value(hum_queue, "75")


# ================================================================
# Negative tests — vendor-specific driver priority over standalone
# ================================================================


def test_vendor_specific_driver_wins_over_standalone(
    default_environment, matter_ikea_timmerflotte
):
    """The TIMMERFLOTTE device exposes both Temperature Sensor (0x0302) and
    Humidity Sensor (0x0307) device types, which the standalone temperature-sensor
    and humidity-sensor drivers would each match by device type alone.  Verify that
    the vendor-specific TIMMERFLOTTE driver claims the device first via its
    vendor/product ID match — evidenced by having both temperature and humidity
    resources (a standalone driver would only provide one)."""
    device = commission_device(
        default_environment,
        matter_ikea_timmerflotte,
        "environmentalSensor",
    )
    client = default_environment.get_client()

    # The vendor-specific driver spans both device types, so both resources must exist
    temp_uri = resource_uri(device, "temperature", endpoint_id="1")
    hum_uri = resource_uri(device, "humidity", endpoint_id="1")
    assert (
        client.get_resource_by_uri(temp_uri) is not None
    ), "Expected temperature resource"
    assert client.get_resource_by_uri(hum_uri) is not None, "Expected humidity resource"


def test_timmerflotte_driver_rejects_wrong_vendor_product(
    default_environment,
):
    """A device with matching device types but different vendor/product ID is rejected by the
    IKEA TIMMERFLOTTE driver and falls through to a standalone generic driver.

    GetDriver tries vendor-specific drivers first (vendorId/productId match), then
    generic device-type drivers. When the vendor-specific driver rejects the device,
    a standalone single-device-type driver claims it, resulting in only one sensor
    resource (temperature or humidity) rather than both."""

    # Reuse MatterIkeaTimmerflotte for its device-type composition (temperature +
    # humidity), but override VID/PID to simulate a different manufacturer's device.
    sensor = MatterIkeaTimmerflotte(vendor_id=0x0001, product_id=0x0002)
    sensor.start()

    try:
        default_environment.get_client().commission_device(
            sensor.get_commissioning_code(), 100
        )
        default_environment.wait_for_device_added()

        # A generic driver should have claimed the device
        devices = default_environment.get_client().get_devices_by_device_class(
            "environmentalSensor"
        )
        assert len(devices) >= 1, "Device should be claimed by a generic driver"

        device = devices[-1]
        client = default_environment.get_client()

        # A standalone generic driver only handles one device type, so the device
        # should have at most one sensor resource — not both temperature and humidity
        # as the vendor-specific TIMMERFLOTTE driver would provide.
        temp_uri = resource_uri(device, "temperature", endpoint_id="1")
        hum_uri = resource_uri(device, "humidity", endpoint_id="1")
        has_temp = client.get_resource_by_uri(temp_uri) is not None
        has_hum = client.get_resource_by_uri(hum_uri) is not None

        assert not (has_temp and has_hum), (
            "Device with wrong VID/PID should not have both temperature and humidity "
            "(that would mean the vendor-specific multi-endpoint driver claimed it)"
        )
    finally:
        sensor._cleanup()
