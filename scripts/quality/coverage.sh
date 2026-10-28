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

# Unit-test line/branch coverage: builds the 'coverage' preset, runs ctest, and
# produces lcov and Cobertura (gcovr) reports for first-party code.

source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
quality_init coverage "Usage: $0
Builds build/coverage, runs ctest, writes lcov.info, cobertura.xml, and an HTML report." "$@"

quality_require_tool lcov gcovr
quality_preset_build coverage > "${REPORT_DIR}/build.log" 2>&1 \
    || quality_finish error "coverage preset build failed (see build.log)"
BDIR="${PRESET_BUILD_DIR}"

lcov --zerocounters --directory "$BDIR" > /dev/null 2>&1 || true
set +e
ctest --test-dir "$BDIR" --output-on-failure --output-junit "${REPORT_DIR}/ctest.xml" > "${REPORT_DIR}/ctest.log" 2>&1
ctest_rc=$?
set -e

# The dev toolchain is clang; gcov must understand clang's .gcda format.
GCOV_TOOL="gcov"
if grep -qs 'set(CMAKE_C_COMPILER_ID "Clang")' "$BDIR"/CMakeFiles/*/CMakeCCompiler.cmake && command -v llvm-cov-18 >/dev/null; then
    printf '#!/bin/sh\nexec llvm-cov-18 gcov "$@"\n' > "${REPORT_DIR}/llvm-gcov.sh"
    chmod +x "${REPORT_DIR}/llvm-gcov.sh"
    GCOV_TOOL="${REPORT_DIR}/llvm-gcov.sh"
fi

lcov --capture --directory "$BDIR" --gcov-tool "$GCOV_TOOL" --output-file "${REPORT_DIR}/lcov.raw.info" \
    --ignore-errors mismatch,inconsistent,unused > "${REPORT_DIR}/lcov.log" 2>&1 \
    || quality_finish error "lcov capture failed (see lcov.log)"
lcov --remove "${REPORT_DIR}/lcov.raw.info" '/usr/*' '*/test/*' '*/third_party/*' '*/_deps/*' '*/matter-install/*' \
    --output-file "${REPORT_DIR}/lcov.info" --ignore-errors unused,inconsistent >> "${REPORT_DIR}/lcov.log" 2>&1 \
    || quality_finish error "lcov filter failed (see lcov.log)"

mkdir -p "${REPORT_DIR}/html"
gcovr --root "$QUALITY_ROOT" --object-directory "$BDIR" --gcov-executable "$GCOV_TOOL" \
    --filter 'api/' --filter 'core/' --filter 'libs/' --exclude '.*/test/.*' \
    --gcov-ignore-errors all \
    --cobertura "${REPORT_DIR}/cobertura.xml" --html-details "${REPORT_DIR}/html/index.html" \
    --txt-summary > "${REPORT_DIR}/gcovr-summary.txt" 2>"${REPORT_DIR}/gcovr.log" \
    || quality_finish error "gcovr failed (see gcovr.log)"

lines="$(grep -E '^lines:' "${REPORT_DIR}/gcovr-summary.txt" | head -1 || true)"
[[ $ctest_rc -ne 0 ]] && quality_finish findings "ctest failed under coverage (ctest.log); ${lines}"
quality_finish pass "${lines:-coverage generated}"
