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
Integration tests for Matter subsystem observability metrics.

Verifies that Matter commissioning and pairing metrics are exported
when devices are commissioned through the Matter subsystem.
"""

from testing.helpers.observability import (
    get_metric_data_points,
    wait_for_metric,
)


def _commission_and_wait(default_environment, matter_light, otlp_receiver):
    """Commission a device and wait for metrics to flush."""
    default_environment.get_client().commission_device(
        matter_light.get_commissioning_code(), 100
    )
    default_environment.wait_for_device_added()


def test_matter_commissioning_attempt(otlp_receiver, default_environment, matter_light):
    """Commissioning a device should export matter.commissioning.attempt."""
    _commission_and_wait(default_environment, matter_light, otlp_receiver)
    assert wait_for_metric(
        otlp_receiver, "matter.commissioning.attempt"
    ), f"Expected 'matter.commissioning.attempt', got: {otlp_receiver.get_metric_names()}"


def test_matter_commissioning_success(
    otlp_receiver, default_environment, matter_light
):
    """Successful commissioning should export matter.commissioning.success."""
    _commission_and_wait(default_environment, matter_light, otlp_receiver)
    assert wait_for_metric(
        otlp_receiver, "matter.commissioning.success"
    ), f"Expected 'matter.commissioning.success', got: {otlp_receiver.get_metric_names()}"


def test_matter_commissioning_duration(
    otlp_receiver, default_environment, matter_light
):
    """Commissioning should export a matter.commissioning.duration histogram."""
    _commission_and_wait(default_environment, matter_light, otlp_receiver)
    assert wait_for_metric(
        otlp_receiver, "matter.commissioning.duration"
    ), f"Expected 'matter.commissioning.duration', got: {otlp_receiver.get_metric_names()}"
    data_points = get_metric_data_points(otlp_receiver, "matter.commissioning.duration")
    assert len(data_points) > 0, "Histogram should have at least one data point"


def test_matter_pairing_counters(otlp_receiver, default_environment, matter_light):
    """Commissioning should export matter.pairing.attempt and .success counters."""
    _commission_and_wait(default_environment, matter_light, otlp_receiver)
    assert wait_for_metric(
        otlp_receiver, "matter.pairing.attempt"
    ), f"Expected 'matter.pairing.attempt', got: {otlp_receiver.get_metric_names()}"
    assert wait_for_metric(
        otlp_receiver, "matter.pairing.success"
    ), f"Expected 'matter.pairing.success', got: {otlp_receiver.get_metric_names()}"


def test_matter_initializing_gauge(otlp_receiver, default_environment):
    """Startup should export the matter.initializing gauge."""
    assert wait_for_metric(
        otlp_receiver, "matter.initializing"
    ), f"Expected 'matter.initializing', got: {otlp_receiver.get_metric_names()}"
