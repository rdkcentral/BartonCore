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

# Binary size of libBartonCore.so (uninstrumented Release `size` preset) via
# bloaty, compared with the committed .size-snapshot.json. Growth above 10% of
# vm_total is a finding. The snapshot tracks relative growth of this build; it
# is not the size of any particular shipped product build.

source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
quality_init binary-size "Usage: $0 [--update-baseline]
Builds build/size (Release, no sanitizers) and measures libBartonCore.so; --update-baseline rewrites .size-snapshot.json." "$@"

quality_require_tool bloaty
SNAPSHOT="${QUALITY_ROOT}/.size-snapshot.json"
if quality_update_requested "$@" && [[ -n "$(git status --porcelain --untracked-files=no)" ]]; then
    quality_finish error "refusing to update .size-snapshot.json from a dirty tree; commit first"
fi
quality_preset_build size --target BartonCore > "${REPORT_DIR}/build.log" 2>&1 \
    || quality_finish error "size preset build failed (see build.log)"
LIB="$(find "${PRESET_BUILD_DIR}/core" -maxdepth 1 -name 'libBartonCore.so' | head -1)"
quality_require_file "${LIB:-/nonexistent}" "libBartonCore.so not found in ${PRESET_BUILD_DIR}/core"

bloaty -d sections -n 0 --csv "$LIB" > "${REPORT_DIR}/bloaty.csv" || quality_finish error "bloaty failed"

DESCRIBE="$(git describe --tags --always --dirty 2>/dev/null || echo unknown)"
python3 - "${REPORT_DIR}/bloaty.csv" "${REPORT_DIR}/current.json" "$DESCRIBE" <<'PY'
import csv
import json
import sys

src, dst, describe = sys.argv[1:4]
sections, vm_total, file_total = {}, 0, 0
with open(src) as f:
    for row in csv.DictReader(f):
        vm, fs = int(row["vmsize"]), int(row["filesize"])
        vm_total += vm
        file_total += fs
        if row["sections"] in (".text", ".rodata", ".data", ".bss"):
            sections[row["sections"]] = vm
with open(dst, "w") as f:
    json.dump({"artifact": "libBartonCore.so", "preset": "size", "tool": "bloaty",
               "generated_from": describe,
               "sections": {k: sections.get(k, 0) for k in (".text", ".rodata", ".data", ".bss")},
               "vm_total": vm_total, "file_total": file_total}, f, indent=2)
    f.write("\n")
PY

if quality_update_requested "$@"; then
    cp "${REPORT_DIR}/current.json" "$SNAPSHOT"
    quality_finish pass "snapshot updated"
fi
quality_require_file "$SNAPSHOT" "no baseline: .size-snapshot.json"
result="$(python3 - "$SNAPSHOT" "${REPORT_DIR}/current.json" <<'PY'
import json
import sys

base, cur = (json.load(open(p)) for p in sys.argv[1:3])
b, c = base["vm_total"], cur["vm_total"]
pct = (c - b) * 100.0 / b if b else 0.0
print(f"{'findings' if pct > 10.0 else 'pass'} vm_total {b} -> {c} bytes ({pct:+.2f}%)")
PY
)"
quality_finish "${result%% *}" "${result#* }"
