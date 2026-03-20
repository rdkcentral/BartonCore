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

#include "../SbmdSpec.h"
#include "MQuickJsRuntime.h"
#include "SbmdScriptImpl.h"
#include "SbmdUtilsLoader.h"
#include <cstring>

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
            JSValue ex = JS_GetException(ctx);

            // First try direct string conversion (works for string exceptions)
            {
                JSCStringBuf buf;
                const char *str = JS_ToCString(ctx, ex, &buf);
                if (str)
                {
                    return std::string(str);
                }
            }

            // If that fails, try to get the "message" property (for Error objects)
            if (JS_IsPtr(ex))
            {
                JSValue msgVal = JS_GetPropertyStr(ctx, ex, "message");
                if (!JS_IsUndefined(msgVal))
                {
                    JSCStringBuf buf;
                    const char *msgStr = JS_ToCString(ctx, msgVal, &buf);
                    if (msgStr)
                    {
                        return std::string(msgStr);
                    }
                }
            }

            return "unknown error";
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
    JSValue scriptResult = JS_Call(ctx, 1);
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

// Requires MQuickJsRuntime::GetMutex() to be held by caller.
bool SbmdScriptImpl::ExtractScriptOutputAsString(JSValue &scriptResult, std::string &outValue)
{
    JSContext *ctx = MQuickJsRuntime::GetSharedContext();

    // Extract the "output" field from the result JSON object
    JSValue outputVal = JS_GetPropertyStr(ctx, scriptResult, "output");
    if (JS_IsUndefined(outputVal))
    {
        icError("Script result missing 'output' field");
        return false;
    }

    JSCStringBuf buf;
    const char *resultStr = JS_ToCString(ctx, outputVal, &buf);
    if (!resultStr)
    {
        icError("Failed to convert output value to string: %s", GetExceptionString(ctx).c_str());
        return false;
    }

    outValue = resultStr;
    icDebug("Script output string: %s", outValue.c_str());
    return true;
}

bool SbmdScriptImpl::MapAttributeRead(const SbmdAttribute &attributeInfo,
                                     chip::TLV::TLVReader &reader,
                                     std::string &outValue)
{
    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
    JSContext *ctx = MQuickJsRuntime::GetSharedContext();

    auto it = attributeReadScripts.find(attributeInfo);
    if (it == attributeReadScripts.end())
    {
        icError("No read mapper found for cluster 0x%X, attribute 0x%X",
                attributeInfo.clusterId,
                attributeInfo.attributeId);
        return false;
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
        return false;
    }

    size_t tlvLength = writer.GetLengthWritten();

    if (tlvLength > UINT16_MAX)
    {
        icError("Attribute TLV data too large for base64 encoding: %zu bytes", tlvLength);
        return false;
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
        return false;
    }

    return ExtractScriptOutputAsString(outJson, outValue);
}

bool SbmdScriptImpl::MapCommandExecuteResponse(const SbmdCommand &commandInfo,
                                              chip::TLV::TLVReader &reader,
                                              std::string &outValue)
{
    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
    JSContext *ctx = MQuickJsRuntime::GetSharedContext();

    auto it = commandExecuteResponseScripts.find(commandInfo);
    if (it == commandExecuteResponseScripts.end())
    {
        icError("No execute response mapper found for cluster 0x%X, command 0x%X",
                commandInfo.clusterId,
                commandInfo.commandId);
        return false;
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
        return false;
    }

    size_t tlvLength = writer.GetLengthWritten();

    if (tlvLength > UINT16_MAX)
    {
        icError("Command response TLV data too large for base64 encoding: %zu bytes", tlvLength);
        return false;
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
        return false;
    }

    return ExtractScriptOutputAsString(outJson, outValue);
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

bool SbmdScriptImpl::MapWrite(const std::string &resourceKey,
                             const std::string &endpointId,
                             const std::string &resourceId,
                             const std::string &inValue,
                             ScriptWriteResult &result)
{
    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
    JSContext *ctx = MQuickJsRuntime::GetSharedContext();

    auto it = writeScripts.find(resourceKey);
    if (it == writeScripts.end())
    {
        icError("No write mapper found for resource %s", resourceKey.c_str());
        return false;
    }

    // Build the sbmdWriteArgs object
    JSValue jsonArg = BuildBaseArgs(endpointId, std::nullopt, resourceId, inValue);

    JSValue outJson;
    if (!ExecuteScript(it->second, "sbmdWriteArgs", jsonArg, outJson))
    {
        icError("Failed to execute write script for resource %s", resourceKey.c_str());
        return false;
    }

    // Validate that the script result is a non-null object
    if (JS_IsException(outJson))
    {
        icError("Script execution returned an exception: %s", GetExceptionString(ctx).c_str());
        return false;
    }

    if (JS_IsNull(outJson) || JS_IsUndefined(outJson) || !JS_IsPtr(outJson))
    {
        icError("Script result is not an object; expected 'invoke' or 'write' object");
        return false;
    }

    // Check for 'invoke' operation (command invocation)
    JSValue invokeVal = JS_GetPropertyStr(ctx, outJson, "invoke");
    if (!JS_IsUndefined(invokeVal) && !JS_IsNull(invokeVal))
    {
        result.type = ScriptWriteResult::OperationType::Invoke;

        if (!JS_IsPtr(invokeVal))
        {
            icError("'invoke' field must be an object");
            return false;
        }

        // Extract clusterId (required)
        JSValue clusterIdVal = JS_GetPropertyStr(ctx, invokeVal, "clusterId");
        if (JS_IsUndefined(clusterIdVal))
        {
            icError("'invoke' missing required 'clusterId' field");
            return false;
        }
        uint32_t clusterId;
        if (JS_ToUint32(ctx, &clusterId, clusterIdVal) < 0)
        {
            icError("Failed to convert 'clusterId' to number");
            return false;
        }
        result.clusterId = static_cast<chip::ClusterId>(clusterId);

        // Extract commandId (required)
        JSValue commandIdVal = JS_GetPropertyStr(ctx, invokeVal, "commandId");
        if (JS_IsUndefined(commandIdVal))
        {
            icError("'invoke' missing required 'commandId' field");
            return false;
        }
        uint32_t commandId;
        if (JS_ToUint32(ctx, &commandId, commandIdVal) < 0)
        {
            icError("Failed to convert 'commandId' to number");
            return false;
        }
        result.commandId = static_cast<chip::CommandId>(commandId);

        // Extract optional endpointId
        JSValue epIdVal = JS_GetPropertyStr(ctx, invokeVal, "endpointId");
        if (!JS_IsUndefined(epIdVal) && !JS_IsNull(epIdVal))
        {
            uint32_t epId;
            if (JS_ToUint32(ctx, &epId, epIdVal) >= 0)
            {
                result.endpointId = static_cast<chip::EndpointId>(epId);
            }
        }

        // Extract optional timedInvokeTimeoutMs
        JSValue timedVal = JS_GetPropertyStr(ctx, invokeVal, "timedInvokeTimeoutMs");
        if (!JS_IsUndefined(timedVal) && !JS_IsNull(timedVal))
        {
            uint32_t timedMs;
            if (JS_ToUint32(ctx, &timedMs, timedVal) >= 0)
            {
                result.timedInvokeTimeoutMs = static_cast<uint16_t>(timedMs);
            }
        }

        // Extract tlvBase64 (optional for invoke - empty/missing means no command args)
        JSValue tlvBase64Val = JS_GetPropertyStr(ctx, invokeVal, "tlvBase64");
        if (!JS_IsUndefined(tlvBase64Val) && !JS_IsNull(tlvBase64Val))
        {
            JSCStringBuf buf;
            const char *base64Str = JS_ToCString(ctx, tlvBase64Val, &buf);
            if (!base64Str)
            {
                icError("Failed to convert 'tlvBase64' to string");
                return false;
            }
            std::string base64String(base64Str);

            // Only decode if non-empty
            if (!base64String.empty())
            {
                // Decode base64 to TLV bytes
                size_t maxDecodedLen = BASE64_MAX_DECODED_LEN(base64String.length());
                if (!result.tlvBuffer.Alloc(maxDecodedLen))
                {
                    icError("Failed to allocate buffer for TLV decoding");
                    return false;
                }

                uint16_t decodedLen = chip::Base64Decode(
                    base64String.c_str(), static_cast<uint16_t>(base64String.length()), result.tlvBuffer.Get());
                if (decodedLen == UINT16_MAX)
                {
                    icError("Failed to decode base64 TLV data for invoke");
                    return false;
                }
                result.tlvLength = decodedLen;
            }
        }

        icDebug("write mapped to invoke: cluster=0x%X, command=0x%X, tlvLen=%zu",
                result.clusterId,
                result.commandId,
                result.tlvLength);
        return true;
    }

    // Check for 'write' operation (attribute write)
    JSValue writeVal = JS_GetPropertyStr(ctx, outJson, "write");
    if (!JS_IsUndefined(writeVal) && !JS_IsNull(writeVal))
    {
        result.type = ScriptWriteResult::OperationType::Write;

        if (!JS_IsPtr(writeVal))
        {
            icError("'write' field must be an object");
            return false;
        }

        // Extract clusterId (required)
        JSValue clusterIdVal = JS_GetPropertyStr(ctx, writeVal, "clusterId");
        if (JS_IsUndefined(clusterIdVal))
        {
            icError("'write' missing required 'clusterId' field");
            return false;
        }
        uint32_t clusterId;
        if (JS_ToUint32(ctx, &clusterId, clusterIdVal) < 0)
        {
            icError("Failed to convert 'clusterId' to number");
            return false;
        }
        result.clusterId = static_cast<chip::ClusterId>(clusterId);

        // Extract attributeId (required)
        JSValue attrIdVal = JS_GetPropertyStr(ctx, writeVal, "attributeId");
        if (JS_IsUndefined(attrIdVal))
        {
            icError("'write' missing required 'attributeId' field");
            return false;
        }
        uint32_t attrId;
        if (JS_ToUint32(ctx, &attrId, attrIdVal) < 0)
        {
            icError("Failed to convert 'attributeId' to number");
            return false;
        }
        result.attributeId = static_cast<chip::AttributeId>(attrId);

        // Extract optional endpointId
        JSValue epIdVal = JS_GetPropertyStr(ctx, writeVal, "endpointId");
        if (!JS_IsUndefined(epIdVal) && !JS_IsNull(epIdVal))
        {
            uint32_t epId;
            if (JS_ToUint32(ctx, &epId, epIdVal) >= 0)
            {
                result.endpointId = static_cast<chip::EndpointId>(epId);
            }
        }

        // Extract tlvBase64 (required)
        JSValue tlvBase64Val = JS_GetPropertyStr(ctx, writeVal, "tlvBase64");
        if (JS_IsUndefined(tlvBase64Val) || JS_IsNull(tlvBase64Val))
        {
            icError("'write' missing required 'tlvBase64' field");
            return false;
        }

        JSCStringBuf buf;
        const char *base64Str = JS_ToCString(ctx, tlvBase64Val, &buf);
        if (!base64Str)
        {
            icError("Failed to convert 'tlvBase64' to string");
            return false;
        }

        // Decode base64 to TLV bytes
        std::string base64String(base64Str);
        size_t maxDecodedLen = BASE64_MAX_DECODED_LEN(base64String.length());
        if (!result.tlvBuffer.Alloc(maxDecodedLen))
        {
            icError("Failed to allocate buffer for TLV decoding");
            return false;
        }

        uint16_t decodedLen = chip::Base64Decode(
            base64String.c_str(), static_cast<uint16_t>(base64String.length()), result.tlvBuffer.Get());
        if (decodedLen == UINT16_MAX)
        {
            icError("Failed to decode base64 TLV data for write");
            return false;
        }
        result.tlvLength = decodedLen;

        icDebug("write mapped to attribute write: cluster=0x%X, attribute=0x%X, tlvLen=%zu",
                result.clusterId,
                result.attributeId,
                result.tlvLength);
        return true;
    }

    icError("Script result missing 'invoke' or 'write' field");
    return false;
}

bool SbmdScriptImpl::MapExecute(const std::string &resourceKey,
                               const std::string &endpointId,
                               const std::string &resourceId,
                               const std::string &inValue,
                               ScriptWriteResult &result)
{
    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
    JSContext *ctx = MQuickJsRuntime::GetSharedContext();

    auto it = executeScripts.find(resourceKey);
    if (it == executeScripts.end())
    {
        icError("No execute mapper found for resource %s", resourceKey.c_str());
        return false;
    }

    // Build the sbmdCommandArgs object
    JSValue jsonArg = BuildBaseArgs(endpointId, std::nullopt, resourceId, inValue);

    JSValue outJson;
    if (!ExecuteScript(it->second, "sbmdCommandArgs", jsonArg, outJson))
    {
        icError("Failed to execute script for resource %s", resourceKey.c_str());
        return false;
    }

    // Validate that the script result is a non-null object
    if (JS_IsException(outJson))
    {
        icError("Script execution returned an exception: %s", GetExceptionString(ctx).c_str());
        return false;
    }

    if (JS_IsNull(outJson) || JS_IsUndefined(outJson) || !JS_IsPtr(outJson))
    {
        icError("Script result is not an object; expected 'invoke' object");
        return false;
    }

    // Check for 'invoke' operation (command invocation) - the only valid operation for execute
    JSValue invokeVal = JS_GetPropertyStr(ctx, outJson, "invoke");
    if (JS_IsUndefined(invokeVal) || JS_IsNull(invokeVal))
    {
        icError("Execute script result missing required 'invoke' field");
        return false;
    }

    result.type = ScriptWriteResult::OperationType::Invoke;

    if (!JS_IsPtr(invokeVal))
    {
        icError("'invoke' field must be an object");
        return false;
    }

    // Extract clusterId (required)
    JSValue clusterIdVal = JS_GetPropertyStr(ctx, invokeVal, "clusterId");
    if (JS_IsUndefined(clusterIdVal))
    {
        icError("'invoke' missing required 'clusterId' field");
        return false;
    }
    uint32_t clusterId;
    if (JS_ToUint32(ctx, &clusterId, clusterIdVal) < 0)
    {
        icError("Failed to convert 'clusterId' to number");
        return false;
    }
    result.clusterId = static_cast<chip::ClusterId>(clusterId);

    // Extract commandId (required)
    JSValue commandIdVal = JS_GetPropertyStr(ctx, invokeVal, "commandId");
    if (JS_IsUndefined(commandIdVal))
    {
        icError("'invoke' missing required 'commandId' field");
        return false;
    }
    uint32_t commandId;
    if (JS_ToUint32(ctx, &commandId, commandIdVal) < 0)
    {
        icError("Failed to convert 'commandId' to number");
        return false;
    }
    result.commandId = static_cast<chip::CommandId>(commandId);

    // Extract optional endpointId
    JSValue epIdVal = JS_GetPropertyStr(ctx, invokeVal, "endpointId");
    if (!JS_IsUndefined(epIdVal) && !JS_IsNull(epIdVal))
    {
        uint32_t epId;
        if (JS_ToUint32(ctx, &epId, epIdVal) >= 0)
        {
            result.endpointId = static_cast<chip::EndpointId>(epId);
        }
    }

    // Extract optional timedInvokeTimeoutMs
    JSValue timedVal = JS_GetPropertyStr(ctx, invokeVal, "timedInvokeTimeoutMs");
    if (!JS_IsUndefined(timedVal) && !JS_IsNull(timedVal))
    {
        uint32_t timedMs;
        if (JS_ToUint32(ctx, &timedMs, timedVal) >= 0)
        {
            result.timedInvokeTimeoutMs = static_cast<uint16_t>(timedMs);
        }
    }

    // Extract tlvBase64 (optional for invoke - empty/missing means no command args)
    JSValue tlvBase64Val = JS_GetPropertyStr(ctx, invokeVal, "tlvBase64");
    if (!JS_IsUndefined(tlvBase64Val) && !JS_IsNull(tlvBase64Val))
    {
        JSCStringBuf buf;
        const char *base64Str = JS_ToCString(ctx, tlvBase64Val, &buf);
        if (!base64Str)
        {
            icError("Failed to convert 'tlvBase64' to string");
            return false;
        }
        std::string base64String(base64Str);

        // Only decode if non-empty
        if (!base64String.empty())
        {
            // Decode base64 to TLV bytes
            size_t maxDecodedLen = BASE64_MAX_DECODED_LEN(base64String.length());
            if (!result.tlvBuffer.Alloc(maxDecodedLen))
            {
                icError("Failed to allocate buffer for TLV decoding");
                return false;
            }

            uint16_t decodedLen = chip::Base64Decode(
                base64String.c_str(), static_cast<uint16_t>(base64String.length()), result.tlvBuffer.Get());
            if (decodedLen == UINT16_MAX)
            {
                icError("Failed to decode base64 TLV data for invoke");
                return false;
            }
            result.tlvLength = decodedLen;
        }
    }

    icDebug("execute mapped to invoke: cluster=0x%X, command=0x%X, tlvLen=%zu",
            result.clusterId,
            result.commandId,
            result.tlvLength);
    return true;
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

bool SbmdScriptImpl::MapEvent(const SbmdEvent &eventInfo,
                             chip::TLV::TLVReader &reader,
                             std::string &outValue)
{
    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
    JSContext *ctx = MQuickJsRuntime::GetSharedContext();

    auto it = eventScripts.find(eventInfo);
    if (it == eventScripts.end())
    {
        icError("No event mapper found for cluster 0x%X, event 0x%X",
                eventInfo.clusterId,
                eventInfo.eventId);
        return false;
    }

    // Build the sbmdEventArgs object
    JSValue jsonArg = BuildBaseArgs(eventInfo.resourceEndpointId.value_or(""), eventInfo.clusterId);
    JS_SetPropertyStr(ctx, jsonArg, "eventId", JS_NewUint32(ctx, eventInfo.eventId));
    JS_SetPropertyStr(ctx, jsonArg, "eventName", JS_NewString(ctx, eventInfo.name.c_str()));

    // Convert TLV to base64 for script to use
    // Make a copy of the reader since encoding may consume it
    chip::TLV::TLVReader readerCopy;
    readerCopy.Init(reader);

    // Get the size needed for encoding
    chip::TLV::TLVReader sizingReader;
    sizingReader.Init(reader);
    uint32_t tlvLen = sizingReader.GetRemainingLength();

    // Use a reasonable buffer size
    if (tlvLen == 0)
    {
        tlvLen = 256;
    }

    chip::Platform::ScopedMemoryBuffer<uint8_t> tlvBuffer;
    if (!tlvBuffer.Calloc(tlvLen))
    {
        icError("Failed to allocate TLV buffer for event 0x%X", eventInfo.eventId);
        return false;
    }

    // Write the TLV data to buffer
    chip::TLV::TLVWriter writer;
    writer.Init(tlvBuffer.Get(), tlvLen);

    CHIP_ERROR err = writer.CopyElement(chip::TLV::AnonymousTag(), readerCopy);
    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to copy event TLV data: %s", chip::ErrorStr(err));
        return false;
    }

    uint32_t encodedLen = writer.GetLengthWritten();

    if (encodedLen > UINT16_MAX)
    {
        icError("Event TLV data too large for base64 encoding: %u bytes", encodedLen);
        return false;
    }

    // Encode to base64
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
        return false;
    }

    // Extract and return the "output" field
    return ExtractScriptOutputAsString(outJson, outValue);
}

} // namespace barton
