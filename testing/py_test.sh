#!/bin/bash
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
# Created by Kevin Funderburg on 12/17/2024
#

# In order for pytests to run successfully with libbrtnDeviceSericeShared.so compiled
# with AddressSanitizer, libasan.so must be preloaded. This script is a wrapper around
# pytest to handle setting the LD_PRELOAD environment variable and passing any extra args
# to pytest. Use this script if you want to execute any pytests on the command line to ensure
# your environment is set up correctly.

# examples:
# ./py_test.sh
# ./py_test.sh -s --log-cli-level=DEBUG


# set -x
set -e

function show_help {
    echo "This is a wrapper script around pytest to ensure the environment is setup correctly."
    echo "Usage: $0 [pytest options]"
}

if [[ "$1" == "-h" ]]; then
    show_help
    exit 0
fi

LD_PRELOAD=$(gcc -print-file-name=libasan.so) pytest "$@"
