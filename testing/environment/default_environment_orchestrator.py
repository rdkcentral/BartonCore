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

#
# Created by Kevin Funderburg on 1/14/2025.
#

import shutil
from pathlib import Path

import pytest

from testing.environment.base_environment_orchestrator import BaseEnvironmentOrchestrator


class DefaultEnvironmentOrchestrator(BaseEnvironmentOrchestrator):
    """
    DefaultEnvironmentOrchestrator is responsible for configuring the Barton
    client within the default environment. It extends the BaseEnvironmentOrchestrator
    and implements the necessary methods to configure the Barton client
    """

    def __init__(self, sbmd_test_specs_dir=None):
        super().__init__(sbmd_test_specs_dir=sbmd_test_specs_dir)

    def _configure_client(self):
        """
        Implement the abstract method from the base class to configure the Barton client.
        """
        # Must do after constructing a new BCoreClient
        propertyProvider = self._barton_client_params.get_property_provider()
        propertyProvider.set_property_string("device.subsystem.disable", "zigbee,thread")
        propertyProvider.set_property_string("barton.matter.setupPasscode", "12345678")
        propertyProvider.set_property_string("barton.matter.setupDiscriminator", "1234")
        propertyProvider.set_property_string("barton.matter.vendorName", "Vendor")
        propertyProvider.set_property_string("barton.matter.vid", "0x1234")
        propertyProvider.set_property_string("barton.matter.productName", "Product")
        propertyProvider.set_property_string("barton.matter.pid", "0x5678")
        propertyProvider.set_property_string("barton.matter.partNumber", "PartNumber")
        propertyProvider.set_property_string("barton.matter.productURL", "http://example.com/product")
        propertyProvider.set_property_string("barton.matter.productLabel", "Product Label")
        propertyProvider.set_property_string("barton.matter.serialNumber", "SN123456789")
        propertyProvider.set_property_string("barton.matter.manufacturingDate", "2025-01-01")
        propertyProvider.set_property_string("barton.matter.hardwareVersion", "1")
        propertyProvider.set_property_string("barton.matter.hardwareVersionString", "1.0")

        # FIXME: This should be configurable per test
        self._barton_client_params.set_account_id("1")


@pytest.fixture
def default_environment():
    """
    Fixture to create and manage the default testing environment.
    This fixture initializes an Environment instance, starts the Barton client,
    and ensures it is properly stopped/cleaned up after the test.

    Yields:
        Environment: An instance of the Environment class.
    """
    default_env = DefaultEnvironmentOrchestrator()
    default_env.start_client()
    default_env.wait_for_client_to_be_ready()
    try:
        yield default_env
    finally:
        default_env._cleanup()


@pytest.fixture
def isolated_sbmd_default_environment(tmp_path):
    """
    Default environment whose SBMD test specs are an isolated per-test copy.

    Tests that mutate a spec file on disk (e.g. to exercise re-read validation) use this instead of
    `default_environment` so they never modify the checked-in `testing/resources/sbmd-specs` files.
    This keeps the working tree clean even if the test process is killed mid-run, and lets such tests
    run in parallel without fighting over a shared spec file. Use `get_sbmd_test_specs_dir()` to find
    the copy to patch.
    """
    source_specs = (
        Path(__file__).parent.parent.parent / "testing" / "resources" / "sbmd-specs"
    )
    temp_specs = tmp_path / "sbmd-specs"
    shutil.copytree(source_specs, temp_specs)

    env = DefaultEnvironmentOrchestrator(sbmd_test_specs_dir=temp_specs)
    env.start_client()
    env.wait_for_client_to_be_ready()
    try:
        yield env
    finally:
        env._cleanup()
