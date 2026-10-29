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

# Copy-paste detection with jscpd using the repo .jscpd.json. Fails when the
# overall duplicated-lines percentage exceeds config/quality/duplication-baseline.json.

source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
quality_init duplication "Usage: $0 [--update-baseline]
Runs jscpd over the repository per .jscpd.json." "$@"

quality_require_tool jscpd
jscpd --config .jscpd.json --output "${REPORT_DIR}" --silent . > "${REPORT_DIR}/jscpd.log" 2>&1 \
    || quality_finish error "jscpd failed (see jscpd.log)"
JSON="${REPORT_DIR}/jscpd-report.json"
quality_require_file "$JSON" "jscpd produced no report"
pct="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["statistics"]["total"]["percentage"])' "$JSON")"

BASELINE="${BASELINE_DIR}/duplication-baseline.json"
if quality_update_requested "$@"; then
    printf '{\n  "percentage": %s\n}\n' "$pct" > "$BASELINE"
    quality_finish pass "baseline updated (${pct}% duplicated lines)"
fi
quality_require_file "$BASELINE" "missing baseline: config/quality/duplication-baseline.json"
base="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["percentage"])' "$BASELINE")"
if python3 -c 'import sys; sys.exit(0 if float(sys.argv[1]) > float(sys.argv[2]) else 1)' "$pct" "$base"; then
    quality_finish findings "duplication ${pct}% exceeds baseline ${base}%"
fi
quality_finish pass "duplication ${pct}% (baseline ${base}%)"
