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

/**
 * SBMD UTF-8 Utilities
 *
 * Provides Sbmd._internal.Utf8 for UTF-8 encoding/decoding.
 * Used internally by Base64 and TLV sub-parts.
 *
 * Requires: sbmd-namespace.js (Sbmd object must exist)
 */

(function (Sbmd) {
    'use strict';

    /**
     * UTF-8 encoding/decoding utilities
     * Needed because String.fromCharCode treats bytes as UCS-2 code units,
     * not UTF-8 bytes. These utilities properly handle multi-byte UTF-8 sequences.
     */
    var Utf8 = {
        /**
         * Decode UTF-8 bytes to a JavaScript string
         * @param {Uint8Array} bytes - UTF-8 encoded bytes
         * @returns {string} Decoded string
         */
        decode: function (bytes) {
            var result = '';
            var i = 0;

            while (i < bytes.length) {
                var b0 = bytes[i];

                if (b0 < 0x80) {
                    // 1-byte sequence (ASCII)
                    result += String.fromCharCode(b0);
                    i += 1;
                } else if ((b0 & 0xe0) === 0xc0) {
                    // 2-byte sequence
                    var b1 = bytes[i + 1];
                    result += String.fromCharCode(((b0 & 0x1f) << 6) | (b1 & 0x3f));
                    i += 2;
                } else if ((b0 & 0xf0) === 0xe0) {
                    // 3-byte sequence
                    var b1_3 = bytes[i + 1];
                    var b2_3 = bytes[i + 2];
                    result += String.fromCharCode(
                        ((b0 & 0x0f) << 12) | ((b1_3 & 0x3f) << 6) | (b2_3 & 0x3f)
                    );
                    i += 3;
                } else if ((b0 & 0xf8) === 0xf0) {
                    // 4-byte sequence (surrogate pair needed)
                    var b1_4 = bytes[i + 1];
                    var b2_4 = bytes[i + 2];
                    var b3_4 = bytes[i + 3];
                    var codePoint =
                        ((b0 & 0x07) << 18) |
                        ((b1_4 & 0x3f) << 12) |
                        ((b2_4 & 0x3f) << 6) |
                        (b3_4 & 0x3f);
                    // Convert to surrogate pair
                    var adjusted = codePoint - 0x10000;
                    result += String.fromCharCode(
                        0xd800 + (adjusted >> 10),
                        0xdc00 + (adjusted & 0x3ff)
                    );
                    i += 4;
                } else {
                    // Invalid UTF-8, skip byte
                    result += '\uFFFD';
                    i += 1;
                }
            }

            return result;
        },

        /**
         * Encode a JavaScript string to UTF-8 bytes
         * @param {string} str - String to encode
         * @returns {Uint8Array} UTF-8 encoded bytes
         */
        encode: function (str) {
            var bytes = [];

            for (var i = 0; i < str.length; i++) {
                var codePoint = str.charCodeAt(i);

                // Handle surrogate pairs
                if (codePoint >= 0xd800 && codePoint <= 0xdbff && i + 1 < str.length) {
                    var next = str.charCodeAt(i + 1);

                    if (next >= 0xdc00 && next <= 0xdfff) {
                        codePoint = 0x10000 + ((codePoint & 0x3ff) << 10) + (next & 0x3ff);
                        i++;
                    }
                }

                if (codePoint < 0x80) {
                    bytes.push(codePoint);
                } else if (codePoint < 0x800) {
                    bytes.push(0xc0 | (codePoint >> 6));
                    bytes.push(0x80 | (codePoint & 0x3f));
                } else if (codePoint < 0x10000) {
                    bytes.push(0xe0 | (codePoint >> 12));
                    bytes.push(0x80 | ((codePoint >> 6) & 0x3f));
                    bytes.push(0x80 | (codePoint & 0x3f));
                } else {
                    bytes.push(0xf0 | (codePoint >> 18));
                    bytes.push(0x80 | ((codePoint >> 12) & 0x3f));
                    bytes.push(0x80 | ((codePoint >> 6) & 0x3f));
                    bytes.push(0x80 | (codePoint & 0x3f));
                }
            }

            return new Uint8Array(bytes);
        }
    };

    // Share Utf8 with other sub-parts (used by sbmd-base64.js and sbmd-tlv.js)
    Sbmd._internal.Utf8 = Utf8;
})(globalThis.Sbmd);
