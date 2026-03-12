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
Integration tests for subsystem manager observability metrics.

Verifies that subsystem initialization metrics are exported when
the device service starts up and initializes its subsystems.
"""

from testing.helpers.observability import (
    get_metric_data_points,
    wait_for_metric,
    wait_for_metric_value,
)


def test_subsystem_init_started(otlp_receiver, default_environment):
    """Startup should export subsystem.init.started counter."""
    assert wait_for_metric(
        otlp_receiver, "subsystem.init.started"
    ), f"Expected 'subsystem.init.started', got: {otlp_receiver.get_metric_names()}"


def test_subsystem_init_completed(otlp_receiver, default_environment):
    """Startup should export subsystem.init.completed counter."""
    assert wait_for_metric(
        otlp_receiver, "subsystem.init.completed"
    ), f"Expected 'subsystem.init.completed', got: {otlp_receiver.get_metric_names()}"


def test_subsystem_initialized_count_gauge(otlp_receiver, default_environment):
    """After startup, subsystem.initialized.count gauge should be > 0."""
    assert wait_for_metric_value(
        otlp_receiver, "subsystem.initialized.count", lambda v: v > 0
    ), (
        f"Expected subsystem.initialized.count > 0, got: "
        f"{get_metric_data_points(otlp_receiver, 'subsystem.initialized.count')}"
    )


def test_subsystem_ready_for_devices_gauge(otlp_receiver, default_environment):
    """After startup completes, subsystem.ready_for_devices gauge should be exported."""
    assert wait_for_metric(
        otlp_receiver, "subsystem.ready_for_devices"
    ), f"Expected 'subsystem.ready_for_devices', got: {otlp_receiver.get_metric_names()}"


def test_subsystem_init_duration_histogram(otlp_receiver, default_environment):
    """Startup should export subsystem.init.duration histogram."""
    assert wait_for_metric(
        otlp_receiver, "subsystem.init.duration"
    ), f"Expected 'subsystem.init.duration', got: {otlp_receiver.get_metric_names()}"
    data_points = get_metric_data_points(otlp_receiver, "subsystem.init.duration")
    assert len(data_points) > 0, "Histogram should have at least one data point"
