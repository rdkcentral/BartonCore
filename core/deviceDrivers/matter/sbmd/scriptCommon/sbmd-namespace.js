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
 * SBMD Namespace
 *
 * Creates the top-level Sbmd object. Sub-parts (Base64, Tlv, result)
 * are attached by their own source files, loaded sequentially after
 * this file.
 *
 * _internal is a namespace for shared implementation details used
 * across sub-parts (e.g. Utf8 encoding used by both Base64 and Tlv).
 * It is not part of the public API and is deleted after assembly.
 */

(function (globalThis) {
    'use strict';

    globalThis.Sbmd = {
        _internal: {}
    };
})(globalThis);

// Export as a top-level var so mquickjs makes it visible as a global variable.
// (mquickjs: properties set directly on globalThis are NOT visible as global vars)
var Sbmd = globalThis.Sbmd;
