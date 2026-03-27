# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 Comcast Cable Communications Management, LLC
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

import pytest

from testing.mocks.devices.matter.matter_device import MatterDevice


class MatterDoorLock(MatterDevice):
    """
    Represents a Matter door lock device backed by a matter.js virtual device.

    This device uses a matter.js Node.js process under the hood, but presents
    the same interface as any other MatterDevice subclass. The side-band
    interface allows tests to simulate user-side interactions (e.g., manually
    turning a thumb-lock).

    Supported clusters:
        - DoorLock (0x0101) on endpoint 1
    """

    def __init__(
        self,
        vendor_id: int = 0,
        product_id: int = 0,
    ):
        super().__init__(
            device_class="doorLock",
            matterjs_entry_point="door-lock-device.js",
            vendor_id=vendor_id,
            product_id=product_id,
        )


@pytest.fixture
def matter_door_lock():
    """
    Fixture to create and manage a MatterDoorLock instance for testing.

    This fixture creates and starts a MatterDoorLock instance (matter.js virtual
    device). After this fixture runs, get_commissioning_code() returns the code
    for commissioning the device with Barton.

    Yields:
        MatterDoorLock: A started instance ready for commissioning.
    """
    lock_instance = MatterDoorLock()
    lock_instance.start()

    try:
        yield lock_instance
    finally:
        lock_instance._cleanup()
