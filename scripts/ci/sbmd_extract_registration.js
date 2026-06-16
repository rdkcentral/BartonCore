#!/usr/bin/env node
// ------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2026 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
// ------------------------------ tabstop = 4 ----------------------------------

//
// SBMD Registration Extractor
//
// Evaluates an .sbmd.js file and extracts the SbmdDriver() registration
// object as JSON.  Functions are serialised as `true`.
//
// Mirrors the runtime's two-pass constant injection:
//   1. Parse constants from the source text.
//   2. Inject them as read-only globals on a VM context.
//   3. Evaluate the file, capturing the SbmdDriver() argument.
//   4. Print the captured registration as JSON to stdout.
//
// Usage:
//   node sbmd_extract_registration.js <spec.sbmd.js>
//

'use strict';

const fs = require('fs');
const vm = require('vm');
const path = require('path');

const specPath = process.argv[2];

if (!specPath) {
    process.stderr.write('Usage: node sbmd_extract_registration.js <spec.sbmd.js>\n');
    process.exit(1);
}

const source = fs.readFileSync(specPath, 'utf8');

// ---------------------------------------------------------------------------
// Extract the constants block from the source text
// ---------------------------------------------------------------------------

function extractConstants(src) {
    // Find "constants" followed by optional whitespace, colon, optional
    // whitespace, then an opening brace.  We match braces to find the
    // full block and evaluate it as a JS object literal.
    const re = /\bconstants\s*:\s*\{/g;
    const match = re.exec(src);

    if (!match) {
        return {};
    }

    const braceStart = match.index + match[0].length - 1; // index of '{'
    let depth = 1;
    let i = braceStart + 1;

    while (i < src.length && depth > 0) {
        const ch = src[i];

        if (ch === '{') {
            depth++;
        } else if (ch === '}') {
            depth--;
        } else if (ch === '/' && src[i + 1] === '/') {
            while (i < src.length && src[i] !== '\n') i++;
        } else if (ch === '/' && src[i + 1] === '*') {
            i += 2;
            while (i < src.length - 1 && !(src[i] === '*' && src[i + 1] === '/')) i++;
            i++;
        } else if (ch === '\'' || ch === '"' || ch === '`') {
            const quote = ch;
            i++;
            while (i < src.length && src[i] !== quote) {
                if (src[i] === '\\') i++;
                i++;
            }
        }

        i++;
    }

    if (depth !== 0) {
        return {};
    }

    const block = src.substring(braceStart, i);

    try {
        // Use indirect eval so it runs in global scope
        return (0, eval)('(' + block + ')');
    } catch {
        return {};
    }
}

const constants = extractConstants(source);

// ---------------------------------------------------------------------------
// Build sandbox and evaluate
// ---------------------------------------------------------------------------

let captured = null;

// Sbmd stub — all methods return the stub for chaining
const sbmdStub = {};

function returnStub() { return sbmdStub; }

sbmdStub.result = returnStub;
sbmdStub.log = returnStub;
sbmdStub.success = returnStub;
sbmdStub.error = returnStub;

sbmdStub.Tlv = {
    encode: () => '',
    decode: () => null,
    encodeStruct: () => '',
    emptyStruct: () => '',
};

sbmdStub.Base64 = {
    encode: () => '',
    decode: () => [],
};

sbmdStub.dataModel = {
    updateResource: returnStub,
    setMetadata: returnStub,
};

sbmdStub.device = {
    sendCommand: returnStub,
    requestCommand: returnStub,
    writeAttribute: returnStub,
    readAttribute: returnStub,
};

sbmdStub.storage = {
    setPersistentData: returnStub,
    setTransientData: returnStub,
};

// Build the sandbox context
const sandbox = {
    SbmdDriver: function(reg) { captured = reg; },
    Sbmd: sbmdStub,
    Uint8Array: Uint8Array,
    parseInt: parseInt,
    parseFloat: parseFloat,
    isNaN: isNaN,
    isFinite: isFinite,
    Math: Math,
    JSON: JSON,
    String: String,
    Number: Number,
    Array: Array,
    Object: Object,
    console: console,
};

// Inject constants
for (const [name, value] of Object.entries(constants)) {
    sandbox[name] = value;
}

const context = vm.createContext(sandbox);

try {
    vm.runInContext(source, context, { filename: path.basename(specPath) });
} catch (e) {
    process.stderr.write('ERROR: Failed to evaluate ' + specPath + ': ' + e.message + '\n');
    process.exit(1);
}

if (captured === null) {
    process.stderr.write('ERROR: No SbmdDriver() call found in ' + specPath + '\n');
    process.exit(1);
}

// ---------------------------------------------------------------------------
// Serialise to JSON (functions → true)
// ---------------------------------------------------------------------------

const json = JSON.stringify(captured, function(key, value) {
    if (typeof value === 'function') {
        return true;
    }
    return value;
}, 2);

process.stdout.write(json + '\n');
