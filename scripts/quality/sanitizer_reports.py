#!/usr/bin/env python3
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

"""Reduce sanitizer output (ctest logs) to stable, line-number-free finding keys.

Each report becomes "<kind>: <file>:<function>", where the location is the first
stack frame in first-party code (inside the repository, outside build/ and
third_party/). Keys are printed sorted and de-duplicated, one per line, so they
can be diffed against a committed baseline.

Failing tests from the optional ctest JUnit file are added as "test-failed: <name>".

Usage:
  sanitizer_reports.py <repo-root> <ctest.log> [ctest-junit.xml]
"""

import os
import re
import sys
import xml.etree.ElementTree as ET

# "#3 0x55d0 in func /abs/path/file.c:12:5 (bin+0x1)" or "#3 func /abs/path/file.c:12 (bin+0x1)".
# Demangled C++ names may contain spaces ("ns::f(int, char const*)"), so the
# function name runs up to the whitespace before the absolute source path.
SAN_FRAME = re.compile(r"^\s*#\d+\s+(?:0x[0-9a-f]+\s+in\s+)?(?P<func>.+?)\s+(?P<file>/\S+?):\d+")
SAN_HEADER = re.compile(
    r"(?:ERROR|WARNING): (?P<tool>AddressSanitizer|LeakSanitizer|ThreadSanitizer|UndefinedBehaviorSanitizer): "
    r"(?P<kind>[A-Za-z0-9_ -]+?)(?: \(|$| on | in | at )"
)
# LeakSanitizer lists one allocation stack per leak under a single report header.
LSAN_LEAK = re.compile(r"^(?P<kind>Direct|Indirect) leak of ")
UBSAN_RUNTIME = re.compile(r"(?P<file>/\S+?):\d+:\d+: runtime error: (?P<msg>.*)")


def first_party(path: str, root: str) -> str | None:
    path = os.path.normpath(path)
    rel = os.path.relpath(path, root) if os.path.isabs(path) else path
    if rel.startswith(("..", "build" + os.sep, "third_party" + os.sep)) or os.sep not in rel:
        return None
    return rel


def normalize_kind(kind: str) -> str:
    kind = re.sub(r"of size \d+", "", kind)
    kind = re.sub(r"0x[0-9a-f]+", "", kind)
    return " ".join(kind.split())


def sanitizer_keys(root: str, log: str, junit: str | None) -> set[str]:
    keys: set[str] = set()
    pending: str | None = None
    in_lsan = False
    with open(log, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = UBSAN_RUNTIME.search(line)
            if m:
                rel = first_party(m["file"], root)
                msg = re.sub(r"0x[0-9a-f]+|\b\d+\b", "N", m["msg"]).strip()
                keys.add(f"ubsan: {rel or os.path.basename(m['file'])}: {msg}")
                continue
            m = SAN_HEADER.search(line)
            if m:
                if pending:
                    keys.add(f"{pending}: <no first-party frame>")
                pending = f"{m['tool']}: {normalize_kind(m['kind'])}"
                in_lsan = m["tool"] == "LeakSanitizer"
                continue
            m = LSAN_LEAK.match(line)
            if in_lsan and m:
                # Each leak block gets its own key from its first first-party frame.
                if pending and not pending.startswith("LeakSanitizer: detected"):
                    keys.add(f"{pending}: <no first-party frame>")
                pending = f"LeakSanitizer: {m['kind'].lower()} leak"
                continue
            if pending:
                m = SAN_FRAME.match(line)
                if m:
                    rel = first_party(m["file"], root)
                    if rel:
                        keys.add(f"{pending}: {rel}:{m['func']}")
                        pending = None
                elif line.startswith("SUMMARY:"):
                    keys.add(f"{pending}: <no first-party frame>")
                    pending = None
            if line.startswith("SUMMARY:"):
                in_lsan = False
    if pending:
        keys.add(f"{pending}: <no first-party frame>")
    if junit and os.path.exists(junit):
        for case in ET.parse(junit).getroot().iter("testcase"):
            if case.find("failure") is not None or case.find("error") is not None:
                keys.add(f"test-failed: {case.get('name')}")
    return keys


def main() -> int:
    if len(sys.argv) not in (3, 4):
        print(__doc__, file=sys.stderr)
        return 2
    root = os.path.realpath(sys.argv[1])
    keys = sanitizer_keys(root, sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else None)
    for key in sorted(keys):
        print(key)
    return 0


if __name__ == "__main__":
    sys.exit(main())
