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

# cppcheck via the CMake 'cppcheck' target. Findings are keyed as
# "<file>: <id>: <message>" and compared against config/quality/cppcheck-baseline.txt.
# Intentional suppressions belong in config/quality/cppcheck-suppressions.txt.

source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
quality_init cppcheck "Usage: $0 [--full] [--update-baseline]
Configures build/dev and runs 'cmake --build build/dev --target cppcheck'.
By default only C/C++ sources changed since the merge base with
\${QUALITY_BASE_REF:-origin/main} (falling back to main), plus sources that
include a changed header, are analyzed. --full and --update-baseline analyze
every translation unit (slow: whole-tree analysis takes CPU-hours)." "$@"

quality_require_tool cppcheck
# Generated headers must exist for cppcheck to resolve includes.
quality_preset_build dev || quality_finish error "dev preset build failed"

full=0
for arg in "$@"; do
    [[ "$arg" == "--full" || "$arg" == "--update-baseline" ]] && full=1
done

unset BCORE_CPPCHECK_ONLY
scope="all translation units"
if [[ $full -eq 0 ]]; then
    base_ref="${QUALITY_BASE_REF:-}"
    if [[ -z "$base_ref" ]]; then
        if git rev-parse -q --verify origin/main > /dev/null; then base_ref=origin/main; else base_ref=main; fi
    fi
    merge_base="$(git merge-base "$base_ref" HEAD 2> /dev/null)" \
        || quality_finish blocked "cannot resolve merge base with '${base_ref}' (set QUALITY_BASE_REF)"
    changed="${REPORT_DIR}/changed-files.txt"
    { git diff --name-only --diff-filter=d "$merge_base"; git ls-files --others --exclude-standard; } \
        | grep -E '\.(c|cc|cpp|h|hpp)$' | sort -u > "$changed" || true
    only="${REPORT_DIR}/analyzed-files.txt"
    {
        grep -E '\.(c|cc|cpp)$' "$changed" || true
        while IFS= read -r header; do
            git grep -l -E "#[[:space:]]*include[[:space:]]*[<\"]([^>\"]*/)?$(basename "$header")[>\"]" \
                -- '*.c' '*.cc' '*.cpp' || true
        done < <(grep -E '\.(h|hpp)$' "$changed" || true)
    } | sort -u > "$only"
    if [[ ! -s "$only" ]]; then
        : > "${REPORT_DIR}/findings.txt"
        quality_finish pass "no C/C++ sources affected since ${merge_base:0:12}"
    fi
    export BCORE_CPPCHECK_ONLY="$only"
    scope="$(wc -l < "$only" | tr -d ' ') files affected since ${merge_base:0:12}"
fi

XML="${REPORT_DIR}/cppcheck.xml"
rm -f "$XML" "${BUILD_ROOT}/dev/cppcheck.xml"
cmake --build "${BUILD_ROOT}/dev" --target cppcheck \
    > "${REPORT_DIR}/cppcheck.log" 2>&1 || quality_finish error "cppcheck target failed (see cppcheck.log)"

quality_require_file "${BUILD_ROOT}/dev/cppcheck.xml" "cppcheck produced no report"
cp "${BUILD_ROOT}/dev/cppcheck.xml" "$XML"
python3 - "$XML" "$QUALITY_ROOT" > "${REPORT_DIR}/findings.txt" <<'PY'
import os
import sys
import xml.etree.ElementTree as ET

root = sys.argv[2] + os.sep
seen = set()
for err in ET.parse(sys.argv[1]).getroot().iter("error"):
    loc = err.find("location")
    path = loc.get("file", "") if loc is not None else ""
    line = loc.get("line", "") if loc is not None else ""
    # One key per distinct location; the line is dropped from the key so
    # unrelated edits do not shift it.
    if (path, line, err.get("id"), err.get("msg")) in seen:
        continue
    seen.add((path, line, err.get("id"), err.get("msg")))
    print(f"{path.removeprefix(root)}: {err.get('id')}: {err.get('msg')}")
PY

BASELINE="${BASELINE_DIR}/cppcheck-baseline.txt"
if quality_update_requested "$@"; then
    quality_write_baseline "${REPORT_DIR}/findings.txt" "$BASELINE" "cppcheck baseline"
    quality_finish pass "baseline updated ($(wc -l < "${REPORT_DIR}/findings.txt") findings)"
fi
new="$(quality_compare_baseline "$BASELINE" "${REPORT_DIR}/findings.txt" "${REPORT_DIR}/new-findings.txt")"
total="$(wc -l < "${REPORT_DIR}/findings.txt" | tr -d ' ')"
[[ "$new" -gt 0 ]] && quality_finish findings "${new} new of ${total} findings in ${scope} (new-findings.txt)"
quality_finish pass "0 new of ${total} findings (${scope})"
