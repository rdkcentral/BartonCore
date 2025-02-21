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

from dataclasses import dataclass
from typing import Optional, Type, override

import pytest
from testing.mocks.zhal.events.responses.heartbeat_response import HeartbeatResponse
from testing.mocks.zhal.requests.request_handler import RequestHandler
from testing.mocks.zhal.requests.zhal_request import ZHALRequest


@dataclass
class HeartbeatRequest(ZHALRequest):
    """
    HeartbeatRequest represents the heartbeat request. It has no additional properties over
    a basic request.
    """

    def __init__(self, request_dict: dict):
        super().__init__(request_dict)

    @override
    def process(
        self, request_handler: Optional[Type[RequestHandler]]
    ) -> HeartbeatResponse:
        if request_handler is None:
            raise ValueError("RequestHandler is not valid.")

        request_data = request_handler.handle_request(self)
        if request_data is None:
            raise ValueError("RequestHandler did not return valid data.")

        return HeartbeatResponse(
            request_id=self.request_id,
            result_code=request_data["resultCode"],
            pid=request_data["pid"],
            initialized=request_data["initialized"],
        )


@pytest.fixture
def heartbeat_request(request):
    heartbeat_request = HeartbeatRequest(request.param)
    try:
        yield heartbeat_request
    finally:
        pass
