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
# Created by Kevin Funderburg on 1/9/2025.
#

"""
This conftest.py file is used to define fixtures and other configurations
for the pytest testing environment. Fixtures defined in this file are
automatically available to all test modules within the same directory
and subdirectories. This allows for reusable setup and teardown code,
ensuring consistency and reducing redundancy across tests. The conftest.py
file is a central place to manage test dependencies and configurations,
making it easier to maintain and scale the test suite.
"""

import logging
import os
import re

import pytest

logger = logging.getLogger(__name__)


def _get_cmake_cache_path() -> str:
    """Get the path to CMakeCache.txt in the build directory."""
    # Try common build directory locations relative to the project root
    project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    return os.path.join(project_root, "build", "CMakeCache.txt")


def _is_matterjs_enabled() -> bool:
    """Check if BCORE_MATTER_USE_MATTERJS is enabled in CMakeCache.txt."""
    cache_path = _get_cmake_cache_path()
    if not os.path.exists(cache_path):
        logger.warning(
            f"CMakeCache.txt not found at {cache_path}, assuming matterjs is enabled"
        )
        return True

    try:
        with open(cache_path, "r") as f:
            for line in f:
                match = re.match(r"^BCORE_MATTER_USE_MATTERJS:BOOL=(.+)$", line.strip())
                if match:
                    value = match.group(1).upper()
                    enabled = value in ("ON", "TRUE", "1", "YES")
                    logger.debug(
                        f"BCORE_MATTER_USE_MATTERJS={value} (enabled={enabled})"
                    )
                    return enabled
    except Exception as e:
        logger.warning(
            f"Error reading CMakeCache.txt: {e}, assuming matterjs is enabled"
        )
        return True

    # If not found, assume enabled (default)
    return True


# Cache the result to avoid reading the file multiple times
_matterjs_enabled = None


def is_matterjs_enabled() -> bool:
    """Check if matter.js integration is enabled (cached)."""
    global _matterjs_enabled
    if _matterjs_enabled is None:
        _matterjs_enabled = _is_matterjs_enabled()
    return _matterjs_enabled


# Skip marker for tests that require matter.js
requires_matterjs = pytest.mark.skipif(
    not is_matterjs_enabled(),
    reason="Test requires matter.js integration (BCORE_MATTER_USE_MATTERJS=ON)",
)

# The following list of plugins are automatically loaded by pytest when running tests.
# Any fixtures defined within these modules are automatically available to all test modules.
pytest_plugins = [
    # Environments
    "testing.environment.default_environment_orchestrator",
    # Devices
    "testing.mocks.devices.matter.matter_light",
    "testing.mocks.devices.matter.device_interactor",
    # ZHAL
    "testing.mocks.zhal.mock_zhal_implementation",
    ## events
    "testing.mocks.zhal.events.zhal_event",
    "testing.mocks.zhal.events.zhal_startup_event",
    "testing.mocks.zhal.events.responses.response",
    "testing.mocks.zhal.events.responses.heartbeat_response",
    ## requests
    "testing.mocks.zhal.requests.heartbeat_request",
    "testing.mocks.zhal.requests.request_deserializer",
    "testing.mocks.zhal.requests.request_receiver",
    "testing.mocks.zhal.requests.network_initialize_request",
]
