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


import logging

import pytest
from testing.utils.barton_utils import (
    assert_device_has_common_resources,
    commission_device,
    resource_update_listener,
    resource_uri,
    wait_for_resource_value,
)

logger = logging.getLogger(__name__)

pytestmark = pytest.mark.requires_matterjs


def _commission_thermostat_with_fan(default_environment, matter_thermostat_with_fan):
    """Helper to commission the thermostat with fan and return the device object."""
    return commission_device(
        default_environment, matter_thermostat_with_fan, "thermostat"
    )


def test_commission_thermostat_with_fan(
    default_environment, matter_thermostat_with_fan
):
    """Commission a virtual thermostat with fan control and verify it appears as a thermostat device with fan resources."""
    thermostat = _commission_thermostat_with_fan(
        default_environment, matter_thermostat_with_fan
    )

    assert_device_has_common_resources(
        default_environment.get_client(),
        thermostat,
        [
            "firmwareVersionString",
            "macAddress",
            "networkType",
            "serialNumber",
        ],
    )

    client = default_environment.get_client()

    fan_mode = client.get_resource_by_uri(
        resource_uri(thermostat, "fanMode", endpoint_id=1)
    )
    assert fan_mode is not None, "fanMode resource should exist on thermostat with Fan Control cluster"

    fan_on = client.get_resource_by_uri(
        resource_uri(thermostat, "fanOn", endpoint_id=1)
    )
    assert fan_on is not None, "fanOn resource should exist on thermostat with Fan Control cluster"


def test_read_fan_mode(default_environment, matter_thermostat_with_fan):
    """Verify fanMode resource is readable with initial value 'auto'."""
    client = default_environment.get_client()
    resource_updated_queue = resource_update_listener(client, "fanMode")

    thermostat = _commission_thermostat_with_fan(
        default_environment, matter_thermostat_with_fan
    )

    wait_for_resource_value(resource_updated_queue, "auto")


def test_read_fan_on(default_environment, matter_thermostat_with_fan):
    """Verify fanOn resource is readable with initial value 'true' (fan mode is Auto)."""
    client = default_environment.get_client()
    resource_updated_queue = resource_update_listener(client, "fanOn")

    thermostat = _commission_thermostat_with_fan(
        default_environment, matter_thermostat_with_fan
    )

    wait_for_resource_value(resource_updated_queue, "true")
