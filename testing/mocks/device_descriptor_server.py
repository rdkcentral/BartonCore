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

"""
Mock device descriptor server for integration tests.

Serves AllowList.xml and DenyList.xml over HTTP via FixtureHTTPServer.
The descriptor environment orchestrator depends on this fixture to
configure allowlist/denylist URLs.

To supply custom descriptor files, parametrize indirectly::

    @pytest.mark.parametrize("device_descriptor_server", [
        {"allowlist": Path("path/to/CustomAllowList.xml")},
    ], indirect=True)
    def test_with_custom(descriptor_environment):
        ...
"""

import hashlib
import shutil
from pathlib import Path

import pytest

from testing.helpers.http_fixture_server import FixtureHTTPServer

_RESOURCES_DIR = Path(__file__).parent.parent / "resources" / "deviceDescriptor"
_ALLOWLIST_FILENAME = "AllowList.xml"
_DENYLIST_FILENAME = "DenyList.xml"


def _md5_of_file(path: Path) -> str:
    """Compute the lowercase hex MD5 digest of a file."""
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            h.update(chunk)
    return h.hexdigest()


class DeviceDescriptorServer:
    """
    A mock device descriptor server backed by FixtureHTTPServer.

    Copies the given allowlist and denylist files into the HTTP server's
    serve directory and exposes their URLs and MD5 digests.  Tests can
    add extra files at runtime via :meth:`add_file`.
    """

    def __init__(self, serve_directory: Path, allowlist_path: Path, denylist_path: Path):
        self._serve_dir = serve_directory
        shutil.copy2(allowlist_path, self._serve_dir / _ALLOWLIST_FILENAME)
        shutil.copy2(denylist_path, self._serve_dir / _DENYLIST_FILENAME)
        self._server = FixtureHTTPServer(str(self._serve_dir))

    @property
    def allowlist_url(self) -> str:
        return self._server.url_for(_ALLOWLIST_FILENAME)

    @property
    def denylist_url(self) -> str:
        return self._server.url_for(_DENYLIST_FILENAME)

    @property
    def allowlist_md5(self) -> str:
        return _md5_of_file(self._serve_dir / _ALLOWLIST_FILENAME)

    @property
    def denylist_md5(self) -> str:
        return _md5_of_file(self._serve_dir / _DENYLIST_FILENAME)

    def url_for(self, filename: str) -> str:
        return self._server.url_for(filename)

    def get_request_count(self, path: str) -> int:
        return self._server.get_request_count(path)

    def wait_for_request_count(self, path: str, count: int, timeout: float = 10.0) -> bool:
        """Block until the request count for *path* reaches at least *count*."""
        return self._server.wait_for_request_count(path, count, timeout)

    def md5_of(self, filename: str) -> str:
        """Return the MD5 hex digest of a file currently being served."""
        return _md5_of_file(self._serve_dir / filename)

    @property
    def allowlist_content(self) -> bytes:
        """Return the raw bytes of the served AllowList."""
        return (self._serve_dir / _ALLOWLIST_FILENAME).read_bytes()

    def add_file(self, filename: str, content: bytes):
        """Write an additional file into the serve directory."""
        (self._serve_dir / filename).write_bytes(content)

    def shutdown(self):
        self._server.shutdown()


@pytest.fixture
def device_descriptor_server(tmp_path, request):
    """Pytest fixture providing a DeviceDescriptorServer.

    The server starts automatically, copies the allowlist and denylist
    into its serve directory, and shuts down after the test.

    To supply custom descriptor files, parametrize indirectly::

        @pytest.mark.parametrize("device_descriptor_server", [
            {"allowlist": Path("path/to/Custom.xml")},
        ], indirect=True)
        def test_with_custom(descriptor_environment):
            ...
    """
    params = getattr(request, "param", None) or {}
    allowlist = params.get("allowlist", _RESOURCES_DIR / _ALLOWLIST_FILENAME)
    denylist = params.get("denylist", _RESOURCES_DIR / _DENYLIST_FILENAME)
    server = DeviceDescriptorServer(tmp_path, allowlist, denylist)
    try:
        yield server
    finally:
        server.shutdown()
