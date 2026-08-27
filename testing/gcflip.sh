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
#
# gcflip.sh — flip the mquickjs DEBUG_GC build flag without any manual rebuild.
#
# DEBUG_GC is a COMPILE-TIME flag baked into libmquickjs.a (statically linked
# into libBartonCore.so), so switching it means rebuilding mquickjs, swapping the
# archive, and relinking BartonCore. Devs should never have to do that dance by
# hand — this script automates it and caches both built archives so every flip
# after the first is a fast archive swap + relink.
#
#   DEBUG_GC on  = dev default: GC before every allocation + a ~128 KB relocating
#                  dummy block. A use-after-move CORRECTNESS stress harness. Heap
#                  peak / GC rate / handler latency / driver load time here are
#                  NOT production-representative.
#   DEBUG_GC off = production/bellard default: lazy compacting GC. This is the
#                  build to use for prod-representative MAGNITUDE numbers.
#
# Counting/correctness metrics and the post-GC live_bytes gauge are valid under
# either build; only magnitude numbers differ.
#
# Usage:
#   testing/gcflip.sh status      # print the current DEBUG_GC state
#   testing/gcflip.sh on          # ensure DEBUG_GC-on build is installed
#   testing/gcflip.sh off         # ensure DEBUG_GC-off build is installed
#
# Env:
#   GCFLIP_CACHE_DIR   where built archives are cached (default ~/.cache/bartoncore-gcflip)
#   GCFLIP_NO_RELINK=1 swap the archive but skip the BartonCore relink+install
#
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CACHE_DIR="${GCFLIP_CACHE_DIR:-$HOME/.cache/bartoncore-gcflip}"
INSTALLED_A="/usr/local/lib/libmquickjs.a"
PATCH_DIR="$REPO/docker/patches/mquickjs"
PIN="$(grep -oE 'MQUICKJS_GIT_TAG="[0-9a-f]+"' "$REPO/docker/Dockerfile" | head -1 | grep -oE '[0-9a-f]{40}')"

log() { printf '\033[1;36m[gcflip]\033[0m %s\n' "$*"; }
err() { printf '\033[1;31m[gcflip] ERROR:\033[0m %s\n' "$*" >&2; }

sha_of() { sha256sum "$1" 2>/dev/null | cut -d' ' -f1; }

# Build the mquickjs archive for a given variant (on|off) into the cache.
build_variant() {
    local variant="$1"
    local out="$CACHE_DIR/libmquickjs-$variant.a"
    local src="$CACHE_DIR/src-$variant"

    log "building DEBUG_GC-$variant mquickjs (pin ${PIN:0:12}) — one-time, then cached"
    rm -rf "$src"
    mkdir -p "$src"
    git clone --quiet --depth 1 https://github.com/bellard/mquickjs.git "$src"
    (
        cd "$src"
        git fetch --quiet --depth 1 origin "$PIN"
        git checkout --quiet "$PIN"
        cp "$PATCH_DIR/CMakeLists.txt" .
        if [[ "$variant" == "off" ]]; then
            # Strip the DEBUG_GC compile definition (its own line in the list).
            sed -i '/^[[:space:]]*DEBUG_GC[[:space:]]*$/d' CMakeLists.txt
        fi
        # Apply the repo patches in order (0001 memory API, 0002 exit status,
        # 0003 GC callback + root count). Verified to apply against the pin.
        for p in "$PATCH_DIR"/0001-*.patch "$PATCH_DIR"/0002-*.patch "$PATCH_DIR"/0003-*.patch; do
            git apply "$p"
        done
        mkdir -p build
        cd build
        cmake .. >/dev/null
        make >/dev/null 2>&1
        find . -name libmquickjs.a -exec cp {} "$out" \;
    )
    rm -rf "$src"
    [[ -f "$out" ]] || { err "build produced no libmquickjs.a for variant '$variant'"; exit 1; }
    local built_sha; built_sha="$(sha_of "$out")"
    echo "$built_sha" > "$out.sha"
    log "cached $out (sha ${built_sha:0:12})"
}

ensure_variant_cached() {
    local variant="$1"
    [[ -f "$CACHE_DIR/libmquickjs-$variant.a" ]] || build_variant "$variant"
}

current_state() {
    # Returns on|off|unknown by matching the installed archive against caches.
    local cur; cur="$(sha_of "$INSTALLED_A")"
    [[ -z "$cur" ]] && { echo "unknown"; return; }
    for v in on off; do
        local c="$CACHE_DIR/libmquickjs-$v.a"
        if [[ -f "$c" && "$(sha_of "$c")" == "$cur" ]]; then echo "$v"; return; fi
    done
    echo "unknown"
}

relink_bartoncore() {
    [[ "${GCFLIP_NO_RELINK:-0}" == "1" ]] && { log "GCFLIP_NO_RELINK=1 — skipping relink"; return; }
    log "relinking + installing BartonCore against the swapped archive"
    rm -f "$REPO"/build/core/libBartonCore.so*
    cmake --build "$REPO/build" --target BartonCore >/dev/null
    cmake --build "$REPO/build" --target install >/dev/null
    log "BartonCore relinked + installed"
}

flip_to() {
    local variant="$1"
    mkdir -p "$CACHE_DIR"
    ensure_variant_cached "$variant"

    local target="$CACHE_DIR/libmquickjs-$variant.a"
    if [[ "$(sha_of "$INSTALLED_A")" == "$(sha_of "$target")" ]]; then
        log "already DEBUG_GC-$variant (installed archive matches cache) — nothing to do"
        return
    fi

    log "swapping installed archive -> DEBUG_GC-$variant"
    cp "$target" "$INSTALLED_A"
    relink_bartoncore
    local now_sha; now_sha="$(sha_of "$INSTALLED_A")"
    log "now DEBUG_GC-$variant (sha ${now_sha:0:12})"
}

cmd="${1:-status}"
case "$cmd" in
    on|off) flip_to "$cmd" ;;
    status)
        st="$(current_state)"
        log "current DEBUG_GC state: $st"
        log "installed archive sha: $(sha_of "$INSTALLED_A")"
        [[ "$st" == "unknown" ]] && log "(no cached archive matches; run 'on' or 'off' to populate the cache)"
        ;;
    *) err "unknown command '$cmd' (use: on | off | status)"; exit 2 ;;
esac
