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

import pytest

from testing.environment.base_environment_orchestrator import BaseEnvironmentOrchestrator


class OnvifEnvironmentOrchestrator(BaseEnvironmentOrchestrator):
    """
    Configures a Barton client for ONVIF camera testing: it disables the other subsystems so the
    native ONVIF driver is the only thing managing the "camera" device class, and points ONVIF
    discovery at a unicast mock address (see the ONVIF driver's "onvif.discovery.address" seam).
    """

    def __init__(self, discovery_address):
        self._discovery_address = discovery_address
        super().__init__()

    def _configure_client(self):
        property_provider = self._barton_client_params.get_property_provider()
        property_provider.set_property_string("device.subsystem.disable", "zigbee,matter,thread")
        property_provider.set_property_string("onvif.discovery.address", self._discovery_address)
        self._barton_client_params.set_account_id("1")


@pytest.fixture
def onvif_environment(onvif_camera):
    """Barton client environment wired to discover the mock ONVIF camera by unicast."""
    env = OnvifEnvironmentOrchestrator(discovery_address=onvif_camera.discovery_address)
    env.start_client()
    env.wait_for_client_to_be_ready()
    try:
        yield env
    finally:
        env._cleanup()
