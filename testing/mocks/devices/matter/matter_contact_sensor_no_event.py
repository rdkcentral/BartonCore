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


class MatterContactSensorNoEvent(MatterDevice):
    """
    A Matter contact sensor (device type 0x0015) whose BooleanState cluster does
    NOT emit the optional StateChange event. State changes are observable only
    via the StateValue attribute, modeling a spec-conformant 1.5.1 sensor that
    omits the optional event.
    """

    def __init__(
        self,
        vendor_id: int = 0,
        product_id: int = 0,
    ):
        super().__init__(
            device_class="sensor",
            matterjs_entry_point="ContactSensorNoEventDevice.js",
            vendor_id=vendor_id,
            product_id=product_id,
        )


@pytest.fixture
def matter_contact_sensor_no_event():
    """
    Fixture to create and manage a MatterContactSensorNoEvent instance.

    Yields:
        MatterContactSensorNoEvent: Started and ready for commissioning.
    """
    sensor = MatterContactSensorNoEvent()
    sensor.start()

    try:
        yield sensor
    finally:
        sensor._cleanup()
