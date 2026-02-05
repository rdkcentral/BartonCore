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
ColorControl Cluster implementation for Matter device testing.
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


class ColorControlCluster(MatterCluster):
    """
    Cluster interface for the Color Control cluster (Cluster ID: 0x0300).

    The Color Control cluster provides commands and attributes for controlling
    the color of a device such as a color-capable light.

    Supports multiple color modes:
        - Hue/Saturation
        - XY (CIE 1931)
        - Color Temperature
        - Enhanced Hue/Saturation
    """

    # Matter Cluster ID for Color Control cluster
    CLUSTER_ID: ClusterType = 0x0300

    def __init__(
        self,
        interactor: ChipToolDeviceInteractor,
        node_id: int,
        endpoint_id: int = 1,
    ):
        """
        Initialize the ColorControlCluster.

        Args:
            interactor: The ChipToolDeviceInteractor for executing commands.
            node_id: The node ID of the target device.
            endpoint_id: The endpoint ID where this cluster resides. Defaults to 1.
        """
        super().__init__(
            interactor=interactor,
            node_id=node_id,
            endpoint_id=endpoint_id,
            cluster_name="colorcontrol",
        )

    @staticmethod
    def cluster_type() -> ClusterType:
        """Return the Matter cluster ID."""
        return ColorControlCluster.CLUSTER_ID

    # -------------------------------------------------------------------------
    # Commands
    # -------------------------------------------------------------------------

    def move_to_hue(
        self,
        hue: int,
        direction: int = 0,
        transition_time: int = 0,
        options_mask: int = 0,
        options_override: int = 0,
        timeout: int = 30,
    ) -> DeviceInteractionResult:
        """
        Move to a specific hue value.

        Args:
            hue: Target hue (0-254).
            direction: 0=Shortest, 1=Longest, 2=Up, 3=Down.
            transition_time: Transition time in 1/10ths of a second.
            options_mask: Options mask for command execution.
            options_override: Options override for command execution.
            timeout: Command timeout in seconds.

        Returns:
            DeviceInteractionResult: Result of the command.
        """
        return self.invoke_command(
            "move-to-hue",
            arguments={
                "Hue": hue,
                "Direction": direction,
                "TransitionTime": transition_time,
                "OptionsMask": options_mask,
                "OptionsOverride": options_override,
            },
            timeout=timeout,
        )

    def move_to_saturation(
        self,
        saturation: int,
        transition_time: int = 0,
        options_mask: int = 0,
        options_override: int = 0,
        timeout: int = 30,
    ) -> DeviceInteractionResult:
        """
        Move to a specific saturation value.

        Args:
            saturation: Target saturation (0-254).
            transition_time: Transition time in 1/10ths of a second.
            options_mask: Options mask for command execution.
            options_override: Options override for command execution.
            timeout: Command timeout in seconds.

        Returns:
            DeviceInteractionResult: Result of the command.
        """
        return self.invoke_command(
            "move-to-saturation",
            arguments={
                "Saturation": saturation,
                "TransitionTime": transition_time,
                "OptionsMask": options_mask,
                "OptionsOverride": options_override,
            },
            timeout=timeout,
        )

    def move_to_hue_and_saturation(
        self,
        hue: int,
        saturation: int,
        transition_time: int = 0,
        options_mask: int = 0,
        options_override: int = 0,
        timeout: int = 30,
    ) -> DeviceInteractionResult:
        """
        Move to a specific hue and saturation.

        Args:
            hue: Target hue (0-254).
            saturation: Target saturation (0-254).
            transition_time: Transition time in 1/10ths of a second.
            options_mask: Options mask for command execution.
            options_override: Options override for command execution.
            timeout: Command timeout in seconds.

        Returns:
            DeviceInteractionResult: Result of the command.
        """
        return self.invoke_command(
            "move-to-hue-and-saturation",
            arguments={
                "Hue": hue,
                "Saturation": saturation,
                "TransitionTime": transition_time,
                "OptionsMask": options_mask,
                "OptionsOverride": options_override,
            },
            timeout=timeout,
        )

    def move_to_color(
        self,
        color_x: int,
        color_y: int,
        transition_time: int = 0,
        options_mask: int = 0,
        options_override: int = 0,
        timeout: int = 30,
    ) -> DeviceInteractionResult:
        """
        Move to a specific XY color.

        Args:
            color_x: Target X coordinate (0-65279).
            color_y: Target Y coordinate (0-65279).
            transition_time: Transition time in 1/10ths of a second.
            options_mask: Options mask for command execution.
            options_override: Options override for command execution.
            timeout: Command timeout in seconds.

        Returns:
            DeviceInteractionResult: Result of the command.
        """
        return self.invoke_command(
            "move-to-color",
            arguments={
                "ColorX": color_x,
                "ColorY": color_y,
                "TransitionTime": transition_time,
                "OptionsMask": options_mask,
                "OptionsOverride": options_override,
            },
            timeout=timeout,
        )

    def move_to_color_temperature(
        self,
        color_temperature_mireds: int,
        transition_time: int = 0,
        options_mask: int = 0,
        options_override: int = 0,
        timeout: int = 30,
    ) -> DeviceInteractionResult:
        """
        Move to a specific color temperature.

        Args:
            color_temperature_mireds: Target color temperature in mireds.
            transition_time: Transition time in 1/10ths of a second.
            options_mask: Options mask for command execution.
            options_override: Options override for command execution.
            timeout: Command timeout in seconds.

        Returns:
            DeviceInteractionResult: Result of the command.
        """
        return self.invoke_command(
            "move-to-color-temperature",
            arguments={
                "ColorTemperatureMireds": color_temperature_mireds,
                "TransitionTime": transition_time,
                "OptionsMask": options_mask,
                "OptionsOverride": options_override,
            },
            timeout=timeout,
        )

    # -------------------------------------------------------------------------
    # Attributes
    # -------------------------------------------------------------------------

    def get_current_hue(self, timeout: int = 30) -> int:
        """
        Get the current hue value.

        Args:
            timeout: Command timeout in seconds.

        Returns:
            int: The current hue (0-254).

        Raises:
            RuntimeError: If the attribute read fails.
        """
        result = self.read_attribute("current-hue", timeout=timeout)
        if not result.success:
            raise RuntimeError(f"Failed to read current-hue attribute: {result.stderr}")
        return self._parse_color_value(result.stdout)

    def get_current_saturation(self, timeout: int = 30) -> int:
        """
        Get the current saturation value.

        Args:
            timeout: Command timeout in seconds.

        Returns:
            int: The current saturation (0-254).

        Raises:
            RuntimeError: If the attribute read fails.
        """
        result = self.read_attribute("current-saturation", timeout=timeout)
        if not result.success:
            raise RuntimeError(
                f"Failed to read current-saturation attribute: {result.stderr}"
            )
        return self._parse_color_value(result.stdout)

    def get_color_temperature(self, timeout: int = 30) -> int:
        """
        Get the current color temperature in mireds.

        Args:
            timeout: Command timeout in seconds.

        Returns:
            int: The current color temperature in mireds.

        Raises:
            RuntimeError: If the attribute read fails.
        """
        result = self.read_attribute("color-temperature-mireds", timeout=timeout)
        if not result.success:
            raise RuntimeError(
                f"Failed to read color-temperature-mireds attribute: {result.stderr}"
            )
        return self._parse_color_value(result.stdout)

    def _parse_color_value(self, output: str) -> int:
        """
        Parse a color attribute value from chip-tool output.

        chip-tool outputs attribute values in the [TOO] (ToolOutput) log lines.

        Example chip-tool output:
            [TOO]   CurrentHue: 0
            [TOO]   CurrentSaturation: 0
            [TOO]   ColorTemperatureMireds: 250

        Args:
            output: The stdout from chip-tool read command.

        Returns:
            int: The parsed value.
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

        logger.warning(f"Could not parse color value from output: {output}")
        return 0
