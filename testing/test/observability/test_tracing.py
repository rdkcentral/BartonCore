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
Integration tests for OpenTelemetry trace span exports.

Verifies that instrumented code paths export the expected spans
via OTLP/HTTP to the mock receiver.
"""

from testing.helpers.observability import wait_for_span


# ---------------------------------------------------------------------------
# Startup-only span tests
# ---------------------------------------------------------------------------


def test_startup_exports_subsystem_init_span(otlp_receiver, default_environment):
    """Starting the client should export a 'subsystem.init' span."""
    assert wait_for_span(otlp_receiver, "subsystem.init"), (
        f"Expected 'subsystem.init' span, got: {otlp_receiver.get_span_names()}"
    )


def test_startup_exports_subsystem_shutdown_span_absent(
    otlp_receiver, default_environment
):
    """At startup (before teardown), 'subsystem.shutdown' span should NOT appear."""
    # Give the exporter a moment to flush any pending spans, but don't
    # wait the full polling timeout — we're asserting absence.
    import time

    time.sleep(2)
    assert "subsystem.shutdown" not in otlp_receiver.get_span_names(), (
        "Unexpected 'subsystem.shutdown' span before client teardown"
    )


# ---------------------------------------------------------------------------
# Device-commission span tests
# ---------------------------------------------------------------------------


def test_device_found_span_has_attributes(
    otlp_receiver, default_environment, matter_light
):
    """Commissioning a device should export a 'device.found' span with device attributes."""
    default_environment.get_client().commission_device(
        matter_light.get_commissioning_code(), 100
    )
    default_environment.wait_for_device_added()

    assert wait_for_span(otlp_receiver, "device.found"), (
        f"Expected 'device.found' span, got: {otlp_receiver.get_span_names()}"
    )
    # Verify the span carries device metadata attributes
    found_spans = [s for s in otlp_receiver.get_spans() if s.get("name") == "device.found"]
    assert len(found_spans) > 0
    attrs = {
        a["key"]: a["value"]
        for a in found_spans[0].get("attributes", [])
    }
    assert "device.class" in attrs, f"Expected 'device.class' attribute, got: {attrs}"


def test_discovery_exports_device_found_span(
    otlp_receiver, default_environment, matter_light
):
    """Commissioning a device should export a 'device.found' span."""
    default_environment.get_client().commission_device(
        matter_light.get_commissioning_code(), 100
    )
    default_environment.wait_for_device_added()

    assert wait_for_span(otlp_receiver, "device.found"), (
        f"Expected 'device.found' span, got: {otlp_receiver.get_span_names()}"
    )


def test_commission_span_parent_child_hierarchy(
    otlp_receiver, default_environment, matter_light
):
    """Commissioning should produce device.commission → device.found parent-child spans."""
    default_environment.get_client().commission_device(
        matter_light.get_commissioning_code(), 100
    )
    default_environment.wait_for_device_added()

    assert wait_for_span(otlp_receiver, "device.commission"), (
        f"Expected 'device.commission' span, got: {otlp_receiver.get_span_names()}"
    )
    assert wait_for_span(otlp_receiver, "device.found"), (
        f"Expected 'device.found' span, got: {otlp_receiver.get_span_names()}"
    )

    spans = otlp_receiver.get_spans()
    commission_spans = [s for s in spans if s.get("name") == "device.commission"]
    found_spans = [s for s in spans if s.get("name") == "device.found"]
    assert len(commission_spans) > 0
    assert len(found_spans) > 0

    # Both should share the same trace ID
    assert commission_spans[0]["traceId"] == found_spans[0]["traceId"], (
        "device.commission and device.found should share the same traceId"
    )


def test_commission_produces_matter_subsystem_spans(
    otlp_receiver, default_environment, matter_light
):
    """Commissioning should produce matter.case.connect and matter.subscribe spans."""
    default_environment.get_client().commission_device(
        matter_light.get_commissioning_code(), 100
    )
    default_environment.wait_for_device_added()

    assert wait_for_span(otlp_receiver, "matter.case.connect"), (
        f"Expected 'matter.case.connect' span, got: {otlp_receiver.get_span_names()}"
    )
    assert wait_for_span(otlp_receiver, "matter.subscribe"), (
        f"Expected 'matter.subscribe' span, got: {otlp_receiver.get_span_names()}"
    )
    # device.commission ends after the thread function returns, so it may arrive after the child spans
    assert wait_for_span(otlp_receiver, "device.commission"), (
        f"Expected 'device.commission' span, got: {otlp_receiver.get_span_names()}"
    )

    spans = otlp_receiver.get_spans()
    commission_spans = [s for s in spans if s.get("name") == "device.commission"]
    case_spans = [s for s in spans if s.get("name") == "matter.case.connect"]
    subscribe_spans = [s for s in spans if s.get("name") == "matter.subscribe"]
    assert len(commission_spans) > 0
    assert len(case_spans) > 0
    assert len(subscribe_spans) > 0

    # All Matter subsystem spans should share the same trace ID as device.commission
    trace_id = commission_spans[0]["traceId"]
    assert case_spans[0]["traceId"] == trace_id, (
        "matter.case.connect should share traceId with device.commission"
    )
    assert subscribe_spans[0]["traceId"] == trace_id, (
        "matter.subscribe should share traceId with device.commission"
    )
