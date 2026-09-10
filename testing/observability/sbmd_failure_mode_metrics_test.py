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

"""
SBMD observability metrics — handler failure mode tests.

These tests verify the outcome taxonomy and counter relationships when
handlers fail in different ways:

  - throwError resource   → outcome="exception"
  - busyLoop resource     → outcome="timeout"
  - errorResult resource  → outcome="error"

Key implementation facts from the spec (sbmd-runtime-observability):
  - sbmd.js.exception is ONLY for "init" and "loading" phase exceptions.
    Handler-phase exceptions are captured by handler.outcome, not by
    js.exception.  There is no phase="invocation" for js.exception.
  - stack_overflow is caught by JS_StackCheck() BEFORE JS_Call().  It records
    only handler.outcome{stack_overflow}.  No duration_ms, no heap_delta_bytes.
  - "error" is a controlled return value (Sbmd.result().error()), not a
    thrown exception, so the exception branch in InvokeHandler is not taken.
  - duration_ms and heap_delta_bytes ARE recorded for exception/timeout/error
    (JS_Call was reached); they are NOT recorded for stack_overflow.

All resources used here are defined in deferred-command-test.sbmd.js and
exercised via the matter_deferred_cmd_test_device fixture.
"""

import pytest

from testing.helpers.sbmd_metrics_helpers import (
    bucket_sum,
    find_datapoint,
    format_histogram,
    get_metrics,
)
from testing.utils.barton_utils import commission_device, resource_uri

pytestmark = [pytest.mark.requires_matterjs]

_DRIVER = "deferred-command-test"
_OP_TYPE = "execute"
_N = 5


def _commission(default_environment, matter_deferred_cmd_test_device):
    return commission_device(
        default_environment, matter_deferred_cmd_test_device, "deferredCmdTest"
    )


def test_exception_outcome_increments_correctly(
    default_environment, matter_deferred_cmd_test_device
):
    """
    _N calls to throwError produce handler.outcome{outcome="exception"} == _N.

    The spec states that handler-phase exceptions are captured by
    handler.outcome ONLY.  sbmd.js.exception is for init/loading phases and is
    NOT incremented by handler invocation failures.

    Also verifies that handler.duration_ms is recorded for the exception path
    (JS_Call was reached before the throw, so timing is valid).
    """
    device = _commission(default_environment, matter_deferred_cmd_test_device)
    client = default_environment.get_client()

    for _ in range(_N):
        client.execute_resource(
            resource_uri(device, "throwError", endpoint_id=1), "", ""
        )

    metrics = get_metrics(client)

    outcome_dp = find_datapoint(
        metrics, "sbmd.handler.outcome",
        driver=_DRIVER, op_type=_OP_TYPE, resource_id="throwError", outcome="exception",
    )
    dur_dp = find_datapoint(
        metrics, "sbmd.handler.duration_ms",
        driver=_DRIVER, op_type=_OP_TYPE, resource_id="throwError",
    )

    print(f"\n  handler.outcome{{exception}}: "
          f"{outcome_dp['value'] if outcome_dp else 'MISSING'}")
    if dur_dp:
        print(f"  handler.duration_ms (exception path):")
        print(format_histogram(dur_dp, unit="ms"))

    assert outcome_dp is not None, (
        f"handler.outcome{{exception}} not found for {_DRIVER}/{_OP_TYPE}/throwError"
    )
    assert outcome_dp["value"] == _N, (
        f"handler.outcome{{exception}} count={outcome_dp['value']}, expected {_N}"
    )

    # duration IS recorded: JS_Call was reached before the throw.
    assert dur_dp is not None, (
        "handler.duration_ms not recorded for exception path — "
        "RecordInvocation should run unconditionally after JS_Call."
    )
    assert dur_dp["count"] == _N


@pytest.mark.slow
def test_timeout_outcome_records_duration_in_high_latency_buckets(
    default_environment, matter_deferred_cmd_test_device
):
    """
    One call to busyLoop busy-loops until the SBMD script timeout fires
    (~5 s), producing handler.outcome{outcome="timeout"} == 1.

    The spec states that timeout outcomes are recorded only in handler.outcome;
    sbmd.js.exception is NOT incremented for handler-phase timeouts.

    The duration observation must land in a high-latency bucket (>= 5000 ms
    bound, index 12+) confirming the script timeout was the cause.

    This test takes ~5 s to complete.
    """
    device = _commission(default_environment, matter_deferred_cmd_test_device)
    client = default_environment.get_client()

    # execute_resource blocks until the handler returns (~5 s for busyLoop).
    client.execute_resource(resource_uri(device, "busyLoop", endpoint_id=1), "", "")

    metrics = get_metrics(client)

    timeout_dp = find_datapoint(
        metrics, "sbmd.handler.outcome",
        driver=_DRIVER, op_type=_OP_TYPE, resource_id="busyLoop", outcome="timeout",
    )
    dur_dp = find_datapoint(
        metrics, "sbmd.handler.duration_ms",
        driver=_DRIVER, op_type=_OP_TYPE, resource_id="busyLoop",
    )

    print(f"\n  handler.outcome{{timeout}}: "
          f"{timeout_dp['value'] if timeout_dp else 'MISSING'}")
    if dur_dp:
        print(f"  handler.duration_ms (timeout path, expect high-latency buckets):")
        print(format_histogram(dur_dp, unit="ms"))

    assert timeout_dp is not None, "handler.outcome{timeout} not recorded for busyLoop"
    assert timeout_dp["value"] == 1

    assert dur_dp is not None, "handler.duration_ms not recorded for timeout path"
    assert dur_dp["count"] == 1
    # Duration must be in a high-latency bucket (>= 5000 ms, bucket index 12+).
    high_latency = bucket_sum(dur_dp, 12, 16)
    print(f"\n  Observations in >=5000 ms buckets: {high_latency}")
    assert high_latency >= 1, (
        f"Timeout handler duration not in expected high-latency buckets. "
        f"Bucket distribution: {dur_dp.get('buckets')}, max={dur_dp.get('max')} ms"
    )


def test_error_result_outcome_does_not_produce_exception_outcome(
    default_environment, matter_deferred_cmd_test_device
):
    """
    _N calls to errorResult (returns Sbmd.result().error()) produce:
      handler.outcome{outcome="error"}  == _N

    "error" is a controlled return value (ResultTerminal::Error), not a thrown
    exception.  Per the spec, handler.outcome increments with outcome="error"
    only after SbmdResultExecutor::Parse() returns and identifies the Error
    terminal.  This is distinct from outcome="exception" (thrown JS exception).
    """
    device = _commission(default_environment, matter_deferred_cmd_test_device)
    client = default_environment.get_client()

    for _ in range(_N):
        client.execute_resource(
            resource_uri(device, "errorResult", endpoint_id=1), "", ""
        )

    metrics = get_metrics(client)

    error_dp = find_datapoint(
        metrics, "sbmd.handler.outcome",
        driver=_DRIVER, op_type=_OP_TYPE, resource_id="errorResult", outcome="error",
    )
    exception_dp = find_datapoint(
        metrics, "sbmd.handler.outcome",
        driver=_DRIVER, op_type=_OP_TYPE, resource_id="errorResult", outcome="exception",
    )
    dur_dp = find_datapoint(
        metrics, "sbmd.handler.duration_ms",
        driver=_DRIVER, op_type=_OP_TYPE, resource_id="errorResult",
    )

    print(f"\n  handler.outcome{{error}}: "
          f"{error_dp['value'] if error_dp else 'MISSING'}")
    print(f"  handler.outcome{{exception}} (should be absent): "
          f"{exception_dp['value'] if exception_dp else 'absent — correct'}")
    if dur_dp:
        print(f"  handler.duration_ms (error result path):")
        print(format_histogram(dur_dp, unit="ms"))

    assert error_dp is not None, (
        f"handler.outcome{{error}} not found for {_DRIVER}/{_OP_TYPE}/errorResult"
    )
    assert error_dp["value"] == _N, (
        f"handler.outcome{{error}} count={error_dp['value']}, expected {_N}"
    )

    assert exception_dp is None, (
        f"handler.outcome{{exception}} was recorded for a controlled error result — "
        f"'error' and 'exception' are distinct outcome labels per the spec."
    )

    # duration IS recorded: JS_Call returned normally.
    assert dur_dp is not None, (
        "handler.duration_ms not recorded for error result path"
    )
    assert dur_dp["count"] == _N
