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
# Created by Christian Leithner on 12/16/2024.
#

set -e

help() {
    echo "Usage: $0 [-h] [-o <dir>] [-c <dir>] [-s] [-a]"
    echo "  -h  Display help"
    echo "  -o  Output directory"
    echo "  -c  Matter configuration directory"
    echo "  -s  Build with stack smash protection"
    echo "  -a  Build with address sanitizer"
}

MY_DIR=$(dirname $(readlink -f $0))
BARTON_COMMON=$(realpath $MY_DIR/../barton-common)
CHIP_ROOT=$MY_DIR/third_party/connectedhomeip
PROJECT_CONFIG_DIR=$MY_DIR/include/project_config

OUT_DIR=$MY_DIR/out
MATTER_CONF_DIR="/tmp"
BUILD_WITH_STACK_SMASH_PROTECTION=false
BUILD_WITH_SANITIZER=false

while getopts ":ho:c:sa" option; do
    case $option in
    h)
        help
        exit
        ;;
    o)
        OUT_DIR=$OPTARG
        ;;
    c)
        MATTER_CONF_DIR="$OPTARG"
        ;;
    s)
        BUILD_WITH_STACK_SMASH_PROTECTION=true
        ;;
    a)
        BUILD_WITH_SANITIZER=true
        ;;
    esac
done

# Matter doesn't directly support stack smash and its sanitizer support is limited to clang only.
# So, we're gonna just force the compiler to do what we want.
CFLAGS="["
CXXFLAGS="["
LDFLAGS="["
# Annoyingly, gn seems incapable of conjuring absolute paths that exists at a higher level than the source root.
# Meaning, we can't reference the include directory containing the generated project config at the time of gn executing
# our .gn file. So we have to pass this path in as a command-line user arg instead.
PROJECT_INCLUDE_DIRS="[\"${PROJECT_CONFIG_DIR}\""

if [ "$BUILD_WITH_STACK_SMASH_PROTECTION" = true ]; then
    CFLAGS+="\"-fstack-protector-strong\","
    CXXFLAGS+="\"-fstack-protector-strong\","
fi

if [ "$BUILD_WITH_SANITIZER" = true ]; then
    CFLAGS+="\"-fsanitize=address\","
    CXXFLAGS+="\"-fsanitize=address\","
    LDFLAGS+="\"-fsanitize=address\","
fi

CFLAGS="${CFLAGS%,}]"
CXXFLAGS="${CXXFLAGS%,}]"
LDFLAGS="${LDFLAGS%,}]"
PROJECT_INCLUDE_DIRS="${PROJECT_INCLUDE_DIRS%,}]"

pushd $CHIP_ROOT
. scripts/activate.sh
popd

# Remove the old project config to ensure fresh generation
rm -rf ${MY_DIR}/include/project_config
echo "Generating BartonProjectConfig"
python3 $MY_DIR/configure_project_config.py $MY_DIR/BartonProjectConfig.h.in $PROJECT_CONFIG_DIR/BartonProjectConfig.h "CHIP_BARTON_CONF_DIR=$MATTER_CONF_DIR"

GN_ARGS="chip_project_config_include_dirs=$PROJECT_INCLUDE_DIRS target_cflags_c=$CFLAGS target_cflags_cc=$CXXFLAGS target_ldflags=$LDFLAGS"

echo "Running: gn --root=$CHIP_ROOT --root-target=$MY_DIR --dotfile=$MY_DIR/.gn gen --check --fail-on-unused-arg --args=\"$GN_ARGS\" $OUT_DIR"
gn --root=$CHIP_ROOT --root-target=$MY_DIR --dotfile=$MY_DIR/.gn gen --check --fail-on-unused-arg --args="$GN_ARGS" $OUT_DIR
ninja -C $OUT_DIR

# Installation

INCLUDE_DIR=$OUT_DIR/include/matter
GEN_DIR=$OUT_DIR/gen
OBJ_DIR=$OUT_DIR/obj

rm -rf $INCLUDE_DIR
mkdir -p $INCLUDE_DIR

# GN will generate an includes directory, but that only contains explicitly exposed transitive headers. Right now,
# the SDK does not expose a lot of its headers for reasons unknown (speculatively, it is meant to build
# applications and not libraries). So, we will copy the headers we need manually.

# First the headers GN actually generates
cp -r $GEN_DIR/include/* $INCLUDE_DIR

# The project headers (which don't get exported by gn for some reason)
cp -r $MY_DIR/include/* $INCLUDE_DIR
cp $PROJECT_CONFIG_DIR/BartonProjectConfig.h $INCLUDE_DIR

# Now snag all the src and third_party headers recursively. rsync is used as a convenience here
rsync -am --include="*/" --include="*.h**" --exclude="*" $CHIP_ROOT/src/ $INCLUDE_DIR
rsync -am --include="*/" --include="*.h**" --exclude="*" $CHIP_ROOT/src/include/ $INCLUDE_DIR
rsync -am --include="*/" --include="*.h**" --exclude="*" $CHIP_ROOT/third_party/nlassert/repo/include/ $INCLUDE_DIR
rsync -am --include="*/" --include="*.h**" --exclude="*" $CHIP_ROOT/third_party/nlio/repo/include/ $INCLUDE_DIR
rsync -am --include="*/" --include="*.h**" --exclude="*" $CHIP_ROOT/third_party/nlfaultinjection/include/ $INCLUDE_DIR
rsync -am --include="*/" --include="*.h**" --exclude="*" $CHIP_ROOT/third_party/inipp/repo/inipp/ $INCLUDE_DIR
rsync -am --include="*/" --include="*.h**" --exclude="*" $CHIP_ROOT/third_party/jsoncpp/repo/include/ $INCLUDE_DIR

# zap generated includes
cp -r $CHIP_ROOT/zzz_generated/app-common/app-common $INCLUDE_DIR
cp -r $CHIP_ROOT/zzz_generated/app-common/clusters $INCLUDE_DIR
mkdir -p $INCLUDE_DIR/zap-generated
cp -r $GEN_DIR/ABS_PATH/${BARTON_COMMON}/zapgen/zap-generated/*.h* $INCLUDE_DIR/zap-generated
