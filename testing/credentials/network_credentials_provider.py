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
# Created by Kevin Funderburg on 12/10/2024
#

from gi.repository import GObject
from gi.repository import BCore


class ExampleNetworkCredentialsProvider(
    GObject.GObject, BCore.NetworkCredentialsProvider
):
    __gtype_name__ = "ExampleNetworkCredentialsProvider2"

    def __init__(self):
        super().__init__()

    # Virtual method implementations must have prefix "do_" in pygobject!
    def do_get_wifi_network_credentials(
        self,
    ) -> BCore.WifiNetworkCredentials:
        creds = BCore.WifiNetworkCredentials()
        creds.props.ssid = "my_wifi_network"
        creds.props.psk = "my_wifi_password"
        return creds
