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
LevelControl Cluster implementation for Matter device testing.
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


class LevelControlCluster(MatterCluster):
    """
    Cluster interface for the Level Control cluster (Cluster ID: 0x0008).

    The Level Control cluster provides commands and attributes for controlling
    the level (brightness, speed, etc.) of a device.

    Attributes:
        - CurrentLevel (uint8): Current level value (0-254)
        - RemainingTime (uint16): Time until target level is reached
        - MinLevel (uint8): Minimum supported level
        - MaxLevel (uint8): Maximum supported level
        - OnOffTransitionTime (uint16): Transition time for on/off
        - OnLevel (uint8): Level when turning on
        - DefaultMoveRate (uint8): Default move rate

    Commands:
        - MoveToLevel: Move to a specific level
        - Move: Start moving up or down
        - Step: Step the level up or down
        - Stop: Stop any ongoing level change
        - MoveToLevelWithOnOff: Move to level with on/off behavior
        - MoveWithOnOff: Move with on/off behavior
        - StepWithOnOff: Step with on/off behavior
        - StopWithOnOff: Stop with on/off behavior
    """

    # Matter Cluster ID for Level Control cluster
    CLUSTER_ID: ClusterType = 0x0008

    def __init__(
        self,
        interactor: ChipToolDeviceInteractor,
        node_id: int,
        endpoint_id: int = 1,
    ):
        """
        Initialize the LevelControlCluster.

        Args:
            interactor: The ChipToolDeviceInteractor for executing commands.
            node_id: The node ID of the target device.
            endpoint_id: The endpoint ID where this cluster resides. Defaults to 1.
        """
        super().__init__(
            interactor=interactor,
            node_id=node_id,
            endpoint_id=endpoint_id,
            cluster_name="levelcontrol",
        )

    @staticmethod
    def cluster_type() -> ClusterType:
        """Return the Matter cluster ID."""
        return LevelControlCluster.CLUSTER_ID

    # -------------------------------------------------------------------------
    # Commands
    # -------------------------------------------------------------------------

    def move_to_level(
        self,
        level: int,
        transition_time: int = 0,
        options_mask: int = 0,
        options_override: int = 0,
        timeout: int = 30,
    ) -> DeviceInteractionResult:
        """
        Move to a specific level.

        Args:
            level: Target level (0-254).
            transition_time: Transition time in 1/10ths of a second.
            options_mask: Options mask for command execution.
            options_override: Options override for command execution.
            timeout: Command timeout in seconds.

        Returns:
            DeviceInteractionResult: Result of the command.
        """
        logger.debug(
            f"Moving node {self._node_id} endpoint {self._endpoint_id} to level {level}"
        )
        return self.invoke_command(
            "move-to-level",
            arguments={
                "Level": level,
                "TransitionTime": transition_time,
                "OptionsMask": options_mask,
                "OptionsOverride": options_override,
            },
            timeout=timeout,
        )

    def move_to_level_with_on_off(
        self,
        level: int,
        transition_time: int = 0,
        options_mask: int = 0,
        options_override: int = 0,
        timeout: int = 30,
    ) -> DeviceInteractionResult:
        """
        Move to a specific level with on/off behavior.

        This command will turn the device on if moving to a non-zero level,
        or off if moving to level 0.

        Args:
            level: Target level (0-254).
            transition_time: Transition time in 1/10ths of a second.
            options_mask: Options mask for command execution.
            options_override: Options override for command execution.
            timeout: Command timeout in seconds.

        Returns:
            DeviceInteractionResult: Result of the command.
        """
        return self.invoke_command(
            "move-to-level-with-on-off",
            arguments={
                "Level": level,
                "TransitionTime": transition_time,
                "OptionsMask": options_mask,
                "OptionsOverride": options_override,
            },
            timeout=timeout,
        )

    def step(
        self,
        step_mode: int,
        step_size: int,
        transition_time: int = 0,
        options_mask: int = 0,
        options_override: int = 0,
        timeout: int = 30,
    ) -> DeviceInteractionResult:
        """
        Step the level up or down.

        Args:
            step_mode: 0 = Up, 1 = Down.
            step_size: Amount to step.
            transition_time: Transition time in 1/10ths of a second.
            options_mask: Options mask for command execution.
            options_override: Options override for command execution.
            timeout: Command timeout in seconds.

        Returns:
            DeviceInteractionResult: Result of the command.
        """
        return self.invoke_command(
            "step",
            arguments={
                "StepMode": step_mode,
                "StepSize": step_size,
                "TransitionTime": transition_time,
                "OptionsMask": options_mask,
                "OptionsOverride": options_override,
            },
            timeout=timeout,
        )

    def stop(self, timeout: int = 30) -> DeviceInteractionResult:
        """
        Stop any ongoing level change.

        Args:
            timeout: Command timeout in seconds.

        Returns:
            DeviceInteractionResult: Result of the command.
        """
        return self.invoke_command("stop", timeout=timeout)

    # -------------------------------------------------------------------------
    # Attributes
    # -------------------------------------------------------------------------

    def get_current_level(self, timeout: int = 30) -> int:
        """
        Get the current level value.

        Args:
            timeout: Command timeout in seconds.

        Returns:
            int: The current level (0-254).

        Raises:
            RuntimeError: If the attribute read fails.
        """
        result = self.read_attribute("current-level", timeout=timeout)
        if not result.success:
            raise RuntimeError(f"Failed to read current-level attribute: {result.stderr}")
        return self._parse_level_value(result.stdout)

    def get_min_level(self, timeout: int = 30) -> int:
        """
        Get the minimum supported level.

        Args:
            timeout: Command timeout in seconds.

        Returns:
            int: The minimum level.

        Raises:
            RuntimeError: If the attribute read fails.
        """
        result = self.read_attribute("min-level", timeout=timeout)
        if not result.success:
            raise RuntimeError(f"Failed to read min-level attribute: {result.stderr}")
        return self._parse_level_value(result.stdout)

    def get_max_level(self, timeout: int = 30) -> int:
        """
        Get the maximum supported level.

        Args:
            timeout: Command timeout in seconds.

        Returns:
            int: The maximum level.

        Raises:
            RuntimeError: If the attribute read fails.
        """
        result = self.read_attribute("max-level", timeout=timeout)
        if not result.success:
            raise RuntimeError(f"Failed to read max-level attribute: {result.stderr}")
        return self._parse_level_value(result.stdout)

    def _parse_level_value(self, output: str) -> int:
        """
        Parse a level value from chip-tool output.

        chip-tool outputs attribute values in the [TOO] (ToolOutput) log lines.

        Example chip-tool output:
            [TOO]   CurrentLevel: 128
            [TOO]   MinLevel: 1
            [TOO]   MaxLevel: 254

        Args:
            output: The stdout from chip-tool read command.

        Returns:
            int: The parsed level value.
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

        logger.warning(f"Could not parse level value from output: {output}")
        return 0
