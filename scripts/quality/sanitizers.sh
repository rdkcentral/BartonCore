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

# Builds a sanitizer preset and runs the unit tests (ctest) under it.

source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
KIND="${1:-}"
CHECK_USAGE="Usage: $0 <asan|ubsan|tsan> [--update-baseline]
Builds build/<kind> from the matching CMake preset and runs ctest."
case "$KIND" in
    asan|ubsan|tsan) ;;
    -h|--help) quality_init sanitizers "$CHECK_USAGE" --help ;;
    *) echo "$CHECK_USAGE" >&2; exit 2 ;;
esac
quality_init "sanitizers-${KIND}" "$CHECK_USAGE" "$@"
QUALITY_SCRIPT_ARGS="$KIND"

quality_preset_build "$KIND" > "${REPORT_DIR}/build.log" 2>&1 \
    || quality_finish error "${KIND} preset build failed (see build.log)"

export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}"
export TSAN_OPTIONS="${TSAN_OPTIONS:-halt_on_error=0:second_deadlock_stack=1}"
set +e
ctest --test-dir "${PRESET_BUILD_DIR}" --output-on-failure --output-junit "${REPORT_DIR}/ctest.xml" \
    > "${REPORT_DIR}/ctest.log" 2>&1
rc=$?
set -e
reports="$(grep -cE 'ERROR: (AddressSanitizer|LeakSanitizer|ThreadSanitizer)|WARNING: ThreadSanitizer|runtime error:' "${REPORT_DIR}/ctest.log" || true)"
python3 "${QUALITY_ROOT}/scripts/quality/sanitizer_reports.py" "$QUALITY_ROOT" \
    "${REPORT_DIR}/ctest.log" "${REPORT_DIR}/ctest.xml" > "${REPORT_DIR}/findings.txt" \
    || quality_finish error "failed to parse ctest.log"
[[ $rc -ne 0 && ! -s "${REPORT_DIR}/findings.txt" ]] \
    && quality_finish error "ctest exited $rc under ${KIND} with no parsable failures (ctest.log)"

BASELINE="${BASELINE_DIR}/sanitizers-${KIND}-baseline.txt"
total="$(wc -l < "${REPORT_DIR}/findings.txt" | tr -d ' ')"
if quality_update_requested "$@"; then
    quality_write_baseline "${REPORT_DIR}/findings.txt" "$BASELINE" "${KIND} baseline (sanitizer report keys and failing tests)"
    quality_finish pass "baseline updated (${total} keys)"
fi
if [[ ! -f "$BASELINE" ]]; then
    [[ "$total" -gt 0 ]] && quality_finish findings "${total} report keys under ${KIND}, no baseline (findings.txt)"
    quality_finish pass "all tests passed under ${KIND}"
fi
new="$(quality_compare_baseline "$BASELINE" "${REPORT_DIR}/findings.txt" "${REPORT_DIR}/new-findings.txt")"
[[ "$new" -gt 0 ]] && quality_finish findings "${new} new of ${total} report keys under ${KIND} (new-findings.txt; ${reports} raw reports)"
quality_finish pass "0 new of ${total} report keys under ${KIND}"
