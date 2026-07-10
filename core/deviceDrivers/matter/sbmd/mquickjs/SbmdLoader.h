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

/*
 * Created by tlea on 6/12/2026
 *
 * Loader for SBMD driver files (.sbmd.js).
 *
 * Handles the two-pass evaluation process:
 *   Pass 1: Extract constants block, evaluate as object literal, produce var declarations.
 *   Pass 2: Prepend constants, IIFE-wrap, evaluate, extract SbmdDriver registration.
 *
 * All operations require the caller to hold MQuickJsRuntime::GetMutex().
 */

#pragma once

#include "../SbmdRegistration.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <mquickjs/mquickjs.h>
}

namespace barton
{
    class SbmdLoader
    {
    public:
        /**
         * Inject the SbmdDriver capture function and __sbmd_registration global
         * into the shared mquickjs context. Must be called once during initialization,
         * after MQuickJsRuntime::Initialize() and SbmdBundleLoader::LoadBundle().
         *
         * @param ctx The mquickjs context
         * @return true if injection succeeded
         */
        static bool InjectCaptureFunction(JSContext *ctx);

        /**
         * Load and evaluate a .sbmd.js file, extracting its registration.
         *
         * This performs the full two-pass evaluation:
         *   1. Extract constants from the source text
         *   2. Wrap in IIFE with constants preamble and evaluate
         *   3. Read __sbmd_registration and extract metadata + handlers
         *
         * Handler JSValues are NOT GC-rooted by this function. The caller must
         * call ActivateHandlers() to root them when the driver has paired devices.
         *
         * @param ctx The mquickjs context (caller must hold the mutex)
         * @param filePath Path to the .sbmd.js file (for diagnostics)
         * @param source The file contents
         * @param sourceLen Length of the file contents
         * @return The extracted registration, or nullptr on failure
         */
        static std::unique_ptr<SbmdRegistration>
        LoadDriver(JSContext *ctx, const std::string &filePath, const char *source, size_t sourceLen);

        /**
         * Extract the constants block from a .sbmd.js source text.
         *
         * Scans for "constants:" or "constants :" followed by "{", then brace-matches
         * to find the closing "}". Evaluates the block as "({...})" to get an object,
         * then walks its properties to produce name=value pairs.
         *
         * @param ctx The mquickjs context
         * @param source The file contents
         * @param sourceLen Length of the file contents
         * @return An engaged optional holding the (name, value-as-JS-literal) pairs on success
         *         (an empty vector means there was no constants block, or it was empty);
         *         std::nullopt if a constants block was present but could not be extracted
         *         (malformed block, eval failure, or a non-primitive value).
         */
        static std::optional<std::vector<std::pair<std::string, std::string>>>
        ExtractConstants(JSContext *ctx, const char *source, size_t sourceLen);

        /**
         * Generate the var declaration preamble from constants pairs.
         *
         * @param constants Pairs of (name, value-as-JS-literal)
         * @return String like "var EP_LIGHT = \"1\";\nvar CL_ON_OFF = 6;\n"
         */
        static std::string GenerateConstantsPreamble(const std::vector<std::pair<std::string, std::string>> &constants);

        /**
         * Count the number of lines in the constants preamble (for error line adjustment).
         */
        static int CountPreambleLines(const std::string &preamble);

    private:
        /**
         * Extract the registration object from the JS context after evaluation.
         * Reads __sbmd_registration, resets it to null, and walks the JSValue
         * to populate a SbmdRegistration struct.
         */
        static std::unique_ptr<SbmdRegistration> ExtractRegistration(JSContext *ctx, const std::string &filePath);

        /**
         * Walk a JSValue registration object and populate metadata fields.
         */
        static bool ExtractMetadata(JSContext *ctx, JSValue reg, SbmdRegistration &out);

        /**
         * Walk the aliases object and populate the aliases map.
         */
        static bool ExtractAliases(JSContext *ctx, JSValue aliasesObj, SbmdRegistration &out);

        /**
         * Walk the endpoints object and populate endpoint/resource structures.
         */
        static bool ExtractEndpoints(JSContext *ctx, JSValue endpointsObj, SbmdRegistration &out);

        /**
         * Walk a resource handler declaration (simple function or {supplements, handler} object).
         */
        static std::optional<SbmdHandler> ExtractResourceHandler(JSContext *ctx, JSValue val);

        /**
         * Walk a device handler array (attributeHandlers, eventHandlers, commandHandlers).
         */
        static bool ExtractDeviceHandlers(JSContext *ctx,
                                          JSValue handlersObj,
                                          std::vector<SbmdDeviceHandler> &out);

        /**
         * Walk a supplements declaration object.
         */
        static SbmdSupplements ExtractSupplements(JSContext *ctx, JSValue supplementsObj);
    };

} // namespace barton
