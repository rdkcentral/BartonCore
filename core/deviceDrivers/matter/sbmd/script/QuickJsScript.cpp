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

#include "../SbmdSpec.h"
#include "MatterJsClusterLoader.h"
#include "QuickJsRuntime.h"
#include "QuickJsScript.h"
#include "SbmdUtilsLoader.h"
#include <cstring>
#include <json/json.h>
#include <lib/core/CHIPError.h>
#include <lib/core/TLVWriter.h>
#include <lib/support/Base64.h>
#include <lib/support/Span.h>
#include <lib/support/jsontlv/JsonToTlv.h>
#include <lib/support/jsontlv/TlvJson.h>
#include <mutex>
#include <platform/CHIPDeviceLayer.h>
#include <sstream>
#include <unordered_map>
#include <vector>

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

    } // anonymous namespace

    std::unique_ptr<QuickJsScript> QuickJsScript::Create(const std::string &deviceId)
    {
        // Ensure the shared runtime is initialized
        if (!QuickJsRuntime::IsInitialized())
        {
            if (!QuickJsRuntime::Initialize())
            {
                icLogError(LOG_TAG, "Failed to initialize shared QuickJS runtime");
                return nullptr;
            }
            // Load SBMD utilities bundle into the shared context (required)
            JSContext *ctx = QuickJsRuntime::GetSharedContext();
            if (!SbmdUtilsLoader::LoadBundle(ctx))
            {
                icLogError(LOG_TAG, "Failed to load SBMD utilities bundle - scripts will not work correctly");
                return nullptr;
            }
            icLogInfo(LOG_TAG, "SBMD utilities loaded from %s", SbmdUtilsLoader::GetSource());

            // Load matter.js cluster bundle into the shared context (optional)
            if (MatterJsClusterLoader::LoadBundle(ctx))
            {
                icLogInfo(LOG_TAG, "matter.js clusters loaded from %s", MatterJsClusterLoader::GetSource());
            }
            else if (MatterJsClusterLoader::IsAvailable())
            {
                icLogWarn(LOG_TAG, "matter.js clusters available but failed to load");
            }
        }

        icLogDebug(LOG_TAG, "QuickJsScript created for device %s (using shared runtime)", deviceId.c_str());
        return std::unique_ptr<QuickJsScript>(new QuickJsScript(deviceId));
    }

QuickJsScript::QuickJsScript(const std::string &deviceId) :
    SbmdScript(deviceId)
{
}

QuickJsScript::~QuickJsScript()
{
    icLogDebug(LOG_TAG, "QuickJsScript destroyed for device %s", deviceId.c_str());
}

bool QuickJsScript::AddAttributeReadMapper(const SbmdAttribute &attributeInfo,
                                           const std::string &script)
{
    std::lock_guard<std::mutex> lock(scriptsMutex);

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

bool QuickJsScript::AddCommandExecuteResponseMapper(const SbmdCommand &commandInfo,
                                                    const std::string &script)
{
    std::lock_guard<std::mutex> lock(scriptsMutex);

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

// Requires QuickJsRuntime::GetMutex() to be held by caller.
bool QuickJsScript::ExecuteScript(const std::string &script,
                                  const std::string &argumentName,
                                  const JSValue &argumentJson,
                                  JSValue &outJson)
{
    JSContext *ctx = QuickJsRuntime::GetSharedContext();

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

// Requires QuickJsRuntime::GetMutex() to be held by caller.
bool QuickJsScript::ParseJsonToJSValue(const std::string &jsonString, const std::string &sourceName, JSValue &outValue)
{
    JSContext *ctx = QuickJsRuntime::GetSharedContext();

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

// Requires QuickJsRuntime::GetMutex() to be held by caller.
bool QuickJsScript::ExtractScriptOutputAsString(JSValue &scriptResult, std::string &outValue)
{
    JSContext *ctx = QuickJsRuntime::GetSharedContext();

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

// Requires QuickJsRuntime::GetMutex() to be held by caller.
bool QuickJsScript::SetJsVariable(const std::string &name, const std::string &value)
{
    JSContext *ctx = QuickJsRuntime::GetSharedContext();

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
    std::lock_guard<std::mutex> lock(QuickJsRuntime::GetMutex());
    JSContext *ctx = QuickJsRuntime::GetSharedContext();

    // Update stack top for cross-thread usage - QuickJS needs this when the
    // runtime is called from a different thread than where it was created
    JS_UpdateStackTop(JS_GetRuntime(ctx));

    auto it = attributeReadScripts.find(attributeInfo);
    if (it == attributeReadScripts.end())
    {
        icLogError(LOG_TAG, "No read mapper found for cluster 0x%X, attribute 0x%X",
                   attributeInfo.clusterId, attributeInfo.attributeId);
        return false;
    }

    // Copy TLV element to a buffer for base64 encoding
    uint8_t tlvBuffer[1024]; // Reasonable size for attribute values
    chip::TLV::TLVWriter writer;
    writer.Init(tlvBuffer, sizeof(tlvBuffer));

    CHIP_ERROR err = writer.CopyElement(chip::TLV::AnonymousTag(), reader);
    if (err != CHIP_NO_ERROR)
    {
        icLogError(LOG_TAG,
                   "Failed to copy TLV element for attribute '%s': %" CHIP_ERROR_FORMAT,
                   attributeInfo.name.c_str(),
                   err.Format());
        return false;
    }

    size_t tlvLength = writer.GetLengthWritten();

    // Base64 encode the TLV bytes
    size_t base64MaxLen = BASE64_ENCODED_LEN(tlvLength);
    std::vector<char> base64Buffer(base64MaxLen + 1);
    uint16_t base64Len = chip::Base64Encode(tlvBuffer, static_cast<uint16_t>(tlvLength), base64Buffer.data());
    base64Buffer[base64Len] = '\0';
    std::string tlvBase64(base64Buffer.data(), base64Len);

    // Build the sbmdReadArgs JSON object with base64 TLV
    Json::Value argsJson;
    argsJson["tlvBase64"] = tlvBase64;
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

bool QuickJsScript::MapCommandExecuteResponse(const SbmdCommand &commandInfo,
                                              chip::TLV::TLVReader &reader,
                                              std::string &outValue)
{
    std::lock_guard<std::mutex> lock(QuickJsRuntime::GetMutex());
    JSContext *ctx = QuickJsRuntime::GetSharedContext();

    // Update stack top for cross-thread usage - QuickJS needs this when the
    // runtime is called from a different thread than where it was created
    JS_UpdateStackTop(JS_GetRuntime(ctx));

    auto it = commandExecuteResponseScripts.find(commandInfo);
    if (it == commandExecuteResponseScripts.end())
    {
        icLogError(LOG_TAG, "No execute response mapper found for cluster 0x%X, command 0x%X",
                   commandInfo.clusterId, commandInfo.commandId);
        return false;
    }

    // Copy TLV element to a buffer for base64 encoding
    uint8_t tlvBuffer[1024]; // Reasonable size for command responses
    chip::TLV::TLVWriter writer;
    writer.Init(tlvBuffer, sizeof(tlvBuffer));

    CHIP_ERROR err = writer.CopyElement(chip::TLV::AnonymousTag(), reader);
    if (err != CHIP_NO_ERROR)
    {
        icLogError(LOG_TAG,
                   "Failed to copy TLV element for command response '%s': %" CHIP_ERROR_FORMAT,
                   commandInfo.name.c_str(),
                   err.Format());
        return false;
    }

    size_t tlvLength = writer.GetLengthWritten();

    // Base64 encode the TLV bytes
    size_t base64MaxLen = BASE64_ENCODED_LEN(tlvLength);
    std::vector<char> base64Buffer(base64MaxLen + 1);
    uint16_t base64Len = chip::Base64Encode(tlvBuffer, static_cast<uint16_t>(tlvLength), base64Buffer.data());
    base64Buffer[base64Len] = '\0';
    std::string tlvBase64(base64Buffer.data(), base64Len);

    // Build the sbmdCommandResponseArgs JSON object with base64 TLV
    Json::Value argsJson;
    argsJson["tlvBase64"] = tlvBase64;
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

bool QuickJsScript::AddWriteMapper(const std::string &resourceKey, const std::string &script)
{
    std::lock_guard<std::mutex> lock(scriptsMutex);

    if (script.empty())
    {
        icLogError(LOG_TAG, "Cannot add write mapper: empty script for resource %s", resourceKey.c_str());
        return false;
    }

    if (resourceKey.empty())
    {
        icLogError(LOG_TAG, "Cannot add write mapper: empty resource key");
        return false;
    }

    writeScripts[resourceKey] = script;
    icLogDebug(LOG_TAG, "Added write mapper for resource %s", resourceKey.c_str());
    return true;
}

bool QuickJsScript::AddExecuteMapper(const std::string &resourceKey,
                                     const std::string &script,
                                     const std::optional<std::string> &responseScript)
{
    std::lock_guard<std::mutex> lock(scriptsMutex);

    if (script.empty())
    {
        icLogError(LOG_TAG, "Cannot add execute mapper: empty script for resource %s", resourceKey.c_str());
        return false;
    }

    if (resourceKey.empty())
    {
        icLogError(LOG_TAG, "Cannot add execute mapper: empty resource key");
        return false;
    }

    executeScripts[resourceKey] = script;
    if (responseScript.has_value() && !responseScript.value().empty())
    {
        executeResponseScripts[resourceKey] = responseScript.value();
    }
    icLogDebug(LOG_TAG, "Added execute mapper for resource %s", resourceKey.c_str());
    return true;
}

bool QuickJsScript::MapWrite(const std::string &resourceKey,
                             const std::string &endpointId,
                             const std::string &resourceId,
                             const std::string &inValue,
                             ScriptWriteResult &result)
{
    std::lock_guard<std::mutex> lock(QuickJsRuntime::GetMutex());
    JSContext *ctx = QuickJsRuntime::GetSharedContext();

    // Update stack top for cross-thread usage
    JS_UpdateStackTop(JS_GetRuntime(ctx));

    auto it = writeScripts.find(resourceKey);
    if (it == writeScripts.end())
    {
        icLogError(LOG_TAG, "No write mapper found for resource %s", resourceKey.c_str());
        return false;
    }

    // Build the sbmdWriteArgs JSON object
    Json::Value argsJson;
    argsJson["input"] = inValue;
    argsJson["deviceUuid"] = deviceId;
    argsJson["endpointId"] = endpointId;
    argsJson["resourceId"] = resourceId;

    // Convert Json::Value to string for parsing in QuickJS
    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    std::string jsonString = Json::writeString(writerBuilder, argsJson);

    icLogDebug(LOG_TAG, "sbmdWriteArgs JSON for write: %s", jsonString.c_str());

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
        icLogError(LOG_TAG, "Failed to execute write script for resource %s", resourceKey.c_str());
        return false;
    }

    // Take ownership of script result for cleanup
    JsValueGuard resultGuard(ctx, outJson);

    // Validate that the script result is a non-null object
    if (JS_IsException(resultGuard.get()))
    {
        icLogError(LOG_TAG, "Script execution returned an exception: %s", GetExceptionString(ctx).c_str());
        return false;
    }

    if (JS_IsNull(resultGuard.get()) || JS_IsUndefined(resultGuard.get()) || !JS_IsObject(resultGuard.get()))
    {
        icLogError(LOG_TAG, "Script result is not an object; expected 'invoke' or 'write' object");
        return false;
    }

    // Check for 'invoke' operation (command invocation)
    JsValueGuard invokeGuard(ctx, JS_GetPropertyStr(ctx, resultGuard.get(), "invoke"));
    if (!JS_IsUndefined(invokeGuard.get()) && !JS_IsNull(invokeGuard.get()))
    {
        result.type = ScriptWriteResult::OperationType::Invoke;

        if (!JS_IsObject(invokeGuard.get()))
        {
            icLogError(LOG_TAG, "'invoke' field must be an object");
            return false;
        }

        // Extract clusterId (required)
        JsValueGuard clusterIdGuard(ctx, JS_GetPropertyStr(ctx, invokeGuard.get(), "clusterId"));
        if (JS_IsUndefined(clusterIdGuard.get()))
        {
            icLogError(LOG_TAG, "'invoke' missing required 'clusterId' field");
            return false;
        }
        uint32_t clusterId;
        if (JS_ToUint32(ctx, &clusterId, clusterIdGuard.get()) < 0)
        {
            icLogError(LOG_TAG, "Failed to convert 'clusterId' to number");
            return false;
        }
        result.clusterId = static_cast<chip::ClusterId>(clusterId);

        // Extract commandId (required)
        JsValueGuard commandIdGuard(ctx, JS_GetPropertyStr(ctx, invokeGuard.get(), "commandId"));
        if (JS_IsUndefined(commandIdGuard.get()))
        {
            icLogError(LOG_TAG, "'invoke' missing required 'commandId' field");
            return false;
        }
        uint32_t commandId;
        if (JS_ToUint32(ctx, &commandId, commandIdGuard.get()) < 0)
        {
            icLogError(LOG_TAG, "Failed to convert 'commandId' to number");
            return false;
        }
        result.commandId = static_cast<chip::CommandId>(commandId);

        // Extract optional endpointId
        JsValueGuard epIdGuard(ctx, JS_GetPropertyStr(ctx, invokeGuard.get(), "endpointId"));
        if (!JS_IsUndefined(epIdGuard.get()) && !JS_IsNull(epIdGuard.get()))
        {
            uint32_t epId;
            if (JS_ToUint32(ctx, &epId, epIdGuard.get()) >= 0)
            {
                result.endpointId = static_cast<chip::EndpointId>(epId);
            }
        }

        // Extract optional timedInvokeTimeoutMs
        JsValueGuard timedGuard(ctx, JS_GetPropertyStr(ctx, invokeGuard.get(), "timedInvokeTimeoutMs"));
        if (!JS_IsUndefined(timedGuard.get()) && !JS_IsNull(timedGuard.get()))
        {
            uint32_t timedMs;
            if (JS_ToUint32(ctx, &timedMs, timedGuard.get()) >= 0)
            {
                result.timedInvokeTimeoutMs = static_cast<uint16_t>(timedMs);
            }
        }

        // Extract tlvBase64 (optional for invoke - defaults to empty TLV structure)
        JsValueGuard tlvBase64Guard(ctx, JS_GetPropertyStr(ctx, invokeGuard.get(), "tlvBase64"));
        std::string base64Str;
        if (JS_IsUndefined(tlvBase64Guard.get()) || JS_IsNull(tlvBase64Guard.get()))
        {
            // No tlvBase64 provided - use empty TLV structure (end-of-container marker: 0x15)
            base64Str = "FQ==";
            icLogDebug(LOG_TAG, "No tlvBase64 provided for invoke, using empty TLV structure");
        }
        else
        {
            JsCStringGuard base64StrGuard(ctx, JS_ToCString(ctx, tlvBase64Guard.get()));
            if (!base64StrGuard)
            {
                icLogError(LOG_TAG, "Failed to convert 'tlvBase64' to string");
                return false;
            }
            base64Str = base64StrGuard.get();
        }

        // Decode base64 to TLV bytes
        size_t maxDecodedLen = BASE64_MAX_DECODED_LEN(base64Str.length());
        if (!result.tlvBuffer.Alloc(maxDecodedLen))
        {
            icLogError(LOG_TAG, "Failed to allocate buffer for TLV decoding");
            return false;
        }

        uint16_t decodedLen =
            chip::Base64Decode(base64Str.c_str(), static_cast<uint16_t>(base64Str.length()), result.tlvBuffer.Get());
        if (decodedLen == UINT16_MAX)
        {
            icLogError(LOG_TAG, "Failed to decode base64 TLV data for invoke");
            return false;
        }
        result.tlvLength = decodedLen;

        icLogDebug(LOG_TAG,
                   "write mapped to invoke: cluster=0x%X, command=0x%X, tlvLen=%zu",
                   result.clusterId,
                   result.commandId,
                   result.tlvLength);
        return true;
    }

    // Check for 'write' operation (attribute write)
    JsValueGuard writeGuard(ctx, JS_GetPropertyStr(ctx, resultGuard.get(), "write"));
    if (!JS_IsUndefined(writeGuard.get()) && !JS_IsNull(writeGuard.get()))
    {
        result.type = ScriptWriteResult::OperationType::Write;

        if (!JS_IsObject(writeGuard.get()))
        {
            icLogError(LOG_TAG, "'write' field must be an object");
            return false;
        }

        // Extract clusterId (required)
        JsValueGuard clusterIdGuard(ctx, JS_GetPropertyStr(ctx, writeGuard.get(), "clusterId"));
        if (JS_IsUndefined(clusterIdGuard.get()))
        {
            icLogError(LOG_TAG, "'write' missing required 'clusterId' field");
            return false;
        }
        uint32_t clusterId;
        if (JS_ToUint32(ctx, &clusterId, clusterIdGuard.get()) < 0)
        {
            icLogError(LOG_TAG, "Failed to convert 'clusterId' to number");
            return false;
        }
        result.clusterId = static_cast<chip::ClusterId>(clusterId);

        // Extract attributeId (required)
        JsValueGuard attrIdGuard(ctx, JS_GetPropertyStr(ctx, writeGuard.get(), "attributeId"));
        if (JS_IsUndefined(attrIdGuard.get()))
        {
            icLogError(LOG_TAG, "'write' missing required 'attributeId' field");
            return false;
        }
        uint32_t attrId;
        if (JS_ToUint32(ctx, &attrId, attrIdGuard.get()) < 0)
        {
            icLogError(LOG_TAG, "Failed to convert 'attributeId' to number");
            return false;
        }
        result.attributeId = static_cast<chip::AttributeId>(attrId);

        // Extract optional endpointId
        JsValueGuard epIdGuard(ctx, JS_GetPropertyStr(ctx, writeGuard.get(), "endpointId"));
        if (!JS_IsUndefined(epIdGuard.get()) && !JS_IsNull(epIdGuard.get()))
        {
            uint32_t epId;
            if (JS_ToUint32(ctx, &epId, epIdGuard.get()) >= 0)
            {
                result.endpointId = static_cast<chip::EndpointId>(epId);
            }
        }

        // Extract tlvBase64 (required)
        JsValueGuard tlvBase64Guard(ctx, JS_GetPropertyStr(ctx, writeGuard.get(), "tlvBase64"));
        if (JS_IsUndefined(tlvBase64Guard.get()) || JS_IsNull(tlvBase64Guard.get()))
        {
            icLogError(LOG_TAG, "'write' missing required 'tlvBase64' field");
            return false;
        }

        JsCStringGuard base64StrGuard(ctx, JS_ToCString(ctx, tlvBase64Guard.get()));
        if (!base64StrGuard)
        {
            icLogError(LOG_TAG, "Failed to convert 'tlvBase64' to string");
            return false;
        }

        // Decode base64 to TLV bytes
        std::string base64Str = base64StrGuard.get();
        size_t maxDecodedLen = BASE64_MAX_DECODED_LEN(base64Str.length());
        if (!result.tlvBuffer.Alloc(maxDecodedLen))
        {
            icLogError(LOG_TAG, "Failed to allocate buffer for TLV decoding");
            return false;
        }

        uint16_t decodedLen =
            chip::Base64Decode(base64Str.c_str(), static_cast<uint16_t>(base64Str.length()), result.tlvBuffer.Get());
        if (decodedLen == UINT16_MAX)
        {
            icLogError(LOG_TAG, "Failed to decode base64 TLV data for write");
            return false;
        }
        result.tlvLength = decodedLen;

        icLogDebug(LOG_TAG,
                   "write mapped to attribute write: cluster=0x%X, attribute=0x%X, tlvLen=%zu",
                   result.clusterId,
                   result.attributeId,
                   result.tlvLength);
        return true;
    }

    icLogError(LOG_TAG, "Script result missing 'invoke' or 'write' field");
    return false;
}

bool QuickJsScript::MapExecute(const std::string &resourceKey,
                               const std::string &endpointId,
                               const std::string &resourceId,
                               const std::string &inValue,
                               ScriptWriteResult &result)
{
    std::lock_guard<std::mutex> lock(QuickJsRuntime::GetMutex());
    JSContext *ctx = QuickJsRuntime::GetSharedContext();

    // Update stack top for cross-thread usage
    JS_UpdateStackTop(JS_GetRuntime(ctx));

    auto it = executeScripts.find(resourceKey);
    if (it == executeScripts.end())
    {
        icLogError(LOG_TAG, "No execute mapper found for resource %s", resourceKey.c_str());
        return false;
    }

    // Build the sbmdCommandArgs JSON object (similar to write but using command args semantics)
    Json::Value argsJson;
    argsJson["input"] = inValue;
    argsJson["deviceUuid"] = deviceId;
    argsJson["endpointId"] = endpointId;
    argsJson["resourceId"] = resourceId;

    // Convert Json::Value to string for parsing in QuickJS
    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    std::string jsonString = Json::writeString(writerBuilder, argsJson);

    icLogDebug(LOG_TAG, "sbmdCommandArgs JSON for execute: %s", jsonString.c_str());

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
        icLogError(LOG_TAG, "Failed to execute script for resource %s", resourceKey.c_str());
        return false;
    }

    // Take ownership of script result for cleanup
    JsValueGuard resultGuard(ctx, outJson);

    // Validate that the script result is a non-null object
    if (JS_IsException(resultGuard.get()))
    {
        icLogError(LOG_TAG, "Script execution returned an exception: %s", GetExceptionString(ctx).c_str());
        return false;
    }

    if (JS_IsNull(resultGuard.get()) || JS_IsUndefined(resultGuard.get()) || !JS_IsObject(resultGuard.get()))
    {
        icLogError(LOG_TAG, "Script result is not an object; expected 'invoke' object");
        return false;
    }

    // Check for 'invoke' operation (command invocation) - the only valid operation for execute
    JsValueGuard invokeGuard(ctx, JS_GetPropertyStr(ctx, resultGuard.get(), "invoke"));
    if (JS_IsUndefined(invokeGuard.get()) || JS_IsNull(invokeGuard.get()))
    {
        icLogError(LOG_TAG, "Execute script result missing required 'invoke' field");
        return false;
    }

    result.type = ScriptWriteResult::OperationType::Invoke;

    if (!JS_IsObject(invokeGuard.get()))
    {
        icLogError(LOG_TAG, "'invoke' field must be an object");
        return false;
    }

    // Extract clusterId (required)
    JsValueGuard clusterIdGuard(ctx, JS_GetPropertyStr(ctx, invokeGuard.get(), "clusterId"));
    if (JS_IsUndefined(clusterIdGuard.get()))
    {
        icLogError(LOG_TAG, "'invoke' missing required 'clusterId' field");
        return false;
    }
    uint32_t clusterId;
    if (JS_ToUint32(ctx, &clusterId, clusterIdGuard.get()) < 0)
    {
        icLogError(LOG_TAG, "Failed to convert 'clusterId' to number");
        return false;
    }
    result.clusterId = static_cast<chip::ClusterId>(clusterId);

    // Extract commandId (required)
    JsValueGuard commandIdGuard(ctx, JS_GetPropertyStr(ctx, invokeGuard.get(), "commandId"));
    if (JS_IsUndefined(commandIdGuard.get()))
    {
        icLogError(LOG_TAG, "'invoke' missing required 'commandId' field");
        return false;
    }
    uint32_t commandId;
    if (JS_ToUint32(ctx, &commandId, commandIdGuard.get()) < 0)
    {
        icLogError(LOG_TAG, "Failed to convert 'commandId' to number");
        return false;
    }
    result.commandId = static_cast<chip::CommandId>(commandId);

    // Extract optional endpointId
    JsValueGuard epIdGuard(ctx, JS_GetPropertyStr(ctx, invokeGuard.get(), "endpointId"));
    if (!JS_IsUndefined(epIdGuard.get()) && !JS_IsNull(epIdGuard.get()))
    {
        uint32_t epId;
        if (JS_ToUint32(ctx, &epId, epIdGuard.get()) >= 0)
        {
            result.endpointId = static_cast<chip::EndpointId>(epId);
        }
    }

    // Extract optional timedInvokeTimeoutMs
    JsValueGuard timedGuard(ctx, JS_GetPropertyStr(ctx, invokeGuard.get(), "timedInvokeTimeoutMs"));
    if (!JS_IsUndefined(timedGuard.get()) && !JS_IsNull(timedGuard.get()))
    {
        uint32_t timedMs;
        if (JS_ToUint32(ctx, &timedMs, timedGuard.get()) >= 0)
        {
            result.timedInvokeTimeoutMs = static_cast<uint16_t>(timedMs);
        }
    }

    // Extract tlvBase64 (optional for invoke - defaults to empty TLV structure)
    JsValueGuard tlvBase64Guard(ctx, JS_GetPropertyStr(ctx, invokeGuard.get(), "tlvBase64"));
    std::string base64Str;
    if (JS_IsUndefined(tlvBase64Guard.get()) || JS_IsNull(tlvBase64Guard.get()))
    {
        // No tlvBase64 provided - use empty TLV structure (end-of-container marker: 0x15)
        base64Str = "FQ==";
        icLogDebug(LOG_TAG, "No tlvBase64 provided for invoke, using empty TLV structure");
    }
    else
    {
        JsCStringGuard base64StrGuard(ctx, JS_ToCString(ctx, tlvBase64Guard.get()));
        if (!base64StrGuard)
        {
            icLogError(LOG_TAG, "Failed to convert 'tlvBase64' to string");
            return false;
        }
        base64Str = base64StrGuard.get();
    }

    // Decode base64 to TLV bytes
    size_t maxDecodedLen = BASE64_MAX_DECODED_LEN(base64Str.length());
    if (!result.tlvBuffer.Alloc(maxDecodedLen))
    {
        icLogError(LOG_TAG, "Failed to allocate buffer for TLV decoding");
        return false;
    }

    uint16_t decodedLen =
        chip::Base64Decode(base64Str.c_str(), static_cast<uint16_t>(base64Str.length()), result.tlvBuffer.Get());
    if (decodedLen == UINT16_MAX)
    {
        icLogError(LOG_TAG, "Failed to decode base64 TLV data for invoke");
        return false;
    }
    result.tlvLength = decodedLen;

    icLogDebug(LOG_TAG,
               "execute mapped to invoke: cluster=0x%X, command=0x%X, tlvLen=%zu",
               result.clusterId,
               result.commandId,
               result.tlvLength);
    return true;
}

} // namespace barton
