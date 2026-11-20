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
import math
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET

import pytest

logger = logging.getLogger(__name__)

def pytest_configure(config):
    """Register custom markers."""
    config.addinivalue_line(
        "markers",
        "requires_matterjs: skip test when Node.js or matter.js is not available",
    )
    config.addinivalue_line(
        "markers",
        "slow: marks tests that take more than 10 seconds. Purely informational — "
        "has no effect on CI test selection (see the ci_gate marker for that).",
    )
    config.addinivalue_line(
        "markers",
        "observability_suite: auto-applied to every test under testing/observability/ "
        "by path (see pytest_collection_modifyitems below). Not meant to be applied "
        "manually.",
    )
    config.addinivalue_line(
        "markers",
        "ci_gate(): marks an observability_suite test as safe for CI to also run as "
        "part of the default integration test selection — i.e. its assertions do not "
        "depend on the mquickjs DEBUG_GC build state and it does not require heavy, "
        "multi-subprocess fixtures. See testing/observability/README.md for the "
        "decision criteria before adding this marker to a new test.",
    )
    config.addinivalue_line(
        "markers",
        "sbmd_report(scenario, metrics, client_fixture='default_environment'): marks a "
        "test as contributing its own pass/fail result to the consolidated SBMD metrics "
        "report via the sbmd_report fixture. 'scenario' names the report entry; "
        "'metrics' is the list of metric names to sample; 'client_fixture' overrides "
        "which fixture to call .get_client() on if not default_environment.",
    )

    # Assign one report run id in the parent process (not the per-test
    # subprocesses, which inherit it via the environment) so every reporting
    # scenario from this invocation lands in the same consolidated artifact.
    # Nanosecond resolution (not whole seconds) avoids two rapid invocations
    # colliding on the same id, which would make _load() reuse a stale report.
    if not os.environ.get(_SUBPROCESS_MARKER_ENV):
        os.environ["SBMD_REPORT_RUN_ID"] = str(time.time_ns())
        _install_termination_handlers()
    else:
        # The outer process starts this child with SIGINT/SIGHUP/SIGTERM blocked
        # (see _run_in_subprocess), and a blocked mask survives exec. Unblock
        # them so this test, and the device processes it starts, get normal
        # signal behavior.
        signal.pthread_sigmask(signal.SIG_UNBLOCK, _child_start_blocked_signums())


# Each test's child pytest runs in its own session (see _run_in_subprocess), so
# a terminal hangup or SIGTERM aimed at this outer process no longer reaches it.
# Turn those signals into KeyboardInterrupt here, which runs the same cleanup
# path as Ctrl-C: kill the child's process group, then exit.
_TERMINATION_SIGNALS = ("SIGHUP", "SIGTERM")
_previous_signal_handlers = {}


def _termination_signums():
    return {getattr(signal, name) for name in _TERMINATION_SIGNALS if hasattr(signal, name)}


def _child_start_blocked_signums():
    """Signals held while a test's child process starts and while it is cleaned
    up: the termination signals plus SIGINT, all of which raise
    KeyboardInterrupt in this process."""
    return _termination_signums() | {signal.SIGINT}


def _raise_keyboard_interrupt(signum, _frame):
    raise KeyboardInterrupt(f"received {signal.Signals(signum).name}")


def _install_termination_handlers():
    for name in _TERMINATION_SIGNALS:
        signum = getattr(signal, name, None)
        # Leave the signal alone if someone else already handles or ignores it
        # (e.g. SIG_IGN under nohup).
        if signum is None or signal.getsignal(signum) is not signal.SIG_DFL:
            continue
        _previous_signal_handlers[signum] = signal.signal(signum, _raise_keyboard_interrupt)


def pytest_unconfigure(config):
    for signum, handler in _previous_signal_handlers.items():
        signal.signal(signum, handler)
    _previous_signal_handlers.clear()


# The following list of plugins are automatically loaded by pytest when running tests.
# Any fixtures defined within these modules are automatically available to all test modules.
pytest_plugins = [
    # Environments
    "testing.environment.default_environment_orchestrator",
    "testing.environment.descriptor_environment_orchestrator",
    "testing.environment.onvif_environment_orchestrator",
    # Helpers
    "testing.helpers.http_fixture_server",
    # Mocks
    "testing.mocks.device_descriptor_server",
    "testing.mocks.onvif_camera_server",
    # Devices
    "testing.mocks.devices.matter.matter_light",
    "testing.mocks.devices.matter.matter_door_lock",
    "testing.mocks.devices.matter.matter_deferred_cmd_test_device",
    "testing.mocks.devices.matter.matter_thermostat",
    "testing.mocks.devices.matter.matter_thermostat_with_fan",
    "testing.mocks.devices.matter.matter_ikea_timmerflotte",
    "testing.mocks.devices.matter.matter_temperature_sensor",
    "testing.mocks.devices.matter.matter_humidity_sensor",
    "testing.mocks.devices.matter.matter_contact_sensor",
    "testing.mocks.devices.matter.matter_contact_sensor_no_event",
    "testing.mocks.devices.matter.matter_water_leak_detector",
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


_OBSERVABILITY_DIR = os.path.join(os.path.dirname(__file__), "observability")


def pytest_collection_modifyitems(config, items):
    skip_marker = pytest.mark.skip(
        reason="Requires Node.js and matter.js (not available)"
    )

    for item in items:
        # Path-based, not opt-in: every test collected from testing/observability/
        # is an observability_suite test, full stop — see the ci_gate marker docs
        # for how a subset of these also run as part of CI.
        if str(item.fspath).startswith(_OBSERVABILITY_DIR + os.sep):
            item.add_marker(pytest.mark.observability_suite)

        if not _has_matterjs and "requires_matterjs" in item.keywords:
            item.add_marker(skip_marker)


# ---------------------------------------------------------------------------
#  Per-test contribution to the consolidated SBMD metrics report.
#
#  A test opts in via @pytest.mark.sbmd_report(scenario=..., metrics=[...]) plus
#  requesting the sbmd_report fixture. The test body is unchanged otherwise: the
#  fixture's teardown reads the test's own outcome (stashed by the hookwrapper
#  below, a standard pytest idiom) and, unless reporting is disabled for this
#  invocation, records a report entry carrying that outcome alongside a fresh
#  sample of the named metrics. This runs inside the per-test subprocess (see
#  the subprocess-isolation section below), which sbmd_report_writer.py's
#  SBMD_REPORT_RUN_ID + fcntl locking already exists to support.
# ---------------------------------------------------------------------------


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item, call):
    outcome = yield
    rep = outcome.get_result()
    setattr(item, f"rep_{rep.when}", rep)


def _sbmd_report_enabled():
    return not os.environ.get("SBMD_REPORT_DISABLE")


def _skip_reason(rep):
    # A skip report's longrepr is a (path, lineno, reason) tuple.
    longrepr = rep.longrepr
    if isinstance(longrepr, tuple) and len(longrepr) == 3:
        return str(longrepr[2])
    return str(longrepr)


def _sbmd_check_outcome(node):
    """Return (status, message) for the sbmd_report entry: PASS, FAIL, or SKIP.
    A skipped test is reported as SKIP, never as FAIL."""
    rep_setup = getattr(node, "rep_setup", None)
    rep_call = getattr(node, "rep_call", None)

    if rep_call is not None:
        if rep_call.passed:
            return "PASS", ""
        if rep_call.skipped:
            return "SKIP", _skip_reason(rep_call)
        return "FAIL", str(rep_call.longrepr)

    if rep_setup is not None and rep_setup.skipped:
        return "SKIP", _skip_reason(rep_setup)

    if rep_setup is not None and rep_setup.failed:
        return "FAIL", str(rep_setup.longrepr)

    return "FAIL", ""


@pytest.fixture
def sbmd_report(request):
    """See the sbmd_report marker (registered in pytest_configure) for usage."""
    ctx = {}
    yield ctx

    marker = request.node.get_closest_marker("sbmd_report")
    if marker is None or not _sbmd_report_enabled():
        return

    from testing.helpers.sbmd_metrics_helpers import get_metrics
    from testing.helpers.sbmd_report_writer import collect, emit_report

    status, message = _sbmd_check_outcome(request.node)

    metric_names = marker.kwargs.get("metrics", [])
    if metric_names:
        client_fixture = marker.kwargs.get("client_fixture", "default_environment")
        client = request.getfixturevalue(client_fixture).get_client()
        metrics = collect(get_metrics(client), metric_names)
    else:
        metrics = {}

    emit_report(marker.kwargs["scenario"], {
        "context": {**ctx, "check_status": status, "check_message": message},
        "metrics": metrics,
    })


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

# Upper bound on one test's child pytest process, so a deadlocked test fails
# with a named node instead of hanging the whole run until the CI job timeout.
# Generous by default (the slowest scale tests take minutes); override with
# BARTON_TEST_TIMEOUT_S.
_DEFAULT_SUBPROCESS_TIMEOUT_S = 1800


def _subprocess_timeout_s():
    raw = os.environ.get("BARTON_TEST_TIMEOUT_S")
    if not raw:
        return _DEFAULT_SUBPROCESS_TIMEOUT_S
    try:
        value = float(raw)
    except ValueError:
        raise pytest.UsageError(f"BARTON_TEST_TIMEOUT_S must be a number of seconds, got {raw!r}")
    if not math.isfinite(value) or value <= 0:
        raise pytest.UsageError(f"BARTON_TEST_TIMEOUT_S must be a positive, finite number, got {raw!r}")
    return value


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
            item.nodeid,
        ]

        if os.environ.get("SBMD_VERBOSE"):
            cmd.append("--capture=no")

        # In CI, py_test.sh preloads libasan via LD_PRELOAD so Python/GI tests can
        # load ASAN-instrumented Barton libraries. Keep the environment intact for
        # this child pytest process.
        child_env = dict(os.environ)
        child_env[_SUBPROCESS_MARKER_ENV] = "1"

        timeout_s = _subprocess_timeout_s()

        # Own process group: on timeout or interrupt the whole group is killed,
        # including mock-device processes (matter.js etc.) the child started, so
        # none of them outlive the test and run alongside later tests. The child
        # is in its own session, so a terminal's SIGINT/SIGHUP no longer reaches
        # it; the finally below takes it down on every exit path this process
        # gets to run. Not covered: SIGKILL of this outer process (it gets no
        # chance to clean up), which leaves the child running until it exits on
        # its own. CI is unaffected because its container is discarded.
        #
        # The signal masks below apply to the calling thread only. That is
        # sufficient because the outer pytest runs single-threaded, so these
        # signals are always delivered to this thread.
        proc = None

        def _kill_group():
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass

        try:
            # Hold SIGINT/SIGHUP/SIGTERM while the child starts so their
            # KeyboardInterrupt can't fire after the fork but before `proc` is
            # assigned, leaving a child nothing would kill. A pending signal is
            # delivered when the mask is restored, with `proc` set.
            old_mask = signal.pthread_sigmask(signal.SIG_BLOCK, _child_start_blocked_signums())
            try:
                proc = subprocess.Popen(
                    cmd,
                    cwd=str(item.config.rootpath),
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    env=child_env,
                    start_new_session=True,
                )
            finally:
                signal.pthread_sigmask(signal.SIG_SETMASK, old_mask)

            try:
                stdout, stderr = proc.communicate(timeout=timeout_s)
            except subprocess.TimeoutExpired:
                _kill_group()
                try:
                    stdout, stderr = proc.communicate(timeout=30)
                except subprocess.TimeoutExpired:
                    # Something outside the group still holds the pipes; give up
                    # on the remaining output rather than hang.
                    stdout, stderr = "", ""

                partial = ((stdout or "") + (stderr or "")).strip()
                raise AssertionError(
                    f"Subprocess test timed out after {timeout_s:g}s and was killed "
                    f"(set BARTON_TEST_TIMEOUT_S to change the limit)\n"
                    f"command: {' '.join(cmd)}\n\n"
                    f"Captured output before the timeout:\n{partial or '<none>'}"
                ) from None
        finally:
            # Any exit while the child still runs (KeyboardInterrupt from Ctrl-C,
            # SIGHUP or SIGTERM, or anything else) kills its whole group. Hold
            # those signals while doing it, so a second Ctrl-C can't abort the
            # cleanup; a signal that lands before the mask is set is absorbed by
            # the retry, and one that arrives during cleanup is delivered when
            # the mask is restored, after the group is gone.
            while True:
                try:
                    cleanup_mask = signal.pthread_sigmask(
                        signal.SIG_BLOCK, _child_start_blocked_signums()
                    )
                    break
                except KeyboardInterrupt:
                    continue

            try:
                if proc is not None and proc.poll() is None:
                    _kill_group()
                    proc.wait()
            finally:
                signal.pthread_sigmask(signal.SIG_SETMASK, cleanup_mask)

        result = subprocess.CompletedProcess(cmd, proc.returncode, stdout, stderr)

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

        if os.environ.get("SBMD_VERBOSE"):
            output = result.stdout + result.stderr
            if output.strip():
                print(output, end="", flush=True)

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
