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

# Dependency health: pip-audit over the image's Python environment and a syft
# SPDX SBOM of the source tree. pip-audit queries vulnerability services and
# needs network access. Findings are keyed as "<package>==<version>: <vuln-id>".

source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
quality_init dependency-health "Usage: $0 [--update-baseline]
Runs pip-audit (installed environment) and syft (SBOM of the repository)." "$@"

quality_require_tool pip-audit syft
syft scan "dir:${QUALITY_ROOT}" --exclude './build/**' --exclude './.worktrees/**' \
    -o "spdx-json=${REPORT_DIR}/sbom.spdx.json" > "${REPORT_DIR}/syft.log" 2>&1 \
    || quality_finish error "syft failed (see syft.log)"

rm -f "${REPORT_DIR}/pip-audit.json"
set +e
pip-audit --format json --output "${REPORT_DIR}/pip-audit.json" --progress-spinner off \
    > "${REPORT_DIR}/pip-audit.log" 2>&1
rc=$?
set -e
[[ -s "${REPORT_DIR}/pip-audit.json" ]] || quality_finish error "pip-audit exited $rc with no report (see pip-audit.log)"

python3 - "${REPORT_DIR}/pip-audit.json" > "${REPORT_DIR}/findings.txt" <<'PY'
import json
import sys

for dep in json.load(open(sys.argv[1])).get("dependencies", []):
    for v in dep.get("vulns", []):
        print(f"{dep['name']}=={dep.get('version', '?')}: {v['id']}")
PY

BASELINE="${BASELINE_DIR}/dependency-health-baseline.txt"
if quality_update_requested "$@"; then
    quality_write_baseline "${REPORT_DIR}/findings.txt" "$BASELINE" "pip-audit baseline (known, accepted vulnerabilities)"
    quality_finish pass "baseline updated ($(wc -l < "${REPORT_DIR}/findings.txt") vulnerabilities)"
fi
new="$(quality_compare_baseline "$BASELINE" "${REPORT_DIR}/findings.txt" "${REPORT_DIR}/new-findings.txt")"
total="$(wc -l < "${REPORT_DIR}/findings.txt" | tr -d ' ')"
[[ "$new" -gt 0 ]] && quality_finish findings "${new} new of ${total} vulnerabilities (new-findings.txt)"
quality_finish pass "0 new of ${total} vulnerabilities"
