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


def test_faulted_tracks_via_attribute_when_event_absent(
    default_environment, matter_contact_sensor_no_event
):
    """faulted still tracks state via the StateValue attribute when the device
    does not emit the optional BooleanState.StateChange event.

    BooleanState.StateChange is optional conformance in Matter 1.5.1, so the
    driver must keep its attribute handler. This sensor emits no StateChange
    event; live updates therefore arrive only via the StateValue attribute
    report. If the attribute handler were removed, faulted would never move and
    this test would fail.
    """
    commission_device(default_environment, matter_contact_sensor_no_event, "sensor")
    client = default_environment.get_client()

    faulted_queue = resource_update_listener(client, "faulted")

    # Open the contact (StateValue=false) -> faulted true, via attribute report only.
    matter_contact_sensor_no_event.sideband.send("setStateValue", {"stateValue": False})
    wait_for_resource_value(faulted_queue, "true", timeout=10)

    # Close again (StateValue=true) -> faulted false.
    matter_contact_sensor_no_event.sideband.send("setStateValue", {"stateValue": True})
    wait_for_resource_value(faulted_queue, "false", timeout=10)


def test_faulted_reseeded_on_synchronize(default_environment, matter_contact_sensor):
    """faulted is re-seeded from StateValue when the device reconnects after a
    comm-fail, mirroring the door lock's seed-on-synchronize behavior.

    The sensor starts closed (faulted=false). It goes offline, its StateValue
    changes to open while Barton is in comm-fail, and on reconnect the primed
    report drives synchronizeDevice -> SeedInitialResourceValues, re-seeding
    faulted to "true".
    """
    sensor = _commission_contact_sensor(default_environment, matter_contact_sensor)
    client = default_environment.get_client()

    # Speed up comm-fail detection (see door_lock_test synchronize test for detail).
    default_environment._barton_client_params.get_property_provider().set_property_string(
        "barton.commFail.monitorIntervalSecs", "1"
    )
    metadata_base = f"/{sensor.props.uuid}/m"
    client.write_metadata(f"{metadata_base}/commFailOverrideSeconds", "1")

    commfail_queue = resource_update_listener(client, "communicationFailure")
    matter_contact_sensor.sideband.send("goOffline")
    wait_for_resource_value(commfail_queue, "true", timeout=5)

    reseed_queue = resource_update_listener(client, "faulted")

    # Open the contact (StateValue=false) while offline, then reconnect.
    matter_contact_sensor.sideband.send("comeOnline", {"stateValue": False})
    client.write_metadata(f"{metadata_base}/matterLivenessTimeoutOverrideMs", "1")

    # On resync, faulted is re-seeded to "true" (open).
    wait_for_resource_value(reseed_queue, "true", timeout=15)
