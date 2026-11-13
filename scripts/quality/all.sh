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

# Runs every quality check and aggregates the per-check summary.json files into
# build/reports/summary.json. Exit code is the most severe result:
# 2 (any error) > 1 (any findings) > 3 (any blocked) > 0.

source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
quality_init all "Usage: $0 [check ...]
Runs all checks (or only the named ones). Check names: clang-tidy cppcheck
build-warnings complexity duplication coverage pytest-cov sanitizers-asan
sanitizers-ubsan sanitizers-tsan abi binary-size python-lint
dependency-health." "$@"

ALL_CHECKS=(build-warnings clang-tidy cppcheck complexity duplication python-lint
    dependency-health binary-size abi coverage sanitizers-asan sanitizers-ubsan
    sanitizers-tsan pytest-cov)
CHECKS=("$@")
[[ ${#CHECKS[@]} -eq 0 ]] && CHECKS=("${ALL_CHECKS[@]}")

HERE="${QUALITY_ROOT}/scripts/quality"
declare -A RESULTS=()
for check in "${CHECKS[@]}"; do
    echo "==> ${check}"
    rm -f "${REPORTS_ROOT}/${check}/summary.json"
    set +e
    case "$check" in
        sanitizers-*) "${HERE}/sanitizers.sh" "${check#sanitizers-}" ;;
        *) "${HERE}/${check}.sh" ;;
    esac
    RESULTS[$check]=$?
    set -e
done

RC_ARGS=()
for check in "${CHECKS[@]}"; do
    RC_ARGS+=("${check}=${RESULTS[$check]}")
done

# The aggregate exit code is derived from the same per-check status written to
# summary.json, so the two always agree.
set +e
python3 - "${REPORTS_ROOT}" "${REPORTS_ROOT}/summary.json" "${RC_ARGS[@]}" <<'PY'
import datetime
import json
import os
import sys

CODES = {"pass": 0, "findings": 1, "error": 2, "blocked": 3}
RANK = {0: 0, 3: 1, 1: 2, 2: 3}  # severity: error > findings > blocked > pass

root, out, pairs = sys.argv[1], sys.argv[2], sys.argv[3:]
results = []
for pair in pairs:
    check, rc = pair.rsplit("=", 1)
    rc = int(rc)
    path = os.path.join(root, check, "summary.json")
    try:
        with open(path) as f:
            result = json.load(f)
    except (OSError, ValueError):
        result = {"check": check, "status": "error", "message": f"no summary.json written (exit {rc})"}
    if CODES.get(result.get("status")) != rc and result["status"] != "error":
        result = {"check": check, "status": "error",
                  "message": f"exit {rc} does not match status '{result.get('status')}': {result.get('message', '')}"}
    results.append(result)
with open(out, "w") as f:
    json.dump({"generated": datetime.datetime.now(datetime.timezone.utc).isoformat(),
               "checks": results}, f, indent=2)
    f.write("\n")
for r in results:
    print(f"{r['check']:<20} {r['status']:<9} {r['message']}")
worst = max((CODES[r["status"]] for r in results), key=RANK.get, default=0)
sys.exit(worst)
PY
worst=$?
set -e
echo "Summary: ${REPORTS_ROOT}/summary.json"
exit $worst
