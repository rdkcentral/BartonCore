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
Integration tests for OpenTelemetry log bridge exports.

Verifies that icLog messages emitted by the device service are
forwarded to the OTel log bridge and exported as OTLP log records.
"""

from testing.helpers.observability import wait_for_log_records


def test_log_records_exported_during_startup(otlp_receiver, default_environment):
    """Startup should produce at least one OTLP log record."""
    assert wait_for_log_records(otlp_receiver, min_count=1), (
        "Expected at least one log record during startup"
    )


def test_log_records_have_non_empty_body(otlp_receiver, default_environment):
    """Exported log records should contain a non-empty body."""
    assert wait_for_log_records(otlp_receiver, min_count=1)

    for record in otlp_receiver.get_log_records():
        body = record.get("body", {})
        # OTLP log body is an AnyValue; check the stringValue variant
        string_val = body.get("stringValue", "")
        if string_val:
            return  # at least one record has a body — pass
    raise AssertionError("No log record had a non-empty stringValue body")
