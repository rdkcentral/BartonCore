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

# Compiler warnings from a clean build of the dev preset (first-party targets
# carry -Wall -Wextra via bCoreWarnings). Findings are keyed as
# "<file>: <warning-flag>: <message>".

source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
quality_init build-warnings "Usage: $0 [--update-baseline]
Performs a clean rebuild of build/dev and collects compiler warnings." "$@"

LOG="${REPORT_DIR}/build.log"
: > "$LOG"
if [[ ! -f "${BUILD_ROOT}/dev/CMakeCache.txt" ]]; then
    cmake --preset dev >> "$LOG" 2>&1 || quality_finish error "dev preset configure failed (see build.log)"
fi
if ! cmake --build "${BUILD_ROOT}/dev" --clean-first --parallel "$(nproc)" >> "$LOG" 2>&1; then
    grep -E ': error: ' "$LOG" > "${REPORT_DIR}/errors.txt" || true
    quality_finish findings "build failed: $(wc -l < "${REPORT_DIR}/errors.txt") errors (errors.txt, build.log)"
fi

grep -E '^[^ ]+:[0-9]+:[0-9]+: warning: ' "$LOG" \
    | sed -E "s|^${QUALITY_ROOT}/||; s|^\.\./\.\./||" \
    | LC_ALL=C sort -u \
    | sed -E 's|^([^:]+):[0-9]+:[0-9]+: warning: (.*) \[(-W[^]]+)\]$|\1: \3: \2|' \
    | grep -vE '^(/|third_party/|build/)' \
    | LC_ALL=C sort > "${REPORT_DIR}/warnings.txt" || true

BASELINE="${BASELINE_DIR}/build-warnings-baseline.txt"
if quality_update_requested "$@"; then
    quality_write_baseline "${REPORT_DIR}/warnings.txt" "$BASELINE" "build warnings baseline"
    quality_finish pass "baseline updated ($(wc -l < "${REPORT_DIR}/warnings.txt") warnings)"
fi
new="$(quality_compare_baseline "$BASELINE" "${REPORT_DIR}/warnings.txt" "${REPORT_DIR}/new-warnings.txt")"
total="$(wc -l < "${REPORT_DIR}/warnings.txt" | tr -d ' ')"
[[ "$new" -gt 0 ]] && quality_finish findings "${new} new of ${total} warnings (new-warnings.txt)"
quality_finish pass "0 new of ${total} warnings"
