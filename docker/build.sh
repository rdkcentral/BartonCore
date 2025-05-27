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

script_dir="$(dirname "$(realpath "$0")")"

docker_env_path=$script_dir/.env

if [ ! -f $docker_env_path ]; then
    $script_dir/setupDockerEnv.sh
fi

# The image tag is stored in the .env file and is generated when running `setupDockerEnv.sh`.
# The default tag is 'latest', so if you want to build a specific tag, edit the value of
# `IMAGE_TAG` within `docker/.env`.
source $docker_env_path

docker build -t $IMAGE_REPO:$IMAGE_TAG ${script_dir}
