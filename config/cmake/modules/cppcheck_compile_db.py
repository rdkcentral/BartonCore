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

"""Write a copy of compile_commands.json without -D defines cppcheck cannot parse.

CMake list-valued config defines (e.g. BARTON_CONFIG_MATTER_*_HEADER_PATHS)
contain ';', which makes cppcheck bail out of every translation unit with
"bad macro syntax". Those defines are only consumed by CMake, so dropping them
does not change what cppcheck analyzes.

If the environment variable BCORE_CPPCHECK_ONLY names a file containing one
source path per line (absolute, or relative to the current directory), only
those translation units are kept.
"""

import json
import os
import shlex
import sys


def keep(arg: str) -> bool:
    return not (arg.startswith("-D") and ";" in arg)


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <compile_commands.json> <output.json>", file=sys.stderr)
        return 2
    with open(sys.argv[1], encoding="utf-8") as f:
        entries = json.load(f)
    only_list = os.environ.get("BCORE_CPPCHECK_ONLY")
    if only_list:
        with open(only_list, encoding="utf-8") as f:
            only = {os.path.realpath(line.strip()) for line in f if line.strip()}
        entries = [
            e for e in entries
            if os.path.realpath(os.path.join(e.get("directory", ""), e["file"])) in only
        ]
    for entry in entries:
        if "arguments" in entry:
            entry["arguments"] = [a for a in entry["arguments"] if keep(a)]
        elif "command" in entry:
            entry["command"] = shlex.join(a for a in shlex.split(entry["command"]) if keep(a))
    with open(sys.argv[2], "w", encoding="utf-8") as f:
        json.dump(entries, f, indent=2)
    return 0


if __name__ == "__main__":
    sys.exit(main())
