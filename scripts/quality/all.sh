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

python3 - "${REPORTS_ROOT}" "${REPORTS_ROOT}/summary.json" "${CHECKS[@]}" <<'PY'
import datetime
import json
import os
import sys

root, out, checks = sys.argv[1], sys.argv[2], sys.argv[3:]
results = []
for check in checks:
    path = os.path.join(root, check, "summary.json")
    try:
        with open(path) as f:
            results.append(json.load(f))
    except (OSError, ValueError):
        results.append({"check": check, "status": "error", "message": "no summary.json written"})
with open(out, "w") as f:
    json.dump({"generated": datetime.datetime.now(datetime.timezone.utc).isoformat(),
               "checks": results}, f, indent=2)
    f.write("\n")
for r in results:
    print(f"{r['check']:<20} {r['status']:<9} {r['message']}")
PY

worst=0
for check in "${CHECKS[@]}"; do
    rc=${RESULTS[$check]}
    case $rc in
        2) worst=2 ;;
        1) [[ $worst -ne 2 ]] && worst=1 ;;
        3) [[ $worst -eq 0 ]] && worst=3 ;;
        0) ;;
        *) worst=2 ;;
    esac
done
echo "Summary: ${REPORTS_ROOT}/summary.json"
exit $worst
