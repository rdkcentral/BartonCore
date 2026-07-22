#!/bin/bash
# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 Comcast Cable Communications Management, LLC
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

#
# Created by Kevin Funderburg on 12/17/2024
#

# In order for pytests to run successfully with libBartonCore.so compiled
# with AddressSanitizer, libasan.so must be preloaded. This script is a wrapper around
# pytest to handle setting the LD_PRELOAD environment variable and passing any extra args
# to pytest. Use this script if you want to execute any pytests on the command line to ensure
# your environment is set up correctly.

# examples:
# ./py_test.sh
# ./py_test.sh -s --log-cli-level=DEBUG
# ./py_test.sh --toolchain=gcc -s --log-cli-level=DEBUG


# set -x
set -e

# Resolve the repository root from this script's own location so the wrapper
# always operates on the working tree it lives in. This is what makes the
# script correct for git worktrees: it does not depend on BARTON_TOP, which is
# baked into docker/.env at container-launch time and points at whichever
# checkout provisioned the container (typically the primary clone).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." &>/dev/null && pwd)"

# Ensure this tree's Python packages (testing.*) take precedence over any stale
# path inherited from the environment (e.g. BARTON_PYTHONPATH from docker/.env).
# The subprocess-per-test runner in testing/conftest.py copies os.environ into
# each child pytest, so this propagates to the isolated test processes too.
export PYTHONPATH="$REPO_ROOT${PYTHONPATH:+:$PYTHONPATH}"

function show_help {
    echo "This is a wrapper script around pytest to ensure the environment is setup correctly."
    echo "Usage: $0 [-t=<clang|gcc>|--toolchain=<clang|gcc>] [pytest options]"
    echo ""
    echo "Options:"
    echo "  -t=<clang|gcc>, --toolchain=<clang|gcc>"
    echo "                           Specify the compiler toolchain used to build"
    echo "                           libBartonCore.so. Determines which ASAN runtime"
    echo "                           to preload. If not specified, auto-detects from"
    echo "                           the system default 'cc'."
}

TOOLCHAIN=""

# Parse our options, pass the rest through to pytest
PYTEST_ARGS=()
for arg in "$@"; do
    case "$arg" in
        -t=*|--toolchain=*)
            TOOLCHAIN="${arg#*=}"
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            PYTEST_ARGS+=("$arg")
            ;;
    esac
done

# Determine the correct ASAN runtime to preload based on the compiler that
# built libBartonCore.so. Clang uses libclang_rt.asan; GCC uses libasan.so.
if [[ -z "$TOOLCHAIN" ]]; then
    # Auto-detect from the system default compiler
    if cc --version 2>/dev/null | grep -qi clang; then
        TOOLCHAIN="clang"
    else
        TOOLCHAIN="gcc"
    fi
fi

case "$TOOLCHAIN" in
    clang)
        ARCH="$(uname -m)"
        case "$ARCH" in
            x86_64) CLANG_ASAN_BASENAME="libclang_rt.asan-x86_64.so" ;;
            aarch64|arm64) CLANG_ASAN_BASENAME="libclang_rt.asan-aarch64.so" ;;
            *)
                echo "Error: unsupported architecture '$ARCH' for clang ASAN runtime." >&2
                exit 1
                ;;
        esac
        ASAN_LIB=$(clang -print-file-name="$CLANG_ASAN_BASENAME")
        ;;
    gcc)
        ASAN_LIB=$(gcc -print-file-name=libasan.so)
        ;;
    *)
        echo "Error: unknown toolchain '$TOOLCHAIN'. Use 'clang' or 'gcc'." >&2
        exit 1
        ;;
esac

LD_PRELOAD="$ASAN_LIB" pytest "${PYTEST_ARGS[@]}"
