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
Lightweight mock OTLP HTTP receiver for integration tests.

Accepts OTLP/HTTP+JSON exports on the standard paths and stores the
decoded payloads for later assertion. Uses only the Python standard library
so no extra dependencies are needed.

A ``threading.Condition`` (``data_received``) is notified on every
incoming POST so that test helpers can *wait* instead of *poll*.

Endpoints handled:
    POST /v1/traces  — trace spans
    POST /v1/metrics — metrics
    POST /v1/logs    — log records

The ``otlp_receiver`` pytest fixture lives here so that the mock and
its fixture are co-located.
"""

import json
import logging
import os
import threading
from http.server import HTTPServer, BaseHTTPRequestHandler

import pytest

logger = logging.getLogger(__name__)


class _OtlpHandler(BaseHTTPRequestHandler):
    """HTTP request handler that captures OTLP JSON payloads."""

    def do_POST(self):
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length) if content_length > 0 else b""

        try:
            payload = json.loads(body) if body else {}
        except (json.JSONDecodeError, UnicodeDecodeError):
            # OTLP protobuf — we only support JSON for now; store raw bytes
            payload = {"_raw": body.hex()}

        with self.server.data_received:
            if self.path == "/v1/traces":
                self.server.captured_spans.append(payload)
            elif self.path == "/v1/metrics":
                self.server.captured_metrics.append(payload)
            elif self.path == "/v1/logs":
                self.server.captured_logs.append(payload)
            else:
                logger.warning("OtlpMockReceiver: unexpected path %s", self.path)
            self.server.data_received.notify_all()

        # Return 200 OK with empty JSON body (OTLP success response)
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(b"{}")

    # Suppress default stderr logging
    def log_message(self, format, *args):
        logger.debug("OtlpMockReceiver: %s", format % args)


class OtlpMockReceiver:
    """
    A lightweight OTLP/HTTP receiver that runs in a background thread.

    All captured payloads are stored as decoded JSON dicts. Helper methods
    extract the inner span/metric/log record lists.

    ``data_received`` is a ``threading.Condition`` that is notified every
    time an OTLP export arrives. Test helpers should use it via
    ``data_received.wait_for(predicate, timeout)`` instead of polling
    with ``time.sleep``.
    """

    def __init__(self):
        self._server = None
        self._thread = None
        self.data_received = threading.Condition()

    def start(self):
        """Start the receiver on a random free port."""
        # Bind to port 0 to let the OS pick a free port
        self._server = HTTPServer(("127.0.0.1", 0), _OtlpHandler)
        self._server.data_received = self.data_received
        self._server.captured_spans = []
        self._server.captured_metrics = []
        self._server.captured_logs = []

        self._thread = threading.Thread(target=self._server.serve_forever, daemon=True)
        self._thread.start()
        logger.info("OtlpMockReceiver started on %s", self.endpoint())

    def stop(self):
        """Stop the receiver."""
        if self._server:
            self._server.shutdown()
            self._thread.join(timeout=5)
            self._server.server_close()
            logger.info("OtlpMockReceiver stopped")

    def endpoint(self) -> str:
        """Return the base URL, e.g. 'http://127.0.0.1:54321'."""
        host, port = self._server.server_address
        return f"http://{host}:{port}"

    # ---- raw payload accessors ----

    def get_raw_trace_payloads(self) -> list:
        """Return the list of raw OTLP trace export JSON payloads."""
        return list(self._server.captured_spans)

    def get_raw_metric_payloads(self) -> list:
        """Return the list of raw OTLP metric export JSON payloads."""
        return list(self._server.captured_metrics)

    def get_raw_log_payloads(self) -> list:
        """Return the list of raw OTLP log export JSON payloads."""
        return list(self._server.captured_logs)

    # ---- flattened record accessors ----

    def get_spans(self) -> list:
        """
        Return a flat list of span dicts extracted from all captured
        trace export payloads.

        The OTLP JSON structure is:
            { "resourceSpans": [ { "scopeSpans": [ { "spans": [ {...}, ... ] } ] } ] }
        """
        spans = []
        for payload in self._server.captured_spans:
            for rs in payload.get("resourceSpans", []):
                for ss in rs.get("scopeSpans", []):
                    spans.extend(ss.get("spans", []))
        return spans

    def get_metrics(self) -> list:
        """
        Return a flat list of metric dicts extracted from all captured
        metric export payloads.

        The OTLP JSON structure is:
            { "resourceMetrics": [ { "scopeMetrics": [ { "metrics": [ {...}, ... ] } ] } ] }
        """
        metrics = []
        for payload in self._server.captured_metrics:
            for rm in payload.get("resourceMetrics", []):
                for sm in rm.get("scopeMetrics", []):
                    metrics.extend(sm.get("metrics", []))
        return metrics

    def get_log_records(self) -> list:
        """
        Return a flat list of log record dicts extracted from all captured
        log export payloads.

        The OTLP JSON structure is:
            { "resourceLogs": [ { "scopeLogs": [ { "logRecords": [ {...}, ... ] } ] } ] }
        """
        records = []
        for payload in self._server.captured_logs:
            for rl in payload.get("resourceLogs", []):
                for sl in rl.get("scopeLogs", []):
                    records.extend(sl.get("logRecords", []))
        return records

    def get_span_names(self) -> list:
        """Return a list of span names from all captured spans."""
        return [s.get("name", "") for s in self.get_spans()]

    def get_metric_names(self) -> list:
        """Return a list of metric names from all captured metrics."""
        return [m.get("name", "") for m in self.get_metrics()]


# ---------------------------------------------------------------------------
# Pytest fixture
# ---------------------------------------------------------------------------

@pytest.fixture
def otlp_receiver():
    """Start a mock OTLP HTTP receiver and set OTEL endpoint env vars.

    This MUST be listed before ``default_environment`` in test signatures so
    that the exporter endpoint is configured before the Barton client starts.

    Export intervals are set as low as the SDK allows so that telemetry
    arrives at the mock receiver almost immediately:

    * Batch span/log processors: ``schedule_delay_millis`` reads from
      ``OTEL_BSP_SCHEDULE_DELAY_MS`` / ``OTEL_BLRP_SCHEDULE_DELAY_MS``.
      The SDK accepts 0 ms (the thread still blocks on a condvar, so it
      just means "export as soon as the batch-size threshold is reached").
      Combined with ``max_export_batch_size=1`` in the C++ init code this
      gives near-instant export.

    * Periodic metric reader: ``export_interval_millis`` **must** be
      strictly greater than ``export_timeout_millis`` or the SDK falls
      back to 60 s / 30 s defaults.  Setting the interval to 100 ms
      (timeout computed as interval/2 = 50 ms) is the practical minimum.
    """
    receiver = OtlpMockReceiver()
    receiver.start()
    saved = {}
    env_overrides = {
        "OTEL_EXPORTER_OTLP_ENDPOINT": receiver.endpoint(),
        "OTEL_EXPORTER_OTLP_PROTOCOL": "http/json",
        "OTEL_METRIC_EXPORT_INTERVAL_MS": "100",
        "OTEL_BSP_SCHEDULE_DELAY_MS": "0",
        "OTEL_BSP_MAX_EXPORT_BATCH_SIZE": "1",
        "OTEL_BLRP_SCHEDULE_DELAY_MS": "0",
        "OTEL_BLRP_MAX_EXPORT_BATCH_SIZE": "1",
    }
    for key, val in env_overrides.items():
        saved[key] = os.environ.get(key)
        os.environ[key] = val
    logger.info("OTLP receiver at %s", receiver.endpoint())
    yield receiver
    receiver.stop()
    for key, old_val in saved.items():
        if old_val is not None:
            os.environ[key] = old_val
        else:
            os.environ.pop(key, None)
