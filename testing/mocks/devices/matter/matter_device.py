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
import subprocess
import logging
import json

from testing.mocks.devices.base_device import BaseDevice
from testing.helpers.matter import code_generators
from testing.utils import io_utils
from testing.utils import process_utils as putils

logger = logging.getLogger(__name__)


class MatterDevice(BaseDevice):
    """
    MatterDevice is a base class, inheriting from BaseDevice class, that provides common
    functionality for matter devices. This class is intended to be inherited by other matter
    device classes to share common methods and attributes such as starting and stopping the
    device, sending messages, and setting commissioning codes.

    Attributes:
        _app_name (str): The name of the matter sample application to run.
        _mdns_port (int): The mDNS port for the device (default is 5540).
        _passcode (int): The passcode for the device.
        _discriminator (int): The discriminator for the device.
        _vendor_id (int): The vendor ID for the device (default is 0).
        _product_id (int): The product ID for the device (default is 0).
        _pipe_name (str): The prefix for the named pipe, unique to the device.
        _pipe_path (str): The path to the named pipe for communication.
        _commissioning_code (str): The commissioning code for the device.
        _process (subprocess.Popen): The process running the device application.
    """

    _app_name: str
    _mdns_port: int
    _passcode: int
    _discriminator: int
    _vendor_id: int
    _product_id: int
    _pipe_name: str
    _pipe_path: str
    _commissioning_code: str
    _process: subprocess.Popen

    def __init__(
        self,
        app_name: str,
        pipe_name: str,
        device_class: str,
        vendor_id: int = 0,
        product_id: int = 0,
        mdns_port: int = 5540,
    ):
        self._app_name = app_name
        self._device_class = device_class
        self._pipe_name = pipe_name
        self._vendor_id = vendor_id
        self._product_id = product_id
        self._mdns_port = mdns_port
        self._passcode = self._set_passcode()
        self._discriminator = self._set_discriminator()
        self._commissioning_code = self._set_commissioning_code()
        self._pipe_path = None
        self._process = None

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
        Returns the commissioning code for the device.
        """
        return self._commissioning_code

    def start(self):
        """
        Starts the device application as a subprocess given the device parameters.

        Raises:
            subprocess.SubprocessError: If the subprocess fails to start.
        """
        logger.debug(
            f"Starting {self._device_class} with passcode {self._passcode} and discriminator {self._discriminator}"
        )

        try:
            self._process = subprocess.Popen(
                [
                    self._app_name,
                    "--passcode",
                    str(self._passcode),
                    "--discriminator",
                    str(self._discriminator),
                    "--vendor-id",
                    str(self._vendor_id),
                    "--product-id",
                    str(self._product_id),
                    "--secured-device-port",
                    str(self._mdns_port),
                ],
                shell=False,
            )

            self._pipe_path = f"/tmp/{self._pipe_name}_fifo_{self._process.pid}"

            logger.debug(f"Started {self._device_class} with PID: {self._process.pid}")

        except subprocess.SubprocessError as e:
            logger.debug(f"Failed to start {self._device_class} process: {e}")
            self._process = None

    def stop(self):
        """
        Stops the device application.
        """
        if self._process:
            logger.debug(f"Stopping {self._device_class} with PID: {self._process.pid}")
            putils.terminate_program(self._process)
            self._process = None

    def send_message(self, message: dict):
        """
        Sends a message to the device's named pipe.

        Args:
            message (dict): The message to be sent to the named pipe.
        """
        if self._pipe_path:
            logger.debug(f"Sending message: {message} to path: {self._pipe_path}")

            # NOTE: There is a strange bug in the lighting-app that will not parse
            # a properly formatted json string correctly. The extra space after the
            # message is seemingly required for the app to parse the message correctly.
            io_utils.write_to_file(f"{json.dumps(message)} ", self._pipe_path)
        else:
            logger.warning(f"Named pipe path not set for {self._device_class}")

    def _cleanup(self):
        """
        Implements the abstract method from BaseDevice.

        Stop the device application and clean up any temporary files.
        """
        logger.debug(f"Cleaning up {self._device_class}")

        self.stop()
        # FIXME: Need to use temporary directories for tests
        chip_files = Path("/tmp")
        for file in chip_files.glob("chip*"):
            file.unlink(missing_ok=True)
