# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 Comcast Cable Communications Management, LLC
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
# Created by Kevin Funderburg on 11/14/2024
#

import pytest

from testing.mocks.devices.matter.matter_device import MatterDevice
from testing.mocks.devices.matter.clusters.onoff_cluster import OnOffCluster
from testing.mocks.devices.matter.clusters.levelcontrol_cluster import (
    LevelControlCluster,
)
from testing.mocks.devices.matter.clusters.colorcontrol_cluster import (
    ColorControlCluster,
)

class MatterLight(MatterDevice):
    """
    Represents a Matter light device inheriting from MatterDevice.

    Supported clusters:
        - OnOff (0x0006)
        - LevelControl (0x0008)
        - ColorControl (0x0300)

    Attributes:
        vendor_id (int): Vendor ID of the light device. Default is 0.
        product_id (int): Product ID of the light device. Default is 0.
        mdns_port (int): mDNS port for the light device. Default is 5540.
    """

    vendor_id: int
    product_id: int
    mdns_port: int

    def __init__(
        self,
        vendor_id: int = 0,
        product_id: int = 0,
        mdns_port: int = 5540,
    ):
        super().__init__(
            app_name="chip-lighting-app",
            device_class="light",
            vendor_id=vendor_id,
            product_id=product_id,
            mdns_port=mdns_port,
        )
        # Register clusters supported by this device.
        # Endpoint 1 is the MA-dimmablelight endpoint per lighting-app.zap
        self._register_cluster(OnOffCluster, endpoint_id=1)
        self._register_cluster(LevelControlCluster, endpoint_id=1)
        self._register_cluster(ColorControlCluster, endpoint_id=1)


@pytest.fixture
def matter_light(device_interactor):
    """
    Fixture to create and manage a MatterLight instance for testing.

    This fixture:
    1. Creates and starts a MatterLight instance
    2. Commissions it with chip-tool (for device interactions in tests)
    3. Opens an ECM commissioning window for multi-admin support
    4. Updates the device's commissioning code to match the new window

    After this fixture runs, get_commissioning_code() returns the code for
    the open commissioning window, allowing Barton (or other controllers)
    to also commission the device.

    Yields:
        Light: An instance of the MatterLight class, ready for multi-admin
               commissioning.
    """
    light_instance = MatterLight(mdns_port=0)
    light_instance.start()
    # Commission with chip-tool and open commissioning window for multi-admin.
    # This updates the device's commissioning code to match the new window.
    device_interactor.register_device(light_instance, open_commissioning_window=True)
    try:
        yield light_instance
    finally:
        light_instance._cleanup()
