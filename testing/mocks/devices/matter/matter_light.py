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


class MatterLight(MatterDevice):
    """
    Represents a Matter light device backed by a matter.js virtual device.

    This device uses a matter.js Node.js process under the hood, but presents
    the same interface as any other MatterDevice subclass. The side-band
    interface allows tests to simulate user-side interactions (e.g., toggling
    the light from the device itself).
    """

    def __init__(
        self,
        vendor_id: int = 0,
        product_id: int = 0,
    ):
        super().__init__(
            device_class="light",
            matterjs_entry_point="LightDevice.js",
            vendor_id=vendor_id,
            product_id=product_id,
        )


@pytest.fixture
def matter_light():
    """
    Fixture to create and manage a MatterLight instance for testing.

    This fixture creates and starts a MatterLight instance (matter.js virtual
    device). After this fixture runs, get_commissioning_code() returns the code
    for commissioning the device with Barton.

    Yields:
        MatterLight: A started instance ready for commissioning.
    """
    light_instance = MatterLight()
    light_instance.start()

    try:
        yield light_instance
    finally:
        light_instance._cleanup()
