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

# Cyclomatic complexity with lizard. Thresholds: CCN 15, length 1000, arguments 10.
# Findings are functions over a threshold, keyed as "<file>: <function>".

source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
quality_init complexity "Usage: $0 [--update-baseline]
Runs lizard (-C 15 -L 1000 -a 10) over api/, core/, libs/, reference/." "$@"

quality_require_tool lizard
SRC=(api core libs reference)
EXCLUDES=(-x "*/test/*" -x "*/tests/*" -x "*/zap-generated/*")

# One lizard pass; thresholds applied here match lizard -w (-C 15 -L 1000 -a 10).
lizard "${EXCLUDES[@]}" --csv "${SRC[@]}" > "${REPORT_DIR}/lizard.csv" \
    || quality_finish error "lizard failed"
# CSV columns: nloc, ccn, token, param, length, location, file, function, long_name, start, end
python3 - "${REPORT_DIR}/lizard.csv" > "${REPORT_DIR}/findings.unsorted.txt" <<'PY' \
    || quality_finish error "cannot parse lizard.csv"
import csv
import sys

with open(sys.argv[1], newline="") as f:
    rows = list(csv.reader(f))
# lizard --csv has no header; check the shape so a layout change fails loudly.
if rows and (len(rows[0]) != 11 or not all(rows[0][i].isdigit() for i in (0, 1, 2, 3, 4))):
    sys.exit(f"unexpected lizard CSV layout: {rows[0]}")
for row in rows:
    ccn, params, length = int(row[1]), int(row[3]), int(row[4])
    if ccn > 15 or length > 1000 or params > 10:
        print(f"{row[6]}: {row[7]}")
PY
LC_ALL=C sort "${REPORT_DIR}/findings.unsorted.txt" > "${REPORT_DIR}/findings.txt"

BASELINE="${BASELINE_DIR}/complexity-baseline.txt"
if quality_update_requested "$@"; then
    quality_write_baseline "${REPORT_DIR}/findings.txt" "$BASELINE" "lizard complexity baseline"
    quality_finish pass "baseline updated ($(wc -l < "${REPORT_DIR}/findings.txt") functions over threshold)"
fi
new="$(quality_compare_baseline "$BASELINE" "${REPORT_DIR}/findings.txt" "${REPORT_DIR}/new-findings.txt")"
total="$(wc -l < "${REPORT_DIR}/findings.txt" | tr -d ' ')"
[[ "$new" -gt 0 ]] && quality_finish findings "${new} new functions over threshold (new-findings.txt); ${total} total"
quality_finish pass "0 new functions over threshold; ${total} total"
