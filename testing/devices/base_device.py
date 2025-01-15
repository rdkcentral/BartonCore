# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 Comcast Cable Communications Management, LLC
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
# Created by Kevin Funderburg on 11/14/2024
#

from abc import ABC
import logging

logger = logging.getLogger(__name__)


class BaseDevice(ABC):
    """
    BaseDevice is an abstract base class that provides common functionality for devices
    across varying subsystems - e.g. matter, zigbee, thread, etc.

    Attributes:
        _device_class (str): The class of the device (e.g. 'light', 'lock').
    """

    _device_class: str
