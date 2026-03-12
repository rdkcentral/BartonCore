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
Integration tests for internal service observability metrics.

Verifies that driver registration, storage, and event metrics are
exported during normal device service operation.
"""

from testing.helpers.observability import (
    get_metric_data_points,
    wait_for_metric,
    wait_for_metric_value,
)


# ---------------------------------------------------------------------------
# Startup-only tests (no device commissioning required)
# ---------------------------------------------------------------------------


def test_driver_registered_count(otlp_receiver, default_environment):
    """Startup should export driver.registered.count counter."""
    assert wait_for_metric(
        otlp_receiver, "driver.registered.count"
    ), f"Expected 'driver.registered.count', got: {otlp_receiver.get_metric_names()}"


def test_driver_init_success(otlp_receiver, default_environment):
    """Startup should export driver.init.success counter."""
    assert wait_for_metric(
        otlp_receiver, "driver.init.success"
    ), f"Expected 'driver.init.success', got: {otlp_receiver.get_metric_names()}"


def test_device_commfail_current_gauge(otlp_receiver, default_environment):
    """Startup should export device.commfail.current gauge."""
    assert wait_for_metric(
        otlp_receiver, "device.commfail.current"
    ), f"Expected 'device.commfail.current', got: {otlp_receiver.get_metric_names()}"


# ---------------------------------------------------------------------------
# Device-commission tests (require matter_light)
# ---------------------------------------------------------------------------


def _commission_and_wait(default_environment, matter_light, otlp_receiver):
    """Commission a device and wait for related metrics to arrive."""
    default_environment.get_client().commission_device(
        matter_light.get_commissioning_code(), 100
    )
    default_environment.wait_for_device_added()
    wait_for_metric(otlp_receiver, "storage.device.persist")


def test_storage_device_persist_on_add(
    otlp_receiver, default_environment, matter_light
):
    """Adding a device should export storage.device.persist counter."""
    _commission_and_wait(default_environment, matter_light, otlp_receiver)
    assert "storage.device.persist" in otlp_receiver.get_metric_names()


def test_storage_device_count_gauge(
    otlp_receiver, default_environment, matter_light
):
    """After adding a device, storage.device.count gauge should be > 0."""
    _commission_and_wait(default_environment, matter_light, otlp_receiver)
    assert wait_for_metric_value(
        otlp_receiver, "storage.device.count", lambda v: v > 0
    ), f"Expected storage.device.count > 0, got: {get_metric_data_points(otlp_receiver, 'storage.device.count')}"


def test_event_produced_on_device_add(
    otlp_receiver, default_environment, matter_light
):
    """Adding a device should produce events tracked by event.produced counter."""
    _commission_and_wait(default_environment, matter_light, otlp_receiver)
    assert wait_for_metric(
        otlp_receiver, "event.produced"
    ), f"Expected 'event.produced', got: {otlp_receiver.get_metric_names()}"
