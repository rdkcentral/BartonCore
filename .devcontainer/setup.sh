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

# This script is used to setup VSC related settings for the project after the container
# has been created.

# set -x
set -e

# This environment file is specifically to set up the Python environment for VSC.
# https://code.visualstudio.com/docs/python/environments#_environment-variable-definitions-file
PYTHON_ENV_FILE=".devcontainer/vsc-python.env"

# Add the ASAN library to the LD_PRELOAD environment variable within the vsc-python.env file
# so pytest invocations will run with the AddressSanitizer preloaded. Without this, pytest
# discovery will fail in VSC.
LD_PRELOAD_LINE="LD_PRELOAD=$(gcc -print-file-name=libasan.so)"

if [ -f "$PYTHON_ENV_FILE" ]; then
    if grep -q "^LD_PRELOAD=" "$PYTHON_ENV_FILE"; then
        sed -i "s|^LD_PRELOAD=.*|$LD_PRELOAD_LINE|" "$PYTHON_ENV_FILE"
    else
        echo "$LD_PRELOAD_LINE" >> "$PYTHON_ENV_FILE"
    fi
else
    echo "$LD_PRELOAD_LINE" > "$PYTHON_ENV_FILE"
fi
