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

#
# Created by Kevin Funderburg on 11/14/2024
#

import random
from stdnum.verhoeff import calc_check_digit


def generate_manual_pairing_code(
    discriminator: int,
    passcode: int,
    vendor_id: int = 0,
    product_id: int = 0,
) -> str:
    """
    Generates a manual pairing code according to Table 39 and 40 of section 5.1.4.1
    of the Matter Core Specification.
    """
    if vendor_id == 0 and product_id != 0:
        raise ValueError("Product ID can only be set if Vendor ID is set")

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


def generate_passcode() -> int:
    """
    Generates a valid random passcode according to 5.1.7 of the
    Matter Core Specification.
    """
    passcode = None

    invalid_passcodes = {
        00000000,
        11111111,
        22222222,
        33333333,
        44444444,
        55555555,
        66666666,
        77777777,
        88888888,
        99999999,
        12345678,
        87654321,
    }

    valid = False
    while not valid:
        passcode = random.randint(1, 99999998)
        if passcode not in invalid_passcodes:
            valid = True

    return passcode


def generate_discriminator() -> int:
    """
    Generates a valid random discriminator according to 5.1.1.5 of the
    Matter Core Specification.
    """
    return random.getrandbits(12)
