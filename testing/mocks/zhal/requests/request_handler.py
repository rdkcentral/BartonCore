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

from abc import ABC, abstractmethod


class RequestHandler(ABC):
    """
    RequestHandler is the base class for a something that
    performs meaningful action in reaction to a request
    """

    @abstractmethod
    def handle_request(self, request) -> dict:
        """
        Perform the action(s) desired by the request. Returns a generic
        dictionary of result data that is specific to the request.

        Implementations should never call request.process()

        request : type[ZHALRequest] - Not type hinted to avoid circular imports.
        """
        pass
