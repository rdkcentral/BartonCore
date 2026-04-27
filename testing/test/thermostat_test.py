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


def _commission_thermostat(default_environment, matter_thermostat):
    """Helper to commission the thermostat and return the device object."""
    return commission_device(default_environment, matter_thermostat, "thermostat")


def test_commission_thermostat(default_environment, matter_thermostat):
    """Commission a virtual thermostat and verify it appears as a thermostat device."""
    thermostat = _commission_thermostat(default_environment, matter_thermostat)

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


def test_read_initial_temperature(default_environment, matter_thermostat):
    """Verify the initial local temperature is reported correctly after commissioning."""
    thermostat = _commission_thermostat(default_environment, matter_thermostat)
    client = default_environment.get_client()

    resource = client.get_resource_by_uri(
        resource_uri(thermostat, "localTemperature", endpoint_id=1)
    )
    assert resource is not None
    assert resource.props.value == "2100"


def test_write_heat_setpoint(default_environment, matter_thermostat):
    """Write a heating setpoint and verify it is updated."""
    thermostat = _commission_thermostat(default_environment, matter_thermostat)
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "heatSetpoint")

    uri = resource_uri(thermostat, "heatSetpoint", endpoint_id=1)
    assert client.write_resource(uri, "2200")
    wait_for_resource_value(resource_updated_queue, "2200")


def test_write_cool_setpoint(default_environment, matter_thermostat):
    """Write a cooling setpoint and verify it is updated."""
    thermostat = _commission_thermostat(default_environment, matter_thermostat)
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "coolSetpoint")

    uri = resource_uri(thermostat, "coolSetpoint", endpoint_id=1)
    assert client.write_resource(uri, "2500")
    wait_for_resource_value(resource_updated_queue, "2500")


def test_write_system_mode(default_environment, matter_thermostat):
    """Write the system mode and verify it is updated."""
    thermostat = _commission_thermostat(default_environment, matter_thermostat)
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "systemMode")

    uri = resource_uri(thermostat, "systemMode", endpoint_id=1)
    assert client.write_resource(uri, "heat")
    wait_for_resource_value(resource_updated_queue, "heat")


def test_sideband_temperature_change(default_environment, matter_thermostat):
    """Change local temperature via side-band and verify Barton receives the update."""
    _commission_thermostat(default_environment, matter_thermostat)
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "localTemperature")

    result = matter_thermostat.sideband.send("setTemperature", {"temperature": 2350})
    assert result["localTemperature"] == 2350

    wait_for_resource_value(resource_updated_queue, "2350", timeout=10)


def test_no_fan_resources_on_base_thermostat(default_environment, matter_thermostat):
    """Verify fanMode and fanOn resources are NOT present on a thermostat without Fan Control cluster."""
    thermostat = _commission_thermostat(default_environment, matter_thermostat)
    client = default_environment.get_client()

    fan_mode = client.get_resource_by_uri(
        resource_uri(thermostat, "fanMode", endpoint_id=1)
    )
    assert fan_mode is None, "fanMode resource should not exist on a thermostat without Fan Control cluster"

    fan_on = client.get_resource_by_uri(
        resource_uri(thermostat, "fanOn", endpoint_id=1)
    )
    assert fan_on is None, "fanOn resource should not exist on a thermostat without Fan Control cluster"
