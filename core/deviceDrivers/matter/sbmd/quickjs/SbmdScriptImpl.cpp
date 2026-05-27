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

#define LOG_TAG "SbmdScriptImpl"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "SbmdScriptImpl.h"
#include "../SbmdSpec.h"
#include "../ScriptResult.h"
#include "QuickJsRuntime.h"
#include "SbmdUtilsLoader.h"
#include "json/writer.h"
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

        // Extract invoke sub-object fields from a JSValue into a Json::Value.
        Json::Value ExtractInvokeSubObject(JSContext *ctx, JSValue invokeObj)
        {
            Json::Value jv(Json::objectValue);

            auto getUint = [ctx, invokeObj](const char *key) -> std::optional<uint32_t> {
                JsValueGuard fg(ctx, JS_GetPropertyStr(ctx, invokeObj, key));

                if (JS_IsUndefined(fg.get()) || JS_IsNull(fg.get()))
                {
                    return std::nullopt;
                }

                uint32_t v = 0;

                if (JS_ToUint32(ctx, &v, fg.get()) < 0)
                {
                    return std::nullopt;
                }

                return v;
            };

            auto getStr = [ctx, invokeObj](const char *key) -> std::optional<std::string> {
                JsValueGuard fg(ctx, JS_GetPropertyStr(ctx, invokeObj, key));

                if (JS_IsUndefined(fg.get()) || JS_IsNull(fg.get()))
                {
                    return std::nullopt;
                }

                JsCStringGuard sg(ctx, JS_ToCString(ctx, fg.get()));

                if (!sg)
                {
                    return std::nullopt;
                }

                return std::string(sg.get());
            };

            if (auto v = getUint("clusterId"))
            {
                jv["clusterId"] = *v;
            }

            if (auto v = getUint("commandId"))
            {
                jv["commandId"] = *v;
            }

            if (auto v = getUint("endpointId"))
            {
                jv["endpointId"] = *v;
            }

            if (auto v = getUint("timedInvokeTimeoutMs"))
            {
                jv["timedInvokeTimeoutMs"] = *v;
            }

            if (auto v = getStr("tlvBase64"))
            {
                jv["tlvBase64"] = *v;
            }

            return jv;
        }

        // Extract write sub-object fields from a JSValue into a Json::Value.
        Json::Value ExtractWriteSubObject(JSContext *ctx, JSValue writeObj)
        {
            Json::Value jv(Json::objectValue);

            auto getUint = [ctx, writeObj](const char *key) -> std::optional<uint32_t> {
                JsValueGuard fg(ctx, JS_GetPropertyStr(ctx, writeObj, key));

                if (JS_IsUndefined(fg.get()) || JS_IsNull(fg.get()))
                {
                    return std::nullopt;
                }

                uint32_t v = 0;

                if (JS_ToUint32(ctx, &v, fg.get()) < 0)
                {
                    return std::nullopt;
                }

                return v;
            };

            auto getStr = [ctx, writeObj](const char *key) -> std::optional<std::string> {
                JsValueGuard fg(ctx, JS_GetPropertyStr(ctx, writeObj, key));

                if (JS_IsUndefined(fg.get()) || JS_IsNull(fg.get()))
                {
                    return std::nullopt;
                }

                JsCStringGuard sg(ctx, JS_ToCString(ctx, fg.get()));

                if (!sg)
                {
                    return std::nullopt;
                }

                return std::string(sg.get());
            };

            if (auto v = getUint("clusterId"))
            {
                jv["clusterId"] = *v;
            }

            if (auto v = getUint("attributeId"))
            {
                jv["attributeId"] = *v;
            }

            if (auto v = getUint("endpointId"))
            {
                jv["endpointId"] = *v;
            }

            if (auto v = getStr("tlvBase64"))
            {
                jv["tlvBase64"] = *v;
            }

            return jv;
        }

        // Convert the script output JSValue to a ScriptResult.
        // Takes ownership of outJson. Caller must have validated it is a non-null JS object.
        ScriptResult ScriptResultFromJsValue(JSContext *ctx, JSValue outJson)
        {
            JsValueGuard outJsonGuard(ctx, outJson);
            Json::Value jv(Json::objectValue);

            // Extract "error" key
            {
                JsValueGuard eg(ctx, JS_GetPropertyStr(ctx, outJsonGuard.get(), "error"));

                if (!JS_IsUndefined(eg.get()) && !JS_IsNull(eg.get()))
                {
                    JsCStringGuard sg(ctx, JS_ToCString(ctx, eg.get()));

                    if (sg)
                    {
                        jv["error"] = std::string(sg.get());
                    }
                }
            }

            // Extract "value" key (JS_ToCString auto-coerces booleans and numbers to strings)
            {
                JsValueGuard vg(ctx, JS_GetPropertyStr(ctx, outJsonGuard.get(), "value"));

                if (!JS_IsUndefined(vg.get()) && !JS_IsNull(vg.get()))
                {
                    JsCStringGuard sg(ctx, JS_ToCString(ctx, vg.get()));

                    if (sg)
                    {
                        jv["value"] = std::string(sg.get());
                    }
                }
            }

            // Extract "invoke" sub-object
            {
                JsValueGuard ig(ctx, JS_GetPropertyStr(ctx, outJsonGuard.get(), "invoke"));

                if (!JS_IsUndefined(ig.get()) && !JS_IsNull(ig.get()))
                {
                    jv["invoke"] = ExtractInvokeSubObject(ctx, ig.get());
                }
            }

            // Extract "write" sub-object
            {
                JsValueGuard wg(ctx, JS_GetPropertyStr(ctx, outJsonGuard.get(), "write"));

                if (!JS_IsUndefined(wg.get()) && !JS_IsNull(wg.get()))
                {
                    jv["write"] = ExtractWriteSubObject(ctx, wg.get());
                }
            }

            return ScriptResult::FromJsonValue(jv);
        }

    } // anonymous namespace

    std::unique_ptr<SbmdScriptImpl> SbmdScriptImpl::Create(const std::string &deviceId)
    {
        // Ensure the shared runtime is initialized
        if (!QuickJsRuntime::IsInitialized())
        {
            if (!QuickJsRuntime::Initialize())
            {
                icError("Failed to initialize shared QuickJS runtime");
                return nullptr;
            }
            // Load SBMD utilities bundle into the shared context (required)
            JSContext *ctx = QuickJsRuntime::GetSharedContext();
            if (!SbmdUtilsLoader::LoadBundle(ctx))
            {
                icError("Failed to load SBMD utilities bundle - scripts will not work correctly");
                return nullptr;
            }
            icInfo("SBMD utilities loaded from %s", SbmdUtilsLoader::GetSource());
        }

        icDebug("SbmdScriptImpl created for device %s (using shared runtime)", deviceId.c_str());
        return std::unique_ptr<SbmdScriptImpl>(new SbmdScriptImpl(deviceId));
    }

SbmdScriptImpl::SbmdScriptImpl(const std::string &deviceId) :
    SbmdScript(deviceId)
{
}

SbmdScriptImpl::~SbmdScriptImpl()
{
    icDebug("SbmdScriptImpl destroyed for device %s", deviceId.c_str());
}

void SbmdScriptImpl::SetClusterFeatureMaps(const std::map<uint32_t, uint32_t> &maps)
{
    std::lock_guard<std::mutex> lock(scriptsMutex);
    clusterFeatureMaps = maps;
    icDebug("Set %zu cluster feature maps for device %s", maps.size(), deviceId.c_str());
}

Json::Value SbmdScriptImpl::BuildBaseArgsJson(const std::optional<std::string> &endpointId,
                                             std::optional<uint32_t> clusterId,
                                             const std::optional<std::string> &resourceId,
                                             const std::optional<std::string> &input) const
{
    Json::Value argsJson;
    argsJson["deviceUuid"] = deviceId;

    // Add cluster feature maps so scripts can check cluster capabilities
    Json::Value featureMapsJson(Json::objectValue);
    for (const auto &pair : clusterFeatureMaps)
    {
        // Use string key for JSON compatibility (JavaScript object keys are strings)
        featureMapsJson[std::to_string(pair.first)] = pair.second;
    }
    argsJson["clusterFeatureMaps"] = featureMapsJson;

    // Add optional common fields
    if (endpointId.has_value())
    {
        argsJson["endpointId"] = endpointId.value();
    }
    if (clusterId.has_value())
    {
        argsJson["clusterId"] = clusterId.value();
    }
    if (resourceId.has_value())
    {
        argsJson["resourceId"] = resourceId.value();
    }
    if (input.has_value())
    {
        argsJson["input"] = input.value();
    }

    return argsJson;
}

bool SbmdScriptImpl::AddAttributeReadMapper(const SbmdAttribute &attributeInfo,
                                           const std::string &script)
{
    std::lock_guard<std::mutex> lock(scriptsMutex);

    if (script.empty())
    {
        icError("Cannot add attribute read mapper: empty script for cluster 0x%X, attribute 0x%X",
                attributeInfo.clusterId,
                attributeInfo.attributeId);
        return false;
    }

    attributeReadScripts[attributeInfo] = script;
    icDebug("Added attribute read mapper for cluster 0x%X, attribute 0x%X",
            attributeInfo.clusterId,
            attributeInfo.attributeId);
    return true;
}

bool SbmdScriptImpl::AddCommandExecuteResponseMapper(const SbmdCommand &commandInfo,
                                                    const std::string &script)
{
    std::lock_guard<std::mutex> lock(scriptsMutex);

    if (script.empty())
    {
        icError("Cannot add command execute response mapper: empty script for cluster 0x%X, command 0x%X",
                commandInfo.clusterId,
                commandInfo.commandId);
        return false;
    }

    commandExecuteResponseScripts[commandInfo] = script;
    icDebug("Added command execute response mapper for cluster 0x%X, command 0x%X",
            commandInfo.clusterId,
            commandInfo.commandId);
    return true;
}

// Requires QuickJsRuntime::GetMutex() to be held by caller.
bool SbmdScriptImpl::ExecuteScript(const std::string &script,
                                  const std::string &argumentName,
                                  const JSValue &argumentJson,
                                  JSValue &outJson)
{
    JSContext *ctx = QuickJsRuntime::GetSharedContext();

    if (script.empty())
    {
        icWarn("Empty script provided");
        return false;
    }

    // Set the JSON object as a global variable (duplicate value to maintain ownership)
    // NOTE: JS_SetPropertyStr consumes the reference to argVal on success or failure,
    // so we don't need to free argVal here
    JSValue argVal = JS_DupValue(ctx, argumentJson);
    JsValueGuard globalGuard(ctx, JS_GetGlobalObject(ctx));
    if (JS_SetPropertyStr(ctx, globalGuard.get(), argumentName.c_str(), argVal) < 0)
    {
        icError("Failed to set argument variable '%s': %s", argumentName.c_str(), GetExceptionString(ctx).c_str());
        return false;
    }

    // Wrap the script body in a function and execute it
    std::string wrappedScript = "(function() { " + script + " })()";

    icDebug("Executing script: %s", wrappedScript.c_str());

    // Execute the script
    JsValueGuard scriptResultGuard(
        ctx, JS_Eval(ctx, wrappedScript.c_str(), wrappedScript.length(), "<sbmd_script>", JS_EVAL_TYPE_GLOBAL));
    if (JS_IsException(scriptResultGuard.get()))
    {
        icError("Script execution failed: %s", GetExceptionString(ctx).c_str());
        return false;
    }

    outJson = scriptResultGuard.release();
    icDebug("Script executed successfully");
    return true;
}

// Requires QuickJsRuntime::GetMutex() to be held by caller.
bool SbmdScriptImpl::ParseJsonToJSValue(const std::string &jsonString, const std::string &sourceName, JSValue &outValue)
{
    JSContext *ctx = QuickJsRuntime::GetSharedContext();

    // Check for pending exception from previous operations - this indicates a bug
    std::string exMsg;
    if (QuickJsRuntime::CheckAndClearPendingException(ctx, &exMsg))
    {
        icError(
            "Found unhandled exception before parsing %s JSON: %s - this is a bug", sourceName.c_str(), exMsg.c_str());
        return false;
    }

    JSValue parsed = JS_ParseJSON(ctx, jsonString.c_str(), jsonString.length(), sourceName.c_str());
    if (JS_IsException(parsed))
    {
        icError("Failed to parse %s JSON: %s", sourceName.c_str(), GetExceptionString(ctx).c_str());
        JS_FreeValue(ctx, parsed);
        return false;
    }
    outValue = parsed;
    return true;
}

// Requires QuickJsRuntime::GetMutex() to be held by caller.
// Requires QuickJsRuntime::GetMutex() to be held by caller.
bool SbmdScriptImpl::SetJsVariable(const std::string &name, const std::string &value)
{
    JSContext *ctx = QuickJsRuntime::GetSharedContext();

    // NOTE: JS_SetPropertyStr consumes the reference to jsValue (on success or failure),
    // so jsValue must NOT be freed manually after this call, and is not wrapped in a guard.
    JSValue jsValue = JS_NewString(ctx, value.c_str());
    JsValueGuard globalGuard(ctx, JS_GetGlobalObject(ctx));

    bool success = JS_SetPropertyStr(ctx, globalGuard.get(), name.c_str(), jsValue) >= 0;
    if (!success)
    {
        icError("Failed to set JS variable '%s': %s", name.c_str(), GetExceptionString(ctx).c_str());
    }

    return success;
}

ScriptResult SbmdScriptImpl::MapAttributeRead(const SbmdAttribute &attributeInfo, chip::TLV::TLVReader &reader)
{
    std::lock_guard<std::mutex> lock(QuickJsRuntime::GetMutex());
    JSContext *ctx = QuickJsRuntime::GetSharedContext();

    // Update stack top for cross-thread usage - QuickJS needs this when the
    // runtime is called from a different thread than where it was created
    JS_UpdateStackTop(JS_GetRuntime(ctx));

    auto it = attributeReadScripts.find(attributeInfo);
    if (it == attributeReadScripts.end())
    {
        icError("No read mapper found for cluster 0x%X, attribute 0x%X",
                attributeInfo.clusterId,
                attributeInfo.attributeId);
        return ScriptResult::MakeError("No read mapper found for attribute");
    }

    // Copy TLV element to a buffer for base64 encoding
    uint8_t tlvBuffer[1024]; // Reasonable size for attribute values
    chip::TLV::TLVWriter writer;
    writer.Init(tlvBuffer, sizeof(tlvBuffer));

    CHIP_ERROR err = writer.CopyElement(chip::TLV::AnonymousTag(), reader);
    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to copy TLV element for attribute '%s': %" CHIP_ERROR_FORMAT,
                attributeInfo.name.c_str(),
                err.Format());
        return ScriptResult::MakeError("Failed to copy TLV data for attribute " + attributeInfo.name);
    }

    size_t tlvLength = writer.GetLengthWritten();

    if (tlvLength > UINT16_MAX)
    {
        icError("Attribute TLV data too large for base64 encoding: %zu bytes", tlvLength);
        return ScriptResult::MakeError("Attribute TLV data too large");
    }

    // Base64 encode the TLV bytes
    size_t base64MaxLen = BASE64_ENCODED_LEN(tlvLength);
    std::vector<char> base64Buffer(base64MaxLen + 1);
    uint16_t base64Len = chip::Base64Encode(tlvBuffer, static_cast<uint16_t>(tlvLength), base64Buffer.data());
    base64Buffer[base64Len] = '\0';
    std::string tlvBase64(base64Buffer.data(), base64Len);

    // Build the sbmdReadArgs JSON object with base64 TLV
    Json::Value argsJson = BuildBaseArgsJson(attributeInfo.resourceEndpointId.value_or(""), attributeInfo.clusterId);
    argsJson["tlvBase64"] = tlvBase64;
    argsJson["attributeId"] = attributeInfo.attributeId;
    argsJson["attributeName"] = attributeInfo.name;
    argsJson["attributeType"] = attributeInfo.type;

    // Convert Json::Value to string for parsing in QuickJS
    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    std::string jsonString = Json::writeString(writerBuilder, argsJson);

    icDebug("sbmdReadArgs JSON: %s", jsonString.c_str());

    // Parse JSON string to JSValue
    JSValue argJsonRaw;
    if (!ParseJsonToJSValue(jsonString, "sbmdReadArgs", argJsonRaw))
    {
        return ScriptResult::MakeError("Failed to parse input args JSON for attribute " + attributeInfo.name);
    }
    JsValueGuard argJsonGuard(ctx, argJsonRaw);

    JSValue outJson;
    if (!ExecuteScript(it->second, "sbmdReadArgs", argJsonGuard.get(), outJson))
    {
        return ScriptResult::MakeError("Script execution failed for attribute " + attributeInfo.name);
    }

    if (!JS_IsObject(outJson) || JS_IsNull(outJson) || JS_IsUndefined(outJson))
    {
        icError("Attribute mapper script returned a non-object for cluster 0x%X, attribute 0x%X",
                attributeInfo.clusterId,
                attributeInfo.attributeId);
        JS_FreeValue(ctx, outJson);
        return ScriptResult::MakeError("Attribute mapper script returned a non-object");
    }

    return ScriptResultFromJsValue(ctx, outJson);
}

ScriptResult SbmdScriptImpl::MapCommandExecuteResponse(const SbmdCommand &commandInfo, chip::TLV::TLVReader &reader)
{
    std::lock_guard<std::mutex> lock(QuickJsRuntime::GetMutex());
    JSContext *ctx = QuickJsRuntime::GetSharedContext();

    // Update stack top for cross-thread usage - QuickJS needs this when the
    // runtime is called from a different thread than where it was created
    JS_UpdateStackTop(JS_GetRuntime(ctx));

    auto it = commandExecuteResponseScripts.find(commandInfo);
    if (it == commandExecuteResponseScripts.end())
    {
        icError("No execute response mapper found for cluster 0x%X, command 0x%X",
                commandInfo.clusterId,
                commandInfo.commandId);
        return ScriptResult::MakeError("No execute response mapper found for command");
    }

    // Copy TLV element to a buffer for base64 encoding
    uint8_t tlvBuffer[1024]; // Reasonable size for command responses
    chip::TLV::TLVWriter writer;
    writer.Init(tlvBuffer, sizeof(tlvBuffer));

    CHIP_ERROR err = writer.CopyElement(chip::TLV::AnonymousTag(), reader);
    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to copy TLV element for command response '%s': %" CHIP_ERROR_FORMAT,
                commandInfo.name.c_str(),
                err.Format());
        return ScriptResult::MakeError("Failed to copy TLV data for command response " + commandInfo.name);
    }

    size_t tlvLength = writer.GetLengthWritten();

    if (tlvLength > UINT16_MAX)
    {
        icError("Command response TLV data too large for base64 encoding: %zu bytes", tlvLength);
        return ScriptResult::MakeError("Command response TLV data too large");
    }

    // Base64 encode the TLV bytes
    size_t base64MaxLen = BASE64_ENCODED_LEN(tlvLength);
    std::vector<char> base64Buffer(base64MaxLen + 1);
    uint16_t base64Len = chip::Base64Encode(tlvBuffer, static_cast<uint16_t>(tlvLength), base64Buffer.data());
    base64Buffer[base64Len] = '\0';
    std::string tlvBase64(base64Buffer.data(), base64Len);

    // Build the sbmdCommandResponseArgs JSON object with base64 TLV
    Json::Value argsJson = BuildBaseArgsJson(commandInfo.resourceEndpointId.value_or(""), commandInfo.clusterId);
    argsJson["tlvBase64"] = tlvBase64;
    argsJson["commandId"] = commandInfo.commandId;
    argsJson["commandName"] = commandInfo.name;

    // Convert Json::Value to string for parsing in QuickJS
    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    std::string jsonString = Json::writeString(writerBuilder, argsJson);

    icDebug("sbmdCommandResponseArgs JSON: %s", jsonString.c_str());

    // Parse JSON string to JSValue
    JSValue argJsonRaw;
    if (!ParseJsonToJSValue(jsonString, "sbmdCommandResponseArgs", argJsonRaw))
    {
        return ScriptResult::MakeError("Failed to parse input args JSON for command response " + commandInfo.name);
    }
    JsValueGuard argJsonGuard(ctx, argJsonRaw);

    JSValue outJson;
    if (!ExecuteScript(it->second, "sbmdCommandResponseArgs", argJsonGuard.get(), outJson))
    {
        return ScriptResult::MakeError("Script execution failed for command response " + commandInfo.name);
    }

    if (!JS_IsObject(outJson) || JS_IsNull(outJson) || JS_IsUndefined(outJson))
    {
        icError("Command response mapper script returned a non-object for cluster 0x%X, command 0x%X",
                commandInfo.clusterId,
                commandInfo.commandId);
        JS_FreeValue(ctx, outJson);
        return ScriptResult::MakeError("Command response mapper script returned a non-object");
    }

    return ScriptResultFromJsValue(ctx, outJson);
}

bool SbmdScriptImpl::AddWriteMapper(const std::string &resourceKey, const std::string &script)
{
    std::lock_guard<std::mutex> lock(scriptsMutex);

    if (script.empty())
    {
        icError("Cannot add write mapper: empty script for resource %s", resourceKey.c_str());
        return false;
    }

    if (resourceKey.empty())
    {
        icError("Cannot add write mapper: empty resource key");
        return false;
    }

    writeScripts[resourceKey] = script;
    icDebug("Added write mapper for resource %s", resourceKey.c_str());
    return true;
}

bool SbmdScriptImpl::AddExecuteMapper(const std::string &resourceKey,
                                     const std::string &script,
                                     const std::optional<std::string> &responseScript)
{
    std::lock_guard<std::mutex> lock(scriptsMutex);

    if (script.empty())
    {
        icError("Cannot add execute mapper: empty script for resource %s", resourceKey.c_str());
        return false;
    }

    if (resourceKey.empty())
    {
        icError("Cannot add execute mapper: empty resource key");
        return false;
    }

    executeScripts[resourceKey] = script;
    if (responseScript.has_value() && !responseScript.value().empty())
    {
        executeResponseScripts[resourceKey] = responseScript.value();
    }
    icDebug("Added execute mapper for resource %s", resourceKey.c_str());
    return true;
}

ScriptResult SbmdScriptImpl::MapWrite(const std::string &resourceKey,
                                      const std::string &endpointId,
                                      const std::string &resourceId,
                                      const std::string &inValue)
{
    std::lock_guard<std::mutex> lock(QuickJsRuntime::GetMutex());
    JSContext *ctx = QuickJsRuntime::GetSharedContext();

    // Update stack top for cross-thread usage
    JS_UpdateStackTop(JS_GetRuntime(ctx));

    auto it = writeScripts.find(resourceKey);

    if (it == writeScripts.end())
    {
        icError("No write mapper found for resource %s", resourceKey.c_str());
        return ScriptResult::MakeError("No write mapper found for resource " + resourceKey);
    }

    // Build the sbmdWriteArgs JSON object
    Json::Value argsJson = BuildBaseArgsJson(endpointId, std::nullopt, resourceId, inValue);

    // Convert Json::Value to string for parsing in QuickJS
    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    std::string jsonString = Json::writeString(writerBuilder, argsJson);

    icDebug("sbmdWriteArgs JSON for write: %s", jsonString.c_str());

    // Parse JSON string to JSValue
    JSValue argJsonRaw;

    if (!ParseJsonToJSValue(jsonString, "sbmdWriteArgs", argJsonRaw))
    {
        return ScriptResult::MakeError("Failed to parse input args JSON for write " + resourceKey);
    }

    JsValueGuard argJsonGuard(ctx, argJsonRaw);

    JSValue outJson;

    if (!ExecuteScript(it->second, "sbmdWriteArgs", argJsonGuard.get(), outJson))
    {
        return ScriptResult::MakeError("Script execution failed for write " + resourceKey);
    }

    if (!JS_IsObject(outJson) || JS_IsNull(outJson) || JS_IsUndefined(outJson))
    {
        icError("Write mapper script returned a non-object for resource %s", resourceKey.c_str());
        JS_FreeValue(ctx, outJson);
        return ScriptResult::MakeError("Write mapper script returned a non-object");
    }

    return ScriptResultFromJsValue(ctx, outJson);
}

ScriptResult SbmdScriptImpl::MapExecute(const std::string &resourceKey,
                                        const std::string &endpointId,
                                        const std::string &resourceId,
                                        const std::string &inValue)
{
    std::lock_guard<std::mutex> lock(QuickJsRuntime::GetMutex());
    JSContext *ctx = QuickJsRuntime::GetSharedContext();

    // Update stack top for cross-thread usage
    JS_UpdateStackTop(JS_GetRuntime(ctx));

    auto it = executeScripts.find(resourceKey);

    if (it == executeScripts.end())
    {
        icError("No execute mapper found for resource %s", resourceKey.c_str());
        return ScriptResult::MakeError("No execute mapper found for resource " + resourceKey);
    }

    // Build the sbmdCommandArgs JSON object
    Json::Value argsJson = BuildBaseArgsJson(endpointId, std::nullopt, resourceId, inValue);

    // Convert Json::Value to string for parsing in QuickJS
    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    std::string jsonString = Json::writeString(writerBuilder, argsJson);

    icDebug("sbmdCommandArgs JSON for execute: %s", jsonString.c_str());

    // Parse JSON string to JSValue
    JSValue argJsonRaw;

    if (!ParseJsonToJSValue(jsonString, "sbmdCommandArgs", argJsonRaw))
    {
        return ScriptResult::MakeError("Failed to parse input args JSON for execute " + resourceKey);
    }

    JsValueGuard argJsonGuard(ctx, argJsonRaw);

    JSValue outJson;

    if (!ExecuteScript(it->second, "sbmdCommandArgs", argJsonGuard.get(), outJson))
    {
        return ScriptResult::MakeError("Script execution failed for execute " + resourceKey);
    }

    if (!JS_IsObject(outJson) || JS_IsNull(outJson) || JS_IsUndefined(outJson))
    {
        icError("Execute mapper script returned a non-object for resource %s", resourceKey.c_str());
        JS_FreeValue(ctx, outJson);
        return ScriptResult::MakeError("Execute mapper script returned a non-object");
    }

    return ScriptResultFromJsValue(ctx, outJson);
}

bool SbmdScriptImpl::AddEventMapper(const SbmdEvent &eventInfo, const std::string &script)
{
    std::lock_guard<std::mutex> lock(scriptsMutex);

    if (script.empty())
    {
        icError("Cannot add event mapper: empty script for cluster 0x%X, event 0x%X",
                eventInfo.clusterId,
                eventInfo.eventId);
        return false;
    }

    eventScripts[eventInfo] = script;
    icDebug("Added event mapper for cluster 0x%X, event 0x%X",
            eventInfo.clusterId,
            eventInfo.eventId);
    return true;
}

ScriptResult SbmdScriptImpl::MapEvent(const SbmdEvent &eventInfo, chip::TLV::TLVReader &reader)
{
    std::lock_guard<std::mutex> lock(QuickJsRuntime::GetMutex());
    JSContext *ctx = QuickJsRuntime::GetSharedContext();

    // Update stack top for cross-thread usage
    JS_UpdateStackTop(JS_GetRuntime(ctx));

    auto it = eventScripts.find(eventInfo);

    if (it == eventScripts.end())
    {
        icError("No event mapper found for cluster 0x%X, event 0x%X",
                eventInfo.clusterId,
                eventInfo.eventId);
        return ScriptResult::MakeError("No event mapper found");
    }

    // Build the sbmdEventArgs JSON object
    Json::Value argsJson = BuildBaseArgsJson(eventInfo.resourceEndpointId.value_or(""), eventInfo.clusterId);
    argsJson["eventId"] = eventInfo.eventId;
    argsJson["eventName"] = eventInfo.name;

    // Convert TLV to base64 for script to use
    chip::TLV::TLVReader readerCopy;
    readerCopy.Init(reader);

    chip::TLV::TLVReader sizingReader;
    sizingReader.Init(reader);
    uint32_t tlvLen = sizingReader.GetRemainingLength();

    if (tlvLen == 0)
    {
        tlvLen = 256;
    }

    chip::Platform::ScopedMemoryBuffer<uint8_t> tlvBuffer;

    if (!tlvBuffer.Calloc(tlvLen))
    {
        icError("Failed to allocate TLV buffer for event 0x%X", eventInfo.eventId);
        return ScriptResult::MakeError("Failed to allocate TLV buffer for event");
    }

    chip::TLV::TLVWriter writer;
    writer.Init(tlvBuffer.Get(), tlvLen);

    CHIP_ERROR err = writer.CopyElement(chip::TLV::AnonymousTag(), readerCopy);

    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to copy event TLV data: %s", chip::ErrorStr(err));
        return ScriptResult::MakeError("Failed to copy event TLV data");
    }

    uint32_t encodedLen = writer.GetLengthWritten();

    if (encodedLen > UINT16_MAX)
    {
        icError("Event TLV data too large for base64 encoding: %u bytes", encodedLen);
        return ScriptResult::MakeError("Event TLV data too large");
    }

    size_t base64Size = ((encodedLen + 2) / 3) * 4 + 1;
    std::unique_ptr<char[]> base64Buffer(new char[base64Size]);
    uint16_t base64Len = chip::Base64Encode(tlvBuffer.Get(), static_cast<uint16_t>(encodedLen), base64Buffer.get());
    base64Buffer[base64Len] = '\0';
    argsJson["tlvBase64"] = std::string(base64Buffer.get());

    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    std::string jsonString = Json::writeString(writerBuilder, argsJson);

    icDebug("sbmdEventArgs JSON: %s", jsonString.c_str());

    JSValue argJsonRaw;

    if (!ParseJsonToJSValue(jsonString, "sbmdEventArgs", argJsonRaw))
    {
        return ScriptResult::MakeError("Failed to parse input args JSON for event");
    }

    JsValueGuard argJsonGuard(ctx, argJsonRaw);

    JSValue outJson;

    if (!ExecuteScript(it->second, "sbmdEventArgs", argJsonGuard.get(), outJson))
    {
        icError("Failed to execute event mapper script for cluster 0x%X, event 0x%X",
                eventInfo.clusterId,
                eventInfo.eventId);
        return ScriptResult::MakeError("Script execution failed for event");
    }

    if (JS_IsUndefined(outJson) || JS_IsNull(outJson) || !JS_IsObject(outJson))
    {
        icError("Event mapper script returned a non-object value for cluster 0x%X, event 0x%X",
                eventInfo.clusterId,
                eventInfo.eventId);
        JS_FreeValue(ctx, outJson);
        return ScriptResult::MakeError("Event mapper script returned a non-object");
    }

    return ScriptResultFromJsValue(ctx, outJson);
}

} // namespace barton
