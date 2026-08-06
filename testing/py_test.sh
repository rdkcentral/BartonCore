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

# And point GObject-introspection at this tree's BCore typelib for the same
# reason: the container's GI_TYPELIB_PATH points at the provisioning clone's
# build/core, so without this a worktree would load that clone's typelib (or
# none at all, if it is unset in the current shell).
export GI_TYPELIB_PATH="$REPO_ROOT/build/core${GI_TYPELIB_PATH:+:$GI_TYPELIB_PATH}"

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
    echo "                           workers. With no value, defaults to min(CPUs/4, 32)"
    echo "                           -- about one worker per two physical cores, since each"
    echo "                           commissioning is a ~2-core crypto burst across Barton and"
    echo "                           a matter.js node process. Tests run"
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
        # Scale the default worker count with the machine, capped at 32.
        #
        # Matter commissioning discovers each virtual device over the shared
        # mDNS plane (port 5353). The CHIP commissioner keeps a fixed cache of
        # discovered commissionable nodes; because every commissioner sees every
        # device's advertisement, concurrent commissionings used to overflow
        # that cache ("Insufficient space") -- raised 10 -> 128 (barton patch
        # 0003), so no overflow is seen even at 32 workers.
        #
        # The binding limit is CPU: commissioning a device is a crypto-heavy
        # PASE/CASE burst that runs concurrently on BOTH the commissioner
        # (multi-threaded Barton) and the target device (a matter.js node
        # process), i.e. it needs ~2 physical cores while it runs. If the target
        # can't get the CPU it misses the PASE handshake and the commission
        # fails. nproc counts logical CPUs (hyperthreads), so use a quarter of
        # it -- about one worker per two physical cores -- which leaves room for
        # both halves of every concurrent commissioning plus the OS and per-test
        # subprocesses. Min 1, capped at 32 (only ~62 tests, and returns diminish
        # well before then).
        #
        # Empirically, on a 64-physical-core box: nproc/2 (one worker per
        # physical core) starves ~10% of runs; nproc/4 and nproc/3 are clean
        # over 20 runs each. nproc/4 is chosen for margin. Raising timeouts is
        # deliberately NOT used to paper over starvation -- the fix is fewer
        # workers. (A separate earlier ceiling of ~4 workers came from the 4-bit
        # short discriminator in the manual pairing code matching the wrong
        # device; that is fixed by commissioning with the full-discriminator QR
        # code, so the remaining limit is purely CPU headroom.)
        PARALLEL_CAP=32
        CPU_COUNT=$(nproc)
        PARALLEL_WORKERS=$(( CPU_COUNT / 4 ))
        (( PARALLEL_WORKERS < 1 )) && PARALLEL_WORKERS=1
        (( PARALLEL_WORKERS > PARALLEL_CAP )) && PARALLEL_WORKERS=$PARALLEL_CAP
    fi
    # Use xdist's loadgroup distribution so tests sharing an xdist_group (e.g. the
    # zhal mock tests that bind fixed IPC ports 18443/8711) stay on one worker and
    # never collide.
    #
    # Raise the commissioning wait timeouts modestly: under concurrent load the
    # crypto-heavy CASE/commissioning phase legitimately takes a little longer
    # than the (fast-failure) serial defaults, so give it some headroom. These
    # override testing/conftest.py's defaults and are forwarded into each
    # per-test subprocess. They are NOT a remedy for CPU starvation -- that is
    # bounded by keeping the worker count at/under the physical core count above.
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
