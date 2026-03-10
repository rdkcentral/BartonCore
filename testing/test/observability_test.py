# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 Comcast Cable Communications Management, LLC
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
Integration tests for OpenTelemetry observability instrumentation.

These tests verify that when BCORE_OBSERVABILITY=ON, the device service
exports traces, metrics, and log records via OTLP/HTTP.

Requirements:
    - Build with -DBCORE_OBSERVABILITY=ON
    - The tests are skipped when the feature is not enabled
"""

import logging
import os
import time

import pytest

from testing.conftest import requires_otel
from testing.helpers.otlp_mock_receiver import OtlpMockReceiver

logger = logging.getLogger(__name__)


@pytest.fixture
def otlp_receiver():
    """Start a mock OTLP HTTP receiver and set the endpoint env var."""
    receiver = OtlpMockReceiver()
    receiver.start()
    old_endpoint = os.environ.get("OTEL_EXPORTER_OTLP_ENDPOINT")
    os.environ["OTEL_EXPORTER_OTLP_ENDPOINT"] = receiver.endpoint()
    logger.info("OTLP receiver at %s", receiver.endpoint())
    yield receiver
    receiver.stop()
    # Restore the original env var
    if old_endpoint is not None:
        os.environ["OTEL_EXPORTER_OTLP_ENDPOINT"] = old_endpoint
    else:
        os.environ.pop("OTEL_EXPORTER_OTLP_ENDPOINT", None)


# --------------------------------------------------------------------------
# Task 7.3: Trace spans exported during startup and discovery
# --------------------------------------------------------------------------

@requires_otel
def test_startup_exports_subsystem_init_span(otlp_receiver, default_environment):
    """
    Verify that starting BCore.Client with OTEL enabled exports a
    'subsystem.init' span to the OTLP receiver.

    The default_environment fixture starts the client and waits until
    it is ready. By that point, subsystem.init should have completed
    and been exported.
    """
    # Give the OTLP exporter a moment to flush
    time.sleep(2)

    span_names = otlp_receiver.get_span_names()
    assert "subsystem.init" in span_names, (
        f"Expected 'subsystem.init' span in exported spans, got: {span_names}"
    )


@requires_otel
def test_discovery_exports_device_discovery_span(
    otlp_receiver, default_environment, matter_light
):
    """
    Verify that commissioning a device exports 'device.discovery' and
    'device.found' spans.
    """
    default_environment.get_client().commission_device(
        matter_light.get_commissioning_code(), 100
    )
    default_environment.wait_for_device_added()

    # Give the OTLP exporter a moment to flush
    time.sleep(2)

    span_names = otlp_receiver.get_span_names()
    assert "device.discovery" in span_names, (
        f"Expected 'device.discovery' span, got: {span_names}"
    )


# --------------------------------------------------------------------------
# Task 7.4: Metrics exported after device operations
# --------------------------------------------------------------------------

@requires_otel
def test_device_active_count_metric_exported(
    otlp_receiver, default_environment, matter_light
):
    """
    Verify that the 'device.active.count' gauge metric is exported
    after adding a device.
    """
    default_environment.get_client().commission_device(
        matter_light.get_commissioning_code(), 100
    )
    default_environment.wait_for_device_added()

    # Give metric export cycle time to fire
    time.sleep(5)

    metric_names = otlp_receiver.get_metric_names()
    assert "device.active.count" in metric_names, (
        f"Expected 'device.active.count' metric, got: {metric_names}"
    )


# --------------------------------------------------------------------------
# Task 7.5: Log records exported via OTel log bridge
# --------------------------------------------------------------------------

@requires_otel
def test_log_records_exported_during_startup(otlp_receiver, default_environment):
    """
    Verify that icLog messages emitted during BCore startup are forwarded
    to the OTel log bridge and exported as OTLP log records.
    """
    # Give the exporter time to flush
    time.sleep(2)

    log_records = otlp_receiver.get_log_records()
    assert len(log_records) > 0, "Expected at least one log record during startup"

    # Check that at least one record has a body (the formatted log message)
    bodies = [r.get("body", {}).get("stringValue", "") for r in log_records]
    non_empty = [b for b in bodies if b]
    assert len(non_empty) > 0, "Expected at least one log record with a non-empty body"
