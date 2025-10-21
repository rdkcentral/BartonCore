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

//
// Created by tlea on 12/5/25
//

#define LOG_TAG "QuickJsScript"

#include "QuickJsScript.h"
#include <cstring>
#include <json/json.h>
#include <lib/core/CHIPError.h>
#include <lib/support/Span.h>
#include <lib/support/jsontlv/JsonToTlv.h>
#include <lib/support/jsontlv/TlvJson.h>
#include <platform/CHIPDeviceLayer.h>

extern "C" {
#include <icLog/logging.h>
#include <quickjs/quickjs.h>
}

namespace barton
{

QuickJsScript::QuickJsScript(const std::string &deviceId)
    : SbmdScript(deviceId), runtime(nullptr), ctx(nullptr)
{
    runtime = JS_NewRuntime();
    if (!runtime)
    {
        icLogError(LOG_TAG, "Failed to create QuickJS runtime for device %s", deviceId.c_str());
        return;
    }

    ctx = JS_NewContext(runtime);
    if (!ctx)
    {
        icLogError(LOG_TAG, "Failed to create QuickJS context for device %s", deviceId.c_str());
        JS_FreeRuntime(runtime);
        runtime = nullptr;
        return;
    }

    icLogDebug(LOG_TAG, "QuickJS runtime initialized for device %s", deviceId.c_str());
}

QuickJsScript::~QuickJsScript()
{
    if (ctx)
    {
        JS_FreeContext(ctx);
    }
    if (runtime)
    {
        JS_FreeRuntime(runtime);
    }
    icLogDebug(LOG_TAG, "QuickJS runtime destroyed for device %s", deviceId.c_str());
}

bool QuickJsScript::AddAttributeReadMapper(const SbmdAttribute &attributeInfo,
                                           const std::string &script)
{
    attributeReadScripts[attributeInfo] = script;
    icLogDebug(LOG_TAG, "Added attribute read mapper for cluster 0x%X, attribute 0x%X",
               attributeInfo.clusterId, attributeInfo.attributeId);
    return true;
}

bool QuickJsScript::AddAttributeWriteMapper(const SbmdAttribute &attributeInfo,
                                            const std::string &script)
{
    attributeWriteScripts[attributeInfo] = script;
    icLogDebug(LOG_TAG, "Added attribute write mapper for cluster 0x%X, attribute 0x%X",
               attributeInfo.clusterId, attributeInfo.attributeId);
    return true;
}

bool QuickJsScript::AddCommandExecuteMapper(const SbmdCommand &commandInfo,
                                            const std::string &script)
{
    commandExecuteScripts[commandInfo] = script;
    icLogDebug(LOG_TAG, "Added command execute mapper for cluster 0x%X, command 0x%X",
               commandInfo.clusterId, commandInfo.commandId);
    return true;
}

bool QuickJsScript::ExecuteScript(const std::string &script,
                                  const std::string &argumentName,
                                  const std::string &argumentValue,
                                  std::string &outValue)
{
    if (!ctx)
    {
        icLogError(LOG_TAG, "QuickJS context not initialized");
        return false;
    }

    if (script.empty())
    {
        icLogWarn(LOG_TAG, "Empty script provided");
        return false;
    }

    // Parse the JSON string in QuickJS context
    JSValue argVal = JS_ParseJSON(ctx, argumentValue.c_str(), argumentValue.length(), "<argument>");
    if (JS_IsException(argVal))
    {
        JSValue exception = JS_GetException(ctx);
        const char *exceptionStr = JS_ToCString(ctx, exception);
        icLogError(LOG_TAG,
                   "Failed to parse JSON for argument '%s': %s",
                   argumentName.c_str(),
                   exceptionStr ? exceptionStr : "unknown error");
        if (exceptionStr)
        {
            JS_FreeCString(ctx, exceptionStr);
        }
        JS_FreeValue(ctx, exception);
        return false;
    }

    // Set the parsed JSON object as a global variable
    JSValue global = JS_GetGlobalObject(ctx);
    if (JS_SetPropertyStr(ctx, global, argumentName.c_str(), argVal) < 0)
    {
        icLogError(LOG_TAG, "Failed to set argument variable '%s'", argumentName.c_str());
        JS_FreeValue(ctx, argVal);
        JS_FreeValue(ctx, global);
        return false;
    }
    JS_FreeValue(ctx, global);

    // Wrap the script body in a function and execute it
    std::string wrappedScript = "(function() { " + script + " })()";

    icLogDebug(LOG_TAG, "Executing script: %s", wrappedScript.c_str());

    // Execute the script
    JSValue scriptResult = JS_Eval(ctx, wrappedScript.c_str(), wrappedScript.length(),
                                   "<sbmd_script>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(scriptResult))
    {
        JSValue exception = JS_GetException(ctx);
        const char *exceptionStr = JS_ToCString(ctx, exception);
        icLogError(LOG_TAG, "Script execution failed: %s",
                   exceptionStr ? exceptionStr : "unknown error");
        if (exceptionStr)
        {
            JS_FreeCString(ctx, exceptionStr);
        }
        JS_FreeValue(ctx, exception);
        return false;
    }

    // Convert result to string
    const char *resultStr = JS_ToCString(ctx, scriptResult);
    if (resultStr != nullptr)
    {
        outValue = resultStr;
        icLogDebug(LOG_TAG, "Script executed successfully, result: %s", outValue.c_str());
        JS_FreeCString(ctx, resultStr);
        JS_FreeValue(ctx, scriptResult);
        return true;
    }
    else
    {
        icLogError(LOG_TAG, "Failed to convert script result to string");
        JS_FreeValue(ctx, scriptResult);
        return false;
    }
}

bool QuickJsScript::SetJsVariable(const std::string &name, const std::string &value)
{
    if (!ctx)
    {
        icLogError(LOG_TAG, "QuickJS context not initialized");
        return false;
    }

    JSValue jsValue = JS_NewString(ctx, value.c_str());
    JSValue global = JS_GetGlobalObject(ctx);
    bool success = JS_SetPropertyStr(ctx, global, name.c_str(), jsValue) >= 0;
    JS_FreeValue(ctx, global);

    if (!success)
    {
        icLogError(LOG_TAG, "Failed to set JS variable '%s'", name.c_str());
    }

    return success;
}

bool QuickJsScript::MapAttributeRead(const SbmdAttribute &attributeInfo,
                                     chip::TLV::TLVReader &reader,
                                     std::string &outValue)
{
    auto it = attributeReadScripts.find(attributeInfo);
    if (it == attributeReadScripts.end())
    {
        icLogError(LOG_TAG, "No read mapper found for cluster 0x%X, attribute 0x%X",
                   attributeInfo.clusterId, attributeInfo.attributeId);
        return false;
    }

    Json::Value argJson;
    if (chip::TlvToJson(reader, argJson) != CHIP_NO_ERROR)
    {
        icLogError(LOG_TAG, "Failed to convert TLV to JSON for argument '%s'", attributeInfo.name.c_str());
        return false;
    }

    // Convert Json::Value to string
    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    std::string jsonString = Json::writeString(writerBuilder, argJson);

    return ExecuteScript(it->second, attributeInfo.name, jsonString, outValue);
}

bool QuickJsScript::MapAttributeWrite(const SbmdAttribute &attributeInfo,
                                      const std::string &inValue,
                                      chip::Platform::ScopedMemoryBuffer<uint8_t> &buffer,
                                      size_t &encodedLength)
{
    auto it = attributeWriteScripts.find(attributeInfo);
    if (it == attributeWriteScripts.end())
    {
        icLogError(LOG_TAG, "No write mapper found for cluster 0x%X, attribute 0x%X",
                   attributeInfo.clusterId, attributeInfo.attributeId);
        return false;
    }

    // Wrap inValue in a JSON object with "input" key
    Json::Value inputJson;
    inputJson["input"] = inValue;
    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    std::string jsonInput = Json::writeString(writerBuilder, inputJson);

    std::string outValue;
    if (!ExecuteScript(it->second, attributeInfo.name, jsonInput, outValue))
    {
        icLogError(LOG_TAG, "Failed to execute write mapping script for cluster 0x%X, attribute 0x%X",
                   attributeInfo.clusterId, attributeInfo.attributeId);
        return false;
    }

    // Allocate buffer for TLV encoding (reasonable max size)
    constexpr size_t kMaxTlvSize = 1024;
    if (!buffer.Alloc(kMaxTlvSize))
    {
        icLogError(LOG_TAG, "Failed to allocate buffer for TLV encoding");
        return false;
    }

    // Initialize TLV writer with the buffer
    chip::TLV::TLVWriter writer;
    writer.Init(buffer.Get(), kMaxTlvSize);

    // Convert outValue (JSON string) back to TLV
    if (chip::JsonToTlv(outValue, writer) != CHIP_NO_ERROR)
    {
        icLogError(LOG_TAG, "Failed to convert JSON to TLV for attribute write");
        return false;
    }

    // Get the length of encoded data
    encodedLength = writer.GetLengthWritten();
    icLogDebug(LOG_TAG, "Encoded %zu bytes of TLV data", encodedLength);

    return true;
}

bool QuickJsScript::MapCommandExecute(const SbmdCommand &commandInfo,
                                      const std::vector<std::string> &argumentValues,
                                      chip::TLV::TLVWriter &writer)
{
    auto it = commandExecuteScripts.find(commandInfo);
    if (it == commandExecuteScripts.end())
    {
        icLogError(LOG_TAG, "No execute mapper found for cluster 0x%X, command 0x%X",
                   commandInfo.clusterId, commandInfo.commandId);
        return false;
    }

    // TODO: Implement command execute mapping with TLVWriter
    icLogError(LOG_TAG, "MapCommandExecute not yet implemented");
    return false;
}

} // namespace barton
