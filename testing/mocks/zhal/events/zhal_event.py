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

from dataclasses import dataclass
from datetime import datetime
import json
import logging
import socket
from typing import Self

import pytest

logger = logging.getLogger(__name__)


@dataclass
class ZHALEvent:
    """
    ZHALEvent is the most base class for ZHAL communication. Widely messaging can be
    bisected into two categories: ipc requests and events. The latter cateogry may be
    independent events or ipc responses; however, in this test harness we simulate
    interprocess communication. This is because the mock zhal implementation is within the
    same process as the tests and thus the Barton library runtime. Our test process
    will still send data over sockets to itself.

    Attributes:
        _event_type (str): The type of zhal event.
        _socket_address (str): The IP address the event should be sent to.
        _socket_port (int): The port the event should be sent to.
        _timestamp (str): The timestamp of the event (creation time) in epoch time millis.
    """

    _event_type: str = "event"
    _socket_address: str = "localhost"
    _socket_port: int = 8711
    _timestamp: str = str(int(datetime.now().timestamp() * 1000))

    @classmethod
    def from_dict(cls, event_dict: dict) -> type[Self]:
        """
        Secondary constructor for ZHAL Events. Subclasses should override _from_dict
        and select from the dictionary the desired fields to construct the event.

        Primary purpose is to trivially translate json payload dictionaries into events for testing.
        """
        if event_dict:
            event = object.__new__(cls)
            event._from_dict(event_dict)
            return event
        else:
            raise ValueError("event_dict must be provided")

    def _from_dict(self, event_dict: dict):
        """
        Subclasses should override this method to select from the dictionary the desired fields
        to construct the event.
        """
        self._event_type = event_dict.get("eventType", self._event_type)
        self._timestamp = event_dict.get("timestamp", self._timestamp)

    @property
    def event_type(self) -> str:
        return self._event_type

    @property
    def socket_address(self) -> str:
        return self._socket_address

    @property
    def socket_port(self) -> int:
        return self._socket_port

    @property
    def timestamp(self) -> str:
        return self._timestamp

    def encode(self) -> dict:
        return {"eventType": self.event_type, "timestamp": self.timestamp}

    @staticmethod
    def _encode(event: type[Self]) -> dict:
        return event.encode()

    def send(self):
        try:
            eventJson = json.dumps(self, default=self._encode)

            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.sendto(eventJson.encode(), (self._socket_address, self._socket_port))

            sock.close()

            logger.debug(f"Sent event: {eventJson}")

        except Exception as e:
            logger.exception("Failed to send event")
        finally:
            pass


@pytest.fixture
def zhal_event(request):
    """
    A ZHALEvent is meant to be derived rather than instantiated itself, but
    this fixture provides one anyway.

    Yields:
        ZHALEvent: An instance of the ZHALEvent class.
    """
    zhal_event = ZHALEvent.from_dict(request.param)
    try:
        yield zhal_event
    finally:
        pass
