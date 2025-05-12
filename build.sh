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

help() {
    echo "Usage: $0 [options] [CMake options]"
    echo "  -h                  Display help"
    echo "  -C <initial-cache>  CMake initial-cache to use. If not supplied (and --no-cache is not supplied), config/cmake/platforms/dev/linux.cmake will be used"
    echo "  --no-initial-cache  Do not use a CMake initial-cache. This will use default Barton public options."
    echo "  -d, --delete-cache  Delete the CMake cache if it exists."
}

CMAKE_CACHE="${MY_DIR}/config/cmake/platforms/dev/linux.cmake"

NO_CMAKE_INITIAL_CACHE=0
DELETE_CACHE=0
CMAKE_ARGS=""

while [[ $# -gt 0 ]]; do
    case $1 in
    -h | --help)
        help
        exit 0
        ;;
    -C)
        shift
        CMAKE_CACHE=$1
        shift
        ;;
    --no-initial-cache)
        NO_CMAKE_INITIAL_CACHE=1
        shift
        ;;
    -d | --delete-cache)
        DELETE_CACHE=1
        shift
        ;;
    *)
        echo "Appending CMake args $@"
        CMAKE_ARGS=$@
        break
        ;;
    esac
done

CMAKE_CACHE_OPTION="-C ${CMAKE_CACHE}"

if [[ $DELETE_CACHE -eq 1 ]]; then
    if [[ -e ${BUILD_DIR}/CMakeCache.txt ]]; then
        echo "Deleting ${BUILD_DIR}/CMakeCache.txt"
        rm -f ${BUILD_DIR}/CMakeCache.txt
    fi

    if [[ -e ${BUILD_DIR}/CMakeFiles ]]; then
        echo "Deleting ${BUILD_DIR}/CMakeFiles"
        rm -rf ${BUILD_DIR}/CMakeFiles
    fi
fi

if [[ $NO_CMAKE_INITIAL_CACHE -eq 1 ]]; then
    echo "No CMake cache specified. Using default Barton public options."
    CMAKE_CACHE_OPTION=""
else
    echo "Using CMake cache: ${CMAKE_CACHE}"
fi

pushd ${MY_DIR}

cmake -B ${BUILD_DIR} ${CMAKE_CACHE_OPTION} $CMAKE_ARGS
cmake --build ${BUILD_DIR} --parallel $(($(nproc) - 1))

popd
