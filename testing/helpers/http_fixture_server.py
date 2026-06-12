# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2026 Comcast Cable Communications Management, LLC
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

import logging
import threading
from http.server import HTTPServer, SimpleHTTPRequestHandler
import pytest

logger = logging.getLogger(__name__)


class _LoggingHandler(SimpleHTTPRequestHandler):
    """HTTP handler that tracks request counts per path."""

    def __init__(self, *args, request_log: dict, request_condition: threading.Condition, **kwargs):
        self._request_log = request_log
        self._request_condition = request_condition
        super().__init__(*args, **kwargs)

    def do_GET(self):
        with self._request_condition:
            self._request_log[self.path] = self._request_log.get(self.path, 0) + 1
            self._request_condition.notify_all()
        super().do_GET()

    def log_message(self, format, *args):
        logger.debug(format, *args)


class FixtureHTTPServer:
    """A simple HTTP server that serves files from a directory on an OS-assigned port."""

    def __init__(self, serve_directory: str):
        self._serve_directory = str(serve_directory)
        self._request_log: dict[str, int] = {}
        self._request_condition = threading.Condition()

        handler_factory = lambda *args, **kwargs: _LoggingHandler(
            *args, request_log=self._request_log, request_condition=self._request_condition,
            directory=self._serve_directory, **kwargs
        )

        self._server = HTTPServer(("127.0.0.1", 0), handler_factory)
        self._thread = threading.Thread(target=self._server.serve_forever, daemon=True)
        self._thread.start()
        logger.info("HTTP fixture server started on port %d serving %s", self.port, self._serve_directory)

    @property
    def port(self) -> int:
        return self._server.server_address[1]

    @property
    def base_url(self) -> str:
        return f"http://127.0.0.1:{self.port}"

    def url_for(self, filename: str) -> str:
        return f"{self.base_url}/{filename}"

    def get_request_count(self, path: str) -> int:
        key = path if path.startswith("/") else f"/{path}"
        with self._request_condition:
            return self._request_log.get(key, 0)

    def wait_for_request_count(self, path: str, count: int, timeout: float = 10.0) -> bool:
        """Block until the request count for *path* reaches at least *count*.

        Returns True if the count was reached, False on timeout.
        """
        key = path if path.startswith("/") else f"/{path}"
        with self._request_condition:
            return self._request_condition.wait_for(
                lambda: self._request_log.get(key, 0) >= count,
                timeout=timeout,
            )

    def reset_request_log(self):
        with self._request_condition:
            self._request_log.clear()

    def shutdown(self):
        self._server.shutdown()
        self._thread.join(timeout=5)
        self._server.server_close()
        logger.info("HTTP fixture server stopped")


@pytest.fixture
def http_fixture_server(tmp_path):
    """Pytest fixture that starts an HTTP server serving from tmp_path.

    Yields a FixtureHTTPServer instance. Copy test fixture files into
    tmp_path before using.
    """
    server = FixtureHTTPServer(str(tmp_path))
    try:
        yield server
    finally:
        server.shutdown()
