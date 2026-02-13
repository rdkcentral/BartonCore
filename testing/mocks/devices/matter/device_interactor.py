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

#
# Created by Christian Leithner on 2/5/2026.
#

from __future__ import annotations

import json
import logging
import os
import re
import shutil
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING, Any

import pytest

if TYPE_CHECKING:
    from testing.mocks.devices.matter.matter_device import MatterDevice

logger = logging.getLogger(__name__)


@dataclass
class DeviceInteractionResult:
    """
    Represents the result of a device interaction operation.

    Attributes:
        success (bool): Whether the operation executed successfully.
        return_code (int): The return code from the operation.
        stdout (str): Standard output from the operation.
        stderr (str): Standard error from the operation.
        data (Any): Parsed JSON data from the output, if available.
    """

    success: bool
    return_code: int
    stdout: str
    stderr: str
    data: Any = None


@dataclass
class RegisteredDevice:
    """
    Represents a device that has been registered (commissioned) with chip-tool.

    Attributes:
        device (MatterDevice): The original MatterDevice instance.
        node_id (int): The node ID assigned by chip-tool during commissioning.
    """

    device: MatterDevice
    node_id: int


class ChipToolDeviceInteractor:
    """
    ChipToolDeviceInteractor provides a wrapper around chip-tool CLI for interacting
    with Matter devices. It manages a chip-tool process and provides methods that
    map to chip-tool CLI operations for cluster interactions.

    This class handles:
    - Commissioning Matter devices via chip-tool
    - Reading/writing attributes on clusters
    - Invoking cluster commands
    - Subscribing to attribute changes
    - Managing chip-tool storage and configuration

    Attributes:
        _chip_tool_path (str): Path to the chip-tool executable.
        _storage_dir (Path): Temporary directory for chip-tool storage.
        _registered_devices (dict): Mapping of node IDs to RegisteredDevice instances.
        _next_node_id (int): The next node ID to assign during commissioning.
        _commissioner_name (str): Name identifier for the commissioner.
        _paa_trust_store_path (str): Path to PAA trust store for device attestation.
    """

    _chip_tool_path: str
    _storage_dir: Path
    _registered_devices: dict[int, RegisteredDevice]
    _next_node_id: int
    _commissioner_name: str
    _paa_trust_store_path: str | None

    def __init__(
        self,
        chip_tool_path: str | None = None,
        commissioner_name: str = "alpha",
        paa_trust_store_path: str | None = None,
    ):
        """
        Initialize the ChipToolDeviceInteractor.

        Args:
            chip_tool_path: Path to chip-tool executable. If None, searches PATH.
            commissioner_name: Name identifier for the commissioner. Valid values
                are "alpha", "beta", "gamma", or integers >= 4.
            paa_trust_store_path: Path to PAA trust store for device attestation.
        """
        # Initialize storage directory first so cleanup can always run
        self._storage_dir = Path(tempfile.mkdtemp(prefix="chip_tool_storage_"))
        self._registered_devices = {}
        self._next_node_id = 1
        self._commissioner_name = commissioner_name
        self._paa_trust_store_path = paa_trust_store_path

        # Find chip-tool after other attributes are set
        self._chip_tool_path = chip_tool_path or self._find_chip_tool()

        logger.debug(f"ChipToolDeviceInteractor initialized with storage at {self._storage_dir}")

    def _find_chip_tool(self) -> str:
        """
        Find the chip-tool executable in the system PATH.

        Returns:
            str: Path to chip-tool executable.

        Raises:
            FileNotFoundError: If chip-tool is not found.
        """
        chip_tool = shutil.which("chip-tool")
        if chip_tool is None:
            raise FileNotFoundError(
                "chip-tool not found in PATH. Please ensure chip-tool is installed "
                "and available in your PATH, or provide the path explicitly."
            )
        return chip_tool

    def _build_options(self) -> list[str]:
        """
        Build the common options for chip-tool commands.

        Returns:
            list[str]: List of option arguments to append after positional args.
        """
        options = [
            "--storage-directory",
            str(self._storage_dir),
            "--commissioner-name",
            self._commissioner_name,
        ]

        if self._paa_trust_store_path:
            options.extend(["--paa-trust-store-path", self._paa_trust_store_path])

        return options

    def _run_command(
        self,
        args: list[str],
        timeout: int = 5,
    ) -> DeviceInteractionResult:
        """
        Execute a chip-tool command and return the result.

        Args:
            args: Arguments to pass to chip-tool.
            timeout: Command timeout in seconds.

        Returns:
            DeviceInteractionResult: Result of the command execution.
        """
        # chip-tool format: chip-tool <cluster> <command> <positional-args> [--options]
        cmd = [self._chip_tool_path] + args + self._build_options()
        logger.debug(f"Running command: {' '.join(cmd)}")

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=timeout,
            )

            # Try to parse JSON from stdout
            data = None
            try:
                # chip-tool sometimes outputs JSON data
                data = json.loads(result.stdout)
            except json.JSONDecodeError:
                pass

            interaction_result = DeviceInteractionResult(
                success=result.returncode == 0,
                return_code=result.returncode,
                stdout=result.stdout,
                stderr=result.stderr,
                data=data,
            )

            if not interaction_result.success:
                logger.warning(
                    f"Command failed with return code {result.returncode}: "
                    f"stderr={result.stderr}"
                )

            return interaction_result

        except subprocess.TimeoutExpired:
            logger.error(f"Command timed out after {timeout} seconds")
            return DeviceInteractionResult(
                success=False,
                return_code=-1,
                stdout="",
                stderr=f"Command timed out after {timeout} seconds",
            )
        except Exception as e:
            logger.error(f"Failed to execute command: {e}")
            return DeviceInteractionResult(
                success=False,
                return_code=-1,
                stdout="",
                stderr=str(e),
            )

    def register_device(
        self,
        device: MatterDevice,
        node_id: int | None = None,
        timeout: int = 5,
        open_commissioning_window: bool = True,
        commissioning_window_timeout: int = 180,
    ) -> RegisteredDevice:
        """
        Commission a MatterDevice using chip-tool and register it for interactions.

        This method uses chip-tool's pairing command to commission the device
        using its manual pairing code. After successful commissioning, it can
        optionally open a new commissioning window for multi-admin scenarios.

        Args:
            device: The MatterDevice subclass instance to commission.
            node_id: Optional specific node ID to assign. If None, auto-assigns.
            timeout: Commissioning timeout in seconds.
            open_commissioning_window: If True, opens a commissioning window after
                successful commissioning to allow other controllers to commission.
            commissioning_window_timeout: Time in seconds for the commissioning
                window to remain open. Specification section 5.4.2.3 demands a minimum
                of 180 seconds (3 minutes).

        Returns:
            RegisteredDevice: The registered device with its assigned node ID.

        Raises:
            RuntimeError: If commissioning fails.
        """
        if node_id is None:
            node_id = self._next_node_id
            self._next_node_id += 1

        commissioning_code = device.get_commissioning_code()
        logger.info(
            f"Commissioning device with code {commissioning_code} as node {node_id}"
        )

        result = self._run_command(
            [
                "pairing",
                "code",
                str(node_id),
                commissioning_code,
            ],
            timeout=timeout,
        )

        if not result.success:
            raise RuntimeError(
                f"Failed to commission device: {result.stderr}\n"
                f"stdout: {result.stdout}"
            )

        # Initialize the device with interactor reference and node ID
        # so it can create cluster instances
        device._set_interactor(self, node_id)

        registered = RegisteredDevice(device=device, node_id=node_id)
        self._registered_devices[node_id] = registered

        logger.info(f"Successfully commissioned device as node {node_id}")

        # Open commissioning window for multi-admin scenarios
        if open_commissioning_window:
            result, new_code = self._open_commissioning_window(
                node_id=node_id,
                timeout=commissioning_window_timeout,
            )
            # If commissioning window fails to open, raise an exception
            if not result.success:
                error_msg = f"Failed to open commissioning window on node {node_id}: {result.stderr}"
                logger.error(error_msg)
                raise RuntimeError(error_msg)
            # Update the device's commissioning code so get_commissioning_code()
            # returns the correct code for other controllers to use
            if new_code:
                device.set_commissioning_code(new_code)
                logger.info(
                    f"Updated device commissioning code for multi-admin commissioning"
                )

        return registered

    def _open_commissioning_window(
        self,
        node_id: int,
        timeout: int = 5,
        use_enhanced: bool = True,
        iteration: int = 1000,
        discriminator: int | None = None,
    ) -> tuple[DeviceInteractionResult, str | None]:
        """
        Open a commissioning window on an already-commissioned device.

        This allows other controllers to commission the device (multi-admin).

        Note: Most Matter sample apps (including chip-lighting-app) only support
        the Enhanced Commissioning Method (ECM), not the Basic Commissioning
        Method (BCM). ECM uses OpenCommissioningWindow (command 0x00) while BCM
        uses OpenBasicCommissioningWindow (command 0x01). Set use_enhanced=True
        (the default) for compatibility with standard sample apps.

        Args:
            node_id: The node ID of the commissioned device.
            timeout: Time in seconds for the commissioning window to remain open.
            use_enhanced: If True (default), use Enhanced Commissioning Method (ECM)
                which sends OpenCommissioningWindow command (0x00).
                If False, use Basic Commissioning Method (BCM) which sends
                OpenBasicCommissioningWindow command (0x01). Note: BCM is not
                supported by most sample apps.
            iteration: PBKDF iterations for ECM (ignored for BCM).
            discriminator: Discriminator for advertising. For ECM, if None,
                a default discriminator (3840) will be used.

        Returns:
            Tuple of (DeviceInteractionResult, new_commissioning_code).
            The new_commissioning_code is the manual pairing code for the new
            commissioning window (parsed from chip-tool output), or None if
            parsing failed.
        """
        option = 1 if use_enhanced else 0

        # For enhanced commissioning, discriminator is required for advertising
        # For basic commissioning, discriminator is ignored but still required by chip-tool
        if discriminator is None:
            discriminator = 3840  # Default discriminator matching common test setups

        cmd_args = [
            "pairing",
            "open-commissioning-window",
            str(node_id),
            str(option),
            str(timeout),
            str(iteration),
            str(discriminator),
        ]

        logger.info(f"Opening commissioning window on node {node_id} for {timeout}s")
        # Use a longer timeout for the command since it needs to communicate with the device
        result = self._run_command(cmd_args, timeout=5)

        new_commissioning_code = None
        if result.success:
            logger.info(f"Commissioning window opened on node {node_id}")
            # Parse the new manual pairing code from chip-tool output
            # chip-tool outputs: [SVR] Manual pairing code: [12345678901]
            new_commissioning_code = self._parse_manual_pairing_code(result.stdout)
            if new_commissioning_code:
                logger.info(
                    f"New commissioning code for node {node_id}: {new_commissioning_code}"
                )
            else:
                logger.warning(
                    f"Could not parse new commissioning code from chip-tool output for node {node_id}"
                )
        else:
            logger.warning(
                f"Failed to open commissioning window on node {node_id}: {result.stderr}"
            )

        return result, new_commissioning_code

    def _parse_manual_pairing_code(self, output: str) -> str | None:
        """
        Parse the manual pairing code from chip-tool output.

        chip-tool outputs lines like:
            [SVR] Manual pairing code: [12345678901]

        Args:
            output: The stdout from chip-tool command.

        Returns:
            The manual pairing code string, or None if not found.
        """
        # Match pattern like: Manual pairing code: [12345678901]
        match = re.search(r"Manual pairing code:\s*\[([0-9]+)\]", output)
        if match:
            return match.group(1)
        return None

    def unregister_device(self, node_id: int, timeout: int = 5) -> DeviceInteractionResult:
        """
        Unpair and unregister a device.

        Args:
            node_id: The node ID of the device to unpair.
            timeout: Operation timeout in seconds.

        Returns:
            DeviceInteractionResult: Result of the unpair operation.
        """
        result = self._run_command(
            ["pairing", "unpair", str(node_id)],
            timeout=timeout,
        )

        if result.success and node_id in self._registered_devices:
            del self._registered_devices[node_id]
            logger.info(f"Unregistered device with node ID {node_id}")

        return result

    def get_registered_device(self, node_id: int) -> RegisteredDevice | None:
        """
        Get a registered device by its node ID.

        Args:
            node_id: The node ID to look up.

        Returns:
            RegisteredDevice or None if not found.
        """
        return self._registered_devices.get(node_id)

    def get_all_registered_devices(self) -> dict[int, RegisteredDevice]:
        """
        Get all registered devices.

        Returns:
            dict: Mapping of node IDs to RegisteredDevice instances.
        """
        return self._registered_devices.copy()

    # -------------------------------------------------------------------------
    # Cluster Interaction Methods
    # -------------------------------------------------------------------------

    def read_attribute(
        self,
        node_id: int,
        endpoint_id: int,
        cluster: str,
        attribute: str,
        timeout: int = 5,
    ) -> DeviceInteractionResult:
        """
        Read an attribute from a cluster on a device.

        Args:
            node_id: The node ID of the target device.
            endpoint_id: The endpoint ID on the device.
            cluster: The cluster name (e.g., "onoff", "levelcontrol").
            attribute: The attribute name to read (e.g., "on-off", "current-level").
            timeout: Operation timeout in seconds.

        Returns:
            DeviceInteractionResult: Result containing the attribute value.
        """
        return self._run_command(
            [cluster, "read", attribute, str(node_id), str(endpoint_id)],
            timeout=timeout,
        )

    def write_attribute(
        self,
        node_id: int,
        endpoint_id: int,
        cluster: str,
        attribute: str,
        value: Any,
        timeout: int = 5,
    ) -> DeviceInteractionResult:
        """
        Write a value to an attribute on a cluster.

        Args:
            node_id: The node ID of the target device.
            endpoint_id: The endpoint ID on the device.
            cluster: The cluster name.
            attribute: The attribute name to write.
            value: The value to write.
            timeout: Operation timeout in seconds.

        Returns:
            DeviceInteractionResult: Result of the write operation.
        """
        return self._run_command(
            [cluster, "write", attribute, str(value), str(node_id), str(endpoint_id)],
            timeout=timeout,
        )

    def invoke_command(
        self,
        node_id: int,
        endpoint_id: int,
        cluster: str,
        command: str,
        arguments: dict[str, Any] | None = None,
        timeout: int = 5,
    ) -> DeviceInteractionResult:
        """
        Invoke a command on a cluster.

        Args:
            node_id: The node ID of the target device.
            endpoint_id: The endpoint ID on the device.
            cluster: The cluster name.
            command: The command name to invoke (e.g., "on", "off", "toggle").
            arguments: Optional dictionary of command arguments.
            timeout: Operation timeout in seconds.

        Returns:
            DeviceInteractionResult: Result of the command invocation.
        """
        # chip-tool format: <cluster> <command> <destination-id> <endpoint-id> [--options]
        cmd_args = [cluster, command, str(node_id), str(endpoint_id)]

        # Add any command-specific arguments as options
        if arguments:
            for key, val in arguments.items():
                cmd_args.extend([f"--{key}", str(val)])

        return self._run_command(cmd_args, timeout=timeout)

    # -------------------------------------------------------------------------
    # Utility Methods
    # -------------------------------------------------------------------------

    def discover_commissionable(self, timeout: int = 5) -> DeviceInteractionResult:
        """
        Discover commissionable devices on the network.

        Args:
            timeout: Discovery timeout in seconds.

        Returns:
            DeviceInteractionResult: Result containing discovered devices.
        """
        return self._run_command(
            ["discover", "commissionables"],
            timeout=timeout,
        )

    def resolve_node(
        self, node_id: int, fabric_id: int = 1, timeout: int = 5
    ) -> DeviceInteractionResult:
        """
        Resolve a node's network address.

        Args:
            node_id: The node ID to resolve.
            fabric_id: The fabric ID the node belongs to.
            timeout: Operation timeout in seconds.

        Returns:
            DeviceInteractionResult: Result containing the node's address.
        """
        return self._run_command(
            ["discover", "resolve", str(node_id), str(fabric_id)],
            timeout=timeout,
        )

    def cleanup(self):
        """
        Clean up resources including storage directory. Assume all devices are stopped processes at this point.
        """
        logger.debug("Cleaning up ChipToolDeviceInteractor")

        # Remove storage directory
        if hasattr(self, "_storage_dir"):
            try:
                if self._storage_dir.exists():
                    shutil.rmtree(self._storage_dir)
                    logger.debug(f"Removed storage directory {self._storage_dir}")
            except Exception as e:
                logger.warning(f"Failed to remove storage directory {self._storage_dir}: {e}")


@pytest.fixture
def device_interactor():
    """
    Pytest fixture to create and manage a ChipToolDeviceInteractor instance.

    This fixture initializes a ChipToolDeviceInteractor, which provides methods
    for interacting with Matter devices. The interactor is automatically cleaned
    up after the test completes.

    Yields:
        ChipToolDeviceInteractor: An instance configured for testing.
    """
    interactor = ChipToolDeviceInteractor()
    try:
        yield interactor
    finally:
        interactor.cleanup()
