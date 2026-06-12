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
from pathlib import Path
from threading import Condition

import pytest

from gi.repository import BCore

from testing.environment.base_environment_orchestrator import BaseEnvironmentOrchestrator

logger = logging.getLogger(__name__)


class DescriptorEnvironmentOrchestrator(BaseEnvironmentOrchestrator):
    """
    Orchestrator for device descriptor integration tests.
    Configures allowlist/denylist URL properties to point at a DeviceDescriptorServer
    and disables Matter and Zigbee subsystems for fast startup.
    """

    REPROCESSING_DELAY_SECS = 1

    def __init__(self, device_descriptor_server):
        self._descriptor_server = device_descriptor_server
        self._ready_for_pairing_condition = Condition()
        self._ready_for_pairing = False
        super().__init__()

    @property
    def descriptor_server(self):
        """Access the underlying DeviceDescriptorServer."""
        return self._descriptor_server

    @property
    def local_allowlist_path(self):
        """Path to the locally downloaded AllowList.xml."""
        return Path(self._barton_storage_path) / "AllowList.xml"

    def _configure_client(self):
        propertyProvider = self._barton_client_params.get_property_provider()

        # Disable unrelated subsystems for fast startup
        propertyProvider.set_property_string("device.subsystem.disable", "zigbee,matter,thread")

        # Configure descriptor URLs to point at the mock server
        propertyProvider.set_property_string(
            "deviceDescriptor.whitelist.url.override", self._descriptor_server.allowlist_url
        )

        propertyProvider.set_property_string(
            "deviceDescriptor.blacklist", self._descriptor_server.denylist_url
        )

        # Use fast retry delays for tests
        delay = str(self.REPROCESSING_DELAY_SECS)
        propertyProvider.set_property_string("cpe.dd.exponentialDelaySecs", delay)
        propertyProvider.set_property_string("cpe.dd.incrementalDelaySecs", delay)

        self._barton_client_params.set_account_id("1")

    def start_client(self):
        super().start_client()
        # Notify the service that dependencies are ready, which triggers
        # the descriptor handler initialization (allServicesAvailableNotify)
        self._barton_client.dependencies_ready()

    def get_property_provider(self):
        """Return the property provider for runtime property changes."""
        return self._barton_client_params.get_property_provider()

    def _on_status_changed(self, _object, statusEvent: BCore.StatusEvent):
        super()._on_status_changed(_object, statusEvent)

        if statusEvent.props.reason == BCore.StatusChangedReason.READY_FOR_PAIRING:
            with self._ready_for_pairing_condition:
                self._ready_for_pairing = True
                self._ready_for_pairing_condition.notify_all()

    def wait_for_ready_for_pairing(self, timeout=10):
        with self._ready_for_pairing_condition:
            return self._ready_for_pairing_condition.wait_for(
                lambda: self._ready_for_pairing, timeout=timeout
            )


@pytest.fixture
def descriptor_environment(device_descriptor_server):
    """
    Fixture to create and manage a descriptor-focused testing environment.
    Requires the device_descriptor_server fixture.
    """
    env = DescriptorEnvironmentOrchestrator(device_descriptor_server)
    env.start_client()
    try:
        yield env
    finally:
        env._cleanup()
