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
# Created by Christian Leithner on 2/9/2026.
#

"""
OnOff Cluster implementation for Matter device testing.
"""

from __future__ import annotations

import logging
import re
from typing import TYPE_CHECKING

from testing.mocks.devices.matter.clusters.matter_cluster import ClusterType, MatterCluster

if TYPE_CHECKING:
    from testing.mocks.devices.matter.device_interactor import (
        ChipToolDeviceInteractor,
        DeviceInteractionResult,
    )

logger = logging.getLogger(__name__)


class OnOffCluster(MatterCluster):
    """
    Cluster interface for the On/Off cluster (Cluster ID: 0x0006).

    The On/Off cluster provides commands and attributes for controlling
    the on/off state of a device such as a light or switch.

    Attributes:
        - OnOff (bool): Current on/off state
        - GlobalSceneControl (bool): Scene control attribute
        - OnTime (uint16): Time to remain on after turning on
        - OffWaitTime (uint16): Time to wait before turning off

    Commands:
        - Off: Turn the device off
        - On: Turn the device on
        - Toggle: Toggle the device state
        - OffWithEffect: Turn off with visual effect
        - OnWithRecallGlobalScene: Turn on and recall global scene
        - OnWithTimedOff: Turn on for a specified duration
    """

    # Matter Cluster ID for On/Off cluster
    CLUSTER_ID: ClusterType = 0x0006

    def __init__(
        self,
        interactor: ChipToolDeviceInteractor,
        node_id: int,
        endpoint_id: int = 1,
    ):
        """
        Initialize the OnOffCluster.

        Args:
            interactor: The ChipToolDeviceInteractor for executing commands.
            node_id: The node ID of the target device.
            endpoint_id: The endpoint ID where this cluster resides. Defaults to 1.
        """
        super().__init__(
            interactor=interactor,
            node_id=node_id,
            endpoint_id=endpoint_id,
            cluster_name="onoff",
        )

    @staticmethod
    def cluster_type() -> ClusterType:
        """Return the Matter cluster ID."""
        return OnOffCluster.CLUSTER_ID

    # -------------------------------------------------------------------------
    # Commands
    # -------------------------------------------------------------------------

    def on(self, timeout: int = 30) -> DeviceInteractionResult:
        """
        Turn the device on.

        Args:
            timeout: Command timeout in seconds.

        Returns:
            DeviceInteractionResult: Result of the command.
        """
        logger.debug(f"Turning on node {self._node_id} endpoint {self._endpoint_id}")
        return self.invoke_command("on", timeout=timeout)

    def off(self, timeout: int = 30) -> DeviceInteractionResult:
        """
        Turn the device off.

        Args:
            timeout: Command timeout in seconds.

        Returns:
            DeviceInteractionResult: Result of the command.
        """
        logger.debug(f"Turning off node {self._node_id} endpoint {self._endpoint_id}")
        return self.invoke_command("off", timeout=timeout)

    def toggle(self, timeout: int = 30) -> DeviceInteractionResult:
        """
        Toggle the device state.

        Args:
            timeout: Command timeout in seconds.

        Returns:
            DeviceInteractionResult: Result of the command.
        """
        logger.debug(f"Toggling node {self._node_id} endpoint {self._endpoint_id}")
        return self.invoke_command("toggle", timeout=timeout)

    def on_with_timed_off(
        self,
        on_off_control: int = 0,
        on_time: int = 0,
        off_wait_time: int = 0,
        timeout: int = 30,
    ) -> DeviceInteractionResult:
        """
        Turn on with a timed off setting.

        Args:
            on_off_control: Control field (bit 0: AcceptOnlyWhenOn).
            on_time: Time in 1/10ths of a second to stay on.
            off_wait_time: Time in 1/10ths of a second to wait before allowing off.
            timeout: Command timeout in seconds.

        Returns:
            DeviceInteractionResult: Result of the command.
        """
        return self.invoke_command(
            "on-with-timed-off",
            arguments={
                "OnOffControl": on_off_control,
                "OnTime": on_time,
                "OffWaitTime": off_wait_time,
            },
            timeout=timeout,
        )

    # -------------------------------------------------------------------------
    # Attributes
    # -------------------------------------------------------------------------

    def is_on(self, timeout: int = 30) -> bool:
        """
        Check if the device is currently on.

        Args:
            timeout: Command timeout in seconds.

        Returns:
            bool: True if the device is on, False otherwise.

        Raises:
            RuntimeError: If the attribute read fails.
        """
        result = self.read_attribute("on-off", timeout=timeout)
        if not result.success:
            raise RuntimeError(f"Failed to read on-off attribute: {result.stderr}")

        # Parse the on-off value from chip-tool output
        return self._parse_onoff_value(result.stdout)

    def _parse_onoff_value(self, output: str) -> bool:
        """
        Parse the on-off attribute value from chip-tool output.

        chip-tool outputs attribute values in the [TOO] (ToolOutput) log lines.
        This method parses the boolean on-off state from that output.

        Example chip-tool output:
            [TOO]   OnOff: FALSE
            [TOO]   OnOff: TRUE

        Args:
            output: The stdout from chip-tool read command.

        Returns:
            bool: The parsed on-off state.
        """
        # Patterns ordered by reliability - [TOO] prefix is most reliable
        # chip-tool outputs: "[TOO]   OnOff: FALSE" or "[TOO]   OnOff: TRUE"
        patterns = [
            # Most reliable: [TOO] prefixed output
            r"\[TOO\]\s+OnOff:\s*(TRUE|FALSE)",
            # Fallback patterns for other output formats
            r"Data\s*=\s*(true|false)",
            r"OnOff:\s*(TRUE|FALSE|true|false|1|0)",
            r"value:\s*(TRUE|FALSE|true|false|1|0)",
        ]

        for pattern in patterns:
            match = re.search(pattern, output, re.IGNORECASE)
            if match:
                value = match.group(1).upper()
                return value in ("TRUE", "1")

        # Default to False if we can't parse
        logger.warning(f"Could not parse on-off value from output: {output}")
        return False

    def get_on_time(self, timeout: int = 30) -> int:
        """
        Get the OnTime attribute value.

        Args:
            timeout: Command timeout in seconds.

        Returns:
            int: The OnTime value in 1/10ths of a second.

        Raises:
            RuntimeError: If the attribute read fails.
        """
        result = self.read_attribute("on-time", timeout=timeout)
        if not result.success:
            raise RuntimeError(f"Failed to read on-time attribute: {result.stderr}")
        return self._parse_uint_value(result.stdout)

    def get_off_wait_time(self, timeout: int = 30) -> int:
        """
        Get the OffWaitTime attribute value.

        Args:
            timeout: Command timeout in seconds.

        Returns:
            int: The OffWaitTime value in 1/10ths of a second.

        Raises:
            RuntimeError: If the attribute read fails.
        """
        result = self.read_attribute("off-wait-time", timeout=timeout)
        if not result.success:
            raise RuntimeError(f"Failed to read off-wait-time attribute: {result.stderr}")
        return self._parse_uint_value(result.stdout)

    def _parse_uint_value(self, output: str) -> int:
        """
        Parse an unsigned integer value from chip-tool output.

        chip-tool outputs attribute values in the [TOO] (ToolOutput) log lines.

        Example chip-tool output:
            [TOO]   OnTime: 0
            [TOO]   OffWaitTime: 100

        Args:
            output: The stdout from chip-tool read command.

        Returns:
            int: The parsed integer value.
        """
        # Patterns ordered by reliability - [TOO] prefix is most reliable
        patterns = [
            # Most reliable: [TOO] prefixed output with attribute name
            r"\[TOO\]\s+\w+:\s*(\d+)",
            # Fallback patterns
            r"Data\s*=\s*(\d+)",
            r"value:\s*(\d+)",
        ]

        for pattern in patterns:
            match = re.search(pattern, output)
            if match:
                return int(match.group(1))

        logger.warning(f"Could not parse uint value from output: {output}")
        return 0
