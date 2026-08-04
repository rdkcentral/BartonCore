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

# Likewise, prepend this tree's freshly-built libBartonCore so it takes
# precedence over a stale library path inherited from the environment (e.g. a
# primary clone's build/core), so parallel/worktree runs load the right library.
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
    echo "                           workers. With no value, defaults to min(CPUs/2, 64)"
    echo "                           -- about one worker per physical core, since each worker"
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
        # Matter commissioning discovers each virtual device over the shared
        # mDNS plane (port 5353). The CHIP commissioner keeps a fixed cache of
        # discovered commissionable nodes; because every commissioner sees every
        # device's advertisement, concurrent commissionings used to overflow
        # that cache ("Insufficient space") -- raised 10 -> 128 (barton patch
        # 0003), so no overflow is seen even at 64 workers.
        #
        # Each worker drives a multi-threaded Barton plus a matter.js node
        # process (and ASAN), so it needs roughly a full physical core. nproc
        # counts logical CPUs (hyperthreads), so use half of it -- about one
        # worker per physical core -- leaving headroom for the OS and the
        # per-test subprocesses. Oversubscribing past the physical core count
        # starves the crypto-heavy CASE/PASE commissioning phase and makes it
        # time out. Min 1, capped at 64 (only ~62 tests, so more never helps).
        #
        # (An earlier ceiling of ~4 workers came from commissioners matching the
        # wrong device via the 4-bit short discriminator in the manual pairing
        # code; that is fixed by commissioning with the full-discriminator QR
        # code, so the limit is now purely CPU headroom.)
        PARALLEL_CAP=64
        CPU_COUNT=$(nproc)
        PARALLEL_WORKERS=$(( CPU_COUNT / 2 ))
        (( PARALLEL_WORKERS < 1 )) && PARALLEL_WORKERS=1
        (( PARALLEL_WORKERS > PARALLEL_CAP )) && PARALLEL_WORKERS=$PARALLEL_CAP
    fi
    # Use xdist's loadgroup distribution so tests sharing an xdist_group (e.g. the
    # zhal mock tests that bind fixed IPC ports 18443/8711) stay on one worker and
    # never collide.
    #
    # Raise the commissioning wait timeouts: under concurrent load the crypto-heavy
    # CASE/commissioning phase legitimately takes longer than the (fast-failure)
    # serial defaults, so give it headroom. These override testing/conftest.py's
    # defaults and are forwarded into each per-test subprocess.
    PARALLEL_ARGS=(
        -n "$PARALLEL_WORKERS" --dist loadgroup
        --client-ready-timeout=30 --device-added-timeout=30 --resource-value-timeout=30
    )
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
