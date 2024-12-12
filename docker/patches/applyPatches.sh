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
# Created by Kevin Funderburg on 09/11/2024
#

#!/usr/bin/env bash

# Apply a set of patches in a directory.
#
# params:
#    1: directory to find patches in
#    2: directory to patch
#
# example:
#    /tmp/patches/applyPatches.sh /path/to/patches /path/to/patch
#

patchesDir=$1
directoryToPatch=$(realpath -e ${2})

if [ -z "${patchesDir}" ]; then
    echo "applyPatches: no patches directory specified."

elif [ ! -d "${patchesDir}" ]; then
    echo "applyPatches: ${patchesDir} is not a directory."

elif [ -z "${directoryToPatch}" ]; then
    echo "applyPatches: no directory to patch specified."

elif [ ! -d "${directoryToPatch}" ]; then
    echo "applyPatches: ${directoryToPatch} is not a directory."

else
    echo "Applying patches from ${patchesDir}"

    if [ -d "${patchesDir}" ]; then
        for p in ${patchesDir}/*.patch; do
            echo "Applying patch ${p}"
            patch -p1 -d ${directoryToPatch} <"${p}"
        done
    fi
fi
