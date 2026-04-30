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

#
# Created by Kevin Funderburg on 1/9/2025.
#

from pathlib import Path
import json
import os
import select
import subprocess
import logging
import threading
import time

from testing.mocks.devices.base_device import BaseDevice
from testing.helpers.matter import code_generators
from testing.helpers.matterjs.sideband_client import SidebandClient
from testing.utils import process_utils as putils

# Path to the matter.js virtual device source directory
MATTERJS_DIR = Path(__file__).resolve().parent.parent / "matterjs"

logger = logging.getLogger(__name__)


class MatterDevice(BaseDevice):
    """
    MatterDevice is a base class, inheriting from BaseDevice class, that provides common
    functionality for matter devices. Each device is backed by a matter.js virtual device
    running as a Node.js subprocess with a side-band HTTP control channel.

    Subclasses specify a `matterjs_entry_point` — the JavaScript file to run.

    Attributes:
        _matterjs_entry_point (str): Relative path to the matter.js JS entry point.
        _passcode (int): The passcode for the device.
        _discriminator (int): The discriminator for the device.
        _vendor_id (int): The vendor ID for the device (default is 0).
        _product_id (int): The product ID for the device (default is 0).
        _commissioning_code (str): The commissioning code for the device.
        _process (subprocess.Popen): The process running the device application.
    """

    _matterjs_entry_point: str
    _passcode: int
    _discriminator: int
    _vendor_id: int
    _product_id: int
    _commissioning_code: str
    _process: subprocess.Popen
    _sideband: SidebandClient | None

    def __init__(
        self,
        device_class: str,
        matterjs_entry_point: str,
        vendor_id: int = 0,
        product_id: int = 0,
    ):
        self._matterjs_entry_point = matterjs_entry_point
        self._device_class = device_class
        self._vendor_id = vendor_id
        self._product_id = product_id
        self._passcode = self._set_passcode()
        self._discriminator = self._set_discriminator()
        self._commissioning_code = self._set_commissioning_code()
        self._process = None
        self._sideband = None

    def _set_passcode(self) -> int:
        """
        Generates and sets a new passcode for the device.

        Returns:
            int: The newly generated passcode.
        """
        return code_generators.generate_passcode()

    def _set_discriminator(self) -> int:
        """
        Generates and sets a new discriminator for the device.

        Returns:
            int: The newly generated discriminator.
        """
        return code_generators.generate_discriminator()

    def _set_commissioning_code(self) -> str:
        """
        Generates and sets the commissioning code for the device.
        """
        return code_generators.generate_manual_pairing_code(
            discriminator=self._discriminator, passcode=self._passcode
        )

    def get_commissioning_code(self) -> str:
        """
        Returns the current commissioning code for the device.
        """
        return self._commissioning_code

    @property
    def sideband(self) -> SidebandClient:
        """Access the side-band client for this device.

        Raises:
            RuntimeError: If the device has not been started yet.
        """
        if self._sideband is None:
            raise RuntimeError(
                "Device has not been started yet. Call start() first."
            )

        return self._sideband

    def start(self):
        """
        Start the matter.js virtual device as a Node.js subprocess.

        Blocks until the device emits its ready signal on stdout, then
        configures the SidebandClient with the reported port.

        Raises:
            FileNotFoundError: If the entry point script does not exist.
            RuntimeError: If the device fails to emit a ready signal.
        """
        logger.debug(
            f"Starting {self._device_class} with passcode {self._passcode} "
            f"and discriminator {self._discriminator}"
        )

        entry_point_path = MATTERJS_DIR / "src" / self._matterjs_entry_point

        if not entry_point_path.exists():
            raise FileNotFoundError(
                f"Entry point not found: {entry_point_path}"
            )

        cmd = [
            "node",
            str(entry_point_path),
            "--passcode",
            str(self._passcode),
            "--discriminator",
            str(self._discriminator),
            "--port",
            "0",
        ]

        if self._vendor_id:
            cmd.extend(["--vendor-id", str(self._vendor_id)])

        if self._product_id:
            cmd.extend(["--product-id", str(self._product_id)])

        # Keep LD_PRELOAD for Python (ASAN-instrumented Barton libs), but do
        # not inject it into Node.js virtual devices to avoid cross-runtime
        # sanitizer issues.
        node_env = dict(os.environ)
        node_env.pop("LD_PRELOAD", None)

        self._process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            cwd=str(MATTERJS_DIR),
            env=node_env,
        )

        # Drain stderr in background to avoid pipe buffer deadlock
        self._stderr_thread = threading.Thread(
            target=self._drain_stderr,
            daemon=True,
        )
        self._stderr_thread.start()

        # Wait for the ready signal on stdout
        ready_signal = self._wait_for_ready()

        if ready_signal is None:
            self.stop()

            raise RuntimeError(
                f"Failed to start {self._device_class}: no ready signal received"
            )

        sideband_port = ready_signal["sidebandPort"]
        self._sideband = SidebandClient("localhost", sideband_port)

        logger.debug(
            f"Started {self._device_class} with PID {self._process.pid}, "
            f"sideband port {sideband_port}, "
            f"matter port {ready_signal.get('matterPort')}"
        )

    def _wait_for_ready(self, timeout: float = 30.0) -> dict | None:
        """Read stdout lines until the JSON ready signal is received."""
        deadline = time.monotonic() + timeout

        while True:
            remaining = deadline - time.monotonic()

            if remaining <= 0:
                logger.error(
                    f"Timeout waiting for ready signal from {self._device_class}"
                )

                return None

            if self._process.poll() is not None:
                logger.error(
                    f"{self._device_class} process exited with code "
                    f"{self._process.returncode} before emitting ready signal"
                )

                return None

            ready, _, _ = select.select([self._process.stdout], [], [], remaining)

            if not ready:
                logger.error(
                    f"Timeout waiting for ready signal from {self._device_class}"
                )

                return None

            line = self._process.stdout.readline()

            if not line:
                return None

            line = line.strip()

            if not line:
                continue

            try:
                data = json.loads(line)

                if data.get("ready"):
                    return data
            except json.JSONDecodeError:
                logger.debug(f"Non-JSON stdout from device: {line}")

    def _drain_stderr(self):
        """Drain stderr in a background thread to prevent pipe buffer deadlock."""
        try:
            for line in self._process.stderr:
                logger.debug(f"[{self._device_class}] {line.rstrip()}")
        except (ValueError, OSError):
            pass  # Process closed

    def stop(self):
        """
        Stops the device application.
        """
        if self._process:
            logger.debug(f"Stopping {self._device_class} with PID: {self._process.pid}")
            putils.terminate_program(self._process)
            self._process = None

    def _cleanup(self):
        """
        Implements the abstract method from BaseDevice.

        Stop the device application and clean up this device's storage directory.
        """
        logger.debug(f"Cleaning up {self._device_class}")

        self.stop()
