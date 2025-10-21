//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2025 Comcast Cable Communications Management, LLC
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
 * Created by Thomas Lea on 10/31/2025
 */

#pragma once

#include "lib/core/TLVReader.h"
#include <string>

// Forward declarations for QuickJS types
struct JSContext;
struct JSValue;

namespace barton
{
    /**
     * Utility class for executing SBMD mapper scripts using QuickJS
     */
    class SbmdScriptExecutor
    {
    public:
        /**
         * Execute a mapper script with the given input and return the result
         *
         * The script should be a function body that returns a string value.
         * The script will be wrapped in a function and executed.
         *
         * Example script bodies:
         *   - "return 'constant value';"
         *   - "if (StateValue) return 'true'; else return 'false';"
         *   - "return parseInt(temperature) * 2;"
         *
         * @param script The JavaScript function body to execute (will be wrapped in function() { ... })
         * @param argumentName Name of the argument variable available in the script
         * @param argumentReader TLV Reader for the argument value
         * @param outputValue final output value for the resource (all resource values are strings)
         * @return true if execution was successful and outputValue is set, false on error
         */
        static bool ExecuteMapperScript(const std::string &script,
                                        const std::string &argumentName,
                                        chip::TLV::TLVReader &argumentReader,
                                        char **outputValue);

    private:
        // No constructor - this is a utility class with only static methods
        SbmdScriptExecutor() = delete;
        ~SbmdScriptExecutor() = delete;
        SbmdScriptExecutor(const SbmdScriptExecutor&) = delete;
        SbmdScriptExecutor& operator=(const SbmdScriptExecutor&) = delete;

        /**
         * Convert a TLV value to a JavaScript value
         * @param ctx The QuickJS context
         * @param reader The TLV reader positioned at the value to convert
         * @return The JSValue representing the TLV value, or JS_EXCEPTION on error
         */
        static JSValue ConvertTlvToJsValue(JSContext *ctx, chip::TLV::TLVReader &reader);
    };

} // namespace barton
