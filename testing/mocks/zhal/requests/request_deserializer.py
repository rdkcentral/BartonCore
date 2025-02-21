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
# Created by Christian Leithner on 1/24/2025.
#


import json

import pytest
from testing.mocks.zhal.requests.heartbeat_request import HeartbeatRequest
from testing.mocks.zhal.requests.network_initialize_request import NetworkInitializeRequest
from testing.mocks.zhal.requests.zhal_request import ZHALRequest


class RequestDeserializer:
    """
    RequestDeserializer deserializes request data from the Barton library runtime
    into a valid Request object. When adding new Request derived types, be sure to
    add the appropriate mapper here.
    """

    def __init__(self):
        pass

    def deserialize(self, data: bytes) -> type[ZHALRequest]:
        """
        Deserialize the request data.

        Args:
            data (bytes): The request data.

        Returns:
            Request: The deserialized request.
        """

        if data is None:
            raise ValueError("Data cannot be None.")

        data_string = data.decode()
        data_dict = json.loads(data_string)

        request_type = data_dict["request"]

        match request_type:
            case "heartbeat":
                return HeartbeatRequest(data_dict)
            case "networkInit":
                return NetworkInitializeRequest(data_dict)
            case _:
                raise ValueError(f"Unknown request type: {request_type}")


@pytest.fixture
def request_deserializer(request):
    request_deserializer = RequestDeserializer(request.param)
    try:
        yield request_deserializer
    finally:
        pass
