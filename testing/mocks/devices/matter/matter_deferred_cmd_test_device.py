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


class MatterDeferredCmdTestDevice(MatterDevice):
    """
    A Matter virtual device backed by DeferredCmdTestDevice.js (Dimmable
    Plug-in Unit, device type 0x010B) used exclusively for deferred-operation
    metrics integration tests.

    This device class is not a production device.  It exists solely to provide
    a driver that uses requestCommand so that sbmd.deferred.* metrics can be
    exercised in integration tests.  See deferred-command-test.sbmd.js for the
    corresponding SBMD driver.

    Supported clusters:
        - OnOff (0x0006) on endpoint 1
        - LevelControl (0x0008) on endpoint 1
    """

    def __init__(
        self,
        vendor_id: int = 0,
        product_id: int = 0,
    ):
        super().__init__(
            device_class="deferredCmdTest",
            matterjs_entry_point="DeferredCmdTestDevice.js",
            vendor_id=vendor_id,
            product_id=product_id,
        )


@pytest.fixture
def matter_deferred_cmd_test_device():
    """
    Fixture to create and manage a MatterDeferredCmdTestDevice instance.

    Creates and starts a matter.js virtual Dimmable Plug-in Unit device whose
    SBMD driver uses requestCommand for the toggle execute resource.  After this
    fixture runs, get_commissioning_code() returns the code for commissioning
    the device with Barton.

    Yields:
        MatterDeferredCmdTestDevice: A started instance ready for commissioning.
    """
    device = MatterDeferredCmdTestDevice()
    device.start()

    try:
        yield device
    finally:
        device._cleanup()
