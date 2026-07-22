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
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

import pytest

logger = logging.getLogger(__name__)

def pytest_configure(config):
    """Register custom markers."""
    config.addinivalue_line(
        "markers",
        "requires_matterjs: skip test when Node.js or matter.js is not available",
    )


def pytest_sessionfinish(session, exitstatus):
    """Sweep the shared, compile-time Matter config dir once, after everything.

    The Matter SDK's PosixConfig ini files always live at the compile-time
    CHIP_BARTON_CONF_DIR (~/.brtn-ds/matter) and cannot be redirected at runtime,
    so every test process shares that one directory. Remove it only from the
    outer, top-level session -- not the per-test subprocess runs, and not the
    xdist workers -- so it is never deleted while another concurrent test is
    still using it. Per-process KVS/dynamic storage lives on tmpfs and is cleaned
    up by each process itself (see base_environment_orchestrator.py).
    """
    if os.environ.get(_SUBPROCESS_MARKER_ENV):
        return  # inner per-test run

    if hasattr(session.config, "workerinput"):
        return  # xdist worker; the controller sweeps after all workers finish

    shutil.rmtree(os.path.join(os.path.expanduser("~"), ".brtn-ds"), ignore_errors=True)


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
    "testing.mocks.devices.matter.matter_door_lock",
    "testing.mocks.devices.matter.matter_thermostat",
    "testing.mocks.devices.matter.matter_thermostat_with_fan",
    "testing.mocks.devices.matter.matter_ikea_timmerflotte",
    "testing.mocks.devices.matter.matter_temperature_sensor",
    "testing.mocks.devices.matter.matter_humidity_sensor",
    # ZHAL
    ## events (load first so pytest can assert-rewrite these modules before
    ## they are imported transitively by other plugins)
    "testing.mocks.zhal.events.zhal_event",
    "testing.mocks.zhal.events.zhal_startup_event",
    "testing.mocks.zhal.events.responses.response",
    "testing.mocks.zhal.events.responses.heartbeat_response",
    ## requests
    "testing.mocks.zhal.requests.heartbeat_request",
    "testing.mocks.zhal.requests.network_initialize_request",
    "testing.mocks.zhal.requests.request_deserializer",
    "testing.mocks.zhal.requests.request_receiver",
    "testing.mocks.zhal.mock_zhal_implementation",
]


def _matterjs_available() -> bool:
    """Check if Node.js and matter.js are available at runtime."""
    if shutil.which("node") is None:
        return False

    matterjs_dir = os.path.join(
        os.path.dirname(__file__), "mocks", "devices", "matterjs"
    )
    node_modules = os.path.join(matterjs_dir, "node_modules", "@matter")

    return os.path.isdir(node_modules)


_has_matterjs = _matterjs_available()


@pytest.hookimpl(tryfirst=True)
def pytest_collection_modifyitems(config, items):
    # Keep tests that bind the fixed zhal IPC/event ports (18443/8711) on a
    # single xdist worker so they never run concurrently and collide on those
    # ports. Requires xdist's loadgroup distribution (testing/py_test.sh
    # --parallel enables it); harmless when running serially. Runs tryfirst so
    # the marker is applied before xdist reads xdist_group during collection.
    for item in items:
        if "/mocks/test/zhal/" in item.nodeid or "mock_zhal_implementation" in item.fixturenames:
            item.add_marker(pytest.mark.xdist_group("zhal"))

    if _has_matterjs:
        return

    skip_marker = pytest.mark.skip(
        reason="Requires Node.js and matter.js (not available)"
    )

    for item in items:
        if "requires_matterjs" in item.keywords:
            item.add_marker(skip_marker)


# ---------------------------------------------------------------------------
#  Subprocess isolation for all tests.
#
#  Every test is invoked in its own subprocess.  This provides:
#    1. Output isolation — C library log output (written directly to fd 1/2 by
#       background threads) is captured by the subprocess pipe and only shown
#       when a test fails.
#    2. State isolation — the Matter SDK is a C++ singleton that cannot
#       reinitialize within a single process, so each matterjs test needs a
#       fresh process anyway.
#
#  The subprocess writes its result to a JUnit XML file.  The outer test
#  parses that file and propagates the true outcome (passed / skipped /
#  xfailed / xpassed / failed) so that CI/reporting is accurate.
# ---------------------------------------------------------------------------

_SUBPROCESS_MARKER_ENV = "_PYTEST_SUBPROCESS_INNER"


def pytest_runtest_protocol(item, nextitem):
    """Run each test in a subprocess to isolate C library output and state."""

    if os.environ.get(_SUBPROCESS_MARKER_ENV):
        return None  # inner run — execute normally

    if "requires_matterjs" in item.keywords and not _has_matterjs:
        # Tests requiring matter.js are already marked skipped during collection.
        # Avoid spawning one subprocess per skipped test.
        return None

    ihook = item.ihook
    ihook.pytest_runtest_logstart(nodeid=item.nodeid, location=item.location)

    from _pytest import runner

    call = runner.CallInfo.from_call(lambda: _run_in_subprocess(item), when="call")
    report = runner.pytest_runtest_makereport(item=item, call=call)
    ihook.pytest_runtest_logreport(report=report)
    ihook.pytest_runtest_logfinish(nodeid=item.nodeid, location=item.location)

    return True


def _parse_junit_outcome(junit_path):
    """Parse a JUnit XML file written by a child pytest to determine test outcome.

    Returns a tuple of (outcome, message) where outcome is one of:
      "passed", "skipped", "xfail", "xpassed", "failed", "unknown".
    """
    try:
        tree = ET.parse(junit_path)
        root = tree.getroot()

        testcase = root.find(".//testcase")

        if testcase is None:
            return "unknown", ""

        skipped = testcase.find("skipped")

        if skipped is not None:
            msg = skipped.get("message", "") or (skipped.text or "")
            skipped_type = (skipped.get("type", "") or "").lower()

            if skipped_type == "pytest.xfail":
                return "xfail", msg

            return "skipped", msg

        failure = testcase.find("failure")

        if failure is not None:
            failure_msg = failure.get("message", "")
            failure_type = (failure.get("type", "") or "").lower()

            if (
                failure_type == "pytest.xfail"
                or "[xpass(strict)]" in failure_msg.lower()
            ):
                return "xpassed", failure_msg

            return "failed", failure_msg

        error = testcase.find("error")

        if error is not None:
            return "failed", error.get("message", "")

        return "passed", ""
    except (ET.ParseError, FileNotFoundError, OSError):
        return "unknown", ""


def _run_in_subprocess(item):
    """Spawn a child pytest that runs exactly one test node.

    The child's JUnit XML output is parsed to propagate the true test outcome
    (passed / skipped / xfailed / xpassed / failed) back to the outer pytest
    session, so that skipped and xfail results are not silently reported as
    passed.
    """
    junit_fd, junit_path = tempfile.mkstemp(suffix=".xml")
    os.close(junit_fd)

    # xdist's loadgroup scheduling appends "@<group>" to the nodeid (e.g. from an
    # xdist_group marker). Strip it so the child pytest receives a real,
    # selectable nodeid. Legitimate nodeids never contain "@".
    child_nodeid = item.nodeid.split("@", 1)[0]

    try:
        cmd = [
            sys.executable,
            "-m",
            "pytest",
            "-x",
            "--override-ini=addopts=",
            "--override-ini=log_cli=false",
            "--no-header",
            "-q",
            f"--junit-xml={junit_path}",
            child_nodeid,
        ]

        # In CI, py_test.sh preloads libasan via LD_PRELOAD so Python/GI tests can
        # load ASAN-instrumented Barton libraries. Keep the environment intact for
        # this child pytest process.
        child_env = dict(os.environ)
        child_env[_SUBPROCESS_MARKER_ENV] = "1"

        result = subprocess.run(
            cmd,
            cwd=str(item.config.rootpath),
            capture_output=True,
            text=True,
            env=child_env,
        )

        outcome, message = _parse_junit_outcome(junit_path)

        if outcome == "skipped":
            pytest.skip(message or "skipped in subprocess")

        if outcome == "xfail":
            pytest.xfail(message or "xfailed in subprocess")

        if outcome == "xpassed":
            # xpassed means the test unexpectedly passed — treat as a failure
            # so the outer session is notified rather than silently passing.
            raise AssertionError(
                f"Subprocess test unexpectedly passed (xpass): {message}"
            )

        if outcome in ("failed", "unknown") or result.returncode != 0:
            output = result.stdout + result.stderr
            lines = output.strip().splitlines()

            if not lines:
                excerpt = "<no output captured from child pytest process>"
            else:
                excerpt = "\n".join(lines)

            raise AssertionError(
                "Subprocess test failed\n"
                f"exit code: {result.returncode}\n"
                f"cwd: {item.config.rootpath}\n"
                f"command: {' '.join(cmd)}\n\n"
                f"Captured output:\n{excerpt}"
            )
    finally:
        try:
            os.unlink(junit_path)
        except OSError:
            pass
