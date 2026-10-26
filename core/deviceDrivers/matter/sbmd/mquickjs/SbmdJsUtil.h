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
 * All functions require the caller to hold MQuickJsRuntime::Instance().GetMutex(), since
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
         * Validate a JS value as an integer in [0, maxValue] without any coercion, so a value that
         * would wrap, narrow, truncate, or be coerced into a different in-range value is rejected
         * instead of silently aliasing it. Returns true and sets `out` only when the value is a JS
         * number that is finite, has no fractional part, and lies in [0, maxValue] (negative zero is
         * the integer 0 and is accepted). Returns false for anything else, including NaN, ±Infinity,
         * fractional values, and non-numbers such as strings, booleans, and null.
         */
        bool ToBoundedUint(JSContext *ctx, JSValue val, uint32_t maxValue, uint32_t &out);

        /**
         * Read an optional integer property with ToBoundedUint() semantics. A property that is absent
         * (undefined) leaves `out` as nullopt and returns true. Returns false if the property is
         * present but is not an integer in [0, maxValue], including an explicit null.
         */
        bool ReadOptBoundedUintProp(JSContext *ctx,
                                    JSValue obj,
                                    const char *name,
                                    uint32_t maxValue,
                                    std::optional<uint32_t> &out);

        /**
         * ReadOptBoundedUintProp() with a maxValue of UINT16_MAX, narrowed to uint16.
         */
        bool ReadOptBoundedUint16Prop(JSContext *ctx, JSValue obj, const char *name, std::optional<uint16_t> &out);

        /**
         * Get the length of a JS array, or 0 if it has no length.
         */
        uint32_t GetArrayLength(JSContext *ctx, JSValue arr);

        /**
         * Whether an active script deadline (MQuickJsRuntime::SetDeadline) has fired; false when no
         * deadline is active. The engine checks the deadline only periodically, so every further
         * call into script code after it fires still runs for a while before being interrupted
         * again; loops that read script values (getters, valueOf/toString) must stop as soon as
         * this returns true.
         */
        bool ScriptTimedOut();

        /**
         * Return true if a JS object has a defined property with the given name.
         */
        bool HasProperty(JSContext *ctx, JSValue obj, const char *name);

        /**
         * Read a JS array of strings.
         */
        std::vector<std::string> GetStringArray(JSContext *ctx, JSValue arr);

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
