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
# Created by Kevin Funderburg on 1/22/2025.
#

set -e

# Resolve the repository root from this script's own location so CI operates on
# the working tree it lives in. This keeps the script correct for git worktrees
# instead of relying on BARTON_TOP, which is baked into docker/.env at
# container-launch time and points at whichever checkout provisioned the
# container (typically the primary clone).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." &>/dev/null && pwd)"

sudo service dbus start
JOBS=$(nproc)
(( JOBS > 1 )) && JOBS=$((JOBS - 1))
(( JOBS < 1 )) && JOBS=1
cmake --build $REPO_ROOT/build --parallel "$JOBS" --target install

# Install matter.js virtual device dependencies
npm --prefix $REPO_ROOT/testing/mocks/devices/matterjs ci

echo ""
echo "***********************************"
echo "End of installation, starting tests"
echo "***********************************"
echo ""

$REPO_ROOT/testing/py_test.sh $REPO_ROOT/testing
