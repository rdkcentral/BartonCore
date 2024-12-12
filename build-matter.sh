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

MY_DIR=$(realpath $(dirname $0))
BUILD_DIR=${MY_DIR}/build
MATTER_BUILD_DIR=${BUILD_DIR}/matter
MATTER_INSTALL_DIR=${BUILD_DIR}/matter-install
MATTER_INSTALL_BIN_DIR=${MATTER_INSTALL_DIR}/bin

if [ -e ${MATTER_BUILD_DIR} ]; then
    echo "Matter build dir already exists at: ${MATTER_BUILD_DIR}. Delete it if you want to rebuild Matter."
else
    rm -rf ${MATTER_INSTALL_DIR}
    mkdir -p ${BUILD_DIR}
    cd ${BUILD_DIR}
    git clone \
        --branch main \
        --depth 1 \
        ssh://rdkgerrithub.stb.r53.xcal.tv:29418/comcast-matter/sdk \
        matter

    cd ${MATTER_BUILD_DIR}
    ./scripts/checkout_submodules.py --shallow --platform linux

    # Ensure that the older OpenSSL version is used for the build
    export PKG_CONFIG_PATH=/usr/local/openssl/lib/pkgconfig:$PKG_CONFIG_PATH

    cd third_party/comcast/config/zilker
    cmake -B cmake-build-debug -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX=${MATTER_INSTALL_DIR} -DCMAKE_BUILD_TYPE=Debug -DMATTER_CONF_DIR:PATH=~/.brtn-ds/matter
    cmake --build cmake-build-debug
    cd cmake-build-debug
    make install
    cd ${MATTER_BUILD_DIR}
    git reset --hard
fi

# Any compiler flags were for the matter sdk library, no longer necessary for
# the example apps (and we wouldn't want to wrestle with loader order for asan
# anyway)
unset LDFLAGS
unset CFLAGS
unset CXXFLAGS

# Build and install the Matter example apps for use as test targets
LIGHTING_APP_NAME=chip-lighting-app
LOCK_APP_NAME=chip-lock-app
THERMOSTAT_APP_NAME=thermostat-app
CHIP_TOOL_NAME=chip-tool

BUILD_LIGHTING_APP=true
BUILD_LOCK_APP=true
BUILD_THERMOSTAT_APP=true
BUILD_CHIP_TOOL=true

if [ -e ${MATTER_INSTALL_BIN_DIR}/${LIGHTING_APP_NAME} ]; then
    echo "Matter example lighting app already exists, skipping build."
    BUILD_LIGHTING_APP=false
fi

if [ -e ${MATTER_INSTALL_BIN_DIR}/${LOCK_APP_NAME} ]; then
    echo "Matter example lock app already exists, skipping build."
    BUILD_LOCK_APP=false
fi

if [ -e ${MATTER_INSTALL_BIN_DIR}/${THERMOSTAT_APP_NAME} ]; then
    echo "Matter example thermostat app already exists, skipping build."
    BUILD_THERMOSTAT_APP=false
fi

if [ -e ${MATTER_INSTALL_BIN_DIR}/${CHIP_TOOL_NAME} ]; then
    echo "Matter example chip tool already exists, skipping build."
    BUILD_CHIP_TOOL=false
fi

if [ "${BUILD_LIGHTING_APP}" = true ] ||
    [ "${BUILD_LOCK_APP}" = true ] ||
    [ "${BUILD_THERMOSTAT_APP}" = true ] ||
    [ "${BUILD_CHIP_TOOL}" = true ]; then

    cd ${MATTER_BUILD_DIR}
    mkdir -p ${MATTER_INSTALL_BIN_DIR}
    export TMPDIR=${BUILD_DIR}/tmp
    export TERM=xterm
    . ./scripts/activate.sh

    if [ "${BUILD_LIGHTING_APP}" = true ]; then
        ./scripts/build/build_examples.py --target linux-x64-light-rpc build && cp out/linux-x64-light-rpc/${LIGHTING_APP_NAME} ${MATTER_INSTALL_BIN_DIR} && rm -rf out
    fi

    if [ "${BUILD_LOCK_APP}" = true ]; then
        ./scripts/build/build_examples.py --target linux-x64-lock build && cp out/linux-x64-lock/${LOCK_APP_NAME} ${MATTER_INSTALL_BIN_DIR} && rm -rf out
    fi

    if [ "${BUILD_THERMOSTAT_APP}" = true ]; then
        ./scripts/build/build_examples.py --target linux-x64-thermostat build && cp out/linux-x64-thermostat/${THERMOSTAT_APP_NAME} ${MATTER_INSTALL_BIN_DIR} && rm -rf out
    fi

    if [ "${BUILD_CHIP_TOOL}" = true ]; then
        ./scripts/build/build_examples.py --target linux-x64-chip-tool build && cp out/linux-x64-chip-tool/${CHIP_TOOL_NAME} ${MATTER_INSTALL_BIN_DIR} && rm -rf out
    fi
fi

# Clean up
rm -rf ${MATTER_BUILD_DIR}/.environment
