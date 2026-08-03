# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2026 Comcast Cable Communications Management, LLC
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

"""Runtime-configurable timeouts (seconds) for integration-test waits.

The defaults are tuned for serial runs. They are overridable at pytest
invocation via the --client-ready-timeout / --device-added-timeout /
--resource-value-timeout options (registered in testing/conftest.py), which
lets parallel runs raise them to tolerate concurrent-commissioning load.

conftest's pytest_configure resolves the options into these module attributes,
and the wait helpers read them at call time -- so always access them as
``timeouts.<name>`` (do not ``from ... import <name>``, which would capture the
default at import time).
"""

# Defaults (seconds), tuned for serial runs. Overridden in pytest_configure.
client_ready = 5
device_added = 5
resource_value = 30
