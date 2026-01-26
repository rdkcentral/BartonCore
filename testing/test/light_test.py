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
