#!/bin/bash
# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 Comcast Cable Communications Management, LLC
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
# Created by Kevin Funderburg on 1/21/2025.
#

# This script is used for user creation within the Docker process, as well as any
# user specific customization. This script is called in two situations:
#
# 1. During runtime when the container is spun up when in the CLI environment.
#    - This is done in the `entrypoint.sh` script.
#
# 2. During the build process when the VSC Docker image is being built.
#    - This is done in `Dockerfile.devcontainer`. This gets around the VSC requirement
#      that the user must be baked into the image before the devcontainer is started.

set -e
# set -x

if [ "$#" -ne 3 ]; then
   echo "Usage: $0 <user_id> <group_id> <user_name>"
   exit 1
fi

USER_ID=$1
GROUP_ID=$2
USER_NAME=$3

#############################
# create the user and group
#############################
if getent group $GROUP_ID &> /dev/null; then
   groupmod -n $USER_NAME $GROUP_ID
else
   addgroup --gid $GROUP_ID $USER_NAME &> /dev/null;
fi

if getent passwd $USER_ID &> /dev/null; then
   usermod -l $USER_NAME -d /home/$USER_NAME $(id -u -n $USER_ID)
else
   adduser --gid $GROUP_ID --uid $USER_ID --shell /bin/bash --disabled-password --gecos "" $USER_NAME  &> /dev/null;
fi

echo $USER_NAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USER_NAME
chmod 0440 /etc/sudoers.d/$USER_NAME

#############################
# further user customization
#############################

# add user to pcap group defined in the Dockerfile
usermod -a -G pcap $USER_NAME

# customize the user's bashrc
USER_BASHRC=/home/$USER_NAME/.bashrc
boundary=$(($(sed -n '/unset color_prompt/=' $USER_BASHRC)-1)) && \
sed -i "${boundary}r/tmp/bashrc_term_mods" $USER_BASHRC
