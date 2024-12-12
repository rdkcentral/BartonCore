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

from gi.repository import BDeviceService

import atexit
import subprocess
import threading
import random
import example_network_credentials_provider
import shutil
from stdnum.verhoeff import calc_check_digit
from pathlib import Path
import psutil

readyForDevicesCondition = threading.Condition()
readyToCommission = False
deviceAddedCondition = threading.Condition()
commissionedDevice = False

# Must match what's compiled with barton matter sdk
barton_storage_path = str(Path.home()) + "/.brtn-ds"
matter_storage_path = barton_storage_path + "/matter"
light_process = None


def cleanup():
    print("Cleaning up atext")
    if light_process:
        terminate_program(light_process)
    # FIXME: Need to use temporary directories for tests
    chip_files = Path("/tmp")
    for file in chip_files.glob("chip*"):
        file.unlink(missing_ok=True)
    shutil.rmtree(barton_storage_path, ignore_errors=True)


atexit.register(cleanup)


def on_status_changed(_object, statusEvent: BDeviceService.StatusEvent):
    global readyToCommission
    if (
        statusEvent.props.reason
        == BDeviceService.StatusChangedReason.READY_FOR_DEVICE_OPERATION
    ):
        with readyForDevicesCondition:
            readyToCommission = True
            readyForDevicesCondition.notify_all()


def on_device_added(_object, device: BDeviceService.DeviceAddedEvent):
    global commissionedDevice
    with deviceAddedCondition:
        commissionedDevice = True
        deviceAddedCondition.notify_all()


def terminate_program(process: subprocess.Popen):
    try:
        parent = psutil.Process(process.pid)
        for child in parent.children(recursive=True):
            child.terminate()
            child.wait()
        parent.terminate()
        parent.wait()
        print(f"Terminated program with PID: {process.pid}")
    except Exception as e:
        print(f"An error occurred while terminating the program: {e}")


def write_to_named_pipe(message: str, path: str):
    try:
        # Open the named pipe in write mode
        with open(path, "w") as pipe:
            # Write the JSON string to the named pipe
            pipe.write(message)
            pipe.write()
    except Exception as e:
        print(f"An error occurred: {e}")


def start_light(
    passcode: int,
    discriminator: int,
    vendor_id: int = 0,
    product_id: int = 0,
    mdns_port: int = 5540,
) -> subprocess.Popen:
    process = subprocess.Popen(
        f"chip-lighting-app --passcode {passcode} --discriminator {discriminator} --vendor-id {vendor_id} --product-id {product_id} --secured-device-port {mdns_port}",
        shell=True,
    )
    return process


def send_to_light(message: str, process: subprocess.Popen):
    # Construct the named pipe path
    pipe_path = f"/tmp/chip_lighting_fifo_{process.pid}"
    write_to_named_pipe(message, pipe_path)


def construct_manual_pairing_code(
    discriminator: int,
    passcode: int,
    vendor_id: int = 0,
    product_id: int = 0,
) -> str:
    if vendor_id == 0 and product_id != 0:
        raise ValueError("Product ID can only be set if Vendor ID is set")

    # See Table 39 and 40 of section 5.1.4.1 (Manual Pairing Code Content) of the Matter Core Specification
    vid_pid_present = 1 if vendor_id != 0 else 0

    first_digit = (vid_pid_present << 2) | (discriminator >> 10)
    two_to_six_digits = ((discriminator & 0x300) << 6) | (passcode & 0x3FFF)
    seven_to_ten_digits = passcode >> 14
    code_without_check = (
        f"{first_digit:01}{two_to_six_digits:05}{seven_to_ten_digits:04}"
    )

    if vid_pid_present != 0:
        eleven_to_fifteen_digits = vendor_id
        sixteen_to_twenty_digits = product_id
        code_without_check += (
            f"{eleven_to_fifteen_digits:05}{sixteen_to_twenty_digits:05}"
        )

    check_digit = calc_check_digit(code_without_check)
    pairing_code = code_without_check + check_digit

    return pairing_code


params = BDeviceService.InitializeParamsContainer()
params.set_storage_dir(barton_storage_path)
params.set_matter_storage_dir(matter_storage_path)
params.set_matter_attestation_trust_store_dir(matter_storage_path)
netCredsProvider = (
    example_network_credentials_provider.ExampleNetworkCredentialsProvider()
)
params.set_network_credentials_provider(netCredsProvider)

# In pygobject, gobject properties map directly to object attributes where
# - is replaced by _
client = BDeviceService.Client(initialize_params=params)

client.connect(BDeviceService.CLIENT_SIGNAL_NAME_STATUS_CHANGED, on_status_changed)
client.connect(BDeviceService.CLIENT_SIGNAL_NAME_DEVICE_ADDED, on_device_added)

# Must do after constructing a new BDeviceServiceClient
propertyProvider = params.get_property_provider()
propertyProvider.set_property_string("device.subsystem.disable", "zigbee,thread")

client.start()

# Wait until we are ready
with readyForDevicesCondition:
    while not readyToCommission:
        readyForDevicesCondition.wait()

passcode = 12345
discriminator = random.getrandbits(12)
commissioningCode = construct_manual_pairing_code(
    discriminator=discriminator, passcode=passcode
)

light_process = start_light(passcode=passcode, discriminator=discriminator, mdns_port=0)
print(f"Started light with PID: {light_process.pid}")

print(f"Commissioning code: {commissioningCode}")
client.commission_device(commissioningCode, 100)

with deviceAddedCondition:
    while not commissionedDevice:
        deviceAddedCondition.wait()

# message = '{"Name":"InitialPress","NewPosition":0} '
# send_to_light(message, light_process)
# time.sleep(1)
# message = '{"Name":"ShortRelease","PreviousPosition":0} '
# send_to_light(message, light_process)

terminate_program(light_process)

client.stop()
