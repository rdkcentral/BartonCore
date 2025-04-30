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
from typing import override
import pytest
from testing.mocks.zhal.events.responses.response import Response


@dataclass
class HeartbeatResponse(Response):
    """
    HeartbeatResponse represents the heartbeat response.
    """

    _response_type: str = field(default="heartbeatResponse", init=False)
    _pid: int = field(default=0)
    _initialized: bool = field(default=False)

    @property
    def pid(self):
        return self._pid

    @property
    def initialized(self):
        return self._initialized

    @override
    def _from_dict(self, event_dict):
        self._pid = event_dict.get("pid", self._pid)
        self._initialized = event_dict.get("initialized", self._initialized)
        super()._from_dict(event_dict)

    @override
    def encode(self) -> dict:
        parent = super().encode()
        parent["pid"] = self._pid
        parent["initialized"] = self._initialized
        return parent


@pytest.fixture
def heartbeat_response(request):
    heartbeat_response = HeartbeatResponse.from_dict(request.param)
    try:
        yield heartbeat_response
    finally:
        pass
