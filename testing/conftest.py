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
import sys

import pytest

logger = logging.getLogger(__name__)

def pytest_configure(config):
    """Patch pytest-forked to emit native (C library) stdout/stderr from child processes.

    By default, pytest-forked redirects the child's fd 1/fd 2 to temp files and
    then discards the content for passing tests.  This means some C-level log
    output (e.g. icLog) can be silently lost.

    This hook patches ``forked_run_report`` to write the child's captured
    stdout/stderr to the parent's stderr, making native log output visible in
    the terminal alongside pytest's live-log output.

    Requires ``--capture=sys`` (set in pyproject.toml) so that pytest's own fd
    capture does not override ForkedFunc's fd redirect.
    """
    try:
        import pytest_forked

        def _patched_forked_run_report(item):
            from _pytest.runner import TestReport

            import marshal

            import py

            def runforked():
                from _pytest.runner import runtestprotocol

                try:
                    reports = runtestprotocol(item, log=False)
                except KeyboardInterrupt:
                    os._exit(4)

                return marshal.dumps(
                    [pytest_forked.serialize_report(x) for x in reports]
                )

            ff = py.process.ForkedFunc(runforked)
            result = ff.waitfinish()

            if result.retval is not None:
                report_dumps = marshal.loads(result.retval)
                reports = [TestReport(**x) for x in report_dumps]
                # Emit the child's native stdout/stderr so C-level logs are visible
                if result.out:
                    if isinstance(result.out, bytes):
                        sys.stderr.buffer.write(result.out)
                        sys.stderr.buffer.flush()
                    else:
                        sys.stderr.write(result.out)
                        sys.stderr.flush()

                if result.err:
                    if isinstance(result.err, bytes):
                        sys.stderr.buffer.write(result.err)
                        sys.stderr.buffer.flush()
                    else:
                        sys.stderr.write(result.err)
                        sys.stderr.flush()

                return reports
            else:
                return [pytest_forked.report_process_crash(item, result)]

        pytest_forked.forked_run_report = _patched_forked_run_report
    except ImportError:
        pass


# The following list of plugins are automatically loaded by pytest when running tests.
# Any fixtures defined within these modules are automatically available to all test modules.
pytest_plugins = [
    # Environments
    "testing.environment.default_environment_orchestrator",
    "testing.environment.descriptor_environment_orchestrator",
    # Helpers
    "testing.helpers.http_fixture_server",
    # Mocks
    "testing.mocks.device_descriptor_server",
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
