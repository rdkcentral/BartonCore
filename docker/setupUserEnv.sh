#!/bin/bash

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
# Created by Kevin Funderburg on 09/17/2024
#

#
# This script is used to as a 'pre-step' to set up the various environment variables
# needed for the Docker build process.
#
# used in:
#  - docker/dockerw
#  - .devcontainer/devcontainer.json

# set -x

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
OUTFILE=$DIR/.env
BARTON_TOP=$DIR/..

# Save off user information for Docker build process
echo "BUILDER_USER=$USER" > $OUTFILE
echo "BUILDER_UID=$(id -u)" >> $OUTFILE
echo "BUILDER_GID=$(id -g)" >> $OUTFILE

# Save off the path to the Barton directory so we can mount it in the same path in the container
echo "BARTON_TOP=$BARTON_TOP" >> $OUTFILE
