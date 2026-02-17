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
# Build script to create a QuickJS-compatible bundle of the matter.js cluster
# library for TLV encoding/decoding in SBMD drivers.
#

set -e

help() {
    echo "Usage: $0 [-o <output_dir>] [-v <version>] [-h]"
    echo "  -o  Output directory for the bundled JavaScript (default: build/matterjs-clusters)"
    echo "  -v  matter.js version/tag to use (default: main)"
    echo "  -h  Display help"
}

MY_DIR=$(realpath "$(dirname "$0")")
PROJECT_ROOT=$(realpath "${MY_DIR}/..")
BUILD_DIR=${PROJECT_ROOT}/build
OUTPUT_DIR=${BUILD_DIR}/matterjs-clusters
MATTERJS_BUILD_DIR=${BUILD_DIR}/matterjs-src
MATTERJS_VERSION="main"

while getopts ":ho:v:" option; do
    case $option in
    h)
        help
        exit
        ;;
    o)
        OUTPUT_DIR="$OPTARG"
        ;;
    v)
        MATTERJS_VERSION="$OPTARG"
        ;;
    *)
        echo "Unknown option: -$OPTARG"
        help
        exit 1
        ;;
    esac
done

BUNDLE_OUTPUT="${OUTPUT_DIR}/matter-clusters.js"

# Check if bundle already exists
if [ -f "${BUNDLE_OUTPUT}" ]; then
    echo "matter.js cluster bundle already exists at: ${BUNDLE_OUTPUT}"
    echo "Delete it to rebuild."
    exit 0
fi

echo "Building matter.js cluster bundle..."
echo "  Version: ${MATTERJS_VERSION}"
echo "  Output: ${BUNDLE_OUTPUT}"

# Create output directory
mkdir -p "${OUTPUT_DIR}"
mkdir -p "${MATTERJS_BUILD_DIR}"

# Check for required tools
if ! command -v npm &> /dev/null; then
    echo "Error: npm is required but not installed."
    exit 1
fi

# Clone matter.js if not already present
if [ ! -d "${MATTERJS_BUILD_DIR}/matter.js" ]; then
    echo "Cloning matter.js repository..."
    cd "${MATTERJS_BUILD_DIR}"
    git clone \
        --branch "${MATTERJS_VERSION}" \
        --depth 1 \
        https://github.com/matter-js/matter.js.git
else
    echo "matter.js source already present at ${MATTERJS_BUILD_DIR}/matter.js"
fi

cd "${MATTERJS_BUILD_DIR}/matter.js"

# Install dependencies with relaxed engine checks
echo "Installing matter.js dependencies..."
npm install --ignore-scripts --legacy-peer-deps 2>&1 || {
    echo "npm install failed, trying with --force..."
    npm install --ignore-scripts --force 2>&1
}

# Install esbuild locally
echo "Installing esbuild..."
npm install --no-save esbuild typescript 2>&1

# Build the matter.js packages we need
echo "Building matter.js packages..."

# Use tsc directly to compile the packages we need
TSC="./node_modules/.bin/tsc"

# Build general package (dependency)
if [ -d "packages/general" ] && [ ! -d "packages/general/dist" ]; then
    echo "Building @matter/general..."
    cd packages/general
    ${TSC} --project tsconfig.build.json 2>&1 || ${TSC} 2>&1 || echo "general build warning - continuing"
    cd ../..
fi

# Build model package (dependency)
if [ -d "packages/model" ] && [ ! -d "packages/model/dist" ]; then
    echo "Building @matter/model..."
    cd packages/model
    ${TSC} --project tsconfig.build.json 2>&1 || ${TSC} 2>&1 || echo "model build warning - continuing"
    cd ../..
fi

# Build types package (the main one we need)
if [ -d "packages/types" ] && [ ! -d "packages/types/dist" ]; then
    echo "Building @matter/types..."
    cd packages/types
    ${TSC} --project tsconfig.build.json 2>&1 || ${TSC} 2>&1 || echo "types build warning - continuing"
    cd ../..
fi

# Verify types dist exists
if [ ! -d "packages/types/dist" ]; then
    echo "Error: Failed to build @matter/types package"
    echo "Attempting alternative: using source TypeScript directly with esbuild..."
fi

# Create entry point for bundling
echo "Creating bundle entry point..."
ENTRY_POINT="${MATTERJS_BUILD_DIR}/matter.js/bundle-entry.ts"

cat > "${ENTRY_POINT}" << 'ENTRY_EOF'
/**
 * matter.js cluster bundle entry point for QuickJS SBMD runtime.
 *
 * This module exports cluster TLV types for encoding/decoding Matter
 * protocol data in SBMD JavaScript mappers.
 */

// Core TLV types - use .ts extension since we're using esbuild's ts loader
export * from "./packages/types/src/tlv/TlvSchema";
export * from "./packages/types/src/tlv/TlvObject";
export * from "./packages/types/src/tlv/TlvNumber";
export * from "./packages/types/src/tlv/TlvString";
export * from "./packages/types/src/tlv/TlvBoolean";
export * from "./packages/types/src/tlv/TlvArray";
export * from "./packages/types/src/tlv/TlvNullable";
export * from "./packages/types/src/tlv/TlvCodec";

// Import cluster modules - the most commonly used ones
export * as LevelControl from "./packages/types/src/clusters/level-control";
export * as OnOff from "./packages/types/src/clusters/on-off";
export * as DoorLock from "./packages/types/src/clusters/door-lock";
export * as ColorControl from "./packages/types/src/clusters/color-control";
export * as Thermostat from "./packages/types/src/clusters/thermostat";
export * as WindowCovering from "./packages/types/src/clusters/window-covering";
export * as FanControl from "./packages/types/src/clusters/fan-control";
export * as Identify from "./packages/types/src/clusters/identify";
export * as Groups from "./packages/types/src/clusters/groups";
export * as Binding from "./packages/types/src/clusters/binding";
export * as BooleanState from "./packages/types/src/clusters/boolean-state";
export * as OccupancySensing from "./packages/types/src/clusters/occupancy-sensing";
export * as TemperatureMeasurement from "./packages/types/src/clusters/temperature-measurement";
export * as RelativeHumidityMeasurement from "./packages/types/src/clusters/relative-humidity-measurement";
export * as IlluminanceMeasurement from "./packages/types/src/clusters/illuminance-measurement";
export * as PressureMeasurement from "./packages/types/src/clusters/pressure-measurement";
export * as FlowMeasurement from "./packages/types/src/clusters/flow-measurement";
export * as PowerSource from "./packages/types/src/clusters/power-source";
export * as BasicInformation from "./packages/types/src/clusters/basic-information";
export * as Descriptor from "./packages/types/src/clusters/descriptor";
export * as SmokeCoAlarm from "./packages/types/src/clusters/smoke-co-alarm";
export * as Switch from "./packages/types/src/clusters/switch";
export * as ModeSelect from "./packages/types/src/clusters/mode-select";
ENTRY_EOF

# Create QuickJS polyfills
POLYFILL_FILE="${MATTERJS_BUILD_DIR}/matter.js/quickjs-polyfills.js"
cat > "${POLYFILL_FILE}" << 'POLYFILL_EOF'
/**
 * Polyfills for QuickJS compatibility.
 */

// TextEncoder polyfill
if (typeof globalThis.TextEncoder === 'undefined') {
    globalThis.TextEncoder = class TextEncoder {
        encode(str) {
            const utf8 = [];
            for (let i = 0; i < str.length; i++) {
                let charcode = str.charCodeAt(i);
                if (charcode < 0x80) {
                    utf8.push(charcode);
                } else if (charcode < 0x800) {
                    utf8.push(0xc0 | (charcode >> 6), 0x80 | (charcode & 0x3f));
                } else if (charcode < 0xd800 || charcode >= 0xe000) {
                    utf8.push(0xe0 | (charcode >> 12), 0x80 | ((charcode >> 6) & 0x3f), 0x80 | (charcode & 0x3f));
                } else {
                    i++;
                    charcode = 0x10000 + (((charcode & 0x3ff) << 10) | (str.charCodeAt(i) & 0x3ff));
                    utf8.push(0xf0 | (charcode >> 18), 0x80 | ((charcode >> 12) & 0x3f), 0x80 | ((charcode >> 6) & 0x3f), 0x80 | (charcode & 0x3f));
                }
            }
            return new Uint8Array(utf8);
        }
    };
}

// TextDecoder polyfill
if (typeof globalThis.TextDecoder === 'undefined') {
    globalThis.TextDecoder = class TextDecoder {
        decode(bytes) {
            if (!bytes || bytes.length === 0) return '';
            const arr = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
            let str = '', i = 0;
            while (i < arr.length) {
                let c = arr[i++];
                if (c < 0x80) str += String.fromCharCode(c);
                else if (c < 0xe0) str += String.fromCharCode(((c & 0x1f) << 6) | (arr[i++] & 0x3f));
                else if (c < 0xf0) str += String.fromCharCode(((c & 0x0f) << 12) | ((arr[i++] & 0x3f) << 6) | (arr[i++] & 0x3f));
                else {
                    const cp = ((c & 0x07) << 18) | ((arr[i++] & 0x3f) << 12) | ((arr[i++] & 0x3f) << 6) | (arr[i++] & 0x3f);
                    const off = cp - 0x10000;
                    str += String.fromCharCode(0xd800 + (off >> 10), 0xdc00 + (off & 0x3ff));
                }
            }
            return str;
        }
    };
}

// btoa polyfill
if (typeof globalThis.btoa === 'undefined') {
    const b64 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
    globalThis.btoa = function(s) {
        let r = '', i = 0;
        while (i < s.length) {
            const a = s.charCodeAt(i++), b = i < s.length ? s.charCodeAt(i++) : 0, c = i < s.length ? s.charCodeAt(i++) : 0;
            const t = (a << 16) | (b << 8) | c;
            r += b64[(t >> 18) & 0x3f] + b64[(t >> 12) & 0x3f];
            r += i > s.length + 1 ? '=' : b64[(t >> 6) & 0x3f];
            r += i > s.length ? '=' : b64[t & 0x3f];
        }
        return r;
    };
}

// atob polyfill
if (typeof globalThis.atob === 'undefined') {
    const b64 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
    globalThis.atob = function(s) {
        s = s.replace(/=+$/, '');
        let r = '', i = 0;
        while (i < s.length) {
            const a = b64.indexOf(s[i++]), b = b64.indexOf(s[i++]), c = b64.indexOf(s[i++]), d = b64.indexOf(s[i++]);
            const t = (a << 18) | (b << 12) | (c << 6) | d;
            r += String.fromCharCode((t >> 16) & 0xff);
            if (c !== -1) r += String.fromCharCode((t >> 8) & 0xff);
            if (d !== -1) r += String.fromCharCode(t & 0xff);
        }
        return r;
    };
}
POLYFILL_EOF

echo "Bundling with esbuild..."

# Bundle using esbuild with proper TypeScript resolution
./node_modules/.bin/esbuild "${ENTRY_POINT}" \
    --bundle \
    --format=iife \
    --global-name=MatterClusters \
    --target=es2020 \
    --platform=neutral \
    --inject:"${POLYFILL_FILE}" \
    --define:process.env.NODE_ENV='"production"' \
    --outfile="${BUNDLE_OUTPUT}" \
    --alias:@matter/general=./packages/general/src/index.ts \
    --alias:@matter/model=./packages/model/src/index.ts \
    --alias:#general=./packages/general/src/index.ts \
    --alias:#model=./packages/model/src/index.ts \
    --alias:#types=./packages/types/src/index.ts \
    --resolve-extensions=.ts,.js \
    --minify \
    2>&1

# Check if bundle was created
if [ ! -f "${BUNDLE_OUTPUT}" ]; then
    echo "Error: Bundle was not created"
    exit 1
fi

echo ""
echo "==============================================="
echo "matter.js cluster bundle created successfully!"
echo "==============================================="
echo ""
echo "Output: ${BUNDLE_OUTPUT}"
echo "Size: $(du -h "${BUNDLE_OUTPUT}" | cut -f1)"
echo ""
echo "The bundle exposes a global 'MatterClusters' object with cluster TLV types."
echo ""
echo "Example usage in SBMD scripts:"
echo "  const { LevelControl } = MatterClusters;"
echo "  const tlv = LevelControl.TlvMoveToLevelRequest.encode({level: 50, ...});"
echo ""

# Cleanup
rm -f "${ENTRY_POINT}" "${POLYFILL_FILE}"

echo "Done!"
