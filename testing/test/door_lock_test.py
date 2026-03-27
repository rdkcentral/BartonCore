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
from queue import Queue

import pytest
from gi.repository import BCore
from testing.utils.barton_utils import wait_for_resource_value

logger = logging.getLogger(__name__)

pytestmark = pytest.mark.requires_matterjs


def assert_device_has_common_resources(client, device, required_resources):
    """Assert that the device has all required common resources."""
    device_uuid = device.props.uuid

    missing_resources = []

    for resource_name in required_resources:
        uri = f"/{device_uuid}/r/{resource_name}"
        resource = client.get_resource_by_uri(uri)

        if resource is None:
            missing_resources.append(resource_name)

    assert not missing_resources, f"Device is missing resources: {missing_resources}"


def _commission_door_lock(default_environment, matter_door_lock):
    """Helper to commission the door lock and return the device object."""
    default_environment.get_client().commission_device(
        matter_door_lock.get_commissioning_code(), 100
    )
    default_environment.wait_for_device_added()
    locks = default_environment.get_client().get_devices_by_device_class("doorLock")
    assert len(locks) == 1

    return locks[0]


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
    device_uuid = lock.props.uuid

    # Listen for lock state resource updates
    resource_updated_queue = Queue()

    def on_resource_updated(
        client: BCore.Client, event: BCore.ResourceUpdatedEvent
    ) -> None:
        resource = event.props.resource
        logger.debug(
            f"Resource updated: {resource.props.id} = {resource.props.value}"
        )

        if resource.props.id == "locked":
            resource_updated_queue.put(resource.props.value)

    client.connect(BCore.CLIENT_SIGNAL_NAME_RESOURCE_UPDATED, on_resource_updated)

    # Unlock via Barton — execute the "unlock" function resource
    unlock_uri = f"/{device_uuid}/ep/1/r/unlock"
    client.execute_resource(unlock_uri, "", "")
    wait_for_resource_value(resource_updated_queue, "false")

    # Verify via side-band
    state = matter_door_lock.sideband.get_state()
    assert state["lockState"] == "unlocked"

    # Lock via Barton — execute the "lock" function resource
    lock_uri = f"/{device_uuid}/ep/1/r/lock"
    client.execute_resource(lock_uri, "", "")
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

    # Listen for lock state resource updates
    resource_updated_queue = Queue()

    def on_resource_updated(
        client: BCore.Client, event: BCore.ResourceUpdatedEvent
    ) -> None:
        resource = event.props.resource
        logger.debug(
            f"Resource updated: {resource.props.id} = {resource.props.value}"
        )

        if resource.props.id == "locked":
            resource_updated_queue.put(resource.props.value)

    client.connect(BCore.CLIENT_SIGNAL_NAME_RESOURCE_UPDATED, on_resource_updated)

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

    # Listen for lock state resource updates
    resource_updated_queue = Queue()

    def on_resource_updated(
        client: BCore.Client, event: BCore.ResourceUpdatedEvent
    ) -> None:
        resource = event.props.resource
        logger.debug(
            f"Resource updated: {resource.props.id} = {resource.props.value}"
        )

        if resource.props.id == "locked":
            resource_updated_queue.put(resource.props.value)

    client.connect(BCore.CLIENT_SIGNAL_NAME_RESOURCE_UPDATED, on_resource_updated)

    # First unlock the device via side-band so we can test locking
    matter_door_lock.sideband.send("unlock")
    wait_for_resource_value(resource_updated_queue, "false")

    # Simulate manual lock via side-band
    result = matter_door_lock.sideband.send("lock")
    assert result["lockState"] == "locked"

    # Barton should receive a resource update for the locked resource
    wait_for_resource_value(resource_updated_queue, "true")
