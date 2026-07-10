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

// Created by Kevin Funderburg on 07/01/2026

/*
 * Small, stateless helpers for working with mquickjs JSValue objects.
 *
 * All functions require the caller to hold MQuickJsRuntime::GetMutex(), since
 * they operate on the shared context.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

extern "C" {
#include <mquickjs/mquickjs.h>
}

namespace barton
{
    namespace mquickjs
    {
        /**
         * Get a string property from a JS object, or empty string if missing.
         */
        std::string GetStringProp(JSContext *ctx, JSValue obj, const char *name);

        /**
         * Get a uint32 property from a JS object, or defaultValue if missing.
         */
        uint32_t GetUint32Prop(JSContext *ctx, JSValue obj, const char *name, uint32_t defaultValue);

        /**
         * Get an optional uint32 property from a JS object (nullopt if missing).
         */
        std::optional<uint32_t> GetOptUint32Prop(JSContext *ctx, JSValue obj, const char *name);

        /**
         * Get an optional uint16 property from a JS object (nullopt if missing).
         */
        std::optional<uint16_t> GetOptUint16Prop(JSContext *ctx, JSValue obj, const char *name);

        /**
         * Get the length of a JS array, or 0 if it has no length.
         */
        uint32_t GetArrayLength(JSContext *ctx, JSValue arr);

        /**
         * Return true if a JS object has a defined property with the given name.
         */
        bool HasProperty(JSContext *ctx, JSValue obj, const char *name);

        /**
         * Read a JS array of strings.
         */
        std::vector<std::string> GetStringArray(JSContext *ctx, JSValue arr);

        /**
         * Read a JS array of uint16_t values.
         */
        std::vector<uint16_t> GetUint16Array(JSContext *ctx, JSValue arr);

        /**
         * Read a JS array of uint32_t values.
         */
        std::vector<uint32_t> GetUint32Array(JSContext *ctx, JSValue arr);

        /**
         * Get the keys of a JS object (via Object.keys()), or empty on failure.
         */
        std::vector<std::string> GetObjectKeys(JSContext *ctx, JSValue obj);

        /**
         * Extract the current mquickjs exception as a string, or "unknown error".
         */
        std::string GetExceptionString(JSContext *ctx);
    } // namespace mquickjs
} // namespace barton
