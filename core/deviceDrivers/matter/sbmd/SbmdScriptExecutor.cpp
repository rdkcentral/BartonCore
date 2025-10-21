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

#define LOG_TAG "SbmdScriptExecutor"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "SbmdScriptExecutor.h"
#include <cstring>
#include <lib/core/CHIPError.h>
#include <lib/support/Span.h>

extern "C" {
#include <icLog/logging.h>
#include <quickjs/quickjs.h>
}

namespace barton
{

    JSValue SbmdScriptExecutor::ConvertTlvToJsValue(JSContext *ctx, chip::TLV::TLVReader &reader)
    {
        auto tlvType = reader.GetType();

        switch (tlvType)
        {
            case chip::TLV::kTLVType_Boolean:
            {
                bool boolValue;
                if (reader.Get(boolValue) == CHIP_NO_ERROR)
                {
                    icDebug("Converting boolean value: %s", boolValue ? "true" : "false");
                    return JS_NewBool(ctx, boolValue ? 1 : 0);
                }
                else
                {
                    icError("Failed to read boolean from TLV");
                    return JS_EXCEPTION;
                }
            }
            case chip::TLV::kTLVType_SignedInteger:
            {
                int64_t intValue;
                if (reader.Get(intValue) == CHIP_NO_ERROR)
                {
                    icDebug("Converting signed integer value: %lld", (long long) intValue);
                    return JS_NewInt64(ctx, intValue);
                }
                else
                {
                    icError("Failed to read signed integer from TLV");
                    return JS_EXCEPTION;
                }
            }
            case chip::TLV::kTLVType_UnsignedInteger:
            {
                uint64_t uintValue;
                if (reader.Get(uintValue) == CHIP_NO_ERROR)
                {
                    icDebug("Converting unsigned integer value: %llu", (unsigned long long) uintValue);
                    return JS_NewBigUint64(ctx, uintValue);
                }
                else
                {
                    icError("Failed to read unsigned integer from TLV");
                    return JS_EXCEPTION;
                }
            }
            case chip::TLV::kTLVType_UTF8String:
            {
                chip::CharSpan stringValue;
                if (reader.Get(stringValue) == CHIP_NO_ERROR)
                {
                    std::string str(stringValue.data(), stringValue.size());
                    icDebug("Converting string value: %s", str.c_str());
                    return JS_NewStringLen(ctx, str.c_str(), str.length());
                }
                else
                {
                    icError("Failed to read string from TLV");
                    return JS_EXCEPTION;
                }
            }
            case chip::TLV::kTLVType_FloatingPointNumber:
            {
                double floatValue;
                if (reader.Get(floatValue) == CHIP_NO_ERROR)
                {
                    icDebug("Converting floating point value: %f", floatValue);
                    return JS_NewFloat64(ctx, floatValue);
                }
                else
                {
                    icError("Failed to read float from TLV");
                    return JS_EXCEPTION;
                }
            }
            default:
            {
                icError("Unsupported TLV type: %d", (int) tlvType);
                return JS_EXCEPTION;
            }
        }
    }

bool SbmdScriptExecutor::ExecuteMapperScript(const std::string &script,
                                             const std::string &argumentName,
                                             chip::TLV::TLVReader &argumentReader,
                                             char **outputValue)
{
    icDebug("Executing mapper script with argument: %s", argumentName.c_str());

    if (script.empty())
    {
        icWarn("Empty script provided");
        return false;
    }

    if (outputValue == nullptr)
    {
        icError("Output value pointer is null");
        return false;
    }

    icDebug("Script to execute: %s", script.c_str());

    // Initialize QuickJS runtime and context
    JSRuntime *runtime = JS_NewRuntime();
    if (!runtime)
    {
        icError("Failed to create QuickJS runtime");
        return false;
    }

    JSContext *ctx = JS_NewContext(runtime);
    if (!ctx)
    {
        icError("Failed to create QuickJS context");
        JS_FreeRuntime(runtime);
        return false;
    }

    bool result = false;

    // Set the argument variable in the JavaScript context by reading from TLV
    JSValue argVal = ConvertTlvToJsValue(ctx, argumentReader);

    if (JS_IsException(argVal))
    {
        icError("Failed to create argument value");
        JS_FreeContext(ctx);
        JS_FreeRuntime(runtime);
        return false;
    }

    JSValue global = JS_GetGlobalObject(ctx);
    if (JS_SetPropertyStr(ctx, global, argumentName.c_str(), argVal) < 0)
    {
        icError("Failed to set argument variable '%s'", argumentName.c_str());
        JS_FreeValue(ctx, global);
        JS_FreeContext(ctx);
        JS_FreeRuntime(runtime);
        return false;
    }
    JS_FreeValue(ctx, global);

    // Wrap the script body in a function and execute it
    // The script is expected to be a function body that returns a string
    std::string wrappedScript = "(function() { " + script + " })()";

    icDebug("Wrapped script: %s", wrappedScript.c_str());

    // Execute the script
    JSValue scriptResult =
        JS_Eval(ctx, wrappedScript.c_str(), wrappedScript.length(), "<sbmd_script>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(scriptResult))
    {
        // Get the exception details
        JSValue exception = JS_GetException(ctx);
        const char *exceptionStr = JS_ToCString(ctx, exception);
        icError("Script execution failed: %s", exceptionStr ? exceptionStr : "unknown error");
        if (exceptionStr)
        {
            JS_FreeCString(ctx, exceptionStr);
        }
        JS_FreeValue(ctx, exception);
        JS_FreeContext(ctx);
        JS_FreeRuntime(runtime);
        return false;
    }

    // Convert result to string
    const char *resultStr = JS_ToCString(ctx, scriptResult);
    if (resultStr != nullptr)
    {
        *outputValue = strdup(resultStr);
        result = (*outputValue != nullptr);
        icDebug("Script executed successfully, result: %s", *outputValue);
        JS_FreeCString(ctx, resultStr);
    }
    else
    {
        icError("Failed to convert script result to string");
        *outputValue = strdup("");
        result = (*outputValue != nullptr);
    }

    // Clean up QuickJS resources
    JS_FreeValue(ctx, scriptResult);
    JS_FreeContext(ctx);
    JS_FreeRuntime(runtime);

    return result;
}

} // namespace barton
