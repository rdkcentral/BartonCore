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

import time
from queue import Empty


def wait_for_resource_value(queue, expected_value, timeout=10):
    """Drain events from the queue until we get the expected value or time out.

    This handles spurious initial subscription events that may arrive before
    the event triggered by the test action.
    """
    deadline = time.monotonic() + timeout

    while True:
        remaining = deadline - time.monotonic()

        if remaining <= 0:
            raise AssertionError(
                f"Timed out waiting for resource value '{expected_value}'"
            )

        try:
            value = queue.get(timeout=remaining)
        except Empty:
            raise AssertionError(
                f"Timed out waiting for resource value '{expected_value}'"
            )

        if value == expected_value:
            return value
