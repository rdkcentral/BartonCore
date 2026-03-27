# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 Comcast Cable Communications Management, LLC
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
import time
from queue import Empty, Queue

import pytest
from gi.repository import BCore

logger = logging.getLogger(__name__)

pytestmark = pytest.mark.requires_matterjs


def _wait_for_resource_value(queue, expected_value, timeout=5):
    """Drain events from the queue until we get the expected value or time out.

    This handles spurious initial subscription events that may arrive before
    the event triggered by the test action.
    """
    deadline = time.monotonic() + timeout

    while True:
        remaining = deadline - time.monotonic()

        if remaining <= 0:
            raise AssertionError(
                f"Timed out waiting for resource value '{expected_value}'"
            )

        try:
            value = queue.get(timeout=remaining)
        except Empty:
            raise AssertionError(
                f"Timed out waiting for resource value '{expected_value}'"
            )

        if value == expected_value:
            return value


def assert_device_has_common_resources(client, device, required_resources):
    """Assert that the device has all required common resources.

    Args:
        device: BCoreDevice object
        required_resources: List of required resource names

    Raises:
        AssertionError: If any required resource is missing
    """
    device_uuid = device.props.uuid

    missing_resources = []
    for resource_name in required_resources:
        uri = f"/{device_uuid}/r/{resource_name}"
        resource = client.get_resource_by_uri(uri)
        if resource is None:
            missing_resources.append(resource_name)

    assert not missing_resources, f"Device is missing resources: {missing_resources}"


def test_commission_light(default_environment, matter_light):
    default_environment.get_client().commission_device(
        matter_light.get_commissioning_code(), 100
    )
    default_environment.wait_for_device_added()
    lights = default_environment.get_client().get_devices_by_device_class("light")
    assert len(lights) == 1

    light = lights[0]
    assert_device_has_common_resources(
        default_environment.get_client(),
        light,
        [
            "firmwareVersionString",
            "macAddress",
            "networkType",
            "serialNumber"
        ]
    )


def test_light_on_off(default_environment, matter_light):
    # TODO: Probably need some kind of helper for repetitious blocks like this
    default_environment.get_client().commission_device(
        matter_light.get_commissioning_code(), 100
    )
    default_environment.wait_for_device_added()
    lights = default_environment.get_client().get_devices_by_device_class("light")
    assert len(lights) == 1

    # Add resource update event listener
    resource_updated_queue = Queue()

    def check_resource_updates(
        client: BCore.Client, resource_updated_event: BCore.ResourceUpdatedEvent
    ) -> None:
        # TODO: Probably need some URI builder/helper
        resource = resource_updated_event.props.resource
        logger.debug(
            f"Resource updated: {resource.props.id} with value {resource.props.value}"
        )
        if resource.props.id == "isOn":
            value = resource.props.value
            if isinstance(value, str):
                normalized = value.strip().lower() not in ("false", "0", "")
            else:
                normalized = bool(value)
            resource_updated_queue.put(normalized)

    default_environment.get_client().connect(
        BCore.CLIENT_SIGNAL_NAME_RESOURCE_UPDATED, check_resource_updates
    )

    is_on = matter_light.sideband.get_state()["onOff"]
    expected_on_off_state = not is_on

    matter_light.sideband.send("toggle")
    _wait_for_resource_value(resource_updated_queue, expected_on_off_state)


def test_light_common_cluster_attribute_report(default_environment, matter_light):
    """Test that Barton correctly handles attribute reports for common clusters from Matter devices.

    This test verifies that when a Matter device sends an attribute report for
    the identify-time attribute, Barton correctly updates the corresponding
    identifySeconds resource. This is verified when the attribute report is sent
    upon subscribing to the device, and also when the attribute report is sent
    after a new value is written to the identify-time attribute.
    """
    resource_updated_queue = Queue()

    def check_resource_updates(
        client: BCore.Client, resource_updated_event: BCore.ResourceUpdatedEvent
    ) -> None:
        resource = resource_updated_event.props.resource
        logger.debug(
            f"Resource updated: {resource.props.id} with value {resource.props.value}"
        )
        if resource.props.id == "identifySeconds":
            resource_updated_queue.put(resource.props.value)

    default_environment.get_client().connect(
        BCore.CLIENT_SIGNAL_NAME_RESOURCE_UPDATED, check_resource_updates
    )

    default_environment.get_client().commission_device(
        matter_light.get_commissioning_code(), 100
    )
    default_environment.wait_for_device_added()
    lights = default_environment.get_client().get_devices_by_device_class("light")
    assert len(lights) == 1

    # Wait for the initial identifySeconds attribute report that arrives as
    # part of the subscription to the device.
    initial_identify_seconds = resource_updated_queue.get(timeout=5)
    assert (
        initial_identify_seconds is not None
    ), "Failed to receive initial identifySeconds attribute report"

    logger.info(f"Initial identify seconds: {initial_identify_seconds}")

    # Parse the string value to get the uint16 value
    try:
        initial_value = int(initial_identify_seconds)
    except ValueError:
        assert False, f"Could not parse initial identifySeconds as int: {initial_identify_seconds}"

    # Use a larger delta to avoid flaky 1-second countdown races where the
    # first post-write report may already be back to 0.
    if initial_value <= 65530:
        new_value = initial_value + 5
    else:
        new_value = initial_value - 5

    # Write the new value to the identify-time attribute on the Matter device
    logger.info(f"Writing identify-time attribute to {new_value}")
    matter_light.sideband.send("setIdentifyTime", {"identifyTime": new_value})

    # Wait for the attribute report triggered by the write above. We may see
    # stale initial values first, and IdentifyTime may tick down before the
    # report is delivered. Accept any first value that changed from initial and
    # is within the expected countdown window.
    deadline = time.monotonic() + 10

    while True:
        remaining = deadline - time.monotonic()

        if remaining <= 0:
            assert False, (
                f"identifySeconds was not updated via attribute report after write. "
                f"Expected a value in [1, {new_value}] different from initial {initial_value}."
            )

        try:
            candidate = resource_updated_queue.get(timeout=remaining)
        except Empty:
            assert False, (
                f"identifySeconds was not updated via attribute report after write. "
                f"Expected a value in [1, {new_value}] different from initial {initial_value}."
            )

        try:
            updated_identify_seconds = int(candidate)
        except (TypeError, ValueError):
            continue

        if (
            updated_identify_seconds != initial_value
            and 1 <= updated_identify_seconds <= new_value
        ):
            break

    logger.info(f"Updated identify seconds: {updated_identify_seconds}")
