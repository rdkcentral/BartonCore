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
# Created by Christian Leithner on 1/21/2025.
#

import logging
import os
import random
import secrets
import threading
from typing import Final, Optional, override

import pytest

from testing.mocks.zhal.events.zhal_startup_event import ZHALStartupEvent
from testing.mocks.zhal.requests.heartbeat_request import HeartbeatRequest
from testing.mocks.zhal.requests.network_initialize_request import (
    NetworkInitializeRequest,
)
from testing.mocks.zhal.requests.request_handler import RequestHandler
from testing.mocks.zhal.requests.request_receiver import RequestReceiver
from testing.mocks.zhal.requests.zhal_request import ZHALRequest
from testing.mocks.zhal.zhal_status import ZHALStatus

logger = logging.getLogger(__name__)


class MockZHALImplementation(RequestHandler):
    """
    MockZHALImplementation is the driving mock zigbee stack for integration tests.

    Attributes:
    """

    _ZIGBEE_CORE_IPC_PORT: Final[int] = 18443
    _ZIGBEE_CORE_EVENT_PORT: Final[int] = 8711
    _INVALID_NETWORK_KEY: Final[str] = "00000000000000000000000000000000"

    _eui64: str = ""
    _region: str = ""
    _network_key: str = _INVALID_NETWORK_KEY
    _channel: int = 0
    _pan_id: int = 0
    _running_lock: threading.Lock = threading.Lock()
    _running: bool = False
    _network_initialized_lock: threading.Lock = threading.Lock()
    _network_initialized: bool = False
    _request_receiver: RequestReceiver
    # TODO: This would be better if you could pair it with a specific request
    _mock_result_code: ZHALStatus = Optional[ZHALStatus.OK]

    def __init__(self):
        self._request_receiver = RequestReceiver(self._ZIGBEE_CORE_IPC_PORT, self)
        pass

    def start(self):
        if self._is_running():
            logger.warning(
                "Asked to start but MockZHALImplementation is already running."
            )
            return

        self._request_receiver.start()

        ZHALStartupEvent().send()
        self._set_running(True)

    def stop(self):
        if not self._is_running():
            logger.warning("Asked to stop but MockZHALImplementation is not running.")
            return

        self._set_running(False)
        self._request_receiver.stop()

    @override
    def handle_request(self, request: type[ZHALRequest]) -> dict:
        # TODO Docs
        if request is None:
            raise ValueError("Request cannot be None.")

        if isinstance(request, HeartbeatRequest):
            result_data = self._handle_heartbeat_request(request) or []
        elif isinstance(request, NetworkInitializeRequest):
            result_data = self._handle_network_initialize_request(request) or []

        result_data["resultCode"] = self._get_and_clear_mock_result_code()

    @property
    def eui64(self) -> str:
        return self._eui64

    @property
    def region(self) -> str:
        return self._region

    @property
    def network_key(self) -> str:
        return self._network_key

    @property
    def channel(self) -> int:
        return self._channel

    @property
    def pan_id(self) -> int:
        return self._pan_id

    @property
    def is_running(self) -> bool:
        return self._is_running()

    @property
    def is_network_initialized(self) -> bool:
        return self._is_network_initialized()

    @property
    def mock_result_code(self) -> ZHALStatus:
        return self._mock_result_code

    @mock_result_code.setter
    def mock_result_code(self, result_code: ZHALStatus):
        self._mock_result_code = result_code

    def _get_and_clear_mock_result_code(self) -> ZHALStatus:
        result_code = self._mock_result_code
        self._mock_result_code = ZHALStatus.OK
        return result_code

    def _set_running(self, running: bool):
        with self._running_lock:
            self._running = running

    def _is_running(self) -> bool:
        with self._running_lock:
            return self._running

    def _set_network_initialized(self, network_initialized: bool):
        with self._network_initialized_lock:
            self._network_initialized = network_initialized

    def _is_network_initialized(self) -> bool:
        with self._network_initialized_lock:
            return self._network_initialized

    def _compare_and_set_network_initialized(self, network_initialized: bool) -> bool:
        with self._network_initialized_lock:
            if not self._network_initialized == network_initialized:
                self._network_initialized = network_initialized
                return True
            return False

    def _initialize_network(
        self,
        eui64: str = "",
        region: str = "",
        network_key: str = "",
        channel: int = 0,
        pan_id: int = 0,
    ):
        somethingChanged = False

        if eui64 and eui64 != self._eui64:
            self._eui64 = eui64
            somethingChanged = True

        if region and region != self._region:
            self._region = region
            somethingChanged = True

        if network_key and network_key != self._network_key:
            self._network_key = network_key
            somethingChanged = True

        if channel and channel != self._channel:
            self._channel = channel
            somethingChanged = True

        if pan_id and pan_id != self._pan_id:
            self._pan_id = pan_id
            somethingChanged = True

        self._set_network_initialized(True)

        return somethingChanged

    def _handle_network_initialize_request(
        self, request: NetworkInitializeRequest
    ) -> dict:
        # TODO Docs
        if self._compare_and_set_network_initialized(True):
            network_key = secrets.token_hex(8)
            pan_id = random.randint(-32768, 32767)
            channel = 25
            self._initialize_network(
                eui64=request.address,
                region=request.region,
                network_key=network_key,
                channel=channel,
                pan_id=pan_id,
            )

            ZHALStartupEvent().send()
        else:
            logger.warning("Network already initialized.")

    def _handle_heartbeat_request(self, request: HeartbeatRequest) -> dict:
        return {"pid": os.getpid(), "initialized": self.is_network_initialized()}


@pytest.fixture
def mock_zhal_implementation(request):
    # TODO Docs
    mock_zhal_implementation = MockZHALImplementation()
    try:
        mock_zhal_implementation.start()
        yield mock_zhal_implementation
    finally:
        mock_zhal_implementation.stop()
