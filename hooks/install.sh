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

MY_DIR=$(realpath "$(dirname "$0")")
TOP_DIR=${MY_DIR}/..

# Resolve the hooks directory via git rather than assuming ${TOP_DIR}/.git/hooks.
# In a linked worktree ".git" is a file (a gitdir pointer), not a directory, so
# the hardcoded path does not exist. "git rev-parse --git-path hooks" returns the
# effective (shared) hooks directory in both a normal clone and a worktree.
# Resolve it to an absolute path (git may return one relative to ${TOP_DIR}) so
# the copies below work regardless of the caller's current directory. Avoid
# "realpath -m" (a GNU extension not available on e.g. macOS): make the path
# absolute manually, create it, then resolve it with plain realpath.
HOOKS_DIR=$(cd "${TOP_DIR}" && git rev-parse --git-path hooks)
case "${HOOKS_DIR}" in
    /*) ;;
    *) HOOKS_DIR="${TOP_DIR}/${HOOKS_DIR}" ;;
esac
mkdir -p "${HOOKS_DIR}"
HOOKS_DIR=$(realpath "${HOOKS_DIR}")

HOOKS_SUPPORTED=(pre-commit post-checkout)

# Copy various hooks, namespaced
cp "${MY_DIR}/format-hooks/clang-format/apply-format" "${HOOKS_DIR}/"
cp "${MY_DIR}/format-hooks/clang-format/git-pre-commit-format" "${HOOKS_DIR}/pre-commit-clang-format"
cp "${MY_DIR}/format-hooks/pre-commit-lint-staged" "${HOOKS_DIR}/"

# Copy our hook(s) that will call the others.
for hook in "${HOOKS_SUPPORTED[@]}"; do
    echo "Installing ${hook} hook."
    cp "${MY_DIR}/${hook}" "${HOOKS_DIR}/${hook}"
done

# Additionally, install the .gitmessage
git config --local commit.template .gitmessage
