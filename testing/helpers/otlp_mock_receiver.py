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

Endpoints handled:
    POST /v1/traces  — trace spans
    POST /v1/metrics — metrics
    POST /v1/logs    — log records

Usage as a pytest fixture (see conftest.py):
    receiver = OtlpMockReceiver()
    receiver.start()          # binds to a random free port
    endpoint = receiver.endpoint()  # e.g. "http://127.0.0.1:54321"
    ...
    receiver.stop()
    spans  = receiver.get_spans()
    metrics = receiver.get_metrics()
    logs   = receiver.get_logs()
"""

import json
import logging
import socket
import threading
from http.server import HTTPServer, BaseHTTPRequestHandler

logger = logging.getLogger(__name__)


class _OtlpHandler(BaseHTTPRequestHandler):
    """HTTP request handler that captures OTLP JSON payloads."""

    def do_POST(self):
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length) if content_length > 0 else b""

        try:
            payload = json.loads(body) if body else {}
        except json.JSONDecodeError:
            # OTLP protobuf — we only support JSON for now; store raw bytes
            payload = {"_raw": body.hex()}

        if self.path == "/v1/traces":
            self.server.captured_spans.append(payload)
        elif self.path == "/v1/metrics":
            self.server.captured_metrics.append(payload)
        elif self.path == "/v1/logs":
            self.server.captured_logs.append(payload)
        else:
            logger.warning("OtlpMockReceiver: unexpected path %s", self.path)

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
    """

    def __init__(self):
        self._server = None
        self._thread = None

    def start(self):
        """Start the receiver on a random free port."""
        # Bind to port 0 to let the OS pick a free port
        self._server = HTTPServer(("127.0.0.1", 0), _OtlpHandler)
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
