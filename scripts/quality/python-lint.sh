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

# Python static analysis with ruff and mypy (configured in pyproject.toml).
# Findings are keyed as "<file>: <code>: <message>" and compared against
# config/quality/python-lint-baseline.txt.

source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
quality_init python-lint "Usage: $0 [--update-baseline]
Runs 'ruff check' and 'mypy' using pyproject.toml settings." "$@"

quality_require_tool ruff mypy
set +e
ruff check --output-format json --output-file "${REPORT_DIR}/ruff.json" . 2>"${REPORT_DIR}/ruff.stderr"
ruff_rc=$?
mypy --no-error-summary --show-error-codes > "${REPORT_DIR}/mypy.txt" 2>&1
mypy_rc=$?
set -e
[[ $ruff_rc -gt 1 ]] && quality_finish error "ruff exited $ruff_rc (see ruff.stderr)"
[[ $mypy_rc -gt 1 ]] && quality_finish error "mypy exited $mypy_rc (see mypy.txt)"

{
    python3 - "${REPORT_DIR}/ruff.json" "$QUALITY_ROOT" <<'PY'
import json
import os
import sys

root = sys.argv[2] + os.sep
for d in json.load(open(sys.argv[1])):
    print(f"{d['filename'].removeprefix(root)}: ruff-{d['code']}: {d['message']}")
PY
    sed -nE 's|^([^:]+):[0-9]+: error: (.*)  \[([a-z-]+)\]$|\1: mypy-\3: \2|p' "${REPORT_DIR}/mypy.txt"
} | LC_ALL=C sort > "${REPORT_DIR}/findings.txt"

BASELINE="${BASELINE_DIR}/python-lint-baseline.txt"
if quality_update_requested "$@"; then
    quality_write_baseline "${REPORT_DIR}/findings.txt" "$BASELINE" "ruff + mypy baseline"
    quality_finish pass "baseline updated ($(wc -l < "${REPORT_DIR}/findings.txt") findings)"
fi
new="$(quality_compare_baseline "$BASELINE" "${REPORT_DIR}/findings.txt" "${REPORT_DIR}/new-findings.txt")"
total="$(wc -l < "${REPORT_DIR}/findings.txt" | tr -d ' ')"
[[ "$new" -gt 0 ]] && quality_finish findings "${new} new of ${total} findings (new-findings.txt)"
quality_finish pass "0 new of ${total} findings"
