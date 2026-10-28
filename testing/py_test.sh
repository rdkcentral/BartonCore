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
# ./py_test.sh --sanitizer=tsan


# set -x
set -e

# Resolve the repository root from this script's own location so the wrapper
# always operates on the working tree it lives in. This is what makes the
# script correct for git worktrees: it does not depend on BARTON_TOP, which is
# baked into docker/.env at container-launch time and points at whichever
# checkout provisioned the container (typically the primary clone).
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" 2>/dev/null && pwd -P)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." 2>/dev/null && pwd -P)"

if [[ -z "$REPO_ROOT" || ! -d "$REPO_ROOT/testing" ]]; then
    echo "Error: failed to resolve repository root from script location." >&2
    exit 1
fi

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
    echo "  --sanitizer=<asan|ubsan|tsan|none>"
    echo "                           Sanitizer libBartonCore.so was built with."
    echo "                           Determines which runtime to preload (default: asan)."
    echo "                           Use 'none' for non-sanitized (e.g. coverage) builds."
    echo ""
    echo "JUnit XML is written to \${BCORE_REPORTS_DIR:-<repo>/build/reports}/pytest/junit.xml"
    echo "unless --junitxml is passed explicitly."
}

TOOLCHAIN=""
SANITIZER="asan"
HAS_JUNITXML=false

# Parse our options, pass the rest through to pytest
PYTEST_ARGS=()
for arg in "$@"; do
    case "$arg" in
        -t=*|--toolchain=*)
            TOOLCHAIN="${arg#*=}"
            ;;
        --sanitizer=*)
            SANITIZER="${arg#*=}"
            ;;
        --junitxml=*|--junit-xml=*)
            HAS_JUNITXML=true
            PYTEST_ARGS+=("$arg")
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

# Determine the sanitizer runtime to preload based on the sanitizer and the
# compiler that built libBartonCore.so. Python itself is not instrumented, so
# the runtime must be preloaded. Clang uses libclang_rt.<san>; GCC uses lib<san>.so.
if [[ -z "$TOOLCHAIN" ]]; then
    # Auto-detect from the system default compiler
    if cc --version 2>/dev/null | grep -qi clang; then
        TOOLCHAIN="clang"
    else
        TOOLCHAIN="gcc"
    fi
fi

case "$SANITIZER" in
    asan)
        CLANG_RT="asan"
        GCC_RT="libasan.so"
        ;;
    ubsan)
        CLANG_RT="ubsan_standalone"
        GCC_RT="libubsan.so"
        ;;
    tsan)
        CLANG_RT="tsan"
        GCC_RT="libtsan.so"
        ;;
    none)
        ;;
    *)
        echo "Error: unknown sanitizer '$SANITIZER'. Use 'asan', 'ubsan', 'tsan', or 'none'." >&2
        exit 1
        ;;
esac

SANITIZER_LIB=""
if [[ "$SANITIZER" != "none" ]]; then
    case "$TOOLCHAIN" in
        clang)
            ARCH="$(uname -m)"
            case "$ARCH" in
                x86_64) CLANG_ARCH="x86_64" ;;
                aarch64|arm64) CLANG_ARCH="aarch64" ;;
                *)
                    echo "Error: unsupported architecture '$ARCH' for clang sanitizer runtime." >&2
                    exit 1
                    ;;
            esac
            SANITIZER_LIB=$(clang -print-file-name="libclang_rt.${CLANG_RT}-${CLANG_ARCH}.so")
            ;;
        gcc)
            SANITIZER_LIB=$(gcc -print-file-name="$GCC_RT")
            ;;
        *)
            echo "Error: unknown toolchain '$TOOLCHAIN'. Use 'clang' or 'gcc'." >&2
            exit 1
            ;;
    esac
fi

if [[ "$HAS_JUNITXML" == false ]]; then
    REPORTS_DIR="${BCORE_REPORTS_DIR:-$REPO_ROOT/build/reports}"
    mkdir -p "$REPORTS_DIR/pytest"
    PYTEST_ARGS+=("--junitxml=$REPORTS_DIR/pytest/junit.xml")
fi

if [[ -n "$SANITIZER_LIB" ]]; then
    LD_PRELOAD="$SANITIZER_LIB" pytest "${PYTEST_ARGS[@]}"
else
    pytest "${PYTEST_ARGS[@]}"
fi
