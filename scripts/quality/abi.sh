#!/usr/bin/env bash
# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2026 Comcast Cable Communications Management, LLC
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

# ABI compatibility of libBartonCore.so against .api/libBartonCore.abi
# (libabigail abidw XML generated from the last release tag; see .api/README.md).

source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
quality_init abi "Usage: $0
Builds build/dev, dumps the current ABI with abidw, and compares it with abidiff." "$@"

quality_require_tool abidw abidiff
BASELINE="${QUALITY_ROOT}/.api/libBartonCore.abi"
quality_require_file "$BASELINE" "no baseline: .api/libBartonCore.abi (see .api/README.md)"

quality_preset_build dev --target BartonCore > "${REPORT_DIR}/build.log" 2>&1 \
    || quality_finish error "dev preset build failed (see build.log)"
LIB="$(find "${PRESET_BUILD_DIR}/core" -maxdepth 1 -name 'libBartonCore.so' | head -1)"
quality_require_file "${LIB:-/nonexistent}" "libBartonCore.so not found in ${PRESET_BUILD_DIR}/core"

abidw --headers-dir api/c/public --out-file "${REPORT_DIR}/libBartonCore.abi" "$LIB" \
    || quality_finish error "abidw failed"
set +e
abidiff --headers-dir2 api/c/public "$BASELINE" "${REPORT_DIR}/libBartonCore.abi" > "${REPORT_DIR}/abidiff.txt" 2>&1
rc=$?
set -e
# abidiff exit status is a bitfield: 1 error, 2 usage error, 4 ABI change, 8 incompatible change.
(( rc & 3 )) && quality_finish error "abidiff exited $rc (see abidiff.txt)"
(( rc & 8 )) && quality_finish findings "incompatible ABI change (abidiff.txt)"
(( rc & 4 )) && quality_finish findings "ABI change, possibly compatible (abidiff.txt)"
quality_finish pass "no ABI changes versus baseline"
