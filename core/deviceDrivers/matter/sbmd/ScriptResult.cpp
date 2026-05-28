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
// Created by Raiyan Chowdhury on 5/26/2026.
//

#define LOG_TAG "ScriptResult"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "ScriptResult.h"

#include <json/json.h>
#include <lib/support/Base64.h>
#include <platform/CHIPDeviceLayer.h>

extern "C" {
#include <icLog/logging.h>
}

namespace barton
{

    namespace
    {
        constexpr const char *kKeyValue = "value";
        constexpr const char *kKeyInvoke = "invoke";
        constexpr const char *kKeyWrite = "write";
        constexpr const char *kKeyError = "error";
        constexpr const char *kKeyClusterId = "clusterId";
        constexpr const char *kKeyCommandId = "commandId";
        constexpr const char *kKeyAttributeId = "attributeId";
        constexpr const char *kKeyEndpointId = "endpointId";
        constexpr const char *kKeyTimedInvokeTimeoutMs = "timedInvokeTimeoutMs";
        constexpr const char *kKeyTlvBase64 = "tlvBase64";

        bool DecodeTlvBase64(const std::string &base64Str,
                             chip::Platform::ScopedMemoryBuffer<uint8_t> &outBuffer,
                             size_t &outLength)
        {
            if (base64Str.empty())
            {
                outLength = 0;
                return true;
            }

            if (base64Str.length() > UINT16_MAX)
            {
                icError("base64 TLV string too large to decode (%zu bytes)", base64Str.length());
                return false;
            }

            size_t maxDecodedLen = BASE64_MAX_DECODED_LEN(base64Str.length());

            if (!outBuffer.Alloc(maxDecodedLen))
            {
                icError("Failed to allocate buffer for TLV decoding");
                return false;
            }

            uint16_t decodedLen =
                chip::Base64Decode(base64Str.c_str(), static_cast<uint16_t>(base64Str.length()), outBuffer.Get());

            if (decodedLen == UINT16_MAX)
            {
                icError("Failed to decode base64 TLV data");
                return false;
            }

            outLength = decodedLen;
            return true;
        }

        ScriptResult ParseInvoke(const Json::Value &invokeObj)
        {
            if (!invokeObj.isObject())
            {
                return ScriptResult::MakeError("'invoke' field must be an object");
            }

            if (!invokeObj.isMember(kKeyClusterId))
            {
                return ScriptResult::MakeError("'invoke' missing required 'clusterId' field");
            }

            if (!invokeObj.isMember(kKeyCommandId))
            {
                return ScriptResult::MakeError("'invoke' missing required 'commandId' field");
            }

            if (!invokeObj[kKeyClusterId].isUInt())
            {
                return ScriptResult::MakeError("'invoke.clusterId' must be a non-negative integer");
            }

            if (!invokeObj[kKeyCommandId].isUInt())
            {
                return ScriptResult::MakeError("'invoke.commandId' must be a non-negative integer");
            }

            ScriptWriteResult result;
            result.type = ScriptWriteResult::OperationType::Invoke;
            result.clusterId = static_cast<chip::ClusterId>(invokeObj[kKeyClusterId].asUInt());
            result.commandId = static_cast<chip::CommandId>(invokeObj[kKeyCommandId].asUInt());

            if (invokeObj.isMember(kKeyEndpointId))
            {
                if (!invokeObj[kKeyEndpointId].isUInt() || invokeObj[kKeyEndpointId].asUInt() > UINT16_MAX)
                {
                    return ScriptResult::MakeError("'invoke.endpointId' must be an integer in [0, 65535]");
                }

                result.endpointId = static_cast<chip::EndpointId>(invokeObj[kKeyEndpointId].asUInt());
            }

            if (invokeObj.isMember(kKeyTimedInvokeTimeoutMs))
            {
                if (!invokeObj[kKeyTimedInvokeTimeoutMs].isUInt() ||
                    invokeObj[kKeyTimedInvokeTimeoutMs].asUInt() > UINT16_MAX)
                {
                    return ScriptResult::MakeError("'invoke.timedInvokeTimeoutMs' must be an integer in [0, 65535]");
                }

                result.timedInvokeTimeoutMs = static_cast<uint16_t>(invokeObj[kKeyTimedInvokeTimeoutMs].asUInt());
            }

            if (invokeObj.isMember(kKeyTlvBase64))
            {
                if (!invokeObj[kKeyTlvBase64].isString())
                {
                    return ScriptResult::MakeError("'invoke.tlvBase64' must be a string");
                }

                std::string base64Str = invokeObj[kKeyTlvBase64].asString();

                if (!DecodeTlvBase64(base64Str, result.tlvBuffer, result.tlvLength))
                {
                    return ScriptResult::MakeError("Failed to decode 'invoke.tlvBase64'");
                }
            }

            icDebug("invoke: cluster=0x%X, command=0x%X, tlvLen=%zu",
                    result.clusterId,
                    result.commandId,
                    result.tlvLength);

            return ScriptResult::MakeWriteResult(std::move(result));
        }

        ScriptResult ParseWrite(const Json::Value &writeObj)
        {
            if (!writeObj.isObject())
            {
                return ScriptResult::MakeError("'write' field must be an object");
            }

            if (!writeObj.isMember(kKeyClusterId))
            {
                return ScriptResult::MakeError("'write' missing required 'clusterId' field");
            }

            if (!writeObj.isMember(kKeyAttributeId))
            {
                return ScriptResult::MakeError("'write' missing required 'attributeId' field");
            }

            if (!writeObj.isMember(kKeyTlvBase64))
            {
                return ScriptResult::MakeError("'write' missing required 'tlvBase64' field");
            }

            if (!writeObj[kKeyClusterId].isUInt())
            {
                return ScriptResult::MakeError("'write.clusterId' must be a non-negative integer");
            }

            if (!writeObj[kKeyAttributeId].isUInt())
            {
                return ScriptResult::MakeError("'write.attributeId' must be a non-negative integer");
            }

            ScriptWriteResult result;
            result.type = ScriptWriteResult::OperationType::Write;
            result.clusterId = static_cast<chip::ClusterId>(writeObj[kKeyClusterId].asUInt());
            result.attributeId = static_cast<chip::AttributeId>(writeObj[kKeyAttributeId].asUInt());

            if (writeObj.isMember(kKeyEndpointId))
            {
                if (!writeObj[kKeyEndpointId].isUInt() || writeObj[kKeyEndpointId].asUInt() > UINT16_MAX)
                {
                    return ScriptResult::MakeError("'write.endpointId' must be an integer in [0, 65535]");
                }

                result.endpointId = static_cast<chip::EndpointId>(writeObj[kKeyEndpointId].asUInt());
            }

            if (!writeObj[kKeyTlvBase64].isString())
            {
                return ScriptResult::MakeError("'write.tlvBase64' must be a string");
            }

            std::string base64Str = writeObj[kKeyTlvBase64].asString();

            if (base64Str.empty())
            {
                return ScriptResult::MakeError("'write.tlvBase64' must not be empty");
            }

            if (!DecodeTlvBase64(base64Str, result.tlvBuffer, result.tlvLength))
            {
                return ScriptResult::MakeError("Failed to decode 'write.tlvBase64'");
            }

            icDebug("write: cluster=0x%X, attribute=0x%X, tlvLen=%zu",
                    result.clusterId,
                    result.attributeId,
                    result.tlvLength);

            return ScriptResult::MakeWriteResult(std::move(result));
        }

    } // anonymous namespace

    ScriptResult ScriptResult::FromJsonValue(const Json::Value &jv)
    {
        if (!jv.isObject())
        {
            return MakeError("Script result must be a JSON object");
        }

        bool hasValue = jv.isMember(kKeyValue);
        bool hasInvoke = jv.isMember(kKeyInvoke);
        bool hasWrite = jv.isMember(kKeyWrite);
        bool hasError = jv.isMember(kKeyError);

        int keyCount = (hasValue ? 1 : 0) + (hasInvoke ? 1 : 0) + (hasWrite ? 1 : 0) + (hasError ? 1 : 0);

        if (keyCount > 1)
        {
            return MakeError("Script result is ambiguous: contains more than one of 'value', 'invoke', 'write', 'error'");
        }

        if (hasError)
        {
            if (!jv[kKeyError].isString() || jv[kKeyError].asString().empty())
            {
                return MakeError("Script returned 'error' key with a non-string or empty value");
            }

            std::string msg = jv[kKeyError].asString();
            icDebug("Script returned error: %s", msg.c_str());
            return MakeError(std::move(msg));
        }

        if (hasValue)
        {
            const Json::Value &val = jv[kKeyValue];
            std::string strVal;

            if (val.isNull())
            {
                // null is a valid way for a script to suppress the update
                // (e.g. when a Matter attribute has no meaningful value yet)
                icDebug("Script returned value: null — suppressing update");
                return MakeSuppress();
            }
            else if (val.isString())
            {
                strVal = val.asString();
            }
            else if (val.isBool())
            {
                strVal = val.asBool() ? "true" : "false";
            }
            else if (val.isNumeric())
            {
                strVal = val.asString();
            }
            else
            {
                return MakeError("'value' field must be a string, number, boolean, or null");
            }

            icDebug("Script returned value: %s", strVal.c_str());
            return MakeResourceUpdate(std::move(strVal));
        }

        if (hasInvoke)
        {
            return ParseInvoke(jv[kKeyInvoke]);
        }

        if (hasWrite)
        {
            return ParseWrite(jv[kKeyWrite]);
        }

        // No recognized keys — suppress
        icDebug("Script returned empty object — suppressing update");
        return MakeSuppress();
    }

} // namespace barton
