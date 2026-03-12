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
Helper functions for observability integration tests.

All ``wait_for_*`` helpers use the mock receiver's ``data_received``
condition variable so they wake immediately when new data arrives
rather than polling with ``time.sleep``.
"""


def get_metric_data_points(receiver, metric_name):
    """Return the list of data-point dicts for a named metric across all exports."""
    all_points = []
    for m in receiver.get_metrics():
        if m.get("name") != metric_name:
            continue
        # Counters use "sum", gauges use "gauge", histograms use "histogram"
        for kind in ("sum", "gauge", "histogram"):
            section = m.get(kind, {})
            pts = section.get("dataPoints", [])
            all_points.extend(pts)
    return all_points


def wait_for_metric(receiver, metric_name, timeout=10):
    """Block until *metric_name* appears in the receiver (or *timeout* expires)."""
    with receiver.data_received:
        return receiver.data_received.wait_for(
            lambda: metric_name in receiver.get_metric_names(),
            timeout=timeout,
        )


def wait_for_metric_value(receiver, metric_name, predicate, timeout=10):
    """Block until a data point for *metric_name* satisfies *predicate*."""

    def _check():
        for dp in get_metric_data_points(receiver, metric_name):
            value = int(dp.get("asInt", dp.get("asDouble", 0)))
            if predicate(value):
                return True
        return False

    with receiver.data_received:
        return receiver.data_received.wait_for(_check, timeout=timeout)


def wait_for_span(receiver, span_name, timeout=10):
    """Block until *span_name* appears in the receiver (or *timeout* expires)."""
    with receiver.data_received:
        return receiver.data_received.wait_for(
            lambda: span_name in receiver.get_span_names(),
            timeout=timeout,
        )


def wait_for_log_records(receiver, min_count=1, timeout=10):
    """Block until at least *min_count* log records have arrived."""
    with receiver.data_received:
        return receiver.data_received.wait_for(
            lambda: len(receiver.get_log_records()) >= min_count,
            timeout=timeout,
        )
