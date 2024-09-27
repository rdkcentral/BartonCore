#------------------------------ tabstop = 4 ----------------------------------
#
# Copyright (C) 2024 Comcast
#
# All rights reserved.
#
# This software is protected by copyright laws of the United States
# and of foreign countries. This material may also be protected by
# patent laws of the United States and of foreign countries.
#
# This software is furnished under a license agreement and/or a
# nondisclosure agreement and may only be used or copied in accordance
# with the terms of those agreements.
#
# The mere transfer of this software does not imply any licenses of trade
# secrets, proprietary technology, copyrights, patents, trademarks, or
# any other form of intellectual property whatsoever.
#
# Comcast retains all ownership rights.
#
#------------------------------ tabstop = 4 ----------------------------------

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
            patch -p1 -d ${directoryToPatch} < "${p}"
        done
    fi
fi
