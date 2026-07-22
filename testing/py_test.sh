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
# operates on the working tree it lives in (correct for git worktrees) instead
# of paths baked into the container environment at launch time. Prepend this
# tree's Python packages and freshly-built libBartonCore so they take precedence
# over any stale paths inherited from the environment (e.g. a primary clone).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." &>/dev/null && pwd)"
export PYTHONPATH="$REPO_ROOT${PYTHONPATH:+:$PYTHONPATH}"
export LD_LIBRARY_PATH="$REPO_ROOT/build/core${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

function show_help {
    echo "This is a wrapper script around pytest to ensure the environment is setup correctly."
    echo "Usage: $0 [-t=<clang|gcc>|--toolchain=<clang|gcc>] [--parallel[=<workers>]] [pytest options]"
    echo ""
    echo "Options:"
    echo "  -t=<clang|gcc>, --toolchain=<clang|gcc>"
    echo "                           Specify the compiler toolchain used to build"
    echo "                           libBartonCore.so. Determines which ASAN runtime"
    echo "                           to preload. If not specified, auto-detects from"
    echo "                           the system default 'cc'."
    echo "  --parallel[=<workers>]   Run tests in parallel across <workers> pytest-xdist"
    echo "                           workers. With no value, defaults to min(CPUs/4, 64)"
    echo "                           -- roughly half the physical cores, since each worker"
    echo "                           drives Barton plus a matter.js node process. Tests run"
    echo "                           SERIALLY unless this flag is given, so interactive/"
    echo "                           individual runs keep readable, interleaved logs."
}

TOOLCHAIN=""
PARALLEL_WORKERS=""

# Parse our options, pass the rest through to pytest
PYTEST_ARGS=()
for arg in "$@"; do
    case "$arg" in
        -t=*|--toolchain=*)
            TOOLCHAIN="${arg#*=}"
            ;;
        --parallel)
            PARALLEL_WORKERS="default"
            ;;
        --parallel=*)
            PARALLEL_WORKERS="${arg#*=}"
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

PARALLEL_ARGS=()
if [[ -n "$PARALLEL_WORKERS" ]]; then
    if [[ "$PARALLEL_WORKERS" == "default" ]]; then
        # Scale the default worker count with the machine, capped at 64.
        #
        # History: Matter commissioning discovers each virtual device over the
        # shared mDNS plane (port 5353). The CHIP commissioner keeps a fixed
        # cache of discovered commissionable nodes; because every commissioner
        # sees every device's advertisement, concurrent commissionings used to
        # overflow that cache ("Insufficient space") and discovery would time
        # out -- which capped reliable parallelism at ~4. That cache was raised
        # 10 -> 128 (barton patch 0003), which removes the mDNS ceiling: no
        # overflow is seen even at 64 workers.
        #
        # The remaining limit at very high concurrency is soft and CPU-bound:
        # each worker drives a multi-threaded Barton plus a matter.js node
        # process (and ASAN), so it realistically needs ~2 physical cores.
        # Running one worker per logical CPU oversubscribes and makes crypto-heavy
        # CASE commissioning time out. nproc counts logical CPUs (hyperthreads),
        # so use a quarter of it -- roughly half the physical cores -- to leave
        # headroom on machines of any size. Min 1, capped at 64 (only ~58 tests,
        # so more never helps).
        PARALLEL_CAP=64
        CPU_COUNT=$(nproc)
        PARALLEL_WORKERS=$(( CPU_COUNT / 4 ))
        (( PARALLEL_WORKERS < 1 )) && PARALLEL_WORKERS=1
        (( PARALLEL_WORKERS > PARALLEL_CAP )) && PARALLEL_WORKERS=$PARALLEL_CAP
    fi
    # Use xdist's loadgroup distribution so tests sharing an xdist_group (e.g. the
    # zhal mock tests that bind fixed IPC ports 18443/8711) stay on one worker and
    # never collide.
    PARALLEL_ARGS=(-n "$PARALLEL_WORKERS" --dist loadgroup)
fi

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

LD_PRELOAD="$ASAN_LIB" pytest "${PARALLEL_ARGS[@]}" "${PYTEST_ARGS[@]}"
