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

import pytest

from testing.environment.base_environment_orchestrator import BaseEnvironmentOrchestrator


class DefaultEnvironmentOrchestrator(BaseEnvironmentOrchestrator):
    """
    DefaultEnvironmentOrchestrator is responsible for configuring the Barton
    client within the default environment. It extends the BaseEnvironmentOrchestrator
    and implements the necessary methods to configure the Barton client
    """

    def __init__(self):
        super().__init__()

    def _configure_client(self):
        """
        Implement the abstract method from the base class to configure the Barton client.
        """
        # Must do after constructing a new BDeviceServiceClient
        propertyProvider = self._barton_client_params.get_property_provider()
        propertyProvider.set_property_string(
            "device.subsystem.disable", "zigbee,thread"
        )


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
