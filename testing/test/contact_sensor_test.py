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
    commission_device,
    resource_update_listener,
    resource_uri,
    wait_for_resource_value,
)

logger = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.requires_matterjs,
]


def _commission_contact_sensor(default_environment, matter_contact_sensor):
    """Helper to commission the contact sensor and return the device object."""
    return commission_device(default_environment, matter_contact_sensor, "sensor")


def test_commission_contact_sensor(default_environment, matter_contact_sensor):
    """Commission a virtual contact sensor and verify it appears as a sensor device."""
    sensor = _commission_contact_sensor(default_environment, matter_contact_sensor)

    assert sensor is not None


def test_faulted_seeded_on_commission(default_environment, matter_contact_sensor):
    """The sensor starts closed (StateValue=true), so faulted is seeded to false."""
    sensor = _commission_contact_sensor(default_environment, matter_contact_sensor)
    client = default_environment.get_client()

    resource = client.get_resource_by_uri(resource_uri(sensor, "faulted", endpoint_id=1))
    assert resource is not None, "faulted resource not found after commission"
    assert resource.props.value == "false", (
        f"Expected faulted to be seeded to 'false' (closed) at commission, "
        f"got '{resource.props.value}'"
    )


def test_state_change_open_sets_faulted(default_environment, matter_contact_sensor):
    """Opening the contact (StateValue=false) emits StateChange and sets faulted to true."""
    _commission_contact_sensor(default_environment, matter_contact_sensor)
    client = default_environment.get_client()

    faulted_queue = resource_update_listener(client, "faulted")

    # StateValue=false means open (faulted)
    matter_contact_sensor.sideband.send("setStateValue", {"stateValue": False})

    wait_for_resource_value(faulted_queue, "true", timeout=10)


def test_state_change_close_clears_faulted(default_environment, matter_contact_sensor):
    """Closing the contact (StateValue=true) emits StateChange and clears faulted."""
    _commission_contact_sensor(default_environment, matter_contact_sensor)
    client = default_environment.get_client()

    faulted_queue = resource_update_listener(client, "faulted")

    # Open first so there is a transition back to closed to observe.
    matter_contact_sensor.sideband.send("setStateValue", {"stateValue": False})
    wait_for_resource_value(faulted_queue, "true", timeout=10)

    # StateValue=true means closed (not faulted)
    matter_contact_sensor.sideband.send("setStateValue", {"stateValue": True})
    wait_for_resource_value(faulted_queue, "false", timeout=10)
