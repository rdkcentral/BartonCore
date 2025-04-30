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

import json
import logging
import socket

import pytest

from testing.mocks.zhal.events.zhal_event import ZHALEvent

logger = logging.getLogger(__name__)

zhal_event_fixtures = [
    {"eventType": "mock_event_type"},
    {"eventType": "mock_event_type", "timestamp": 1234567890},
]


@pytest.mark.parametrize("zhal_event", zhal_event_fixtures, indirect=True)
def test_zhal_event(zhal_event: ZHALEvent):
    client_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    client_sock.bind((zhal_event.socket_address, zhal_event.socket_port))
    client_sock.settimeout(5)

    zhal_event.send()

    data, _ = client_sock.recvfrom(1024)
    received_string = data.decode()

    logger.debug(f"Received data: {received_string}")

    received_json_dict = json.loads(received_string)
    received_event = ZHALEvent.from_dict(received_json_dict)

    assert received_event == zhal_event

    client_sock.close()
