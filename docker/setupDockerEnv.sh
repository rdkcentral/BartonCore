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
IMAGE_REPO="ghcr.io/rdkcentral/barton_builder"

# Docker image version management
#
# This system automatically updates to the latest compatible Docker builder image in the
# current lineage while handling user customizations in a predictable way:
# - Always updates to the latest builder version in the current lineage to ensure builds
#   use compatible toolchains and dependencies
# - Warns when custom tags exist based on different builder versions
# - Allows restoring custom tags after updates if needed
#
# The version is notated by the `version` file.

VERSION_FILE="$DIR/version"
if [ ! -s "$VERSION_FILE" ]; then
    echo "Error: Docker builder version file '$VERSION_FILE' is missing or empty." >&2
    echo "Please ensure the repository is fully checked out and that '$VERSION_FILE' contains a valid version string." >&2
    exit 1
fi

HIGHEST_BUILDER_TAG=$(cat "$VERSION_FILE")
IMAGE_TAG=$HIGHEST_BUILDER_TAG
BUILDER_TAG_CHANGED=false
existingSilabsDevice=""
existingBackboneIf=""
existingSilabsSocket=""
existingSilabsSocketHost=""
existingBtUsbipSocket=""
existingBtUsbipSocketHost=""

if [ -f "$OUTFILE" ]; then
    existingSilabsDevice=$(grep '^SILABS_DEVICE=' "$OUTFILE" | sed 's/^SILABS_DEVICE=//' || true)
    existingBackboneIf=$(grep '^BACKBONE_IF=' "$OUTFILE" | sed 's/^BACKBONE_IF=//' || true)
    existingSilabsSocket=$(grep '^SILABS_SOCKET=' "$OUTFILE" | sed 's/^SILABS_SOCKET=//' || true)
    existingSilabsSocketHost=$(grep '^SILABS_SOCKET_HOST=' "$OUTFILE" | sed 's/^SILABS_SOCKET_HOST=//' || true)
    existingBtUsbipSocket=$(grep '^BT_USBIP_SOCKET=' "$OUTFILE" | sed 's/^BT_USBIP_SOCKET=//' || true)
    existingBtUsbipSocketHost=$(grep '^BT_USBIP_SOCKET_HOST=' "$OUTFILE" | sed 's/^BT_USBIP_SOCKET_HOST=//' || true)

    CURRENT_BUILDER_TAG=$(grep "CURRENT_BUILDER_TAG=" "$OUTFILE" | sed 's/CURRENT_BUILDER_TAG=//')

    if [ "$HIGHEST_BUILDER_TAG" != "$CURRENT_BUILDER_TAG" ]; then
        echo "Current barton_builder tag ($CURRENT_BUILDER_TAG) is not in sync with the latest barton_builder available in this lineage ($HIGHEST_BUILDER_TAG)."
        echo "The value of IMAGE_TAG in docker/.env will be updated to use the latest barton_builder version in this lineage."
        BUILDER_TAG_CHANGED=true
    fi

    IMAGE_TAG=$(grep "IMAGE_TAG" "$OUTFILE" | sed 's/IMAGE_TAG=//')

    CUSTOM_TAG=false
    if [ "$IMAGE_TAG" != "$CURRENT_BUILDER_TAG" ]; then
        CUSTOM_TAG=true
    fi

    if [ "$CUSTOM_TAG" = true ] && [ "$BUILDER_TAG_CHANGED" = true ]; then
        echo "WARNING: The custom image tag '$IMAGE_TAG' is based on a different barton_builder version ($CURRENT_BUILDER_TAG)."
        echo "To continue using your custom tag, you should:"
        echo "1. Rebuild your custom image"
        echo "2. Set the value of IMAGE_TAG in docker/.env back to '$IMAGE_TAG'"
        echo "3. Rebuild your environment - either devcontainer or CLI container"
    fi

    if [ "$BUILDER_TAG_CHANGED" = true ]; then
        IMAGE_TAG=$HIGHEST_BUILDER_TAG
    fi

fi

CURRENT_BUILDER_TAG=$HIGHEST_BUILDER_TAG

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
# Save off a workspace identifier (basename of the repo directory) to uniquely identify
# this clone in Docker Compose project names and network names, enabling multiple clones
# to run simultaneously without sharing networks.
# Use realpath to resolve the canonical path before taking basename so that the trailing
# "/.." in BARTON_TOP does not result in ".." as the workspace id. Sanitize to lowercase
# alphanumeric-and-hyphens to satisfy Docker Compose project name restrictions.
workspacePath=$(realpath "$BARTON_TOP")
workspaceName=$(basename -- "$workspacePath")
BARTON_WORKSPACE_ID=$(printf '%s' "$workspaceName" | tr '[:upper:]' '[:lower:]' | tr -cs 'a-z0-9' '-')

# Trim leading and trailing hyphens and ensure a non-empty, reasonably sized workspace ID
BARTON_WORKSPACE_ID=${BARTON_WORKSPACE_ID##-}
BARTON_WORKSPACE_ID=${BARTON_WORKSPACE_ID%%-}

if [ -z "$BARTON_WORKSPACE_ID" ]; then
    BARTON_WORKSPACE_ID="workspace"
fi

maxWorkspaceIdLen=40
if [ ${#BARTON_WORKSPACE_ID} -gt $maxWorkspaceIdLen ]; then
    BARTON_WORKSPACE_ID=${BARTON_WORKSPACE_ID:0:$maxWorkspaceIdLen}
fi
echo "BARTON_WORKSPACE_ID=$BARTON_WORKSPACE_ID" >> $OUTFILE
# Save off the image repo/tag into the .env file so it can be used in the compose process
echo "IMAGE_REPO=$IMAGE_REPO" >> $OUTFILE
echo "IMAGE_TAG=$IMAGE_TAG" >> $OUTFILE
# Save off the current builder tag to keep track of the latest version
echo "CURRENT_BUILDER_TAG=$CURRENT_BUILDER_TAG" >> $OUTFILE
# Save off the Matter version for building sample apps in Docker
echo "MATTER_REF=$(cat $BARTON_TOP/matter-version)" >> $OUTFILE
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
# path to libbCore.so
echo "LIB_BARTON_SHARED_PATH=/usr/local/lib" >> $OUTFILE
##############################################################################

##############################################################################
# Optional remote-radio variables (used by docker/compose.remote-radios.yaml).
#
# These are populated automatically from the developer's radio config written
# by scripts/remote-radios/remote-radios-setup.sh, which drops a file at
# ~/.remote-radios/radios.env on the dev server.  If that file is absent (the
# developer has no forwarded radios), all values stay empty and the overlay is
# simply not used — the environment behaves exactly as before.
#
# SILABS_SOCKET_HOST: host path of the bind-mounted Silabs serial tunnel socket.
# SILABS_SOCKET:      path of that socket INSIDE the container.
# SILABS_DEVICE:      host path of a locally-attached Silabs USB radio (instead
#                     of the remote tunnel socket).
# BACKBONE_IF:        network interface used by otbr-agent for Thread backbone
#                     routing.  Defaults to the host default-route interface.
# BT_USBIP_SOCKET_HOST: host path of the bind-mounted usbipd UNIX socket.
# BT_USBIP_SOCKET:      path of that socket INSIDE the container.
#
# Existing values in docker/.env are preserved unless overridden by exporting
# the variable, or by a fresher ~/.remote-radios/radios.env.
##############################################################################

# Source the developer's radios.env (written by remote-radios-setup.sh) so the
# radio parameters flow through automatically without any manual export.
RADIOS_ENV="${REMOTE_RADIOS_ENV:-$HOME/.remote-radios/radios.env}"
if [ -f "$RADIOS_ENV" ]; then
    echo "Using remote-radios config from $RADIOS_ENV"
    # shellcheck disable=SC1090
    . "$RADIOS_ENV"
fi

# Auto-detect the default-route network interface for the Thread backbone.
# The entrypoint will also re-detect at runtime, so this is only used when
# BACKBONE_IF is not already set in the environment.
detectedBackboneIf=""
if command -v ip >/dev/null 2>&1; then
    detectedBackboneIf=$(ip route show default 2>/dev/null | awk '/default/ {print $5; exit}')
fi

silabsSocketValue="${SILABS_SOCKET:-${existingSilabsSocket:-/run/remote-radios/radios/silabs.sock}}"
silabsSocketHostValue="${SILABS_SOCKET_HOST:-${existingSilabsSocketHost:-$silabsSocketValue}}"
silabsDeviceValue="${SILABS_DEVICE:-$existingSilabsDevice}"
backboneIfValue="${BACKBONE_IF:-${existingBackboneIf:-$detectedBackboneIf}}"
btUsbipSocketValue="${BT_USBIP_SOCKET:-$existingBtUsbipSocket}"
btUsbipSocketHostValue="${BT_USBIP_SOCKET_HOST:-$existingBtUsbipSocketHost}"

# The compose overlay bind-mounts the socket's parent DIRECTORY (not the file)
# to avoid Docker auto-creating a bogus directory when the tunnel is not up.
silabsSocketDirValue=$(dirname "$silabsSocketValue")
silabsSocketDirHostValue=$(dirname "$silabsSocketHostValue")
# Ensure the host-side socket directory exists so the bind-mount source is a
# real directory (Docker would otherwise create it as root-owned).
mkdir -p "$silabsSocketDirHostValue" 2>/dev/null || true

echo "SILABS_SOCKET=$silabsSocketValue" >> $OUTFILE
echo "SILABS_SOCKET_HOST=$silabsSocketHostValue" >> $OUTFILE
echo "SILABS_SOCKET_DIR=$silabsSocketDirValue" >> $OUTFILE
echo "SILABS_SOCKET_DIR_HOST=$silabsSocketDirHostValue" >> $OUTFILE
# Claim directory — a sibling of radios/ in the same ~/.remote-radios tree.  A
# consumer that cannot share the radio (an hh4 QEMU guest runs its own cpcd and
# needs the raw CPC byte stream) drops a file here, and the remote-radios
# container releases the Silabs radio until it is removed.
radioClaimDirValue="$(dirname "$silabsSocketDirValue")/claims"
radioClaimDirHostValue="$(dirname "$silabsSocketDirHostValue")/claims"
mkdir -p "$radioClaimDirHostValue" 2>/dev/null || true
echo "RADIO_CLAIM_DIR=$radioClaimDirValue" >> $OUTFILE
echo "RADIO_CLAIM_DIR_HOST=$radioClaimDirHostValue" >> $OUTFILE
echo "SILABS_DEVICE=$silabsDeviceValue" >> $OUTFILE
echo "BACKBONE_IF=$backboneIfValue" >> $OUTFILE
echo "BT_USBIP_SOCKET=$btUsbipSocketValue" >> $OUTFILE
echo "BT_USBIP_SOCKET_HOST=$btUsbipSocketHostValue" >> $OUTFILE
# Bind-mount the usbip socket's parent DIRECTORY (not the file), same rationale
# as the Silabs socket above.
if [ -n "$btUsbipSocketValue" ]; then
    btUsbipSocketDirValue=$(dirname "$btUsbipSocketValue")
    btUsbipSocketDirHostValue=$(dirname "$btUsbipSocketHostValue")
    mkdir -p "$btUsbipSocketDirHostValue" 2>/dev/null || true
    echo "BT_USBIP_SOCKET_DIR=$btUsbipSocketDirValue" >> $OUTFILE
    echo "BT_USBIP_SOCKET_DIR_HOST=$btUsbipSocketDirHostValue" >> $OUTFILE
fi
##############################################################################

# Ensure the container network exists
NETWORK_NAME="$USER-$BARTON_WORKSPACE_ID-barton-ip6net"
_SUBNET_HASH=$(echo "$USER-$BARTON_WORKSPACE_ID" | sha256sum | cut -c1-8)
IPV6_SUBNET="fd00:${_SUBNET_HASH:0:4}:${_SUBNET_HASH:4:4}::/64"
unset _SUBNET_HASH

if ! docker network ls --format '{{.Name}}' | grep -Fxq "$NETWORK_NAME"; then
    echo "Network $NETWORK_NAME does not exist. Creating it..."
    docker network create --ipv6 --subnet $IPV6_SUBNET $NETWORK_NAME
fi

# Build the image if it doesn't exist
IMAGE="$IMAGE_REPO:$IMAGE_TAG"

if docker images --format '{{.Repository}}:{{.Tag}}' | grep -q "^$IMAGE$"; then
    echo "Using existing $IMAGE"
else
    echo "Image $IMAGE not found, building..."
    $DIR/build.sh
fi
