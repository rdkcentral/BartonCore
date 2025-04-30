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
# Created by Kevin Funderburg on 1/10/2025.
#


from abc import ABC, abstractmethod
from pathlib import Path
from threading import Condition
import shutil

import gi
gi.require_version("BDeviceService", "1.0")
from gi.repository import BDeviceService

from testing.credentials import network_credentials_provider

class BaseEnvironmentOrchestrator(ABC):
    """
    The BaseEnvironmentOrchestrator class is responsible for setting up and managing the
    environment required for testing Barton devices. It initializes the Barton client,
    handles synchronization primitives, and provides various timing methods to ensure
    the environment is ready for testing. Further configuration can be done by derived
    classes.

    Attributes:
        _barton_client (BDeviceService.Client): The client used to interact with the
            Barton device service.
        _barton_client_params (BDeviceService.InitializeParamsContainer): The parameters
            used to initialize and configure Barton client.
        _ready_for_devices_condition (Condition): Condition variable to signal when
            the environment is ready for devices.
        _ready_to_commission (bool): Flag indicating if the environment is ready to
            commission devices.
        _device_added_condition (Condition): Condition variable to signal when a
            device has been added.
        _commissioned_device (bool): Flag indicating if a device has been commissioned.
        _barton_storage_path (str): Path to the storage directory for Barton data.
        _matter_storage_path (str): Path to the storage directory for Matter data.
    """

    _barton_client: BDeviceService.Client
    _barton_client_params: BDeviceService.InitializeParamsContainer
    _ready_for_devices_condition: Condition
    _ready_to_commission: bool
    _device_added_condition: Condition
    _commissioned_device: bool
    _barton_storage_path: str
    _matter_storage_path: str

    def __init__(self):
        """
        Set up the necessary synchronization primitives and initialize the Barton client.
        """
        self._ready_for_devices_condition = Condition()
        self._ready_to_commission = False
        self._device_added_condition = Condition()
        self._commissioned_device = False

        # Must match what's compiled with barton matter sdk
        self._barton_storage_path = str(Path.home()) + "/.brtn-ds"
        self._matter_storage_path = self._barton_storage_path + "/matter"
        self._init_client()
        self._configure_client()

    def _init_client(self):
        """
        Initialize the Barton client.
        """
        self._barton_client_params = BDeviceService.InitializeParamsContainer()
        self._barton_client_params.set_storage_dir(self._barton_storage_path)
        self._barton_client_params.set_matter_storage_dir(self._matter_storage_path)
        self._barton_client_params.set_matter_attestation_trust_store_dir(self._matter_storage_path)
        net_creds_provider = (
            network_credentials_provider.ExampleNetworkCredentialsProvider()
        )
        self._barton_client_params.set_network_credentials_provider(net_creds_provider)

        # In pygobject, gobject properties map directly to object attributes where
        # - is replaced by _
        self._barton_client = BDeviceService.Client(initialize_params=self._barton_client_params)

        self._barton_client.connect(
            BDeviceService.CLIENT_SIGNAL_NAME_STATUS_CHANGED, self._on_status_changed
        )
        self._barton_client.connect(
            BDeviceService.CLIENT_SIGNAL_NAME_DEVICE_ADDED, self._on_device_added
        )

    @abstractmethod
    def _configure_client(self):
        """
        Configures the Barton client with the necessary parameters and configurations.
        This method should be implemented by the derived class to provide custom
        configurations per environment.
        """
        pass

    def get_client(self) -> BDeviceService.Client:
        """
        Returns the Barton client instance.

        Returns:
            BDeviceService.Client: The Barton client instance.
        """
        return self._barton_client

    def start_client(self):
        """
        Starts the Barton client.
        """
        self._barton_client.start()

    def _on_status_changed(self, _object, statusEvent: BDeviceService.StatusEvent):
        """
        Handles the status change event for a device service when the status of the device
        service changes.

        Args:
            _object: The object that triggered the status change event.
            statusEvent (BDeviceService.StatusEvent): The event containing the status change details.
        """
        if (
            statusEvent.props.reason
            == BDeviceService.StatusChangedReason.READY_FOR_DEVICE_OPERATION
        ):
            with self._ready_for_devices_condition:
                self._ready_to_commission = True
                self._ready_for_devices_condition.notify_all()

    def wait_for_client_to_be_ready(self, timeout=5):
        """
        Waits for the Barton client to be ready before proceeding with the test.
        """
        with self._ready_for_devices_condition:
            if not self._ready_to_commission:
                self._ready_for_devices_condition.wait(timeout=timeout)

        assert (
            self._ready_to_commission
        ), "Timed out waiting for Barton to be ready for commissioning"

    def _on_device_added(self, _object, device: BDeviceService.DeviceAddedEvent):
        """
        Callback function that is triggered when a device is added.

        Args:
            _object: The object that triggered the event.
            device (BDeviceService.DeviceAddedEvent): The event object containing
                details about the added device.
        """
        with self._device_added_condition:
            self._commissioned_device = True
            self._device_added_condition.notify_all()

    def wait_for_device_added(self, timeout=5):
        """
        Waits for a device to be added and commissioned within a specified timeout.

        Args:
            timeout (int, optional): The maximum time to wait for the device to be
                commissioned, in seconds. Defaults to 5.
        Raises:
            AssertionError: If the device is not commissioned within the specified
            timeout.
        """
        with self._device_added_condition:
            if not self._commissioned_device:
                self._device_added_condition.wait(timeout=timeout)

        assert (
            self._commissioned_device
        ), "Timeout waiting for device to be commissioned"

    def _cleanup(self):
        """
        Cleans up the environment after testing.
        """
        self._barton_client.stop()
        self._barton_client = None
        shutil.rmtree(self._barton_storage_path, ignore_errors=True)

        self._ready_for_devices_condition = None
        self._device_added_condition = None
        self._ready_to_commission = False
        self._commissioned_device = False
