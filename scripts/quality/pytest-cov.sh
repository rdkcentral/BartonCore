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

# Python coverage of the integration test harness (testing/) via pytest-cov.
# Requires libBartonCore built and installed for the integration tests.
# Extra arguments are passed through to testing/py_test.sh.

source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
quality_init pytest-cov "Usage: $0 [--sanitizer=<asan|ubsan|tsan|none>] [pytest args]
Runs testing/py_test.sh with --cov (settings from [tool.coverage] in pyproject.toml)." "$@"

quality_require_tool pytest
python3 -c 'import pytest_cov' 2>/dev/null || quality_finish blocked "missing tool: pytest-cov"
# The integration tests import BartonCore through GObject Introspection; the
# library must already be installed in this container (as scripts/ci/run_integration_tests.sh does).
python3 -c 'import sys, gi; gi.require_version("GIRepository", "2.0"); from gi.repository import GIRepository; sys.exit(0 if GIRepository.Repository.get_default().enumerate_versions("BCore") else 1)' 2>/dev/null \
    || quality_finish blocked "BCore typelib not installed; run 'sudo cmake --build build --target install' (see scripts/ci/run_integration_tests.sh) in the same container first"

set +e
COVERAGE_FILE="${REPORT_DIR}/.coverage" testing/py_test.sh --cov --cov-report="xml:${REPORT_DIR}/coverage.xml" \
    --cov-report=term "$@" > "${REPORT_DIR}/pytest.log" 2>&1
rc=$?
set -e
quality_require_file "${REPORT_DIR}/coverage.xml" "pytest-cov produced no coverage.xml (see pytest.log)"
rate="$(python3 -c 'import sys,xml.etree.ElementTree as E; print(round(float(E.parse(sys.argv[1]).getroot().get("line-rate"))*100,2))' "${REPORT_DIR}/coverage.xml")"
case $rc in
    0) quality_finish pass "line coverage ${rate}%" ;;
    1) quality_finish findings "tests failed (pytest.log); line coverage ${rate}%" ;;
    *) quality_finish error "pytest exited $rc (see pytest.log)" ;;
esac
