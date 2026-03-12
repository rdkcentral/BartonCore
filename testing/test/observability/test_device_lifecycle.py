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
Integration tests for device lifecycle observability metrics.

Verifies that device discovery, add, and active-count metrics are
exported when devices are commissioned and managed.
"""

from testing.helpers.observability import (
    get_metric_data_points,
    wait_for_metric,
    wait_for_metric_value,
)


def _commission_and_wait(default_environment, matter_light, otlp_receiver):
    """Commission a device and wait for metrics to flush."""
    default_environment.get_client().commission_device(
        matter_light.get_commissioning_code(), 100
    )
    default_environment.wait_for_device_added()


def test_device_active_count_gauge(otlp_receiver, default_environment, matter_light):
    """Commissioning a device should export a device.active.count gauge > 0."""
    _commission_and_wait(default_environment, matter_light, otlp_receiver)
    assert wait_for_metric_value(
        otlp_receiver, "device.active.count", lambda v: v > 0
    ), f"Expected device.active.count > 0, got: {get_metric_data_points(otlp_receiver, 'device.active.count')}"


def test_device_discovery_counters(otlp_receiver, default_environment, matter_light):
    """Commissioning should export device.discovery.completed and device.discovered.count counters."""
    _commission_and_wait(default_environment, matter_light, otlp_receiver)
    assert wait_for_metric(
        otlp_receiver, "device.discovery.completed"
    ), f"Expected 'device.discovery.completed', got: {otlp_receiver.get_metric_names()}"
    assert (
        "device.discovered.count" in otlp_receiver.get_metric_names()
    ), f"Expected 'device.discovered.count', got: {otlp_receiver.get_metric_names()}"


def test_device_add_success_counter(otlp_receiver, default_environment, matter_light):
    """Successfully commissioning a device should increment device.add.success."""
    _commission_and_wait(default_environment, matter_light, otlp_receiver)
    assert wait_for_metric(
        otlp_receiver, "device.add.success"
    ), f"Expected 'device.add.success', got: {otlp_receiver.get_metric_names()}"


def test_device_discovered_count(otlp_receiver, default_environment, matter_light):
    """Commissioning should increment device.discovered.count."""
    _commission_and_wait(default_environment, matter_light, otlp_receiver)
    assert wait_for_metric(
        otlp_receiver, "device.discovered.count"
    ), f"Expected 'device.discovered.count', got: {otlp_receiver.get_metric_names()}"


def test_device_discovery_duration_histogram(
    otlp_receiver, default_environment, matter_light
):
    """Commissioning should export a device.discovery.duration histogram."""
    _commission_and_wait(default_environment, matter_light, otlp_receiver)
    assert wait_for_metric(
        otlp_receiver, "device.discovery.duration"
    ), f"Expected 'device.discovery.duration', got: {otlp_receiver.get_metric_names()}"
    data_points = get_metric_data_points(otlp_receiver, "device.discovery.duration")
    assert len(data_points) > 0, "Histogram should have at least one data point"
