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
# Created by Christian Leithner on 1/30/2025.
#


from dataclasses import dataclass, field
from typing import Optional, Type, override

import pytest
from testing.mocks.zhal.events.responses.network_initialize_response import (
    NetworkInitializeResponse,
)
from testing.mocks.zhal.requests.request_handler import RequestHandler
from testing.mocks.zhal.requests.zhal_request import ZHALRequest


@dataclass
class NetworkInitializeRequest(ZHALRequest):
    """
    NetworkInitializeRequest represents the network initialize request from the zhal client.

    Attributes:
        _address (str): The address of the network. This should be reflected in the mock zhal implementation's EUI64 address.
        _region (str): The region of the network (ex. "US").
        _properties (dict): The properties of the network.
    """

    _address: str = ""
    _region: str = ""
    _custom_properties: dict = field(default_factory=dict)

    def __init__(self, request_dict: dict):
        super().__init__(request_dict)
        self._address = request_dict["address"]
        self._region = request_dict["region"]
        self._custom_properties = request_dict["properties"]

    @property
    def address(self) -> str:
        return self._address

    @property
    def region(self) -> str:
        return self._region

    @property
    def custom_properties(self) -> dict:
        return self._custom_properties

    @override
    def process(
        self, request_handler: Optional[Type[RequestHandler]]
    ) -> NetworkInitializeResponse:
        if request_handler is None:
            raise ValueError("RequestHandler is not valid.")

        request_data = request_handler.handle_request(self)
        if request_data is None:
            raise ValueError("RequestHandler did not return valid data.")

        return NetworkInitializeResponse(
            request_id=self.request_id, result_code=request_data["resultCode"]
        )


@pytest.fixture
def network_initialize_request(request):
    network_initialize_request = NetworkInitializeRequest.from_dict(request.param)
    try:
        yield network_initialize_request
    finally:
        pass
