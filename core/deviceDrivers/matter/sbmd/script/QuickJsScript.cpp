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

#include "json/writer.h"
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
#include <mutex>
#include <platform/CHIPDeviceLayer.h>
#include <sstream>
#include <unordered_map>

extern "C" {
#include <icLog/logging.h>
#include <quickjs/quickjs.h>
}

namespace barton
{

    namespace
    {
        /**
         * RAII wrapper for QuickJS JSValue.
         * Automatically frees the JSValue when the guard goes out of scope.
         */
        class JsValueGuard
        {
        public:
            JsValueGuard(JSContext *ctx, JSValue value) : ctx_(ctx), value_(value) {}
            ~JsValueGuard()
            {
                if (ctx_)
                {
                    JS_FreeValue(ctx_, value_);
                }
            }

            // Non-copyable
            JsValueGuard(const JsValueGuard &) = delete;
            JsValueGuard &operator=(const JsValueGuard &) = delete;

            // Movable
            JsValueGuard(JsValueGuard &&other) noexcept : ctx_(other.ctx_), value_(other.value_)
            {
                other.ctx_ = nullptr;
            }

            JsValueGuard &operator=(JsValueGuard &&other) noexcept
            {
                if (this != &other)
                {
                    if (ctx_)
                    {
                        JS_FreeValue(ctx_, value_);
                    }
                    ctx_ = other.ctx_;
                    value_ = other.value_;
                    other.ctx_ = nullptr;
                }
                return *this;
            }

            JSValue get() const { return value_; }
            JSValue *ptr() { return &value_; }

            // Release ownership without freeing (for returning values to caller)
            JSValue release()
            {
                ctx_ = nullptr;
                return value_;
            }

        private:
            JSContext *ctx_;
            JSValue value_;
        };

        /**
         * RAII wrapper for QuickJS C strings.
         * Automatically frees the string when the guard goes out of scope.
         */
        class JsCStringGuard
        {
        public:
            JsCStringGuard(JSContext *ctx, const char *str) : ctx_(ctx), str_(str) {}
            ~JsCStringGuard()
            {
                if (ctx_ && str_)
                {
                    JS_FreeCString(ctx_, str_);
                }
            }

            // Non-copyable
            JsCStringGuard(const JsCStringGuard &) = delete;
            JsCStringGuard &operator=(const JsCStringGuard &) = delete;

            // Movable
            JsCStringGuard(JsCStringGuard &&other) noexcept : ctx_(other.ctx_), str_(other.str_)
            {
                other.ctx_ = nullptr;
                other.str_ = nullptr;
            }

            const char *get() const { return str_; }
            explicit operator bool() const { return str_ != nullptr; }

        private:
            JSContext *ctx_;
            const char *str_;
        };

        /**
         * Extracts the current QuickJS exception as a string.
         * Clears the exception from the context.
         * @param ctx The QuickJS context
         * @return The exception message, or "unknown error" if unavailable
         */
        std::string GetExceptionString(JSContext *ctx)
        {
            JsValueGuard exceptionGuard(ctx, JS_GetException(ctx));

            // First try direct string conversion (works for string exceptions)
            JsCStringGuard strGuard(ctx, JS_ToCString(ctx, exceptionGuard.get()));
            if (strGuard)
            {
                return strGuard.get();
            }

            // If that fails, try to get the "message" property (for Error objects)
            if (JS_IsObject(exceptionGuard.get()))
            {
                JsValueGuard msgGuard(ctx, JS_GetPropertyStr(ctx, exceptionGuard.get(), "message"));
                if (!JS_IsUndefined(msgGuard.get()))
                {
                    JsCStringGuard msgStrGuard(ctx, JS_ToCString(ctx, msgGuard.get()));
                    if (msgStrGuard)
                    {
                        return msgStrGuard.get();
                    }
                }
            }

            return "unknown error";
        }

        /**
         * Maps Matter type strings to the JsonToTlv type format strings.
         * @param matterType The Matter type string (e.g., "bool", "uint8", "int16", "string", etc.)
         * @return The JsonToTlv type string (e.g., "BOOL", "UINT", "INT", "STRING", etc.)
         */
        std::string matterTypeToJsonTlvType(const std::string &matterType)
        {
            // clang-format off
            static const std::unordered_map<std::string, std::string> typeMap = {
                // Boolean types
                {"bool", "BOOL"}, {"boolean", "BOOL"},
                // Unsigned integer types
                {"uint8", "UINT"}, {"uint16", "UINT"}, {"uint32", "UINT"}, {"uint64", "UINT"},
                {"enum8", "UINT"}, {"enum16", "UINT"},
                {"bitmap8", "UINT"}, {"bitmap16", "UINT"}, {"bitmap32", "UINT"}, {"bitmap64", "UINT"},
                {"percent", "UINT"}, {"percent100ths", "UINT"},
                {"epoch-s", "UINT"}, {"epoch-us", "UINT"}, {"posix-ms", "UINT"}, {"elapsed-s", "UINT"}, {"utc", "UINT"},
                {"fabric-idx", "UINT"}, {"fabric-id", "UINT"}, {"node-id", "UINT"}, {"vendor-id", "UINT"},
                {"devtype-id", "UINT"}, {"group-id", "UINT"}, {"endpoint-no", "UINT"}, {"cluster-id", "UINT"},
                {"attrib-id", "UINT"}, {"field-id", "UINT"}, {"event-id", "UINT"}, {"command-id", "UINT"},
                {"action-id", "UINT"}, {"trans-id", "UINT"}, {"data-ver", "UINT"}, {"entry-idx", "UINT"},
                {"systime-ms", "UINT"}, {"systime-us", "UINT"},
                // Signed integer types
                {"int8", "INT"}, {"int16", "INT"}, {"int24", "INT"}, {"int32", "INT"},
                {"int40", "INT"}, {"int48", "INT"}, {"int56", "INT"}, {"int64", "INT"},
                {"temperature", "INT"}, {"amperage-ma", "INT"}, {"voltage-mv", "INT"},
                {"power-mw", "INT"}, {"energy-mwh", "INT"},
                // Floating point types
                {"single", "FLOAT"}, {"float", "FLOAT"}, {"double", "DOUBLE"},
                // String types
                {"string", "STRING"}, {"char_string", "STRING"}, {"long_char_string", "STRING"},
                // Byte string types
                {"octstr", "BYTES"}, {"octet_string", "BYTES"}, {"long_octet_string", "BYTES"},
                {"ipadr", "BYTES"}, {"ipv4adr", "BYTES"}, {"ipv6adr", "BYTES"}, {"ipv6pre", "BYTES"},
                {"hwadr", "BYTES"}, {"semtag", "BYTES"},
                // Struct type
                {"struct", "STRUCT"},
                // Array/list types
                {"list", "ARRAY-?"}, {"array", "ARRAY-?"},
                // Null type
                {"null", "NULL"},
            };
            // clang-format on

            auto it = typeMap.find(matterType);
            if (it != typeMap.end())
            {
                return it->second;
            }

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
            if (outputJson.isArray())
            {
                if (args.size() != 1)
                {
                    icLogError(
                        LOG_TAG, "Array output with multiple arguments (%zu) is not yet implemented", args.size());
                    return Json::writeString(Json::StreamWriterBuilder(), tlvJson);
                }

                const SbmdArgument &arg = args[0];
                std::string tlvType = matterTypeToJsonTlvType(arg.type);

                if (tlvType != "BYTES")
                {
                    icLogError(
                        LOG_TAG, "Array output for non-BYTES type '%s' is not yet implemented", arg.type.c_str());
                    return Json::writeString(Json::StreamWriterBuilder(), tlvJson);
                }

                // Convert array of integers to bytes and base64 encode
                std::vector<uint8_t> bytes;
                bytes.reserve(outputJson.size());
                for (const auto &elem : outputJson)
                {
                    if (elem.isIntegral())
                    {
                        int value = elem.asInt();
                        if (value < 0 || value > 255)
                        {
                            icLogError(
                                LOG_TAG, "Invalid value in byte array for argument '%s': %d", arg.name.c_str(), value);
                            return Json::writeString(Json::StreamWriterBuilder(), tlvJson);
                        }
                        bytes.push_back(static_cast<uint8_t>(value));
                    }
                    else
                    {
                        icLogWarn(
                            LOG_TAG, "Ignoring non-integral value in byte array for argument '%s'", arg.name.c_str());
                    }
                }

                // Check size limit before Base64 encoding (chip::Base64Encode takes uint16_t)
                if (bytes.size() > UINT16_MAX)
                {
                    icLogError(LOG_TAG,
                               "Byte array too large for argument '%s': %zu bytes exceeds maximum of %u",
                               arg.name.c_str(),
                               bytes.size(),
                               UINT16_MAX);
                    return Json::writeString(Json::StreamWriterBuilder(), tlvJson);
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

    std::unique_ptr<QuickJsScript> QuickJsScript::Create(const std::string &deviceId)
    {
        JSRuntime *runtime = JS_NewRuntime();
        if (!runtime)
        {
            icLogError(LOG_TAG, "Failed to create QuickJS runtime for device %s", deviceId.c_str());
            return nullptr;
        }

        JSContext *ctx = JS_NewContext(runtime);
        if (!ctx)
        {
            icLogError(LOG_TAG, "Failed to create QuickJS context for device %s", deviceId.c_str());
            JS_FreeRuntime(runtime);
            return nullptr;
        }

        icLogDebug(LOG_TAG, "QuickJS runtime initialized for device %s", deviceId.c_str());
        return std::unique_ptr<QuickJsScript>(new QuickJsScript(deviceId, runtime, ctx));
}

QuickJsScript::QuickJsScript(const std::string &deviceId, JSRuntime *runtime, JSContext *ctx) :
    SbmdScript(deviceId), runtime(runtime), ctx(ctx)
{
}

QuickJsScript::~QuickJsScript()
{
    std::lock_guard<std::mutex> lock(qjsMtx);
    JS_FreeContext(ctx);
    JS_FreeRuntime(runtime);
    icLogDebug(LOG_TAG, "QuickJS runtime destroyed for device %s", deviceId.c_str());
}

bool QuickJsScript::AddAttributeReadMapper(const SbmdAttribute &attributeInfo,
                                           const std::string &script)
{
    std::lock_guard<std::mutex> lock(qjsMtx);

    if (script.empty())
    {
        icLogError(LOG_TAG,
                   "Cannot add attribute read mapper: empty script for cluster 0x%X, attribute 0x%X",
                   attributeInfo.clusterId,
                   attributeInfo.attributeId);
        return false;
    }

    attributeReadScripts[attributeInfo] = script;
    icLogDebug(LOG_TAG, "Added attribute read mapper for cluster 0x%X, attribute 0x%X",
               attributeInfo.clusterId, attributeInfo.attributeId);
    return true;
}

bool QuickJsScript::AddAttributeWriteMapper(const SbmdAttribute &attributeInfo,
                                            const std::string &script)
{
    std::lock_guard<std::mutex> lock(qjsMtx);

    if (script.empty())
    {
        icLogError(LOG_TAG,
                   "Cannot add attribute write mapper: empty script for cluster 0x%X, attribute 0x%X",
                   attributeInfo.clusterId,
                   attributeInfo.attributeId);
        return false;
    }

    attributeWriteScripts[attributeInfo] = script;
    icLogDebug(LOG_TAG, "Added attribute write mapper for cluster 0x%X, attribute 0x%X",
               attributeInfo.clusterId, attributeInfo.attributeId);
    return true;
}

bool QuickJsScript::AddCommandExecuteMapper(const SbmdCommand &commandInfo,
                                            const std::string &script)
{
    std::lock_guard<std::mutex> lock(qjsMtx);

    if (script.empty())
    {
        icLogError(LOG_TAG,
                   "Cannot add command execute mapper: empty script for cluster 0x%X, command 0x%X",
                   commandInfo.clusterId,
                   commandInfo.commandId);
        return false;
    }

    commandExecuteScripts[commandInfo] = script;
    icLogDebug(LOG_TAG, "Added command execute mapper for cluster 0x%X, command 0x%X",
               commandInfo.clusterId, commandInfo.commandId);
    return true;
}

bool QuickJsScript::AddCommandExecuteResponseMapper(const SbmdCommand &commandInfo,
                                                    const std::string &script)
{
    std::lock_guard<std::mutex> lock(qjsMtx);

    if (script.empty())
    {
        icLogError(LOG_TAG,
                   "Cannot add command execute response mapper: empty script for cluster 0x%X, command 0x%X",
                   commandInfo.clusterId,
                   commandInfo.commandId);
        return false;
    }

    commandExecuteResponseScripts[commandInfo] = script;
    icLogDebug(LOG_TAG, "Added command execute response mapper for cluster 0x%X, command 0x%X",
               commandInfo.clusterId, commandInfo.commandId);
    return true;
}

std::string QuickJsScript::GenerateCommandsKey(const std::vector<SbmdCommand> &commands)
{
    // Generate a deterministic key from the commands vector
    // Uses resource identity (endpointId:resourceId) to differentiate different resources
    // that may use the same command set
    if (commands.empty())
    {
        return "";
    }

    // All commands in a write command set belong to the same resource, so use the first command's identity
    const auto &first = commands[0];
    std::string key = first.resourceEndpointId.value_or("") + ":" + first.resourceId;

    return key;
}

bool QuickJsScript::AddCommandsWriteMapper(const std::vector<SbmdCommand> &commands, const std::string &script)
{
    std::lock_guard<std::mutex> lock(qjsMtx);

    if (script.empty())
    {
        icLogError(LOG_TAG, "Cannot add write commands mapper: empty script");
        return false;
    }

    if (commands.empty())
    {
        icLogError(LOG_TAG, "Cannot add write commands mapper: no commands");
        return false;
    }

    std::string key = GenerateCommandsKey(commands);
    writeCommandsScripts[key] = script;
    icLogDebug(LOG_TAG, "Added write commands mapper for key %s", key.c_str());
    return true;
}

bool QuickJsScript::ExecuteScript(const std::string &script,
                                  const std::string &argumentName,
                                  const JSValue &argumentJson,
                                  JSValue &outJson)
{
    if (script.empty())
    {
        icLogWarn(LOG_TAG, "Empty script provided");
        return false;
    }

    // Set the JSON object as a global variable (duplicate value to maintain ownership)
    // NOTE: JS_SetPropertyStr consumes the reference to argVal on success or failure,
    // so we don't need to free argVal here
    JSValue argVal = JS_DupValue(ctx, argumentJson);
    JsValueGuard globalGuard(ctx, JS_GetGlobalObject(ctx));
    if (JS_SetPropertyStr(ctx, globalGuard.get(), argumentName.c_str(), argVal) < 0)
    {
        icLogError(
            LOG_TAG, "Failed to set argument variable '%s': %s", argumentName.c_str(), GetExceptionString(ctx).c_str());
        return false;
    }

    // Wrap the script body in a function and execute it
    std::string wrappedScript = "(function() { " + script + " })()";

    icLogDebug(LOG_TAG, "Executing script: %s", wrappedScript.c_str());

    // Execute the script
    JsValueGuard scriptResultGuard(
        ctx, JS_Eval(ctx, wrappedScript.c_str(), wrappedScript.length(), "<sbmd_script>", JS_EVAL_TYPE_GLOBAL));
    if (JS_IsException(scriptResultGuard.get()))
    {
        icLogError(LOG_TAG, "Script execution failed: %s", GetExceptionString(ctx).c_str());
        return false;
    }

    outJson = scriptResultGuard.release();
    icLogDebug(LOG_TAG, "Script executed successfully");
    return true;
}

bool QuickJsScript::ParseJsonToJSValue(const std::string &jsonString, const std::string &sourceName, JSValue &outValue)
{
    // Clear any pending exception from previous operations that might interfere
    JSValue pendingException = JS_GetException(ctx);
    if (!JS_IsNull(pendingException) && !JS_IsUndefined(pendingException))
    {
        icLogWarn(LOG_TAG, "Cleared pending exception before parsing %s JSON", sourceName.c_str());
    }
    JS_FreeValue(ctx, pendingException);

    JSValue parsed = JS_ParseJSON(ctx, jsonString.c_str(), jsonString.length(), sourceName.c_str());
    if (JS_IsException(parsed))
    {
        icLogError(LOG_TAG, "Failed to parse %s JSON: %s", sourceName.c_str(), GetExceptionString(ctx).c_str());
        JS_FreeValue(ctx, parsed);
        return false;
    }
    outValue = parsed;
    return true;
}

bool QuickJsScript::ExtractScriptOutputAsJson(JSValue &scriptResult, Json::Value &outJson)
{
    // Take ownership of scriptResult for automatic cleanup
    JsValueGuard scriptResultGuard(ctx, scriptResult);

    // Extract the "output" field from the result JSON object
    JsValueGuard outputValGuard(ctx, JS_GetPropertyStr(ctx, scriptResultGuard.get(), "output"));
    if (JS_IsUndefined(outputValGuard.get()))
    {
        icLogError(LOG_TAG, "Script result missing 'output' field");
        return false;
    }

    // Convert the output JSValue to JSON string
    JsValueGuard jsonStrGuard(ctx, JS_JSONStringify(ctx, outputValGuard.get(), JS_UNDEFINED, JS_UNDEFINED));
    if (JS_IsException(jsonStrGuard.get()))
    {
        icLogError(LOG_TAG, "Failed to stringify output value: %s", GetExceptionString(ctx).c_str());
        return false;
    }

    JsCStringGuard outValueStrGuard(ctx, JS_ToCString(ctx, jsonStrGuard.get()));
    if (!outValueStrGuard)
    {
        icLogError(LOG_TAG, "Failed to convert JSON string to C string: %s", GetExceptionString(ctx).c_str());
        return false;
    }

    icLogDebug(LOG_TAG, "Script output JSON: %s", outValueStrGuard.get());
    std::string jsonString = outValueStrGuard.get();

    // Parse the JSON string to Json::Value
    Json::CharReaderBuilder readerBuilder;
    std::istringstream jsonStream(jsonString);
    std::string parseErrors;
    if (!Json::parseFromStream(readerBuilder, jsonStream, &outJson, &parseErrors))
    {
        icLogError(LOG_TAG, "Failed to parse output JSON: %s", parseErrors.c_str());
        return false;
    }

    return true;
}

bool QuickJsScript::ExtractScriptOutputAsString(JSValue &scriptResult, std::string &outValue)
{
    // Take ownership of scriptResult for automatic cleanup
    JsValueGuard scriptResultGuard(ctx, scriptResult);

    // Extract the "output" field from the result JSON object
    JsValueGuard outputValGuard(ctx, JS_GetPropertyStr(ctx, scriptResultGuard.get(), "output"));
    if (JS_IsUndefined(outputValGuard.get()))
    {
        icLogError(LOG_TAG, "Script result missing 'output' field");
        return false;
    }

    JsCStringGuard resultStrGuard(ctx, JS_ToCString(ctx, outputValGuard.get()));
    if (!resultStrGuard)
    {
        icLogError(LOG_TAG, "Failed to convert output value to string: %s", GetExceptionString(ctx).c_str());
        return false;
    }

    outValue = resultStrGuard.get();
    icLogDebug(LOG_TAG, "Script output string: %s", outValue.c_str());
    return true;
}

bool QuickJsScript::EncodeJsonToTlv(const std::string &tlvFormattedJson,
                                    chip::Platform::ScopedMemoryBuffer<uint8_t> &buffer,
                                    size_t &encodedLength)
{
    icLogDebug(LOG_TAG, "TLV formatted JSON: %s", tlvFormattedJson.c_str());

    // Allocate buffer for TLV encoding.
    // JSON text is always larger than the equivalent TLV binary encoding due to
    // JSON overhead (quotes, colons, commas, type annotations, base64 expansion for bytes).
    size_t bufferSize = tlvFormattedJson.length();
    if (!buffer.Alloc(bufferSize))
    {
        icLogError(LOG_TAG, "Failed to allocate buffer for TLV encoding");
        return false;
    }

    // Initialize TLV writer with the buffer
    chip::TLV::TLVWriter writer;
    writer.Init(buffer.Get(), bufferSize);

    // Convert formatted JSON to TLV
    if (chip::JsonToTlv(tlvFormattedJson, writer) != CHIP_NO_ERROR)
    {
        icLogError(LOG_TAG, "Failed to convert JSON to TLV");
        return false;
    }

    encodedLength = writer.GetLengthWritten();
    icLogDebug(LOG_TAG, "Encoded %zu bytes of TLV data", encodedLength);
    return true;
}

bool QuickJsScript::SetJsVariable(const std::string &name, const std::string &value)
{
    // NOTE: JS_SetPropertyStr consumes the reference to jsValue (on success or failure),
    // so jsValue must NOT be freed manually after this call, and is not wrapped in a guard.
    JSValue jsValue = JS_NewString(ctx, value.c_str());
    JsValueGuard globalGuard(ctx, JS_GetGlobalObject(ctx));

    bool success = JS_SetPropertyStr(ctx, globalGuard.get(), name.c_str(), jsValue) >= 0;
    if (!success)
    {
        icLogError(LOG_TAG, "Failed to set JS variable '%s': %s", name.c_str(), GetExceptionString(ctx).c_str());
    }

    return success;
}

bool QuickJsScript::MapAttributeRead(const SbmdAttribute &attributeInfo,
                                     chip::TLV::TLVReader &reader,
                                     std::string &outValue)
{
    std::lock_guard<std::mutex> lock(qjsMtx);
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

    // Validate that the converted JSON has the expected "value" field
    if (!inputJson.isObject() || !inputJson.isMember("value"))
    {
        icLogError(LOG_TAG,
                   "TLV to JSON result for attribute '%s' does not contain expected 'value' field",
                   attributeInfo.name.c_str());
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
    JSValue argJsonRaw;
    if (!ParseJsonToJSValue(jsonString, "sbmdReadArgs", argJsonRaw))
    {
        return false;
    }
    JsValueGuard argJsonGuard(ctx, argJsonRaw);

    JSValue outJson;
    if (!ExecuteScript(it->second, "sbmdReadArgs", argJsonGuard.get(), outJson))
    {
        return false;
    }

    return ExtractScriptOutputAsString(outJson, outValue);
}

bool QuickJsScript::MapAttributeWrite(const SbmdAttribute &attributeInfo,
                                      const std::string &inValue,
                                      chip::Platform::ScopedMemoryBuffer<uint8_t> &buffer,
                                      size_t &encodedLength)
{
    std::lock_guard<std::mutex> lock(qjsMtx);
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
    JSValue argJsonRaw;
    if (!ParseJsonToJSValue(jsonString, "sbmdWriteArgs", argJsonRaw))
    {
        return false;
    }
    JsValueGuard argJsonGuard(ctx, argJsonRaw);

    JSValue outJson;
    if (!ExecuteScript(it->second, "sbmdWriteArgs", argJsonGuard.get(), outJson))
    {
        icLogError(LOG_TAG, "Failed to execute write mapping script for cluster 0x%X, attribute 0x%X",
                   attributeInfo.clusterId, attributeInfo.attributeId);
        return false;
    }

    Json::Value outputJson;
    if (!ExtractScriptOutputAsJson(outJson, outputJson))
    {
        return false;
    }

    // Format the value for JsonToTlv using the attribute's type information
    std::string tlvFormattedJson = formatValueForJsonToTlv(outputJson, attributeInfo.type);
    return EncodeJsonToTlv(tlvFormattedJson, buffer, encodedLength);
}

bool QuickJsScript::MapCommandExecute(const SbmdCommand &commandInfo,
                                      const std::vector<std::string> &argumentValues,
                                      chip::Platform::ScopedMemoryBuffer<uint8_t> &buffer,
                                      size_t &encodedLength)
{
    std::lock_guard<std::mutex> lock(qjsMtx);
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
    JSValue argJsonRaw;
    if (!ParseJsonToJSValue(jsonString, "sbmdCommandArgs", argJsonRaw))
    {
        return false;
    }
    JsValueGuard argJsonGuard(ctx, argJsonRaw);

    JSValue outJson;
    if (!ExecuteScript(it->second, "sbmdCommandArgs", argJsonGuard.get(), outJson))
    {
        icLogError(LOG_TAG,
                   "Failed to execute command mapping script for cluster 0x%X, command 0x%X",
                   commandInfo.clusterId,
                   commandInfo.commandId);
        return false;
    }

    Json::Value outputJson;
    if (!ExtractScriptOutputAsJson(outJson, outputJson))
    {
        return false;
    }

    // Format the command arguments for JsonToTlv using the command's arg type information
    std::string tlvFormattedJson = formatCommandArgsForJsonToTlv(outputJson, commandInfo.args);
    return EncodeJsonToTlv(tlvFormattedJson, buffer, encodedLength);
}

bool QuickJsScript::MapCommandExecuteResponse(const SbmdCommand &commandInfo,
                                              chip::TLV::TLVReader &reader,
                                              std::string &outValue)
{
    std::lock_guard<std::mutex> lock(qjsMtx);
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

    // Validate that the converted JSON has the expected "value" field
    if (!inputJson.isObject() || !inputJson.isMember("value"))
    {
        icLogError(LOG_TAG,
                   "TLV to JSON result for command response '%s' does not contain expected 'value' field",
                   commandInfo.name.c_str());
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
    JSValue argJsonRaw;
    if (!ParseJsonToJSValue(jsonString, "sbmdCommandResponseArgs", argJsonRaw))
    {
        return false;
    }
    JsValueGuard argJsonGuard(ctx, argJsonRaw);

    JSValue outJson;
    if (!ExecuteScript(it->second, "sbmdCommandResponseArgs", argJsonGuard.get(), outJson))
    {
        return false;
    }

    return ExtractScriptOutputAsString(outJson, outValue);
}

bool QuickJsScript::MapWriteCommand(const std::vector<SbmdCommand> &availableCommands,
                                    const std::string &inValue,
                                    std::string &selectedCommandName,
                                    chip::Platform::ScopedMemoryBuffer<uint8_t> &buffer,
                                    size_t &encodedLength)
{
    std::lock_guard<std::mutex> lock(qjsMtx);

    std::string key = GenerateCommandsKey(availableCommands);
    auto it = writeCommandsScripts.find(key);
    if (it == writeCommandsScripts.end())
    {
        icLogError(LOG_TAG, "No write commands mapper found for key %s", key.c_str());
        return false;
    }

    // Build the sbmdWriteArgs JSON object with available command names
    Json::Value argsJson;
    argsJson["input"] = inValue;
    argsJson["deviceUuid"] = deviceId;

    Json::Value commandsArray(Json::arrayValue);
    for (const auto &cmd : availableCommands)
    {
        commandsArray.append(cmd.name);
    }
    argsJson["commands"] = commandsArray;

    // Convert Json::Value to string for parsing in QuickJS
    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    std::string jsonString = Json::writeString(writerBuilder, argsJson);

    icLogDebug(LOG_TAG, "sbmdWriteArgs JSON for write-command: %s", jsonString.c_str());

    // Parse JSON string to JSValue
    JSValue argJsonRaw;
    if (!ParseJsonToJSValue(jsonString, "sbmdWriteArgs", argJsonRaw))
    {
        return false;
    }
    JsValueGuard argJsonGuard(ctx, argJsonRaw);

    JSValue outJson;
    if (!ExecuteScript(it->second, "sbmdWriteArgs", argJsonGuard.get(), outJson))
    {
        icLogError(LOG_TAG, "Failed to execute write-command mapping script for key %s", key.c_str());
        return false;
    }

    // Take ownership of script result for cleanup
    JsValueGuard resultGuard(ctx, outJson);

    // Validate that the script result is a non-null object before accessing properties.
    // If the script returned null, a primitive, or threw an exception, JS_GetPropertyStr
    // would produce cascading failures.
    if (JS_IsException(resultGuard.get()))
    {
        icLogError(LOG_TAG, "Script execution returned an exception: %s", GetExceptionString(ctx).c_str());
        return false;
    }

    if (JS_IsNull(resultGuard.get()) || JS_IsUndefined(resultGuard.get()))
    {
        icLogError(LOG_TAG, "Script result is null or undefined; expected an object with 'output' field");
        return false;
    }

    if (!JS_IsObject(resultGuard.get()))
    {
        icLogError(LOG_TAG, "Script result is not an object; expected an object with 'output' field");
        return false;
    }

    // Determine the selected command
    const SbmdCommand *selectedCommand = nullptr;

    if (availableCommands.size() == 1)
    {
        // Auto-select when there's only one command - no need for "command" field in script output
        selectedCommand = &availableCommands[0];
        selectedCommandName = selectedCommand->name;
        icLogDebug(LOG_TAG, "Auto-selected single command: %s", selectedCommandName.c_str());
    }
    else
    {
        // Extract the "command" field to get the selected command name
        JsValueGuard commandValGuard(ctx, JS_GetPropertyStr(ctx, resultGuard.get(), "command"));
        if (JS_IsException(commandValGuard.get()))
        {
            icLogError(LOG_TAG, "Failed to access 'command' field: %s", GetExceptionString(ctx).c_str());
            return false;
        }
        if (JS_IsUndefined(commandValGuard.get()))
        {
            icLogError(LOG_TAG, "Script result missing 'command' field (required when multiple commands available)");
            return false;
        }

        JsCStringGuard commandStrGuard(ctx, JS_ToCString(ctx, commandValGuard.get()));
        if (!commandStrGuard)
        {
            icLogError(LOG_TAG, "Failed to convert 'command' field to string: %s", GetExceptionString(ctx).c_str());
            return false;
        }
        selectedCommandName = commandStrGuard.get();

        icLogDebug(LOG_TAG, "Script selected command: %s", selectedCommandName.c_str());

        // Find the selected command in the available commands
        for (const auto &cmd : availableCommands)
        {
            if (cmd.name == selectedCommandName)
            {
                selectedCommand = &cmd;
                break;
            }
        }

        if (selectedCommand == nullptr)
        {
            icLogError(LOG_TAG, "Script selected unknown command '%s'", selectedCommandName.c_str());
            return false;
        }
    }

    // Extract the "output" field for the command arguments
    JsValueGuard outputValGuard(ctx, JS_GetPropertyStr(ctx, resultGuard.get(), "output"));
    if (JS_IsException(outputValGuard.get()))
    {
        icLogError(LOG_TAG, "Failed to access 'output' field: %s", GetExceptionString(ctx).c_str());
        return false;
    }
    if (JS_IsUndefined(outputValGuard.get()))
    {
        icLogError(LOG_TAG, "Script result missing 'output' field");
        return false;
    }

    // If output is null, the command is assumed to have no arguments - encode an empty structure.
    // Validate that the selected command actually declares no arguments to catch script/spec mismatches.
    if (JS_IsNull(outputValGuard.get()))
    {
        if (!selectedCommand->args.empty())
        {
            icLogError(LOG_TAG,
                       "Script returned 'output: null' for command '%s', but the command declares arguments",
                       selectedCommandName.c_str());
            return false;
        }
        icLogDebug(LOG_TAG, "Command %s has no arguments (output is null)", selectedCommandName.c_str());
        // Encode an empty TLV structure
        std::string emptyJson = "{}";
        return EncodeJsonToTlv(emptyJson, buffer, encodedLength);
    }

    // Convert output to JSON string for TLV encoding
    JsValueGuard jsonStrGuard(ctx, JS_JSONStringify(ctx, outputValGuard.get(), JS_UNDEFINED, JS_UNDEFINED));
    if (JS_IsException(jsonStrGuard.get()))
    {
        icLogError(LOG_TAG, "Failed to stringify output value: %s", GetExceptionString(ctx).c_str());
        return false;
    }

    JsCStringGuard outJsonStrGuard(ctx, JS_ToCString(ctx, jsonStrGuard.get()));
    if (!outJsonStrGuard)
    {
        icLogError(LOG_TAG, "Failed to convert output JSON to C string: %s", GetExceptionString(ctx).c_str());
        return false;
    }

    // Parse the JSON output
    Json::CharReaderBuilder readerBuilder;
    Json::Value outputJson;
    std::string parseErrors;
    std::istringstream jsonStream(outJsonStrGuard.get());
    if (!Json::parseFromStream(readerBuilder, jsonStream, &outputJson, &parseErrors))
    {
        icLogError(LOG_TAG, "Failed to parse output JSON: %s", parseErrors.c_str());
        return false;
    }

    // Format the command arguments for JsonToTlv using the selected command's arg type information
    std::string tlvFormattedJson = formatCommandArgsForJsonToTlv(outputJson, selectedCommand->args);
    return EncodeJsonToTlv(tlvFormattedJson, buffer, encodedLength);
}

} // namespace barton
