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
Integration tests for device telemetry observability metrics.

Verifies that device resource telemetry, Matter subscription lifecycle,
and CASE session metrics are emitted when Matter devices are
commissioned and interacted with.
"""

from testing.helpers.observability import (
    get_metric_data_points,
    wait_for_metric,
    wait_for_metric_value,
)


def _commission_and_wait(default_environment, matter_light, otlp_receiver):
    """Commission a Matter light fixture and wait for device add to complete."""
    default_environment.get_client().commission_device(
        matter_light.get_commissioning_code(), 100
    )
    default_environment.wait_for_device_added()


def test_resource_update_counter_after_commissioning(
    otlp_receiver, default_environment, matter_light
):
    """Commissioning a device should emit device.resource.update counter."""
    _commission_and_wait(default_environment, matter_light, otlp_receiver)
    assert wait_for_metric(
        otlp_receiver, "device.resource.update"
    ), f"Expected 'device.resource.update', got: {otlp_receiver.get_metric_names()}"


def test_subscription_established_after_commissioning(
    otlp_receiver, default_environment, matter_light
):
    """Commissioning a Matter device should emit matter.subscription.established."""
    _commission_and_wait(default_environment, matter_light, otlp_receiver)
    assert wait_for_metric(
        otlp_receiver, "matter.subscription.established"
    ), f"Expected 'matter.subscription.established', got: {otlp_receiver.get_metric_names()}"


def test_subscription_report_after_commissioning(
    otlp_receiver, default_environment, matter_light
):
    """Commissioning a Matter device should produce at least one subscription report."""
    _commission_and_wait(default_environment, matter_light, otlp_receiver)
    assert wait_for_metric(
        otlp_receiver, "matter.subscription.report"
    ), f"Expected 'matter.subscription.report', got: {otlp_receiver.get_metric_names()}"


def test_case_session_duration_after_commissioning(
    otlp_receiver, default_environment, matter_light
):
    """Commissioning a Matter device should record a CASE session duration histogram."""
    _commission_and_wait(default_environment, matter_light, otlp_receiver)
    assert wait_for_metric(
        otlp_receiver, "matter.case.session.duration"
    ), f"Expected 'matter.case.session.duration', got: {otlp_receiver.get_metric_names()}"
