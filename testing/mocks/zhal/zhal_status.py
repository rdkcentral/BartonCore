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

from enum import Enum


class ZHALStatus(Enum):
    """
    ZHALStatus represents status codes of ZHAL operations.
    """

    OK = 0
    FAIL = -1
    INVALID_ARG = -2
    NOT_IMPLEMENTED = -3
    TIMEOUT = -4
    OUT_OF_MEMORY = -5
    MESSAGE_DELIVERY_FAILED = -6
    NETWORK_BUSY = -7
    NOT_READY = -8
    LPM = -9
