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

from testing.mocks.zhal.events.responses.heartbeat_response import HeartbeatResponse


logger = logging.getLogger(__name__)

heartbeat_response_fixtures = [
    {"pid": 100, "initialized": False},
    {"pid": 1000, "initialized": True},
]


@pytest.mark.parametrize(
    "heartbeat_response", heartbeat_response_fixtures, indirect=True
)
def test_heartbeat_response(heartbeat_response: HeartbeatResponse):
    client_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    client_sock.bind(
        (heartbeat_response.socket_address, heartbeat_response.socket_port)
    )
    client_sock.settimeout(5)

    heartbeat_response.send()

    data, _ = client_sock.recvfrom(1024)
    received_string = data.decode()

    logger.debug(f"Received data: {received_string}")

    received_json_dict = json.loads(received_string)
    received_event = HeartbeatResponse.from_dict(received_json_dict)

    assert received_event == heartbeat_response

    client_sock.close()
