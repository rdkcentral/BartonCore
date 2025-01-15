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

from testing.devices.matter.matter_device import MatterDevice


class MatterLight(MatterDevice):
    """
    Represents a Matter light device inheriting from MatterDevice.

    Attributes:
        vendor_id (int): Vendor ID of the light device. Default is 0.
        product_id (int): Product ID of the light device. Default is 0.
        mdns_port (int): mDNS port for the light device. Default is 5540.
    """

    vendor_id: int
    product_id: int
    mdns_prt: int

    def __init__(
        self,
        vendor_id: int = 0,
        product_id: int = 0,
        mdns_port: int = 5540,
    ):
        super().__init__(
            app_name="chip-lighting-app",
            pipe_name="chip_lighting",
            device_class="light",
            vendor_id=vendor_id,
            product_id=product_id,
            mdns_port=mdns_port,
        )

    def turn_on(self):
        """
        Turn on the light device.
        """
        self.send_message(
            {"Name": "InitialPress", "NewPosition": 0}
        )


@pytest.fixture
def matter_light():
    """
    Fixture to create and manage a MatterLight instance for testing.
    This fixture initializes a MatterLight instance, starts the light process,
    and ensures it is properly stopped/cleaned up after the test.

    Yields:
        Light: An instance of the MatterLight class.
    """
    light_instance = MatterLight(mdns_port=0)
    light_instance.start()
    try:
        yield light_instance
    finally:
        light_instance._cleanup()
