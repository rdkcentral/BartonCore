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


def _commission_door_lock(default_environment, matter_door_lock):
    """Helper to commission the door lock and return the device object."""
    return commission_device(default_environment, matter_door_lock, "doorLock")


def test_commission_door_lock(default_environment, matter_door_lock):
    """Commission a virtual door lock and verify it appears as a doorLock device."""
    lock = _commission_door_lock(default_environment, matter_door_lock)

    assert_device_has_common_resources(
        default_environment.get_client(),
        lock,
        [
            "firmwareVersionString",
            "macAddress",
            "networkType",
            "serialNumber",
        ],
    )


def test_lock_unlock_via_barton(default_environment, matter_door_lock):
    """Lock and unlock the door lock via Barton resource writes, verify via side-band."""
    lock = _commission_door_lock(default_environment, matter_door_lock)
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "locked")

    # Unlock via Barton — execute the "unlock" function resource
    client.execute_resource(resource_uri(lock, "unlock", endpoint_id=1), "", "")
    wait_for_resource_value(resource_updated_queue, "false")

    # Verify via side-band
    state = matter_door_lock.sideband.get_state()
    assert state["lockState"] == "unlocked"

    # Lock via Barton — execute the "lock" function resource
    client.execute_resource(resource_uri(lock, "lock", endpoint_id=1), "", "")
    wait_for_resource_value(resource_updated_queue, "true", timeout=10)

    # Verify via side-band
    state = matter_door_lock.sideband.get_state()
    assert state["lockState"] == "locked"


def test_sideband_unlock_triggers_barton_update(
    default_environment, matter_door_lock
):
    """Simulate a manual unlock via side-band and verify Barton receives the update."""
    _commission_door_lock(default_environment, matter_door_lock)
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "locked")

    # The door lock starts in locked state. Simulate manual unlock via side-band.
    result = matter_door_lock.sideband.send("unlock")
    assert result["lockState"] == "unlocked"

    # Barton should receive a resource update for the locked resource
    wait_for_resource_value(resource_updated_queue, "false", timeout=10)


def test_sideband_lock_triggers_barton_update(
    default_environment, matter_door_lock
):
    """Simulate a manual lock via side-band and verify Barton receives the update."""
    _commission_door_lock(default_environment, matter_door_lock)
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "locked")

    # First unlock the device via side-band so we can test locking
    matter_door_lock.sideband.send("unlock")
    wait_for_resource_value(resource_updated_queue, "false")

    # Simulate manual lock via side-band
    result = matter_door_lock.sideband.send("lock")
    assert result["lockState"] == "locked"

    # Barton should receive a resource update for the locked resource
    wait_for_resource_value(resource_updated_queue, "true")


def test_locked_resource_seeded_on_commission(default_environment, matter_door_lock):
    """Verify that the locked resource is seeded with the correct initial value at commission.

    The virtual door lock starts in the locked state. After commission, Barton should
    populate the locked resource from the LockState attribute cache (via seedFrom mapper)
    without requiring a LockOperation event.
    """
    client = default_environment.get_client()
    resource_updated_queue = resource_update_listener(client, "locked")

    _commission_door_lock(default_environment, matter_door_lock)

    # The device starts locked — the seedFrom mapper should emit "true" at configure time
    wait_for_resource_value(resource_updated_queue, "true", timeout=15)


def test_locked_resource_updated_by_event(default_environment, matter_door_lock):
    """Verify that the locked resource updates when a LockOperation event is received.

    After commission (initial seed), trigger sideband unlock and verify the resource
    transitions to "false" via the LockOperation event. Then lock and verify "true".
    """
    client = default_environment.get_client()
    resource_updated_queue = resource_update_listener(client, "locked")

    _commission_door_lock(default_environment, matter_door_lock)

    # Wait for initial seed (locked state)
    wait_for_resource_value(resource_updated_queue, "true", timeout=15)

    # Trigger sideband unlock — DoorLockDevice.js emits LockOperation (Unlock) event
    result = matter_door_lock.sideband.send("unlock")
    assert result["lockState"] == "unlocked"

    # Barton should receive a resource update driven by the LockOperation event
    wait_for_resource_value(resource_updated_queue, "false", timeout=10)

    # Trigger sideband lock — DoorLockDevice.js emits LockOperation (Lock) event
    result = matter_door_lock.sideband.send("lock")
    assert result["lockState"] == "locked"

    # Barton should receive a resource update driven by the LockOperation event
    wait_for_resource_value(resource_updated_queue, "true", timeout=10)
