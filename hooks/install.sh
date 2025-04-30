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

# Installs git commit hooks.

MY_DIR=$(realpath $(dirname $0))
TOP_DIR=${MY_DIR}/..
HOOKS_DIR=${TOP_DIR}/.git/hooks

HOOKS_SUPPORTED=(pre-commit)

# Copy various hooks, namespaced
cp ${MY_DIR}/clang-format-hooks/apply-format ${HOOKS_DIR}/
cp ${MY_DIR}/clang-format-hooks/git-pre-commit-format ${HOOKS_DIR}/pre-commit-clang-format

# Copy our hook(s) that will call the others.
for hook in "${HOOKS_SUPPORTED[@]}"; do
    echo "Installing ${hook} hook."
    cp ${MY_DIR}/${hook} ${HOOKS_DIR}/${hook}
done
