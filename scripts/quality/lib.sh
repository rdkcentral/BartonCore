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

# Shared helpers for scripts/quality/<check>.sh.
#
# Contract: every check writes its native report(s) and a summary.json to
# ${BCORE_REPORTS_DIR:-<repo>/build/reports}/<check>/ and exits with:
#   0  pass      tool ran, no new findings versus the committed baseline
#   1  findings  tool ran, new findings
#   2  error     tool failed to run
#   3  blocked   missing tool, build artifact, or baseline
# Scripts never install anything.

set -euo pipefail

QUALITY_ROOT="$(git -C "$(dirname "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)"
REPORTS_ROOT="${BCORE_REPORTS_DIR:-${QUALITY_ROOT}/build/reports}"
BASELINE_DIR="${QUALITY_ROOT}/config/quality"
BUILD_ROOT="${QUALITY_ROOT}/build"

# quality_init <check-name> <usage-text> "$@"
# Creates the report directory and handles --help.
quality_init() {
    CHECK_NAME="$1"
    local usage="$2"
    shift 2
    for arg in "$@"; do
        if [[ "$arg" == "-h" || "$arg" == "--help" ]]; then
            printf '%s\n' "$usage"
            printf '\nExit codes: 0 pass, 1 new findings, 2 tool error, 3 blocked.\n'
            printf 'Reports: %s/%s/\n' "${REPORTS_ROOT}" "${CHECK_NAME}"
            exit 0
        fi
    done
    REPORT_DIR="${REPORTS_ROOT}/${CHECK_NAME}"
    mkdir -p "${REPORT_DIR}"
    cd "${QUALITY_ROOT}"
}

# quality_summary <pass|findings|error|blocked> <message>
quality_summary() {
    local status="$1" message="$2"
    python3 - "${REPORT_DIR}/summary.json" "${CHECK_NAME}" "${status}" "${message}" <<'PY'
import datetime
import json
import sys

path, check, status, message = sys.argv[1:5]
with open(path, "w") as f:
    json.dump({"check": check, "status": status, "message": message,
               "generated": datetime.datetime.now(datetime.timezone.utc).isoformat()},
              f, indent=2)
    f.write("\n")
PY
    echo "${CHECK_NAME}: ${status} - ${message} (${REPORT_DIR})"
}

# quality_finish <status> <message>: write summary.json and exit with the mapped code.
quality_finish() {
    local status="$1" message="$2" code
    case "$status" in
        pass) code=0 ;;
        findings) code=1 ;;
        error) code=2 ;;
        blocked) code=3 ;;
        *) echo "quality_finish: unknown status '$status'" >&2; code=2; status=error ;;
    esac
    quality_summary "$status" "$message"
    exit "$code"
}

quality_require_tool() {
    local tool
    for tool in "$@"; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            quality_finish blocked "missing tool: $tool"
        fi
    done
}

quality_require_file() {
    if [[ ! -e "$1" ]]; then
        quality_finish blocked "${2:-missing file: $1}"
    fi
}

# quality_preset_build <preset> [cmake --build args...]
# Configures build/<preset> via CMakePresets.json if needed, then builds it.
# Sets PRESET_BUILD_DIR. Returns non-zero on configure or build failure.
quality_preset_build() {
    local preset="$1"
    shift
    PRESET_BUILD_DIR="${BUILD_ROOT}/${preset}"
    if [[ ! -f "${PRESET_BUILD_DIR}/CMakeCache.txt" ]]; then
        cmake --preset "$preset" || return 1
    fi
    cmake --build "${PRESET_BUILD_DIR}" --parallel "$(nproc)" "$@" || return 1
}

# quality_compare_baseline <baseline-file> <current-file> <new-findings-file>
# Both inputs are line-oriented finding keys compared as multisets: a key that
# occurs more often in current than in baseline counts once per extra
# occurrence. Writes the extra keys to the output file and echoes their count.
# A missing baseline is treated as empty.
quality_compare_baseline() {
    local baseline="$1" current="$2" out="$3"
    if [[ -f "$baseline" ]]; then
        LC_ALL=C comm -13 <(grep -v '^#' "$baseline" | LC_ALL=C sort) <(LC_ALL=C sort "$current") > "$out"
    else
        LC_ALL=C sort "$current" > "$out"
    fi
    wc -l < "$out" | tr -d ' '
}

# quality_update_requested "$@": true when --update-baseline was passed.
quality_update_requested() {
    local arg
    for arg in "$@"; do
        [[ "$arg" == "--update-baseline" ]] && return 0
    done
    return 1
}

# quality_write_baseline <current-file> <baseline-file> <description>
quality_write_baseline() {
    local current="$1" baseline="$2" desc="$3"
    {
        echo "# ${desc}"
        echo "# Generated by scripts/quality/${0##*/}${QUALITY_SCRIPT_ARGS:+ ${QUALITY_SCRIPT_ARGS}} --update-baseline."
        echo "# One finding key per occurrence; repeated keys are compared by count."
        LC_ALL=C sort "$current"
    } > "$baseline"
}
