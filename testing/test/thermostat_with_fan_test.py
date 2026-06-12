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

pytestmark = [
    pytest.mark.requires_matterjs,
]


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


def test_read_initial_temperature(default_environment, matter_thermostat_with_fan):
    """Verify the initial local temperature is reported correctly after commissioning."""
    client = default_environment.get_client()
    resource_updated_queue = resource_update_listener(client, "localTemperature")

    _commission_thermostat_with_fan(default_environment, matter_thermostat_with_fan)

    wait_for_resource_value(resource_updated_queue, "2100")


def test_read_initial_heat_setpoint(default_environment, matter_thermostat_with_fan):
    """Verify the initial heating setpoint is reported correctly after commissioning."""
    client = default_environment.get_client()
    resource_updated_queue = resource_update_listener(client, "heatSetpoint")

    _commission_thermostat_with_fan(default_environment, matter_thermostat_with_fan)

    wait_for_resource_value(resource_updated_queue, "2000")


def test_read_initial_cool_setpoint(default_environment, matter_thermostat_with_fan):
    """Verify the initial cooling setpoint is reported correctly after commissioning."""
    client = default_environment.get_client()
    resource_updated_queue = resource_update_listener(client, "coolSetpoint")

    _commission_thermostat_with_fan(default_environment, matter_thermostat_with_fan)

    wait_for_resource_value(resource_updated_queue, "2600")


def test_read_absolute_heat_limits(default_environment, matter_thermostat_with_fan):
    """Verify the absolute heat setpoint limits are reported correctly."""
    client = default_environment.get_client()

    # Use heatSetpoint (which has emitEvents) as a sync point to confirm the
    # thermostat cluster attributes have all been read from the device.  The
    # absolute limit resources are read-only without emitEvents, so they do not
    # emit update events; we verify them via direct read_resource() after the
    # cluster data has arrived.
    heat_setpoint_queue = resource_update_listener(client, "heatSetpoint")

    thermostat = _commission_thermostat_with_fan(
        default_environment, matter_thermostat_with_fan
    )

    wait_for_resource_value(heat_setpoint_queue, "2000")

    # absMinHeatSetpointLimit=700 -> "0700", absMaxHeatSetpointLimit=3000 -> "3000"
    assert client.read_resource(
        resource_uri(thermostat, "absoluteMinHeatLimit", endpoint_id=1)
    ) == "0700"
    assert client.read_resource(
        resource_uri(thermostat, "absoluteMaxHeatLimit", endpoint_id=1)
    ) == "3000"


def test_read_absolute_cool_limits(default_environment, matter_thermostat_with_fan):
    """Verify the absolute cool setpoint limits are reported correctly."""
    client = default_environment.get_client()

    # Use coolSetpoint (which has emitEvents) as a sync point to confirm the
    # thermostat cluster attributes have all been read from the device.
    cool_setpoint_queue = resource_update_listener(client, "coolSetpoint")

    thermostat = _commission_thermostat_with_fan(
        default_environment, matter_thermostat_with_fan
    )

    wait_for_resource_value(cool_setpoint_queue, "2600")

    # absMinCoolSetpointLimit=1600 -> "1600", absMaxCoolSetpointLimit=3200 -> "3200"
    assert client.read_resource(
        resource_uri(thermostat, "absoluteMinCoolLimit", endpoint_id=1)
    ) == "1600"
    assert client.read_resource(
        resource_uri(thermostat, "absoluteMaxCoolLimit", endpoint_id=1)
    ) == "3200"


def test_read_fan_mode(default_environment, matter_thermostat_with_fan):
    """Verify fanMode resource is readable with initial value 'auto'."""
    client = default_environment.get_client()
    resource_updated_queue = resource_update_listener(client, "fanMode")

    _commission_thermostat_with_fan(
        default_environment, matter_thermostat_with_fan
    )

    wait_for_resource_value(resource_updated_queue, "auto")


def test_write_fan_mode(default_environment, matter_thermostat_with_fan):
    """Write the fan mode and verify it is updated on both Barton and the virtual device."""
    thermostat = _commission_thermostat_with_fan(
        default_environment, matter_thermostat_with_fan
    )
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "fanMode")

    uri = resource_uri(thermostat, "fanMode", endpoint_id=1)
    assert client.write_resource(uri, "on")
    wait_for_resource_value(resource_updated_queue, "on")

    # Verify the device-side fan mode changed
    fan_state = matter_thermostat_with_fan.sideband.send("getFanState", {})
    assert fan_state["fanMode"] == "on"


def test_write_fan_mode_off(default_environment, matter_thermostat_with_fan):
    """Write fan mode to 'off' and verify it is updated on both Barton and the device."""
    thermostat = _commission_thermostat_with_fan(
        default_environment, matter_thermostat_with_fan
    )
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "fanMode")

    uri = resource_uri(thermostat, "fanMode", endpoint_id=1)
    assert client.write_resource(uri, "off")
    wait_for_resource_value(resource_updated_queue, "off")

    fan_state = matter_thermostat_with_fan.sideband.send("getFanState", {})
    assert fan_state["fanMode"] == "off"


def test_write_fan_mode_cycle(default_environment, matter_thermostat_with_fan):
    """Cycle through all writable fan modes and verify each updates the device."""
    thermostat = _commission_thermostat_with_fan(
        default_environment, matter_thermostat_with_fan
    )
    client = default_environment.get_client()
    uri = resource_uri(thermostat, "fanMode", endpoint_id=1)

    for mode in ("off", "on", "auto"):
        queue = resource_update_listener(client, "fanMode")
        assert client.write_resource(uri, mode)
        wait_for_resource_value(queue, mode)

        fan_state = matter_thermostat_with_fan.sideband.send("getFanState", {})
        assert fan_state["fanMode"] == mode


def test_read_fan_on(default_environment, matter_thermostat_with_fan):
    """Verify fanOn resource is readable with initial value 'false' (PercentCurrent is 0)."""

    client = default_environment.get_client()
    resource_updated_queue = resource_update_listener(client, "fanOn")

    _commission_thermostat_with_fan(
        default_environment, matter_thermostat_with_fan
    )

    wait_for_resource_value(resource_updated_queue, "false")


def test_write_heat_setpoint(default_environment, matter_thermostat_with_fan):
    """Write the heating setpoint and verify it is updated on both Barton and the device."""
    thermostat = _commission_thermostat_with_fan(
        default_environment, matter_thermostat_with_fan
    )
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "heatSetpoint")

    uri = resource_uri(thermostat, "heatSetpoint", endpoint_id=1)
    assert client.write_resource(uri, "2200")
    wait_for_resource_value(resource_updated_queue, "2200")
    assert client.read_resource(uri) == "2200"

    # Verify device-side setpoint changed
    state = matter_thermostat_with_fan.sideband.send("getState", {})
    assert state["occupiedHeatingSetpoint"] == 2200


def test_write_cool_setpoint(default_environment, matter_thermostat_with_fan):
    """Write the cooling setpoint and verify it is updated on both Barton and the device."""
    thermostat = _commission_thermostat_with_fan(
        default_environment, matter_thermostat_with_fan
    )
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "coolSetpoint")

    uri = resource_uri(thermostat, "coolSetpoint", endpoint_id=1)
    assert client.write_resource(uri, "2700")
    wait_for_resource_value(resource_updated_queue, "2700")

    # Verify device-side setpoint changed
    state = matter_thermostat_with_fan.sideband.send("getState", {})
    assert state["occupiedCoolingSetpoint"] == 2700


def test_write_system_mode(default_environment, matter_thermostat_with_fan):
    """Write the system mode and verify it is updated on both Barton and the device."""
    thermostat = _commission_thermostat_with_fan(
        default_environment, matter_thermostat_with_fan
    )
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "systemMode")

    uri = resource_uri(thermostat, "systemMode", endpoint_id=1)
    assert client.write_resource(uri, "cool")
    wait_for_resource_value(resource_updated_queue, "cool")

    state = matter_thermostat_with_fan.sideband.send("getState", {})
    assert state["systemMode"] == "cool"


def test_sideband_temperature_change(default_environment, matter_thermostat_with_fan):
    """Change local temperature via side-band and verify Barton receives the update."""
    _commission_thermostat_with_fan(default_environment, matter_thermostat_with_fan)
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "localTemperature")

    result = matter_thermostat_with_fan.sideband.send(
        "setTemperature", {"temperature": 2350}
    )
    assert result["localTemperature"] == 2350

    wait_for_resource_value(resource_updated_queue, "2350", timeout=10)


def test_sideband_fan_mode_change(default_environment, matter_thermostat_with_fan):
    """Change fan mode via side-band and verify Barton receives the update."""
    _commission_thermostat_with_fan(default_environment, matter_thermostat_with_fan)
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "fanMode")

    result = matter_thermostat_with_fan.sideband.send("setFanMode", {"mode": "off"})
    assert result["fanMode"] == "off"

    wait_for_resource_value(resource_updated_queue, "off", timeout=10)


def test_write_heat_setpoint_near_min_limit(
    default_environment, matter_thermostat_with_fan
):
    """Write heating setpoint near the absolute minimum limit and verify it is accepted."""
    thermostat = _commission_thermostat_with_fan(
        default_environment, matter_thermostat_with_fan
    )
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "heatSetpoint")

    # absMinHeatSetpointLimit is 700 (0700); write a value just above it
    uri = resource_uri(thermostat, "heatSetpoint", endpoint_id=1)
    assert client.write_resource(uri, "0800")
    wait_for_resource_value(resource_updated_queue, "0800")

    state = matter_thermostat_with_fan.sideband.send("getState", {})
    assert state["occupiedHeatingSetpoint"] == 800


def test_write_heat_setpoint_near_max_limit(
    default_environment, matter_thermostat_with_fan
):
    """Write heating setpoint near the absolute maximum limit and verify it is accepted."""
    thermostat = _commission_thermostat_with_fan(
        default_environment, matter_thermostat_with_fan
    )
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "heatSetpoint")

    # absMaxHeatSetpointLimit is 3000; write a value just below it
    uri = resource_uri(thermostat, "heatSetpoint", endpoint_id=1)
    assert client.write_resource(uri, "2900")
    wait_for_resource_value(resource_updated_queue, "2900")

    state = matter_thermostat_with_fan.sideband.send("getState", {})
    assert state["occupiedHeatingSetpoint"] == 2900


def test_write_cool_setpoint_near_min_limit(
    default_environment, matter_thermostat_with_fan
):
    """Write cooling setpoint near the absolute minimum limit and verify it is accepted."""
    thermostat = _commission_thermostat_with_fan(
        default_environment, matter_thermostat_with_fan
    )
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "coolSetpoint")

    # absMinCoolSetpointLimit is 1600; write a value just above it
    uri = resource_uri(thermostat, "coolSetpoint", endpoint_id=1)
    assert client.write_resource(uri, "1700")
    wait_for_resource_value(resource_updated_queue, "1700")

    state = matter_thermostat_with_fan.sideband.send("getState", {})
    assert state["occupiedCoolingSetpoint"] == 1700


def test_write_cool_setpoint_near_max_limit(
    default_environment, matter_thermostat_with_fan
):
    """Write cooling setpoint near the absolute maximum limit and verify it is accepted."""
    thermostat = _commission_thermostat_with_fan(
        default_environment, matter_thermostat_with_fan
    )
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "coolSetpoint")

    # absMaxCoolSetpointLimit is 3200; write a value just below it
    uri = resource_uri(thermostat, "coolSetpoint", endpoint_id=1)
    assert client.write_resource(uri, "3100")
    wait_for_resource_value(resource_updated_queue, "3100")

    state = matter_thermostat_with_fan.sideband.send("getState", {})
    assert state["occupiedCoolingSetpoint"] == 3100


def test_write_control_sequence_of_operation(
    default_environment, matter_thermostat_with_fan
):
    """Write controlSequenceOfOperation and verify the enum round-trips correctly."""
    thermostat = _commission_thermostat_with_fan(
        default_environment, matter_thermostat_with_fan
    )
    client = default_environment.get_client()

    uri = resource_uri(thermostat, "controlSequenceOfOperation", endpoint_id=1)

    for seq_value in (
        "coolingOnly",
        "heatingOnly",
        "coolingAndHeatingFourPipes",
    ):
        queue = resource_update_listener(client, "controlSequenceOfOperation")
        assert client.write_resource(uri, seq_value)
        wait_for_resource_value(queue, seq_value)
        assert client.read_resource(uri) == seq_value

