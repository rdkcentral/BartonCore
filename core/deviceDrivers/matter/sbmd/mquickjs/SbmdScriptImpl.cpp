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
#include "MQuickJsRuntime.h"
#include "SbmdUtilsLoader.h"
#include <cstring>
#include <json/json.h>

#include <lib/core/CHIPError.h>
#include <lib/core/TLVWriter.h>
#include <lib/support/Base64.h>
#include <lib/support/Span.h>
#include <mutex>
#include <platform/CHIPDeviceLayer.h>
#include <vector>

extern "C" {
#include <icLog/logging.h>
#include <mquickjs/mquickjs.h>
}

namespace barton
{

    namespace
    {
        /**
         * Extracts the current mquickjs exception as a string.
         * Clears the exception from the context.
         * @param ctx The mquickjs context
         * @return The exception message, or "unknown error" if unavailable
         */
        std::string GetExceptionString(JSContext *ctx)
        {
            // JS_GetException clears the exception from the context's exception slot.
            // Register it on the GC root stack so it stays alive across any internal
            // allocations (e.g. property lookups) that could trigger a GC pass.
            JSGCRef ex_ref;
            JSValue ex = JS_GetException(ctx);
            JS_PUSH_VALUE(ctx, ex);

            std::string result;

            // First try direct string conversion (works for string exceptions)
            {
                JSCStringBuf buf;
                const char *str = JS_ToCString(ctx, ex, &buf);
                if (str)
                {
                    result = str;
                }
            }

            // If that fails, try to get the "message" property (for Error objects)
            if (result.empty() && JS_IsPtr(ex))
            {
                JSGCRef msgVal_ref;
                JSValue msgVal = JS_GetPropertyStr(ctx, ex, "message");
                JS_PUSH_VALUE(ctx, msgVal);

                if (!JS_IsUndefined(msgVal))
                {
                    JSCStringBuf buf;
                    const char *msgStr = JS_ToCString(ctx, msgVal, &buf);
                    if (msgStr)
                    {
                        result = msgStr;
                    }
                }

                JS_POP_VALUE(ctx, msgVal);
            }

            JS_POP_VALUE(ctx, ex);
            return result.empty() ? "unknown error" : result;
        }

        // Convert the script output JSValue to a ScriptResult.
        // Caller must have validated outJson is a non-null, non-string JS object.
        ScriptResult ScriptResultFromMqJsValue(JSContext *ctx, JSValue outJson)
        {
            Json::Value jv(Json::objectValue);

            // Helper: extract a named uint32_t property from a JSValue object.
            auto getPropertyUint = [ctx](JSValue obj, const char *key) -> std::optional<uint32_t> {
                JSValue fv = JS_GetPropertyStr(ctx, obj, key);

                if (JS_IsUndefined(fv) || JS_IsNull(fv))
                {
                    return std::nullopt;
                }

                uint32_t v = 0;

                if (JS_ToUint32(ctx, &v, fv) < 0)
                {
                    return std::nullopt;
                }

                return v;
            };

            // Helper: extract a named string property from a JSValue object.
            auto getPropertyStr = [ctx](JSValue obj, const char *key) -> std::optional<std::string> {
                JSValue fv = JS_GetPropertyStr(ctx, obj, key);

                if (JS_IsUndefined(fv) || JS_IsNull(fv))
                {
                    return std::nullopt;
                }

                JSCStringBuf buf;
                const char *s = JS_ToCString(ctx, fv, &buf);

                if (!s)
                {
                    return std::nullopt;
                }

                return std::string(s);
            };

            // Extract "error" key
            {
                JSValue ev = JS_GetPropertyStr(ctx, outJson, "error");

                if (!JS_IsUndefined(ev))
                {
                    if (JS_IsNull(ev))
                    {
                        jv["error"] = Json::Value(); // null → type error in FromJsonValue
                    }
                    else if (JS_IsString(ctx, ev))
                    {
                        JSCStringBuf buf;
                        const char *s = JS_ToCString(ctx, ev, &buf);

                        if (s)
                        {
                            jv["error"] = std::string(s);
                        }
                        else
                        {
                            return ScriptResult::MakeError(GetExceptionString(ctx));
                        }
                    }
                    else
                    {
                        jv["error"] = Json::Value(); // non-string → type error in FromJsonValue
                    }
                }
            }

            // Extract "value" key — preserve JS type (string/bool/number/null); reject objects/arrays
            {
                JSValue vv = JS_GetPropertyStr(ctx, outJson, "value");

                if (!JS_IsUndefined(vv))
                {
                    if (JS_IsNull(vv))
                    {
                        jv["value"] = Json::Value(); // null → suppress signal
                    }
                    else if (JS_IsString(ctx, vv))
                    {
                        JSCStringBuf buf;
                        const char *s = JS_ToCString(ctx, vv, &buf);

                        if (s)
                        {
                            jv["value"] = std::string(s);
                        }
                        else
                        {
                            return ScriptResult::MakeError(GetExceptionString(ctx));
                        }
                    }
                    else if (JS_IsBool(vv))
                    {
                        jv["value"] = static_cast<bool>(JS_VALUE_GET_SPECIAL_VALUE(vv));
                    }
                    else if (JS_IsNumber(ctx, vv))
                    {
                        double d = 0.0;

                        if (JS_ToNumber(ctx, &d, vv) < 0)
                        {
                            return ScriptResult::MakeError(GetExceptionString(ctx));
                        }

                        jv["value"] = d;
                    }
                    else
                    {
                        return ScriptResult::MakeError("'value' field must be a string, number, boolean, or null");
                    }
                }
            }

            // Extract "invoke" sub-object
            {
                JSValue iv = JS_GetPropertyStr(ctx, outJson, "invoke");

                if (!JS_IsUndefined(iv))
                {
                    if (!JS_IsNull(iv) && JS_IsPtr(iv) && !JS_IsString(ctx, iv))
                    {
                        Json::Value invokeJv(Json::objectValue);

                        if (auto v = getPropertyUint(iv, "clusterId"))
                        {
                            invokeJv["clusterId"] = *v;
                        }

                        if (auto v = getPropertyUint(iv, "commandId"))
                        {
                            invokeJv["commandId"] = *v;
                        }

                        if (auto v = getPropertyUint(iv, "endpointId"))
                        {
                            invokeJv["endpointId"] = *v;
                        }

                        if (auto v = getPropertyUint(iv, "timedInvokeTimeoutMs"))
                        {
                            invokeJv["timedInvokeTimeoutMs"] = *v;
                        }

                        if (auto v = getPropertyStr(iv, "tlvBase64"))
                        {
                            invokeJv["tlvBase64"] = *v;
                        }

                        jv["invoke"] = invokeJv;
                    }
                    else
                    {
                        // Property present but not a valid object — preserve the key as
                        // null so ParseInvoke() reports a type error and ambiguity
                        // detection in FromJsonValue() fires correctly.
                        jv["invoke"] = Json::Value();
                    }
                }
            }

            // Extract "write" sub-object
            {
                JSValue wv = JS_GetPropertyStr(ctx, outJson, "write");

                if (!JS_IsUndefined(wv))
                {
                    if (!JS_IsNull(wv) && JS_IsPtr(wv) && !JS_IsString(ctx, wv))
                    {
                        Json::Value writeJv(Json::objectValue);

                        if (auto v = getPropertyUint(wv, "clusterId"))
                        {
                            writeJv["clusterId"] = *v;
                        }

                        if (auto v = getPropertyUint(wv, "attributeId"))
                        {
                            writeJv["attributeId"] = *v;
                        }

                        if (auto v = getPropertyUint(wv, "endpointId"))
                        {
                            writeJv["endpointId"] = *v;
                        }

                        if (auto v = getPropertyStr(wv, "tlvBase64"))
                        {
                            writeJv["tlvBase64"] = *v;
                        }

                        jv["write"] = writeJv;
                    }
                    else
                    {
                        // Property present but not a valid object — preserve the key as
                        // null so ParseWrite() reports a type error and ambiguity
                        // detection in FromJsonValue() fires correctly.
                        jv["write"] = Json::Value();
                    }
                }
            }

            return ScriptResult::FromJsonValue(jv);
        }

    } // anonymous namespace

    std::unique_ptr<SbmdScriptImpl> SbmdScriptImpl::Create(const std::string &deviceId)
    {
        // Ensure the shared runtime is initialized
        if (!MQuickJsRuntime::IsInitialized())
        {
            if (!MQuickJsRuntime::Initialize(BARTON_CONFIG_MQUICKJS_MEMSIZE_BYTES))
            {
                icError("Failed to initialize shared mquickjs context");
                return nullptr;
            }
            // Load SBMD utilities bundle into the shared context (required)
            JSContext *ctx = MQuickJsRuntime::GetSharedContext();
            if (!SbmdUtilsLoader::LoadBundle(ctx))
            {
                icError("Failed to load SBMD utilities bundle - scripts will not work correctly");
                return nullptr;
            }
            icInfo("SBMD utilities loaded from %s", SbmdUtilsLoader::GetSource());
            {
                std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
                MQuickJsRuntime::LogMemoryUsage("post-sbmd-utils-load", IC_LOG_DEBUG);
                JS_GC(ctx);
                MQuickJsRuntime::LogMemoryUsage("post-sbmd-utils-load-after-gc", IC_LOG_DEBUG);
            }
        }

        icDebug("SbmdScriptImpl created for device %s (using shared mquickjs context)", deviceId.c_str());
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

JSValue SbmdScriptImpl::BuildBaseArgs(const std::optional<std::string> &endpointId,
                                     std::optional<uint32_t> clusterId,
                                     const std::optional<std::string> &resourceId,
                                     const std::optional<std::string> &input) const
{
    JSContext *ctx = MQuickJsRuntime::GetSharedContext();

    JSValue args = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, args, "deviceUuid", JS_NewString(ctx, deviceId.c_str()));

    // Add cluster feature maps so scripts can check cluster capabilities
    JSValue featureMaps = JS_NewObject(ctx);
    {
        std::lock_guard<std::mutex> lock(scriptsMutex);
        for (const auto &pair : clusterFeatureMaps)
        {
            // Use string key (JavaScript object keys are strings)
            JS_SetPropertyStr(ctx, featureMaps, std::to_string(pair.first).c_str(), JS_NewUint32(ctx, pair.second));
        }
    }
    JS_SetPropertyStr(ctx, args, "clusterFeatureMaps", featureMaps);

    // Add optional common fields
    if (endpointId.has_value())
    {
        JS_SetPropertyStr(ctx, args, "endpointId", JS_NewString(ctx, endpointId.value().c_str()));
    }
    if (clusterId.has_value())
    {
        JS_SetPropertyStr(ctx, args, "clusterId", JS_NewUint32(ctx, clusterId.value()));
    }
    if (resourceId.has_value())
    {
        JS_SetPropertyStr(ctx, args, "resourceId", JS_NewString(ctx, resourceId.value().c_str()));
    }
    if (input.has_value())
    {
        JS_SetPropertyStr(ctx, args, "input", JS_NewString(ctx, input.value().c_str()));
    }

    return args;
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

// Requires MQuickJsRuntime::GetMutex() to be held by caller.
bool SbmdScriptImpl::ExecuteScript(const std::string &script,
                                  const std::string &argumentName,
                                  JSValue jsonArg,
                                  JSValue &outJson)
{
    JSContext *ctx = MQuickJsRuntime::GetSharedContext();

    if (script.empty())
    {
        icWarn("Empty script provided");
        return false;
    }

    // Check for pending exception from previous operations
    std::string exMsg;
    if (MQuickJsRuntime::CheckAndClearPendingException(ctx, &exMsg))
    {
        icError("Found unhandled exception before script execution: %s - this is a bug", exMsg.c_str());
        return false;
    }

    // mquickjs restriction: properties set directly on the global object are NOT
    // visible as global variables in executing scripts. To pass arguments, we
    // wrap the script in an IIFE and call it with the parsed JSON via JS_PushArg/JS_Call.
    std::string wrappedScript = "(function(" + argumentName + ") { " + script + " })";

    icDebug("Executing script with %s arg", argumentName.c_str());

    // Compile the IIFE wrapper (JS_EVAL_RETVAL to get the function value)
    JSValue func = JS_Eval(ctx, wrappedScript.c_str(), wrappedScript.length(), "<sbmd_script>", JS_EVAL_RETVAL);
    if (JS_IsException(func))
    {
        std::string err = GetExceptionString(ctx);
        icError("Script compilation failed: %s", err.c_str());
        MQuickJsRuntime::LogMemoryUsage("compilation-failed", IC_LOG_ERROR, true);
        return false;
    }

    // Call the function with the parsed JSON argument (stack order: arg, func, this)
    if (JS_StackCheck(ctx, 3))
    {
        icError("Stack overflow before script call");
        MQuickJsRuntime::LogMemoryUsage("stack-overflow-pre-call", IC_LOG_ERROR, true);
        return false;
    }
    JS_PushArg(ctx, jsonArg);
    JS_PushArg(ctx, func);
    JS_PushArg(ctx, JS_NULL);

    // Arm the execution timeout before calling into JS
    MQuickJsRuntime::SetDeadline(std::chrono::steady_clock::now() +
                                 std::chrono::milliseconds(BARTON_CONFIG_SBMD_SCRIPT_TIMEOUT_MS));

    JSValue scriptResult = JS_Call(ctx, 1);

    // Disarm the deadline immediately after JS returns
    MQuickJsRuntime::ClearDeadline();

    if (JS_IsException(scriptResult))
    {
        std::string err = GetExceptionString(ctx);
        icError("Script execution failed: %s", err.c_str());
        MQuickJsRuntime::LogMemoryUsage("execution-failed", IC_LOG_ERROR, true);
        return false;
    }

    outJson = scriptResult;

    // here we do the more expensive heap walk for our dump to capture the impact of the executed script,
    // which may have caused significant deallocations that may not have been compacted yet until the next GC.
    MQuickJsRuntime::LogMemoryUsage("post-script-exec", IC_LOG_DEBUG, true);

    icDebug("Script executed successfully");
    return true;
}

ScriptResult SbmdScriptImpl::MapAttributeRead(const SbmdAttribute &attributeInfo, chip::TLV::TLVReader &reader)
{
    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
    JSContext *ctx = MQuickJsRuntime::GetSharedContext();

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

    // Build the sbmdReadArgs object with base64 TLV
    JSValue jsonArg = BuildBaseArgs(attributeInfo.resourceEndpointId.value_or(""), attributeInfo.clusterId);
    JS_SetPropertyStr(ctx, jsonArg, "tlvBase64", JS_NewString(ctx, tlvBase64.c_str()));
    JS_SetPropertyStr(ctx, jsonArg, "attributeId", JS_NewUint32(ctx, attributeInfo.attributeId));
    JS_SetPropertyStr(ctx, jsonArg, "attributeName", JS_NewString(ctx, attributeInfo.name.c_str()));
    JS_SetPropertyStr(ctx, jsonArg, "attributeType", JS_NewString(ctx, attributeInfo.type.c_str()));

    JSValue outJson;

    if (!ExecuteScript(it->second, "sbmdReadArgs", jsonArg, outJson))
    {
        return ScriptResult::MakeError("Script execution failed for attribute " + attributeInfo.name);
    }

    if (JS_IsNull(outJson) || JS_IsUndefined(outJson) || !JS_IsPtr(outJson) || JS_IsString(ctx, outJson))
    {
        icError("Attribute mapper script returned a non-object for cluster 0x%X, attribute 0x%X",
                attributeInfo.clusterId,
                attributeInfo.attributeId);
        return ScriptResult::MakeError("Attribute mapper script returned a non-object");
    }

    return ScriptResultFromMqJsValue(ctx, outJson);
}

ScriptResult SbmdScriptImpl::MapCommandExecuteResponse(const SbmdCommand &commandInfo, chip::TLV::TLVReader &reader)
{
    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
    JSContext *ctx = MQuickJsRuntime::GetSharedContext();

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

    // Build the sbmdCommandResponseArgs object with base64 TLV
    JSValue jsonArg = BuildBaseArgs(commandInfo.resourceEndpointId.value_or(""), commandInfo.clusterId);
    JS_SetPropertyStr(ctx, jsonArg, "tlvBase64", JS_NewString(ctx, tlvBase64.c_str()));
    JS_SetPropertyStr(ctx, jsonArg, "commandId", JS_NewUint32(ctx, commandInfo.commandId));
    JS_SetPropertyStr(ctx, jsonArg, "commandName", JS_NewString(ctx, commandInfo.name.c_str()));

    JSValue outJson;

    if (!ExecuteScript(it->second, "sbmdCommandResponseArgs", jsonArg, outJson))
    {
        return ScriptResult::MakeError("Script execution failed for command response " + commandInfo.name);
    }

    if (JS_IsNull(outJson) || JS_IsUndefined(outJson) || !JS_IsPtr(outJson) || JS_IsString(ctx, outJson))
    {
        icError("Command response mapper script returned a non-object for cluster 0x%X, command 0x%X",
                commandInfo.clusterId,
                commandInfo.commandId);
        return ScriptResult::MakeError("Command response mapper script returned a non-object");
    }

    return ScriptResultFromMqJsValue(ctx, outJson);
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
    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
    JSContext *ctx = MQuickJsRuntime::GetSharedContext();

    auto it = writeScripts.find(resourceKey);

    if (it == writeScripts.end())
    {
        icError("No write mapper found for resource %s", resourceKey.c_str());
        return ScriptResult::MakeError("No write mapper found for resource " + resourceKey);
    }

    // Build the sbmdWriteArgs object
    JSValue jsonArg = BuildBaseArgs(endpointId, std::nullopt, resourceId, inValue);

    JSValue outJson;

    if (!ExecuteScript(it->second, "sbmdWriteArgs", jsonArg, outJson))
    {
        return ScriptResult::MakeError("Script execution failed for write " + resourceKey);
    }

    if (JS_IsNull(outJson) || JS_IsUndefined(outJson) || !JS_IsPtr(outJson) || JS_IsString(ctx, outJson))
    {
        icError("Write mapper script returned a non-object for resource %s", resourceKey.c_str());
        return ScriptResult::MakeError("Write mapper script returned a non-object");
    }

    return ScriptResultFromMqJsValue(ctx, outJson);
}

ScriptResult SbmdScriptImpl::MapExecute(const std::string &resourceKey,
                                        const std::string &endpointId,
                                        const std::string &resourceId,
                                        const std::string &inValue)
{
    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
    JSContext *ctx = MQuickJsRuntime::GetSharedContext();

    auto it = executeScripts.find(resourceKey);

    if (it == executeScripts.end())
    {
        icError("No execute mapper found for resource %s", resourceKey.c_str());
        return ScriptResult::MakeError("No execute mapper found for resource " + resourceKey);
    }

    // Build the sbmdCommandArgs object
    JSValue jsonArg = BuildBaseArgs(endpointId, std::nullopt, resourceId, inValue);

    JSValue outJson;

    if (!ExecuteScript(it->second, "sbmdCommandArgs", jsonArg, outJson))
    {
        return ScriptResult::MakeError("Script execution failed for execute " + resourceKey);
    }

    if (JS_IsNull(outJson) || JS_IsUndefined(outJson) || !JS_IsPtr(outJson) || JS_IsString(ctx, outJson))
    {
        icError("Execute mapper script returned a non-object for resource %s", resourceKey.c_str());
        return ScriptResult::MakeError("Execute mapper script returned a non-object");
    }

    return ScriptResultFromMqJsValue(ctx, outJson);
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
    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
    JSContext *ctx = MQuickJsRuntime::GetSharedContext();

    auto it = eventScripts.find(eventInfo);

    if (it == eventScripts.end())
    {
        icError("No event mapper found for cluster 0x%X, event 0x%X",
                eventInfo.clusterId,
                eventInfo.eventId);
        return ScriptResult::MakeError("No event mapper found");
    }

    // Build the sbmdEventArgs object
    JSValue jsonArg = BuildBaseArgs(eventInfo.resourceEndpointId.value_or(""), eventInfo.clusterId);
    JS_SetPropertyStr(ctx, jsonArg, "eventId", JS_NewUint32(ctx, eventInfo.eventId));
    JS_SetPropertyStr(ctx, jsonArg, "eventName", JS_NewString(ctx, eventInfo.name.c_str()));

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

    JS_SetPropertyStr(ctx, jsonArg, "tlvBase64", JS_NewStringLen(ctx, base64Buffer.get(), base64Len));

    // Execute the mapper script
    JSValue outJson;

    if (!ExecuteScript(it->second, "sbmdEventArgs", jsonArg, outJson))
    {
        icError("Failed to execute event mapper script for cluster 0x%X, event 0x%X",
                eventInfo.clusterId,
                eventInfo.eventId);
        return ScriptResult::MakeError("Script execution failed for event");
    }

    if (JS_IsNull(outJson) || JS_IsUndefined(outJson) || !JS_IsPtr(outJson) || JS_IsString(ctx, outJson))
    {
        icError("Event mapper script returned a non-object for cluster 0x%X, event 0x%X",
                eventInfo.clusterId,
                eventInfo.eventId);
        return ScriptResult::MakeError("Event mapper script returned a non-object");
    }

    return ScriptResultFromMqJsValue(ctx, outJson);
}

} // namespace barton
