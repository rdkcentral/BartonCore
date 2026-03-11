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
from queue import Queue

from gi.repository import BCore

from testing.mocks.devices.matter.clusters.onoff_cluster import OnOffCluster

logger = logging.getLogger(__name__)
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
            resource_updated_queue.put(bool(resource.props.value))

    default_environment.get_client().connect(
        BCore.CLIENT_SIGNAL_NAME_RESOURCE_UPDATED, check_resource_updates
    )

    is_on = matter_light.get_cluster(OnOffCluster.CLUSTER_ID).is_on()
    expected_on_off_state = not is_on

    matter_light.get_cluster(OnOffCluster.CLUSTER_ID).toggle()
    resource_updated_result = resource_updated_queue.get(timeout=5)
    assert (
        resource_updated_result == expected_on_off_state
    ), "Light on/off state did not update as expected"


def test_light_common_cluster_attribute_report(default_environment, matter_light):
    """Test that Barton correctly handles attribute reports for common clusters from Matter devices.

    This test verifies that when a Matter device sends an attribute report for
    the identify-time attribute, Barton correctly updates the corresponding
    identifySeconds resource. This is verified when the attribute report is sent
    upon subscribing to the device, and also when the attribute report is sent
    after a new value is written to the identify-time attribute.
    """
    # Commission the device
    default_environment.get_client().commission_device(
        matter_light.get_commissioning_code(), 100
    )
    default_environment.wait_for_device_added()
    lights = default_environment.get_client().get_devices_by_device_class("light")
    assert len(lights) == 1

    light = lights[0]
    device_uuid = light.props.uuid

    # Read the initial identifySeconds resource from Barton.
    # This should have been set as a result of receiving the first attribute report
    # from subscribing to the device.
    identify_seconds_uri = f"/{device_uuid}/r/identifySeconds"

    def wait_for_condition(client, uri, predicate, timeout=1, poll_interval=0.1):
        """Poll for a resource value until predicate(value) is True or timeout is reached."""
        start = time.time()
        while time.time() - start < timeout:
            value = client.read_resource(uri)
            if predicate(value):
                return value
            time.sleep(poll_interval)
        return None

    # Wait for the initial identifySeconds resource value to be available and parse it as an integer.
    initial_identify_seconds = wait_for_condition(
        default_environment.get_client(),
        identify_seconds_uri,
        lambda v: v is not None,
        timeout=1,
        poll_interval=0.1,
    )
    assert (
        initial_identify_seconds is not None
    ), "Failed to read initial identifySeconds"

    logger.info(f"Initial identify seconds: {initial_identify_seconds}")

    # Parse the string value to get the uint16 value
    try:
        initial_value = int(initial_identify_seconds)
    except ValueError:
        assert (
            False
        ), f"Could not parse initial identifySeconds as int: {initial_identify_seconds}"

    # Calculate a different uint16 value to write (ensure it's within uint16 range: 0-65535)
    new_value = (initial_value + 1) if initial_value < 65535 else 0

    # Write the new value to the identify-time attribute on the Matter device
    # Cluster: identify, Endpoint: 1, Attribute: identify-time
    logger.info(f"Writing identify-time attribute to {new_value}")
    result = matter_light._interactor.write_attribute(
        node_id=matter_light._chip_tool_node_id,
        endpoint_id=1,
        cluster="identify",
        attribute="identify-time",
        value=new_value
    )
    assert result.success, f"Failed to write identify-time attribute: {result.stderr}"

    # Read the identifySeconds resource again from Barton.
    # This should have been updated as a result of receiving the attribute report from the
    # device after its identify-time attribute was written over.
    updated_identify_seconds = wait_for_condition(
        default_environment.get_client(),
        identify_seconds_uri,
        lambda v: (v is not None and v.isdigit() and int(v) == new_value),
        timeout=1,
        poll_interval=0.1,
    )
    assert (
        updated_identify_seconds is not None
    ), f"identifySeconds was not updated correctly via attribute report. Expected {new_value}"

    logger.info(f"Updated identify seconds: {updated_identify_seconds}")
