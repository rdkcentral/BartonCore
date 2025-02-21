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
# Created by Christian Leithner on 1/27/2025.
#


from dataclasses import dataclass, field
from typing import Self, override

import pytest
from testing.mocks.zhal.events.zhal_event import ZHALEvent
from testing.mocks.zhal.zhal_status import ZHALStatus


@dataclass
class Response(ZHALEvent):
    """
    Response represents a response to a request.

    Attributes:
        _response_type (str): The response type descriptor.
        _request_id (int): The request id that matches this response.
        _result_code (ZHALStatus): The result code of the response.
    """

    _event_type: str = field(default="ipcResponse", init=False)
    _response_type: str = "response"
    _request_id: int = -1
    _result_code: ZHALStatus = ZHALStatus.OK

    @override
    def _from_dict(self, event_dict: dict):
        self._response_type = event_dict.get("responseType", self._response_type)
        self._request_id = event_dict.get("requestId", self._request_id)

        result_code = event_dict.get("resultCode", ZHALStatus.OK.value)
        self._result_code = ZHALStatus(result_code)
        super()._from_dict(event_dict)

    @property
    def response_type(self):
        return self._response_type

    @property
    def request_id(self):
        return self._request_id

    @override
    def encode(self) -> dict:
        parent = super().encode()
        parent["responseType"] = self._response_type
        parent["requestId"] = self._request_id
        parent["resultCode"] = self._result_code.value
        return parent


@pytest.fixture
def response(request):
    response = Response.from_dict(request.param)
    try:
        yield response
    finally:
        pass
