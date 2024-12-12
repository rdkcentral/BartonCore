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

#
# This script is used to as a 'pre-step' to set up the various environment variables
# needed for the Docker build process.
#
# used in:
#  - docker/dockerw
#  - .devcontainer/devcontainer.json

# set -x

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
OUTFILE=$DIR/.env
BARTON_TOP=$DIR/..

# Save off user information for Docker build process
echo "BUILDER_USER=$USER" >$OUTFILE
echo "BUILDER_UID=$(id -u)" >>$OUTFILE
echo "BUILDER_GID=$(id -g)" >>$OUTFILE

# Save off the path to the Barton directory so we can mount it in the same path in the container
echo "BARTON_TOP=$BARTON_TOP" >>$OUTFILE
