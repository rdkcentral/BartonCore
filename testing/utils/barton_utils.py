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
import time
from queue import Empty, Queue

from gi.repository import BCore

logger = logging.getLogger(__name__)


def commission_device(environment, device, device_class):
    """Commission a device and return the single device of the expected class.

    Commissions the device via the environment's client, waits for the
    device-added event, then asserts exactly one device of the given class
    exists and returns it.
    """
    environment.get_client().commission_device(device.get_commissioning_code(), 100)
    environment.wait_for_device_added()
    devices = environment.get_client().get_devices_by_device_class(device_class)
    assert (
        len(devices) == 1
    ), f"Expected 1 '{device_class}' device, found {len(devices)}"

    return devices[0]


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


def resource_update_listener(client, resource_id, transform=None):
    """Connect a resource-update listener that captures values for a single resource.

    Returns a Queue that receives the resource value each time the specified
    resource is updated.  An optional *transform* callable can normalize the
    value before it is enqueued.
    """
    queue = Queue()

    def _on_resource_updated(_client, event):
        resource = event.props.resource

        if resource.props.id == resource_id:
            value = resource.props.value

            if transform is not None:
                value = transform(value)

            queue.put(value)

    client.connect(BCore.CLIENT_SIGNAL_NAME_RESOURCE_UPDATED, _on_resource_updated)

    return queue


def wait_for_resource_value(queue, expected_value, timeout=10):
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
