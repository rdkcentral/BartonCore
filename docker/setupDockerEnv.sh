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
# Created by Kevin Funderburg on 09/17/2024
#

# This script is used as a 'pre-step' to set up the Docker environment before any Docker
# build/compose/run invocations occur. The goal of this script is to:
#
# - Set the various environment variables needed for the both the Docker build process,
#   and the runtime environment within a container.
# - Ensure the container network exists before run time.
#
# One result of this script is a .env file containing several variables. These variables
# are needed for two goals:
#
# 1. The compose process:
#    - The compose process will source this file first, which then can be used within the
#      `docker/compose.yaml` file to define some build arguments and a mount volume.
#
# 2. The running container:
#    - The .env file must also be used to define the environment variables within the running
#      container. This file path must be passed as an argument to the `docker run` or `docker compose`
#      commands to setup the runtime environment. This is already defined for you in `dockerw`
#      and `docker/compose.yaml`.
#      After the container has started, see `docker/entrypoint.sh` and `.devcontainer/devcontainer.json`
#      to see how these variables are used to define the custom PATHs within the the CLI container
#      and devcontainer respectively.

set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
OUTFILE=$DIR/.env
BARTON_TOP=$DIR/..
IMAGE_TAG="1.0" 

# Check if there is an image tag already defined in the .env file. If so, this could
# imply the user has has defined a custom tag to use for the build process.
if [ -f "$OUTFILE" ]; then
    if grep -q "IMAGE_TAG" "$OUTFILE"; then
        IMAGE_TAG=$(grep "IMAGE_TAG" "$OUTFILE" | sed 's/IMAGE_TAG=//')
    fi
fi

##############################################################################
# Variables needed to facilitate the Docker compose process. See docker/compose.yaml
# to see how these variables are used.
#
# Save off user information for Docker build process
echo "BUILDER_USER=$USER" > $OUTFILE
echo "BUILDER_UID=$(id -u)" >> $OUTFILE
echo "BUILDER_GID=$(id -g)" >> $OUTFILE

# Save off the path to the Barton directory so we can mount it in the same path in the container
echo "BARTON_TOP=$BARTON_TOP" >> $OUTFILE
# Save off the image tag into the .env file so it can be used in the compose process
echo "IMAGE_TAG=$IMAGE_TAG" >> $OUTFILE
##############################################################################

##############################################################################
# The following are environment variables used to set up the environment within the container.
# Some are used to extend standard PATHs within the container, while others are used to define
# the variable globally within the container.
#
# NOTE: If any new PATHs must be added to the container, they should be defined here. Then
#       appended to the existing PATHs within the container after the container has started
#       using `docker/entrypoint.sh` for the CLI container and `.devcontainer/devcontainer.json`
#       for the devcontainer.
#
# The idea behind this custom PATH approach is to:
# 1. Define the custom PATHs we want to append to the running container's standard PATHs before
#    the container is created.
# 2. Pass these custom PATHs into the container at run time using the generated environment
#    variable file `docker/.env`.
# 3. Append the passed in custom PATHs to the existing standard PATHs within the container after
#    the container has started. See `docker/entrypoint.sh` and `.devcontainer/devcontainer.json`
#    to see how/when these PATHs are defined in the CLI container and devcontainer respectively.

# path to LSAN suppressions file to ignore known leaks from 3rd party libraries
echo "LSAN_OPTIONS=suppressions=$BARTON_TOP/testing/lsan.supp" >> $OUTFILE
# path to various necessary python packages, including the modules defined in the $BARTON_TOP/testing directory
echo "BARTON_PYTHONPATH=/usr/local/lib/python3.x/dist-packages:/usr/lib/python3/dist-packages:$BARTON_TOP" >> $OUTFILE
# path to the matter sample apps
echo "MATTER_SAMPLE_APPS_PATH=$BARTON_TOP/build/matter-install/bin" >> $OUTFILE
# path to libbrtnDeviceServiceShared.so
echo "LIB_BARTON_SHARED_PATH=/usr/local/lib" >> $OUTFILE
##############################################################################

# Ensure the container network exists
NETWORK_NAME="$USER-barton-ip6net"
IPV6_SUBNET="fd00:$(echo $USER | sha256sum | cut -c1-4)::/64"

if ! docker network ls | grep -q "$NETWORK_NAME"; then
    echo "Network $NETWORK_NAME does not exist. Creating it..."
    docker network create --ipv6 --subnet $IPV6_SUBNET $NETWORK_NAME
fi
