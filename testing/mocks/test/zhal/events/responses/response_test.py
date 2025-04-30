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

import json
import logging
import socket

import pytest

from testing.mocks.zhal.events.responses.response import Response


logger = logging.getLogger(__name__)

response_fixtures = [
    {"requestId": 1, "responseType": "mockResponseType"},
    {"requestId": 100, "responseType": "mockResponseType"},
]


@pytest.mark.parametrize("response", response_fixtures, indirect=True)
def test_heartbeat_response(response: Response):
    client_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    client_sock.bind((response.socket_address, response.socket_port))
    client_sock.settimeout(5)

    response.send()

    data, _ = client_sock.recvfrom(1024)
    received_string = data.decode()

    logger.debug(f"Received data: {received_string}")

    received_json_dict = json.loads(received_string)
    received_event = Response.from_dict(received_json_dict)

    assert received_event == response

    client_sock.close()
