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
# Created by Kevin Funderburg on 1/9/2025.
#

from pathlib import Path
import shutil
import subprocess
import logging
import tempfile
from typing import TYPE_CHECKING

from testing.mocks.devices.base_device import BaseDevice
from testing.helpers.matter import code_generators
from testing.utils import process_utils as putils
from testing.mocks.devices.matter.clusters.matter_cluster import (
    ClusterType,
    MatterCluster,
)

if TYPE_CHECKING:
    from testing.mocks.devices.matter.device_interactor import ChipToolDeviceInteractor

logger = logging.getLogger(__name__)


class MatterDevice(BaseDevice):
    """
    MatterDevice is a base class, inheriting from BaseDevice class, that provides common
    functionality for matter devices. This class is intended to be inherited by other matter
    device classes to share common methods and attributes such as starting and stopping the
    device, sending messages, and setting commissioning codes.

    Attributes:
        _app_name (str): The name of the matter sample application to run.
        _mdns_port (int): The mDNS port for the device (default is 5540).
        _passcode (int): The passcode for the device.
        _discriminator (int): The discriminator for the device.
        _vendor_id (int): The vendor ID for the device (default is 0).
        _product_id (int): The product ID for the device (default is 0).
        _commissioning_code (str): The commissioning code for the device.
        _process (subprocess.Popen): The process running the device application.
    """

    _app_name: str
    _mdns_port: int
    _passcode: int
    _discriminator: int
    _vendor_id: int
    _product_id: int
    _commissioning_code: str
    _process: subprocess.Popen
    _cluster_classes: dict[ClusterType, tuple[type[MatterCluster], int]]
    _interactor: "ChipToolDeviceInteractor"
    _chip_tool_node_id: int
    _kvs_dir: Path

    def __init__(
        self,
        app_name: str,
        device_class: str,
        vendor_id: int = 0,
        product_id: int = 0,
        mdns_port: int = 5540,
    ):
        self._app_name = app_name
        self._device_class = device_class
        self._vendor_id = vendor_id
        self._product_id = product_id
        self._mdns_port = mdns_port
        self._passcode = self._set_passcode()
        self._discriminator = self._set_discriminator()
        self._commissioning_code = self._set_commissioning_code()
        self._process = None
        self._cluster_classes = {}
        self._interactor = None
        self._chip_tool_node_id = None
        # Create a unique temp directory for this device's KVS storage
        self._kvs_dir = Path(tempfile.mkdtemp(prefix=f"matter_device_{device_class}_"))

    def _register_cluster(
        self, cluster_class: type[MatterCluster], endpoint_id: int
    ) -> None:
        """
        Register a cluster class that this device supports on a specific endpoint.

        Subclasses should call this in their __init__ to declare which
        clusters they support and on which endpoints. The endpoint ID should
        match the device's ZAP configuration.

        Args:
            cluster_class: The cluster class to register (e.g., OnOffCluster).
            endpoint_id: The endpoint ID where this cluster resides.
        """
        self._cluster_classes[cluster_class.CLUSTER_ID] = (cluster_class, endpoint_id)

    def _set_interactor(
        self, interactor: "ChipToolDeviceInteractor", node_id: int
    ) -> None:
        """
        Set the interactor and node ID for this device.

        Called by the interactor when the device is registered/commissioned.
        A device may be commissioned to multiple fabrics with different node IDs;
        this stores the node ID for the chip-tool fabric specifically.

        Args:
            interactor: The ChipToolDeviceInteractor for executing commands.
            node_id: The node ID assigned by chip-tool during commissioning.
        """
        self._interactor = interactor
        self._chip_tool_node_id = node_id

    def get_cluster(self, cluster_type: ClusterType) -> MatterCluster:
        """
        Get a cluster interface for this device.

        Args:
            cluster_type: The Matter cluster ID (e.g., OnOffCluster.CLUSTER_ID).

        Returns:
            MatterCluster: An instance of the appropriate cluster class.

        Raises:
            ValueError: If the cluster type is not supported by this device.
            RuntimeError: If the device has not been registered with an interactor.

        Example:
            from testing.mocks.devices.matter.clusters.onoff_cluster import OnOffCluster

            onoff = light.get_cluster(OnOffCluster.CLUSTER_ID)
            onoff.on()
        """
        if self._interactor is None or self._chip_tool_node_id is None:
            raise RuntimeError(
                "Device must be registered with an interactor before getting clusters. "
                "Use device_interactor.register_device(device) first."
            )

        cluster_entry = self._cluster_classes.get(cluster_type)
        if cluster_entry is None:
            raise ValueError(
                f"Cluster type 0x{cluster_type:04X} is not supported by this device. "
                f"Supported clusters: {[f'0x{c:04X}' for c in self._cluster_classes.keys()]}"
            )

        cluster_class, registered_endpoint = cluster_entry

        return cluster_class(
            interactor=self._interactor,
            node_id=self._chip_tool_node_id,
            endpoint_id=registered_endpoint,
        )

    def _set_passcode(self) -> int:
        """
        Generates and sets a new passcode for the device.

        Returns:
            int: The newly generated passcode.
        """
        return code_generators.generate_passcode()

    def _set_discriminator(self) -> int:
        """
        Generates and sets a new discriminator for the device.

        Returns:
            int: The newly generated discriminator.
        """
        return code_generators.generate_discriminator()

    def _set_commissioning_code(self) -> str:
        """
        Generates and sets the commissioning code for the device.
        """
        return code_generators.generate_manual_pairing_code(
            discriminator=self._discriminator, passcode=self._passcode
        )

    def get_commissioning_code(self) -> str:
        """
        Returns the current commissioning code for the device.

        Note: This may change if a commissioning window is opened with ECM,
        which generates a new passcode. Use set_commissioning_code() to update
        after opening an ECM commissioning window.
        """
        return self._commissioning_code

    def set_commissioning_code(self, code: str) -> None:
        """
        Update the commissioning code for this device.

        This should be called when an ECM commissioning window is opened,
        as ECM generates a new passcode and the device will advertise with
        a new commissioning code.

        Args:
            code: The new manual pairing code (e.g., from chip-tool output).
        """
        self._commissioning_code = code

    def start(self):
        """
        Starts the device application as a subprocess given the device parameters.

        Raises:
            subprocess.SubprocessError: If the subprocess fails to start.
        """
        logger.debug(
            f"Starting {self._device_class} with passcode {self._passcode} and discriminator {self._discriminator}"
        )

        try:
            self._process = subprocess.Popen(
                [
                    self._app_name,
                    "--passcode",
                    str(self._passcode),
                    "--discriminator",
                    str(self._discriminator),
                    "--vendor-id",
                    str(self._vendor_id),
                    "--product-id",
                    str(self._product_id),
                    "--secured-device-port",
                    str(self._mdns_port),
                    "--KVS",
                    str(self._kvs_dir / "chip_kvs"),
                ],
                shell=False,
            )

            logger.debug(f"Started {self._device_class} with PID: {self._process.pid}")

        except subprocess.SubprocessError as e:
            logger.debug(f"Failed to start {self._device_class} process: {e}")
            self._process = None

    def stop(self):
        """
        Stops the device application.
        """
        if self._process:
            logger.debug(f"Stopping {self._device_class} with PID: {self._process.pid}")
            putils.terminate_program(self._process)
            self._process = None

    def _cleanup(self):
        """
        Implements the abstract method from BaseDevice.

        Stop the device application and clean up this device's storage directory.
        """
        logger.debug(f"Cleaning up {self._device_class}")

        self.stop()
        # Clean up only this device's KVS directory
        if self._kvs_dir and self._kvs_dir.exists():
            shutil.rmtree(self._kvs_dir, ignore_errors=True)
            logger.debug(f"Removed device storage directory {self._kvs_dir}")
