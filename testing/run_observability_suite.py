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

"""
Do-everything runner for the SBMD observability suite.

The observability characterization / report tests live in ``testing/observability/``
and are deliberately excluded from the CI integration run (they are heavy and
emit a metrics report rather than gate merges).  This script does the whole
dance in one command: build + install BartonCore, bring up the runtime
prerequisites, run the entire observability suite, and point at the consolidated
metrics report it produces.

The report itself is written by the suite to ``testing/.metrics-reports/``
(``sbmd_metrics.txt`` / ``.json`` + per-scenario ``.csv``).  This runner also
tees the console output to ``testing/.metrics-reports/observability_run.log``.

Usage:
    python3 testing/run_observability_suite.py                 # build + deps + run
    python3 testing/run_observability_suite.py --no-build      # skip cmake build/install
    python3 testing/run_observability_suite.py --no-deps       # skip dbus + npm ci
    python3 testing/run_observability_suite.py -- -k heap -v   # pass args through to pytest
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
OBS_DIR = REPO / "testing" / "observability"
REPORT_DIR = REPO / "testing" / ".metrics-reports"
RUN_LOG = REPORT_DIR / "observability_run.log"


def _run(cmd, check=True):
    print(f"\n$ {' '.join(str(c) for c in cmd)}", flush=True)
    return subprocess.run(cmd, cwd=REPO, check=check)


def main():
    parser = argparse.ArgumentParser(
        description="Build, prep, run the SBMD observability suite, and emit a report."
    )
    parser.add_argument("--no-build", action="store_true", help="skip cmake build + install")
    parser.add_argument("--no-deps", action="store_true", help="skip dbus start + matter.js npm ci")
    parser.add_argument(
        "pytest_args",
        nargs=argparse.REMAINDER,
        help="args after `--` are passed through to pytest (e.g. -- -k heap)",
    )
    args = parser.parse_args()

    # Strip a leading `--` separator if argparse kept it.
    extra = args.pytest_args[1:] if args.pytest_args[:1] == ["--"] else args.pytest_args

    if not args.no_build:
        _run(["cmake", "--build", "build", "--target", "install"])

    if not args.no_deps:
        # BartonCore's client needs the D-Bus session bus; best-effort (already up is fine).
        _run(["sudo", "-n", "service", "dbus", "start"], check=False)
        _run(["npm", "--prefix", "testing/mocks/devices/matterjs", "ci"])

    REPORT_DIR.mkdir(parents=True, exist_ok=True)

    print(f"\n{'=' * 72}\nRunning observability suite: {OBS_DIR}\n{'=' * 72}", flush=True)
    pytest_cmd = ["./testing/py_test.sh", str(OBS_DIR), "-v", *extra]
    print(f"$ {' '.join(pytest_cmd)}", flush=True)

    env = {**os.environ, "SBMD_VERBOSE": "1"}  # surface passing-test report output
    with RUN_LOG.open("w", encoding="utf-8") as log:
        proc = subprocess.Popen(
            pytest_cmd,
            cwd=REPO,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        for line in proc.stdout:
            sys.stdout.write(line)
            log.write(line)
        rc = proc.wait()

    report_txt = REPORT_DIR / "sbmd_metrics.txt"
    print(f"\n{'=' * 72}")
    print(f"pytest exit code : {rc}")
    print(f"console log      : {RUN_LOG}")
    if report_txt.exists():
        print(f"metrics report   : {report_txt}")
        print(f"                   (+ sbmd_metrics.json and <scenario>.csv in {REPORT_DIR})")
    else:
        print(f"WARNING: no report at {report_txt} — did any report scenario run?")
    print(f"{'=' * 72}")
    return rc


if __name__ == "__main__":
    sys.exit(main())
