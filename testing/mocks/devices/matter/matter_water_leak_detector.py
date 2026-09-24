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

import pytest

from testing.mocks.devices.matter.matter_device import MatterDevice


class MatterWaterLeakDetector(MatterDevice):
    """
    Represents a standalone Matter water leak detector backed by a matter.js
    virtual device with a single endpoint (device type 0x0043, BooleanState
    cluster).

    The side-band interface allows tests to change the BooleanState.StateValue
    attribute, which causes the device to emit a BooleanState.StateChange event.
    """

    def __init__(
        self,
        vendor_id: int = 0,
        product_id: int = 0,
    ):
        super().__init__(
            device_class="sensor",
            matterjs_entry_point="WaterLeakDetectorDevice.js",
            vendor_id=vendor_id,
            product_id=product_id,
        )


@pytest.fixture
def matter_water_leak_detector():
    """
    Fixture to create and manage a MatterWaterLeakDetector instance.

    Yields:
        MatterWaterLeakDetector: Started and ready for commissioning.
    """
    sensor = MatterWaterLeakDetector()
    sensor.start()

    try:
        yield sensor
    finally:
        sensor._cleanup()
