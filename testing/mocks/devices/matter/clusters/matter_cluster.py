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
Matter Cluster Abstractions for Testing.

This module provides abstract cluster interfaces that allow tests to interact
with Matter device clusters without needing to know the low-level details of
chip-tool commands or endpoint IDs.

Example usage:
    # Get a cluster interface from a device using Matter cluster IDs
    from testing.mocks.devices.matter.clusters.onoff_cluster import OnOffCluster

    onoff = device.get_cluster(OnOffCluster.CLUSTER_ID)
    onoff.on()
    assert onoff.is_on()

    from testing.mocks.devices.matter.clusters.levelcontrol_cluster import LevelControlCluster

    level = device.get_cluster(LevelControlCluster.CLUSTER_ID)
    level.move_to_level(128)
    assert level.get_current_level() == 128
"""

from __future__ import annotations

import logging
from abc import ABC, abstractmethod
from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    from testing.mocks.devices.matter.device_interactor import (
        ChipToolDeviceInteractor,
        DeviceInteractionResult,
    )

logger = logging.getLogger(__name__)


# ClusterId is simply an alias for int - concrete clusters define their
# Matter cluster ID as a class constant (e.g., OnOffCluster.CLUSTER_ID = 0x0006)
ClusterId = int


class MatterCluster(ABC):
    """
    Abstract base class for Matter cluster interactions.

    This class provides a common interface for interacting with Matter clusters
    on a device. Subclasses implement cluster-specific methods that map to
    chip-tool commands.

    Attributes:
        _interactor: The ChipToolDeviceInteractor for executing commands.
        _node_id: The node ID of the target device.
        _endpoint_id: The endpoint ID where this cluster resides.
        _cluster_name: The chip-tool cluster name (e.g., "onoff").
    """

    _interactor: ChipToolDeviceInteractor
    _node_id: int
    _endpoint_id: int
    _cluster_name: str

    def __init__(
        self,
        interactor: ChipToolDeviceInteractor,
        node_id: int,
        endpoint_id: int,
        cluster_name: str,
    ):
        """
        Initialize the MatterCluster.

        Args:
            interactor: The ChipToolDeviceInteractor for executing commands.
            node_id: The node ID of the target device.
            endpoint_id: The endpoint ID where this cluster resides.
            cluster_name: The chip-tool cluster name (e.g., "onoff").
        """
        self._interactor = interactor
        self._node_id = node_id
        self._endpoint_id = endpoint_id
        self._cluster_name = cluster_name

    @property
    def node_id(self) -> int:
        """The node ID of the target device."""
        return self._node_id

    @property
    def endpoint_id(self) -> int:
        """The endpoint ID where this cluster resides."""
        return self._endpoint_id

    @property
    def cluster_name(self) -> str:
        """The chip-tool cluster name."""
        return self._cluster_name

    def read_attribute(self, attribute: str, timeout: int = 5) -> DeviceInteractionResult:
        """
        Read an attribute from this cluster.

        Args:
            attribute: The attribute name to read.
            timeout: Command timeout in seconds.

        Returns:
            DeviceInteractionResult: Result containing the attribute value.
        """
        return self._interactor.read_attribute(
            node_id=self._node_id,
            endpoint_id=self._endpoint_id,
            cluster=self._cluster_name,
            attribute=attribute,
            timeout=timeout,
        )

    def write_attribute(
        self, attribute: str, value: Any, timeout: int = 5
    ) -> DeviceInteractionResult:
        """
        Write a value to an attribute on this cluster.

        Args:
            attribute: The attribute name to write.
            value: The value to write.
            timeout: Command timeout in seconds.

        Returns:
            DeviceInteractionResult: Result of the write operation.
        """
        return self._interactor.write_attribute(
            node_id=self._node_id,
            endpoint_id=self._endpoint_id,
            cluster=self._cluster_name,
            attribute=attribute,
            value=value,
            timeout=timeout,
        )

    def invoke_command(
        self,
        command: str,
        arguments: dict[str, Any] | None = None,
        timeout: int = 5,
    ) -> DeviceInteractionResult:
        """
        Invoke a command on this cluster.

        Args:
            command: The command name to invoke.
            arguments: Optional dictionary of command arguments.
            timeout: Command timeout in seconds.

        Returns:
            DeviceInteractionResult: Result of the command invocation.
        """
        return self._interactor.invoke_command(
            node_id=self._node_id,
            endpoint_id=self._endpoint_id,
            cluster=self._cluster_name,
            command=command,
            arguments=arguments,
            timeout=timeout,
        )

    @staticmethod
    @abstractmethod
    def cluster_type() -> ClusterId:
        """
        Return the Matter cluster ID for this cluster.

        Returns:
            ClusterId: The Matter cluster ID (e.g., 0x0006 for OnOff).
        """
        pass
