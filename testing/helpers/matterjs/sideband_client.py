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

import json
import logging
import urllib.request
import urllib.error

logger = logging.getLogger(__name__)


class SidebandOperationError(Exception):
    """Raised when a side-band operation returns a failure response."""
    pass


class SidebandClient:
    """
    Client for communicating with matter.js virtual devices via their
    side-band HTTP interface.

    The side-band interface allows test code to interact with virtual devices
    outside of the Matter protocol — for example, simulating a user manually
    locking a door or querying device state directly.

    Args:
        host: Hostname of the device's side-band server.
        port: Port number of the device's side-band server.
        timeout: Default timeout in seconds for HTTP requests.
    """

    def __init__(self, host: str, port: int, timeout: float = 5.0):
        self._host = host
        self._port = port
        self._timeout = timeout
        self._base_url = f"http://{host}:{port}/sideband"

    def send(self, operation: str, params: dict = None) -> dict:
        """
        Send a side-band operation to the virtual device.

        Args:
            operation: The operation name (e.g., "lock", "unlock", "getState").
            params: Optional parameters for the operation.

        Returns:
            The result dict from the device's response.

        Raises:
            SidebandOperationError: If the device reports a failure.
            ConnectionError: If the device is unreachable.
            TimeoutError: If the request times out.
        """
        payload = {"operation": operation}

        if params is not None:
            payload["params"] = params

        data = json.dumps(payload).encode("utf-8")
        req = urllib.request.Request(
            self._base_url,
            data=data,
            headers={"Content-Type": "application/json"},
            method="POST",
        )

        try:
            with urllib.request.urlopen(req, timeout=self._timeout) as response:
                body = json.loads(response.read().decode("utf-8"))
        except urllib.error.HTTPError as e:
            # VirtualDevice returns 400/500 with JSON body on bad requests.
            # Parse the response to surface the device-provided error message.
            try:
                body = json.loads(e.read().decode("utf-8"))
            except (json.JSONDecodeError, UnicodeDecodeError):
                raise SidebandOperationError(
                    f"Side-band operation '{operation}' failed with HTTP {e.code}"
                ) from e

            error_msg = body.get("error", f"HTTP {e.code}")

            raise SidebandOperationError(
                f"Side-band operation '{operation}' failed: {error_msg}"
            ) from e
        except urllib.error.URLError as e:
            if isinstance(e.reason, TimeoutError):
                raise TimeoutError(
                    f"Side-band operation '{operation}' timed out after {self._timeout}s"
                ) from e

            if isinstance(e.reason, OSError):
                raise ConnectionError(
                    f"Cannot connect to side-band server at {self._base_url}: {e.reason}"
                ) from e

            raise
        except TimeoutError:
            raise TimeoutError(
                f"Side-band operation '{operation}' timed out after {self._timeout}s"
            )

        if not body.get("success"):
            error_msg = body.get("error", "Unknown error")

            raise SidebandOperationError(
                f"Side-band operation '{operation}' failed: {error_msg}"
            )

        return body.get("result", {})

    def get_state(self) -> dict:
        """
        Convenience method to query the device's current state.

        Returns:
            The device state dict (contents vary by device type).
        """
        return self.send("getState")
