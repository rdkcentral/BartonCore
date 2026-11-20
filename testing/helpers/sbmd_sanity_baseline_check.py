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
Compare current measured values for the sanity-ceiling metrics (see
sbmd_metrics_report_test.py) against their checked-in ceiling constants.

This is a human-assistance tool ONLY -- it never writes to the constants.
Run it after a testing/run_observability_suite.py pass, read its output, and
decide by hand whether a ceiling needs raising via a reviewed code change.
Never run as part of CI; not a pytest test (no test_ prefix, not collected).

Usage:
    python3 testing/helpers/sbmd_sanity_baseline_check.py [report_json_path]
"""

import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(REPO))

from testing.observability.sbmd_metrics_report_test import (  # noqa: E402
    _SANITY_CEILING_DEFERRED_DEPTH_TOGGLE,
    _SANITY_CEILING_GC_ROOTS,
)

DEFAULT_REPORT = REPO / "testing" / ".metrics-reports" / "sbmd_metrics.json"


def _gauge(scenario_metrics, name):
    dps = scenario_metrics.get(name, {}).get("dataPoints", [])
    return dps[0]["value"] if dps else None


def _hist_attr_sum(scenario_metrics, name, **attrs):
    for dp in scenario_metrics.get(name, {}).get("dataPoints", []):
        dp_attrs = dp.get("attributes", {})
        if all(dp_attrs.get(k) == v for k, v in attrs.items()):
            return dp.get("sum")
    return None


def main():
    report_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_REPORT

    if not report_path.exists():
        print(f"No report found at {report_path}. Run testing/run_observability_suite.py first.")
        return 1

    data = json.loads(report_path.read_text())
    scenarios = data.get("scenarios", {})

    print(f"Comparing {report_path} (run_id={data.get('run_id')}) against checked-in ceilings.")
    print("Advisory only -- this script never edits the constants.\n")

    handler_profile = scenarios.get("handler_profile", {}).get("metrics", {})
    gc_roots = _gauge(handler_profile, "sbmd.js.gc_roots")
    print(f"sbmd.js.gc_roots (handler_profile): measured={gc_roots}  ceiling={_SANITY_CEILING_GC_ROOTS}")
    if gc_roots is not None and gc_roots > _SANITY_CEILING_GC_ROOTS * 0.75:
        print("  NOTE: within 75% of the ceiling -- worth a look even though it's still passing.")

    deferred_profile = scenarios.get("deferred_profile", {}).get("metrics", {})
    toggle_depth = _hist_attr_sum(
        deferred_profile, "sbmd.deferred.depth",
        driver="deferred-command-test", op_type="execute", resource_id="toggle",
    )
    print(
        f"sbmd.deferred.depth{{toggle}} (deferred_profile): measured={toggle_depth}  "
        f"ceiling={_SANITY_CEILING_DEFERRED_DEPTH_TOGGLE}"
    )

    return 0


if __name__ == "__main__":
    sys.exit(main())
