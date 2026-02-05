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

//
// Created by tlea on 12/5/25
//

#define LOG_TAG "QuickJsScript"

#include "QuickJsScript.h"
#include "../SbmdSpec.h"
#include <cstring>
#include <json/json.h>
#include <lib/core/CHIPError.h>
#include <lib/support/Base64.h>
#include <lib/support/Span.h>
#include <lib/support/jsontlv/JsonToTlv.h>
#include <lib/support/jsontlv/TlvJson.h>
#include <platform/CHIPDeviceLayer.h>
#include <sstream>

extern "C" {
#include <icLog/logging.h>
#include <quickjs/quickjs.h>
}

namespace barton
{

    namespace
    {

        /**
         * Maps Matter type strings to the JsonToTlv type format strings.
         * @param matterType The Matter type string (e.g., "bool", "uint8", "int16", "string", etc.)
         * @return The JsonToTlv type string (e.g., "BOOL", "UINT", "INT", "STRING", etc.)
         */
        std::string matterTypeToJsonTlvType(const std::string &matterType)
        {
            // Boolean type
            if (matterType == "bool" || matterType == "boolean")
            {
                return "BOOL";
            }
            // Unsigned integer types
            if (matterType == "uint8" || matterType == "uint16" || matterType == "uint32" || matterType == "uint64" ||
                matterType == "enum8" || matterType == "enum16" || matterType == "bitmap8" ||
                matterType == "bitmap16" || matterType == "bitmap32" || matterType == "bitmap64" ||
                matterType == "percent" || matterType == "percent100ths" || matterType == "epoch-s" ||
                matterType == "epoch-us" || matterType == "posix-ms" || matterType == "elapsed-s" ||
                matterType == "utc" || matterType == "fabric-idx" || matterType == "fabric-id" ||
                matterType == "node-id" || matterType == "vendor-id" || matterType == "devtype-id" ||
                matterType == "group-id" || matterType == "endpoint-no" || matterType == "cluster-id" ||
                matterType == "attrib-id" || matterType == "field-id" || matterType == "event-id" ||
                matterType == "command-id" || matterType == "action-id" || matterType == "trans-id" ||
                matterType == "data-ver" || matterType == "entry-idx" || matterType == "systime-ms" ||
                matterType == "systime-us")
            {
                return "UINT";
            }
            // Signed integer types
            if (matterType == "int8" || matterType == "int16" || matterType == "int24" || matterType == "int32" ||
                matterType == "int40" || matterType == "int48" || matterType == "int56" || matterType == "int64" ||
                matterType == "temperature" || matterType == "amperage-ma" || matterType == "voltage-mv" ||
                matterType == "power-mw" || matterType == "energy-mwh")
            {
                return "INT";
            }
            // Floating point types
            if (matterType == "single" || matterType == "float")
            {
                return "FLOAT";
            }
            if (matterType == "double")
            {
                return "DOUBLE";
            }
            // String types
            if (matterType == "string" || matterType == "char_string" || matterType == "long_char_string")
            {
                return "STRING";
            }
            // Byte string types
            if (matterType == "octstr" || matterType == "octet_string" || matterType == "long_octet_string" ||
                matterType == "ipadr" || matterType == "ipv4adr" || matterType == "ipv6adr" ||
                matterType == "ipv6pre" || matterType == "hwadr" || matterType == "semtag")
            {
                return "BYTES";
            }
            // Struct type
            if (matterType == "struct")
            {
                return "STRUCT";
            }
            // Array/list types
            if (matterType == "list" || matterType == "array")
            {
                return "ARRAY-?";
            }
            // Null type
            if (matterType == "null")
            {
                return "NULL";
            }

            // Default: treat unknown types as generic (let JsonToTlv infer)
            icLogWarn(LOG_TAG, "Unknown Matter type '%s', using STRUCT as fallback", matterType.c_str());
            return "STRUCT";
        }

        /**
         * Converts a value to the JsonToTlv format expected by CHIP SDK.
         * For scalar values, wraps them as {"0:TYPE": value}
         * @param value The JSON value to convert
         * @param matterType The Matter type string for type mapping
         * @return The JSON string formatted for JsonToTlv
         */
        std::string formatValueForJsonToTlv(const Json::Value &value, const std::string &matterType)
        {
            std::string tlvType = matterTypeToJsonTlvType(matterType);

            Json::Value wrappedJson;
            std::string key = "0:" + tlvType;
            wrappedJson[key] = value;

            Json::StreamWriterBuilder writerBuilder;
            writerBuilder["indentation"] = "";
            return Json::writeString(writerBuilder, wrappedJson);
        }

        /**
         * Formats command arguments for JsonToTlv.
         * Handles special cases like octstr (byte arrays) that need base64 encoding.
         * @param outputJson The script output (can be array of bytes, single value, or struct)
         * @param args The command argument definitions from SBMD
         * @return The JSON string formatted for JsonToTlv
         */
        std::string formatCommandArgsForJsonToTlv(const Json::Value &outputJson, const std::vector<SbmdArgument> &args)
        {
            Json::Value tlvJson(Json::objectValue);

            // If there are no args defined, or output is null/empty, return empty struct
            if (args.empty() || outputJson.isNull())
            {
                Json::StreamWriterBuilder writerBuilder;
                writerBuilder["indentation"] = "";
                return Json::writeString(writerBuilder, tlvJson);
            }

            // If output is an array and we have a single BYTES-type argument,
            // treat it as a byte array that needs base64 encoding
            if (outputJson.isArray() && args.size() == 1)
            {
                const SbmdArgument &arg = args[0];
                std::string tlvType = matterTypeToJsonTlvType(arg.type);

                if (tlvType == "BYTES")
                {
                    // Convert array of integers to bytes and base64 encode
                    std::vector<uint8_t> bytes;
                    bytes.reserve(outputJson.size());
                    for (const auto &elem : outputJson)
                    {
                        if (elem.isIntegral())
                        {
                            bytes.push_back(static_cast<uint8_t>(elem.asInt()));
                        }
                        else
                        {
                            icLogWarn(LOG_TAG, "Ignoring non-integral value in byte array for argument '%s'", arg.name.c_str());
                        }
                    }

                    // Base64 encode the bytes
                    size_t base64Size = BASE64_ENCODED_LEN(bytes.size()) + 1;
                    std::vector<char> base64(base64Size);
                    uint16_t encodedLen =
                        chip::Base64Encode(bytes.data(), static_cast<uint16_t>(bytes.size()), base64.data());
                    base64[encodedLen] = '\0';

                    std::string key = "0:" + tlvType;
                    tlvJson[key] = std::string(base64.data());

                    Json::StreamWriterBuilder writerBuilder;
                    writerBuilder["indentation"] = "";
                    return Json::writeString(writerBuilder, tlvJson);
                }
            }

            // For a single non-array value with a single argument
            if (!outputJson.isArray() && !outputJson.isObject() && args.size() == 1)
            {
                const SbmdArgument &arg = args[0];
                std::string tlvType = matterTypeToJsonTlvType(arg.type);
                std::string key = "0:" + tlvType;
                tlvJson[key] = outputJson;

                Json::StreamWriterBuilder writerBuilder;
                writerBuilder["indentation"] = "";
                return Json::writeString(writerBuilder, tlvJson);
            }

            // For struct/object output with multiple args, map each field by index
            if (outputJson.isObject())
            {
                for (size_t i = 0; i < args.size(); ++i)
                {
                    const SbmdArgument &arg = args[i];
                    if (outputJson.isMember(arg.name))
                    {
                        std::string tlvType = matterTypeToJsonTlvType(arg.type);
                        std::string key = std::to_string(i) + ":" + tlvType;
                        tlvJson[key] = outputJson[arg.name];
                    }
                }
            }

            Json::StreamWriterBuilder writerBuilder;
            writerBuilder["indentation"] = "";
            return Json::writeString(writerBuilder, tlvJson);
        }

    } // anonymous namespace

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

bool QuickJsScript::AddCommandExecuteResponseMapper(const SbmdCommand &commandInfo,
                                                    const std::string &script)
{
    commandExecuteResponseScripts[commandInfo] = script;
    icLogDebug(LOG_TAG, "Added command execute response mapper for cluster 0x%X, command 0x%X",
               commandInfo.clusterId, commandInfo.commandId);
    return true;
}

bool QuickJsScript::ExecuteScript(const std::string &script,
                                  const std::string &argumentName,
                                  const JSValue &argumentJson,
                                  JSValue &outJson)
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

    // Set the JSON object as a global variable (duplicate value to maintain ownership)
    JSValue argVal = JS_DupValue(ctx, argumentJson);
    JSValue global = JS_GetGlobalObject(ctx);
    if (JS_SetPropertyStr(ctx, global, argumentName.c_str(), argVal) < 0)
    {
        icLogError(LOG_TAG, "Failed to set argument variable '%s'", argumentName.c_str());
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
        JS_FreeValue(ctx, scriptResult);
        return false;
    }

    outJson = scriptResult;
    icLogDebug(LOG_TAG, "Script executed successfully");
    return true;
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

    // Convert TLV to JSON for the input value
    Json::Value inputJson;
    if (chip::TlvToJson(reader, inputJson) != CHIP_NO_ERROR)
    {
        icLogError(LOG_TAG, "Failed to convert TLV to JSON for argument '%s'", attributeInfo.name.c_str());
        return false;
    }

    // Build the sbmdReadArgs JSON object
    Json::Value argsJson;
    argsJson["input"] = inputJson["value"];
    argsJson["deviceUuid"] = deviceId;
    argsJson["clusterId"] = attributeInfo.clusterId;
    argsJson["featureMap"] = attributeInfo.featureMap;
    argsJson["endpointId"] = attributeInfo.resourceEndpointId.value_or("");
    argsJson["attributeId"] = attributeInfo.attributeId;
    argsJson["attributeName"] = attributeInfo.name;
    argsJson["attributeType"] = attributeInfo.type;

    // Convert Json::Value to string for parsing in QuickJS
    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    std::string jsonString = Json::writeString(writerBuilder, argsJson);

    icLogDebug(LOG_TAG, "sbmdReadArgs JSON: %s", jsonString.c_str());

    // Parse JSON string to JSValue
    JSValue argJson = JS_ParseJSON(ctx, jsonString.c_str(), jsonString.length(), "<sbmdReadArgs>");
    if (JS_IsException(argJson))
    {
        JSValue exception = JS_GetException(ctx);
        const char *exceptionStr = JS_ToCString(ctx, exception);
        icLogError(LOG_TAG, "Failed to parse sbmdReadArgs JSON: %s", exceptionStr ? exceptionStr : "unknown error");
        if (exceptionStr)
        {
            JS_FreeCString(ctx, exceptionStr);
        }
        JS_FreeValue(ctx, exception);
        return false;
    }

    JSValue outJson;
    bool success = ExecuteScript(it->second, "sbmdReadArgs", argJson, outJson);
    JS_FreeValue(ctx, argJson);

    if (!success)
    {
        return false;
    }

    // Extract the "output" field from the result JSON object
    JSValue outputVal = JS_GetPropertyStr(ctx, outJson, "output");
    if (JS_IsUndefined(outputVal))
    {
        icLogError(LOG_TAG, "Script result missing 'output' field");
        JS_FreeValue(ctx, outJson);
        return false;
    }

    const char *resultStr = JS_ToCString(ctx, outputVal);
    if (resultStr != nullptr)
    {
        outValue = resultStr;
        icLogDebug(LOG_TAG, "MapAttributeRead result: %s", outValue.c_str());
        JS_FreeCString(ctx, resultStr);
    }
    else
    {
        icLogError(LOG_TAG, "Failed to convert output value to string");
        JS_FreeValue(ctx, outputVal);
        JS_FreeValue(ctx, outJson);
        return false;
    }

    JS_FreeValue(ctx, outputVal);
    JS_FreeValue(ctx, outJson);
    return true;
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

    // Build the sbmdWriteArgs JSON object
    Json::Value argsJson;
    argsJson["input"] = inValue;
    argsJson["deviceUuid"] = deviceId;
    argsJson["clusterId"] = attributeInfo.clusterId;
    argsJson["featureMap"] = attributeInfo.featureMap;
    argsJson["endpointId"] = attributeInfo.resourceEndpointId.value_or("");
    argsJson["attributeId"] = attributeInfo.attributeId;
    argsJson["attributeName"] = attributeInfo.name;
    argsJson["attributeType"] = attributeInfo.type;

    // Convert Json::Value to string for parsing in QuickJS
    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    std::string jsonString = Json::writeString(writerBuilder, argsJson);

    icLogDebug(LOG_TAG, "sbmdWriteArgs JSON: %s", jsonString.c_str());

    // Parse JSON string to JSValue
    JSValue argJson = JS_ParseJSON(ctx, jsonString.c_str(), jsonString.length(), "<sbmdWriteArgs>");
    if (JS_IsException(argJson))
    {
        JSValue exception = JS_GetException(ctx);
        const char *exceptionStr = JS_ToCString(ctx, exception);
        icLogError(LOG_TAG, "Failed to parse sbmdWriteArgs JSON: %s", exceptionStr ? exceptionStr : "unknown error");
        if (exceptionStr)
        {
            JS_FreeCString(ctx, exceptionStr);
        }
        JS_FreeValue(ctx, exception);
        return false;
    }

    JSValue outJson;
    bool success = ExecuteScript(it->second, "sbmdWriteArgs", argJson, outJson);
    JS_FreeValue(ctx, argJson);

    if (!success)
    {
        icLogError(LOG_TAG, "Failed to execute write mapping script for cluster 0x%X, attribute 0x%X",
                   attributeInfo.clusterId, attributeInfo.attributeId);
        return false;
    }

    icLogDebug(LOG_TAG, "Script execution succeeded, extracting output field");

    // Extract the "output" field from the result JSON object
    JSValue outputVal = JS_GetPropertyStr(ctx, outJson, "output");
    if (JS_IsUndefined(outputVal))
    {
        icLogError(LOG_TAG, "Script result missing 'output' field");
        JS_FreeValue(ctx, outJson);
        return false;
    }

    icLogDebug(LOG_TAG, "Got output field, stringifying");

    // Convert the output JSValue back to JSON string for TLV conversion
    JSValue jsonStr = JS_JSONStringify(ctx, outputVal, JS_UNDEFINED, JS_UNDEFINED);
    icLogDebug(LOG_TAG, "Stringify done, converting to C string");
    const char *outValueStr = JS_ToCString(ctx, jsonStr);
    if (outValueStr == nullptr)
    {
        icLogError(LOG_TAG, "Failed to stringify output value");
        JS_FreeValue(ctx, jsonStr);
        JS_FreeValue(ctx, outputVal);
        JS_FreeValue(ctx, outJson);
        return false;
    }

    icLogDebug(LOG_TAG, "Output JSON string: %s", outValueStr);
    std::string outValue = outValueStr;
    JS_FreeCString(ctx, outValueStr);
    JS_FreeValue(ctx, jsonStr);
    JS_FreeValue(ctx, outputVal);
    JS_FreeValue(ctx, outJson);

    // Parse the output JSON to wrap it in the JsonToTlv format
    Json::CharReaderBuilder readerBuilder;
    std::istringstream jsonStream(outValue);
    Json::Value outputJson;
    std::string parseErrors;
    if (!Json::parseFromStream(readerBuilder, jsonStream, &outputJson, &parseErrors))
    {
        icLogError(LOG_TAG, "Failed to parse output JSON: %s", parseErrors.c_str());
        return false;
    }

    // Format the value for JsonToTlv using the attribute's type information
    std::string tlvFormattedJson = formatValueForJsonToTlv(outputJson, attributeInfo.type);
    icLogDebug(LOG_TAG, "TLV formatted JSON: %s", tlvFormattedJson.c_str());

    icLogDebug(LOG_TAG, "Allocating TLV buffer");

    // Allocate buffer for TLV encoding (reasonable max size)
    constexpr size_t kMaxTlvSize = 1024;
    if (!buffer.Alloc(kMaxTlvSize))
    {
        icLogError(LOG_TAG, "Failed to allocate buffer for TLV encoding");
        return false;
    }

    icLogDebug(LOG_TAG, "Buffer allocated, initializing TLV writer");

    // Initialize TLV writer with the buffer
    chip::TLV::TLVWriter writer;
    writer.Init(buffer.Get(), kMaxTlvSize);

    icLogDebug(LOG_TAG, "Converting JSON to TLV: %s", tlvFormattedJson.c_str());

    // Convert the properly formatted JSON back to TLV
    if (chip::JsonToTlv(tlvFormattedJson, writer) != CHIP_NO_ERROR)
    {
        icLogError(LOG_TAG, "Failed to convert JSON to TLV for attribute write");
        return false;
    }

    icLogDebug(LOG_TAG, "JSON to TLV conversion succeeded");

    // Get the length of encoded data
    encodedLength = writer.GetLengthWritten();
    icLogDebug(LOG_TAG, "Encoded %zu bytes of TLV data", encodedLength);

    return true;
}

bool QuickJsScript::MapCommandExecute(const SbmdCommand &commandInfo,
                                      const std::vector<std::string> &argumentValues,
                                      chip::Platform::ScopedMemoryBuffer<uint8_t> &buffer,
                                      size_t &encodedLength)
{
    auto it = commandExecuteScripts.find(commandInfo);
    if (it == commandExecuteScripts.end())
    {
        icLogError(LOG_TAG, "No execute mapper found for cluster 0x%X, command 0x%X",
                   commandInfo.clusterId, commandInfo.commandId);
        return false;
    }

    // Build the sbmdCommandArgs JSON object
    Json::Value argsJson;
    Json::Value inputArray(Json::arrayValue);
    for (const auto &arg : argumentValues)
    {
        inputArray.append(arg);
    }
    argsJson["input"] = inputArray;
    argsJson["deviceUuid"] = deviceId;
    argsJson["clusterId"] = commandInfo.clusterId;
    argsJson["featureMap"] = commandInfo.featureMap;
    argsJson["endpointId"] = commandInfo.resourceEndpointId.value_or("");
    argsJson["commandId"] = commandInfo.commandId;
    argsJson["commandName"] = commandInfo.name;

    // Convert Json::Value to string for parsing in QuickJS
    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    std::string jsonString = Json::writeString(writerBuilder, argsJson);

    icLogDebug(LOG_TAG, "sbmdCommandArgs JSON: %s", jsonString.c_str());

    // Parse JSON string to JSValue
    JSValue argJson = JS_ParseJSON(ctx, jsonString.c_str(), jsonString.length(), "<sbmdCommandArgs>");
    if (JS_IsException(argJson))
    {
        JSValue exception = JS_GetException(ctx);
        const char *exceptionStr = JS_ToCString(ctx, exception);
        icLogError(LOG_TAG, "Failed to parse sbmdCommandArgs JSON: %s", exceptionStr ? exceptionStr : "unknown error");
        if (exceptionStr)
        {
            JS_FreeCString(ctx, exceptionStr);
        }
        JS_FreeValue(ctx, exception);
        return false;
    }

    JSValue outJson;
    bool success = ExecuteScript(it->second, "sbmdCommandArgs", argJson, outJson);
    JS_FreeValue(ctx, argJson);

    if (!success)
    {
        icLogError(LOG_TAG,
                   "Failed to execute command mapping script for cluster 0x%X, command 0x%X",
                   commandInfo.clusterId,
                   commandInfo.commandId);
        return false;
    }

    icLogDebug(LOG_TAG, "Script execution succeeded, extracting output field");

    // Extract the "output" field from the result JSON object
    JSValue outputVal = JS_GetPropertyStr(ctx, outJson, "output");
    if (JS_IsUndefined(outputVal))
    {
        icLogError(LOG_TAG, "Script result missing 'output' field");
        JS_FreeValue(ctx, outJson);
        return false;
    }

    icLogDebug(LOG_TAG, "Got output field, stringifying");

    // Convert the output JSValue back to JSON string for TLV conversion
    JSValue jsonStr = JS_JSONStringify(ctx, outputVal, JS_UNDEFINED, JS_UNDEFINED);
    icLogDebug(LOG_TAG, "Stringify done, converting to C string");
    const char *outValueStr = JS_ToCString(ctx, jsonStr);
    if (outValueStr == nullptr)
    {
        icLogError(LOG_TAG, "Failed to stringify output value");
        JS_FreeValue(ctx, jsonStr);
        JS_FreeValue(ctx, outputVal);
        JS_FreeValue(ctx, outJson);
        return false;
    }

    icLogDebug(LOG_TAG, "Output JSON string: %s", outValueStr);
    std::string outValue = outValueStr;
    JS_FreeCString(ctx, outValueStr);
    JS_FreeValue(ctx, jsonStr);
    JS_FreeValue(ctx, outputVal);
    JS_FreeValue(ctx, outJson);

    // Parse the output JSON to format it for JsonToTlv
    Json::CharReaderBuilder readerBuilder;
    std::istringstream jsonStream(outValue);
    Json::Value outputJson;
    std::string parseErrors;
    if (!Json::parseFromStream(readerBuilder, jsonStream, &outputJson, &parseErrors))
    {
        icLogError(LOG_TAG, "Failed to parse output JSON: %s", parseErrors.c_str());
        return false;
    }

    // Format the command arguments for JsonToTlv using the command's arg type information
    std::string tlvFormattedJson = formatCommandArgsForJsonToTlv(outputJson, commandInfo.args);
    icLogDebug(LOG_TAG, "TLV formatted JSON: %s", tlvFormattedJson.c_str());

    icLogDebug(LOG_TAG, "Allocating TLV buffer");

    // Allocate buffer for TLV encoding (reasonable max size)
    constexpr size_t kMaxTlvSize = 1024;
    if (!buffer.Alloc(kMaxTlvSize))
    {
        icLogError(LOG_TAG, "Failed to allocate buffer for TLV encoding");
        return false;
    }

    icLogDebug(LOG_TAG, "Buffer allocated, initializing TLV writer");

    // Initialize TLV writer with the buffer
    chip::TLV::TLVWriter writer;
    writer.Init(buffer.Get(), kMaxTlvSize);

    icLogDebug(LOG_TAG, "Converting JSON to TLV: %s", tlvFormattedJson.c_str());

    // Convert the formatted JSON to TLV
    if (chip::JsonToTlv(tlvFormattedJson, writer) != CHIP_NO_ERROR)
    {
        icLogError(LOG_TAG, "Failed to convert JSON to TLV for command execute");
        return false;
    }

    icLogDebug(LOG_TAG, "JSON to TLV conversion succeeded");

    // Get the length of encoded data
    encodedLength = writer.GetLengthWritten();
    icLogDebug(LOG_TAG, "Encoded %zu bytes of TLV data", encodedLength);

    return true;
}

bool QuickJsScript::MapCommandExecuteResponse(const SbmdCommand &commandInfo,
                                              chip::TLV::TLVReader &reader,
                                              std::string &outValue)
{
    auto it = commandExecuteResponseScripts.find(commandInfo);
    if (it == commandExecuteResponseScripts.end())
    {
        icLogError(LOG_TAG, "No execute response mapper found for cluster 0x%X, command 0x%X",
                   commandInfo.clusterId, commandInfo.commandId);
        return false;
    }

    // Convert TLV to JSON for the input value
    Json::Value inputJson;
    if (chip::TlvToJson(reader, inputJson) != CHIP_NO_ERROR)
    {
        icLogError(LOG_TAG, "Failed to convert TLV to JSON for command response '%s'", commandInfo.name.c_str());
        return false;
    }

    // Build the sbmdCommandResponseArgs JSON object
    Json::Value argsJson;
    argsJson["input"] = inputJson["value"];
    argsJson["deviceUuid"] = deviceId;
    argsJson["clusterId"] = commandInfo.clusterId;
    argsJson["featureMap"] = commandInfo.featureMap;
    argsJson["endpointId"] = commandInfo.resourceEndpointId.value_or("");
    argsJson["commandId"] = commandInfo.commandId;
    argsJson["commandName"] = commandInfo.name;

    // Convert Json::Value to string for parsing in QuickJS
    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    std::string jsonString = Json::writeString(writerBuilder, argsJson);

    icLogDebug(LOG_TAG, "sbmdCommandResponseArgs JSON: %s", jsonString.c_str());

    // Parse JSON string to JSValue
    JSValue argJson = JS_ParseJSON(ctx, jsonString.c_str(), jsonString.length(), "<sbmdCommandResponseArgs>");
    if (JS_IsException(argJson))
    {
        JSValue exception = JS_GetException(ctx);
        const char *exceptionStr = JS_ToCString(ctx, exception);
        icLogError(LOG_TAG, "Failed to parse sbmdCommandResponseArgs JSON: %s", exceptionStr ? exceptionStr : "unknown error");
        if (exceptionStr)
        {
            JS_FreeCString(ctx, exceptionStr);
        }
        JS_FreeValue(ctx, exception);
        return false;
    }

    JSValue outJson;
    bool success = ExecuteScript(it->second, "sbmdCommandResponseArgs", argJson, outJson);
    JS_FreeValue(ctx, argJson);

    if (!success)
    {
        return false;
    }

    // Extract the "output" field from the result JSON object
    JSValue outputVal = JS_GetPropertyStr(ctx, outJson, "output");
    if (JS_IsUndefined(outputVal))
    {
        icLogError(LOG_TAG, "Script result missing 'output' field");
        JS_FreeValue(ctx, outJson);
        return false;
    }

    const char *resultStr = JS_ToCString(ctx, outputVal);
    if (resultStr != nullptr)
    {
        outValue = resultStr;
        icLogDebug(LOG_TAG, "MapCommandExecuteResponse result: %s", outValue.c_str());
        JS_FreeCString(ctx, resultStr);
    }
    else
    {
        icLogError(LOG_TAG, "Failed to convert output value to string");
        JS_FreeValue(ctx, outputVal);
        JS_FreeValue(ctx, outJson);
        return false;
    }

    JS_FreeValue(ctx, outputVal);
    JS_FreeValue(ctx, outJson);
    return true;
}

} // namespace barton
