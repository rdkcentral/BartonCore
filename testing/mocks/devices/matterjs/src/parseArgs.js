//------------------------------ tabstop = 4 ----------------------------------
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
//------------------------------ tabstop = 4 ----------------------------------

/**
 * Parse command-line arguments common to all virtual device entry points.
 *
 * Supported arguments:
 *   --passcode <int>       Matter passcode (default handled by device)
 *   --discriminator <int>  Matter discriminator (default handled by device)
 *   --port <int>           Matter port (default handled by device)
 *   --vendor-id <int>      Vendor ID (default handled by device)
 *   --product-id <int>     Product ID (default handled by device)
 */
export function parseArgs(argv) {
    const args = {};
    const validFlags = new Set([
        "--passcode",
        "--discriminator",
        "--port",
        "--vendor-id",
        "--product-id"
    ]);

    for (let i = 2; i < argv.length; i += 2) {
        const key = argv[i];

        if (!validFlags.has(key)) {
            throw new Error(`Unknown flag: ${key}`);
        }

        const value = argv[i + 1];

        if (value === undefined || value.startsWith("--")) {
            throw new Error(`Flag ${key} requires a value`);
        }

        const parsedValue = parseInt(value, 10);

        if (isNaN(parsedValue)) {
            throw new Error(`Flag ${key} requires an integer value, got: ${value}`);
        }

        switch (key) {
            case "--passcode":
                args.passcode = parsedValue;
                break;
            case "--discriminator":
                args.discriminator = parsedValue;
                break;
            case "--port":
                args.port = parsedValue;
                break;
            case "--vendor-id":
                args.vendorId = parsedValue;
                break;
            case "--product-id":
                args.productId = parsedValue;
                break;
        }
    }

    return args;
}
