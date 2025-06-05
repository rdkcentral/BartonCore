#!/usr/bin/env bash

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

set -e

help() {
    echo "Usage: $0 [-h] [-c <dir>] [-s] [-a]"
    echo "  -h  Display help"
    echo "  -c  Matter configuration directory"
    echo "  -s  Build with stack smash protection"
    echo "  -a  Build with address sanitizer"
}

MY_DIR=$(realpath $(dirname $0))
BUILD_DIR=${MY_DIR}/build
MATTER_BUILD_DIR=${BUILD_DIR}/matter
MATTER_INSTALL_DIR=${BUILD_DIR}/matter-install
MATTER_INSTALL_BIN_DIR=${MATTER_INSTALL_DIR}/bin

BUILD_WITH_STACK_SMASH_PROTECTION=""
BUILD_WITH_SANITIZER=""
MATTER_CONF_DIR=""

while getopts ":hc:sa" option; do
    case $option in
    h)
        help
        exit
        ;;
    c)
        MATTER_CONF_DIR="$OPTARG"
        ;;
    s)
        BUILD_WITH_STACK_SMASH_PROTECTION="-s"
        ;;
    a)
        BUILD_WITH_SANITIZER="-a"
        ;;
    esac
done

# if [ -e ${MATTER_BUILD_DIR} ]; then
#     echo "Matter build dir already exists at: ${MATTER_BUILD_DIR}. Delete it if you want to rebuild Matter."
# else
rm -rf ${MATTER_INSTALL_DIR}
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

# git clone \
#     https://github.com/project-chip/connectedhomeip.git \
#     matter

cd ${MATTER_BUILD_DIR}

git checkout 0e56f4cde846647ce69aa2b785b7d153fb3a0fa5

./scripts/checkout_submodules.py --shallow --platform linux

# Ensure that the older OpenSSL version is used for the build
export PKG_CONFIG_PATH=/usr/local/openssl/lib/pkgconfig:$PKG_CONFIG_PATH

${MY_DIR}/third_party/matter/barton-library/linux/build.sh -o ${MATTER_INSTALL_DIR} -c ${MATTER_CONF_DIR} ${BUILD_WITH_STACK_SMASH_PROTECTION} ${BUILD_WITH_SANITIZER}

cd ${MATTER_BUILD_DIR}
git reset --hard
# fi

# Build and install the Matter example apps for use as test targets
#
# Note: To add a new app, create an entry for the name as well as to
# the MATTER_SAMPLE_APPS_TARGETS and MATTER_SAMPLE_APPS_SHOULD_BUILD dictionaries.
LIGHTING_APP_NAME=chip-lighting-app
LOCK_APP_NAME=chip-lock-app
THERMOSTAT_APP_NAME=thermostat-app
CAMERA_APP_NAME=chip-camera-app
CHIP_TOOL_NAME=chip-tool

declare -A MATTER_SAMPLE_APPS_TARGETS
MATTER_SAMPLE_APPS_TARGETS=(
    ["$LIGHTING_APP_NAME"]="linux-x64-light-rpc"
    # ["$LOCK_APP_NAME"]="linux-x64-lock"
    # ["$THERMOSTAT_APP_NAME"]="linux-x64-thermostat"
    # ["$CAMERA_APP_NAME"]="linux-x64-camera"
    # ["$CHIP_TOOL_NAME"]="linux-x64-chip-tool"
)

declare -A MATTER_SAMPLE_APPS_SHOULD_BUILD
MATTER_SAMPLE_APPS_SHOULD_BUILD=(
    ["$LIGHTING_APP_NAME"]=true
    # ["$LOCK_APP_NAME"]=true
    # ["$THERMOSTAT_APP_NAME"]=true
    # ["$CAMERA_APP_NAME"]=true
    # ["$CHIP_TOOL_NAME"]=true
)

SHOULD_BUILD_ANY=false

for app in "${!MATTER_SAMPLE_APPS_SHOULD_BUILD[@]}"; do
    if [ -e ${MATTER_INSTALL_BIN_DIR}/${app} ]; then
        echo "Matter example ${app} already exists, skipping build."
        MATTER_SAMPLE_APPS_SHOULD_BUILD[$app]=false
    else
        SHOULD_BUILD_ANY=true
    fi
done

if [ "${SHOULD_BUILD_ANY}" = true ]; then

    cd ${MATTER_BUILD_DIR}
    mkdir -p ${MATTER_INSTALL_BIN_DIR}
    export TMPDIR=${BUILD_DIR}/tmp
    export TERM=xterm
    . ./scripts/activate.sh

    for app in "${!MATTER_SAMPLE_APPS_SHOULD_BUILD[@]}"; do
        if [ "${MATTER_SAMPLE_APPS_SHOULD_BUILD[$app]}" = true ]; then
            ./scripts/build/build_examples.py --target ${MATTER_SAMPLE_APPS_TARGETS[$app]} build &&
                cp out/${MATTER_SAMPLE_APPS_TARGETS[$app]}/${app} ${MATTER_INSTALL_BIN_DIR}
        fi
    done

fi

# Clean up
rm -rf ${MATTER_BUILD_DIR}/.environment
