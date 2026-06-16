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
 * SBMD Base64 Utilities
 *
 * Provides Sbmd.Base64 for base64 encoding/decoding.
 *
 * Requires: sbmd-namespace.js (Sbmd object must exist)
 */

(function(Sbmd)
{
    'use strict';

    /**
     * Base64 encoding/decoding utilities
     */
    var Base64 =
    {
        chars: 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/',

        /**
         * Decode a base64 string to a Uint8Array
         * @param {string} base64 - Base64 encoded string
         * @returns {Uint8Array} Decoded bytes
         */
        decode: function(base64)
        {
            var bytes = [];

            for (var i = 0; i < base64.length; i += 4)
            {
                var c0 = this.chars.indexOf(base64[i]);
                var c1 = this.chars.indexOf(base64[i + 1]);
                var c2 = base64[i + 2] === '=' ? 0 : this.chars.indexOf(base64[i + 2]);
                var c3 = base64[i + 3] === '=' ? 0 : this.chars.indexOf(base64[i + 3]);

                if (c0 === -1 || c1 === -1 || c2 === -1 || c3 === -1)
                {
                    var badIndex = c0 === -1 ? i : c1 === -1 ? i + 1 : c2 === -1 ? i + 2 : i + 3;
                    throw new Error('Invalid Base64 character at index ' + badIndex + ': \'' + base64[badIndex] + '\'');
                }

                bytes.push((c0 << 2) | (c1 >> 4));

                if (base64[i + 2] !== '=')
                {
                    bytes.push(((c1 & 0x0F) << 4) | (c2 >> 2));
                }

                if (base64[i + 3] !== '=')
                {
                    bytes.push(((c2 & 0x03) << 6) | c3);
                }
            }

            return new Uint8Array(bytes);
        },

        /**
         * Encode a Uint8Array to a base64 string
         * @param {Uint8Array} bytes - Bytes to encode
         * @returns {string} Base64 encoded string
         */
        encode: function(bytes)
        {
            var result = '';

            for (var i = 0; i < bytes.length; i += 3)
            {
                var b0 = bytes[i];
                var b1 = i + 1 < bytes.length ? bytes[i + 1] : 0;
                var b2 = i + 2 < bytes.length ? bytes[i + 2] : 0;

                result += this.chars[b0 >> 2];
                result += this.chars[((b0 & 0x03) << 4) | (b1 >> 4)];
                result += i + 1 < bytes.length ? this.chars[((b1 & 0x0F) << 2) | (b2 >> 6)] : '=';
                result += i + 2 < bytes.length ? this.chars[b2 & 0x3F] : '=';
            }

            return result;
        }
    };

    // Public API
    Sbmd.Base64 = Base64;

})(globalThis.Sbmd);
