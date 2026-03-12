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

"""
Shared fixtures for observability integration tests.

The ``otlp_receiver`` fixture is defined in
``testing.mocks.otlp_mock_receiver`` and auto-loaded via
``pytest_plugins`` in the root ``testing/conftest.py``.

Every test should list ``otlp_receiver`` before ``default_environment``
in its parameter list so the OTLP endpoint env-var is set before the
Barton client starts.
"""

import logging

from testing.conftest import requires_otel

logger = logging.getLogger(__name__)

# Skip every test in this directory when observability is not compiled in.
pytestmark = requires_otel
