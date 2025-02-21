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
# Created by Christian Leithner on 1/23/2025.
#

from abc import ABC, abstractmethod
from dataclasses import dataclass
import logging
from typing import Optional, Type

import pytest

from testing.mocks.test.zhal.events.zhal_event_test import ZHALEvent
from testing.mocks.zhal.requests.request_handler import RequestHandler

logger = logging.getLogger(__name__)


@dataclass
class ZHALRequest(ABC):
    """
    ZHALRequest is the most base class for incoming ZHAL requests.

    Attributes:
        _request (str): The request type descriptor.
        _request_id (int): The request id. This is used to matched asynchronous requests with asynchronous events (ipc responses).
    """

    _request: str
    _request_id: int

    def __init__(self, request_dict: dict):
        self._request = request_dict["request"]
        self._request_id = request_dict["requestId"]

    @property
    def request(self) -> str:
        return self._request

    @property
    def request_id(self) -> int:
        return self._request_id

    @abstractmethod
    def process(
        self, request_handler: Optional[Type[RequestHandler]]
    ) -> Type[ZHALEvent]:
        """
        Process this request and produce a response message.
        """
        pass
