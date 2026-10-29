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

# clang-tidy over first-party sources using build/dev/compile_commands.json and
# the repo .clang-tidy. Findings are keyed as "<file>: <check>: <message>"
# (line numbers omitted so unrelated edits do not churn the baseline).

source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
quality_init clang-tidy "Usage: $0 [--update-baseline]
Runs clang-tidy (via run-clang-tidy) on api/, core/, libs/, reference/ sources
listed in build/dev/compile_commands.json and compares against
config/quality/clang-tidy-baseline.txt." "$@"

quality_require_tool clang-tidy
RUN_CLANG_TIDY="$(command -v run-clang-tidy-18 || command -v run-clang-tidy || true)"
[[ -n "$RUN_CLANG_TIDY" ]] || quality_finish blocked "missing tool: run-clang-tidy"

quality_preset_build dev || quality_finish error "dev preset build failed"
BASELINE="${BASELINE_DIR}/clang-tidy-baseline.txt"
RAW="${REPORT_DIR}/clang-tidy.txt"

set +e
"$RUN_CLANG_TIDY" -quiet -p "${PRESET_BUILD_DIR}" -j "$(nproc)" \
    -header-filter="^${QUALITY_ROOT}/(api|core|libs|reference)/" \
    "^${QUALITY_ROOT}/(api|core|libs|reference)/" > "$RAW" 2>"${REPORT_DIR}/clang-tidy.stderr"
rc=$?
set -e
# run-clang-tidy exits non-zero when any diagnostic is reported; only treat it as
# a tool error when no diagnostics were produced at all.
if [[ $rc -ne 0 ]] && ! grep -qE ': (warning|error): ' "$RAW"; then
    quality_finish error "run-clang-tidy exited $rc (see clang-tidy.stderr)"
fi

grep -E '^/.*:[0-9]+:[0-9]+: (warning|error): .*\[[^]]+\]$' "$RAW" \
    | sed -E "s|^${QUALITY_ROOT}/||" \
    | LC_ALL=C sort -u \
    | sed -E "s|^([^:]+):[0-9]+:[0-9]+: (warning\|error): (.*) \[([^]]+)\]$|\1: \4: \3|" \
    | LC_ALL=C sort > "${REPORT_DIR}/findings.txt" || true

if quality_update_requested "$@"; then
    quality_write_baseline "${REPORT_DIR}/findings.txt" "$BASELINE" "clang-tidy baseline"
    quality_finish pass "baseline updated ($(wc -l < "${REPORT_DIR}/findings.txt") findings)"
fi

new="$(quality_compare_baseline "$BASELINE" "${REPORT_DIR}/findings.txt" "${REPORT_DIR}/new-findings.txt")"
total="$(wc -l < "${REPORT_DIR}/findings.txt" | tr -d ' ')"
if [[ "$new" -gt 0 ]]; then
    quality_finish findings "${new} new of ${total} findings (new-findings.txt)"
fi
quality_finish pass "0 new of ${total} findings"
