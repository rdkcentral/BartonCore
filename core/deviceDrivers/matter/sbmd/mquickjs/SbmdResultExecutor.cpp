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

/*
 * Created by tlea on 6/12/2026
 */

#define LOG_TAG "SbmdResultExecutor"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "SbmdResultExecutor.h"
#include "SbmdWireContract.h"

#include <string>

extern "C" {
#include <icLog/logging.h>
#include <mquickjs/mquickjs.h>
}

namespace barton
{
    namespace
    {
        std::string GetStringProp(JSContext *ctx, JSValue obj, const char *name)
        {
            JSValue val = JS_GetPropertyStr(ctx, obj, name);

            if (JS_IsUndefined(val) || JS_IsNull(val))
            {
                return "";
            }

            JSCStringBuf buf;
            const char *str = JS_ToCString(ctx, val, &buf);

            return str ? std::string(str) : "";
        }

        uint32_t GetUint32Prop(JSContext *ctx, JSValue obj, const char *name)
        {
            JSValue val = JS_GetPropertyStr(ctx, obj, name);

            if (JS_IsUndefined(val) || JS_IsNull(val))
            {
                return 0;
            }

            uint32_t result = 0;
            JS_ToUint32(ctx, &result, val);

            return result;
        }

        std::optional<uint32_t> GetOptUint32Prop(JSContext *ctx, JSValue obj, const char *name)
        {
            JSValue val = JS_GetPropertyStr(ctx, obj, name);

            if (JS_IsUndefined(val) || JS_IsNull(val))
            {
                return std::nullopt;
            }

            uint32_t result = 0;
            JS_ToUint32(ctx, &result, val);

            return result;
        }

        std::optional<uint16_t> GetOptUint16Prop(JSContext *ctx, JSValue obj, const char *name)
        {
            auto opt = GetOptUint32Prop(ctx, obj, name);

            if (!opt.has_value())
            {
                return std::nullopt;
            }

            return static_cast<uint16_t>(*opt);
        }

        uint32_t GetArrayLength(JSContext *ctx, JSValue arr)
        {
            JSValue lenVal = JS_GetPropertyStr(ctx, arr, "length");

            if (JS_IsUndefined(lenVal))
            {
                return 0;
            }

            uint32_t len = 0;
            JS_ToUint32(ctx, &len, lenVal);

            return len;
        }

        bool HasProperty(JSContext *ctx, JSValue obj, const char *name)
        {
            JSValue val = JS_GetPropertyStr(ctx, obj, name);

            return !JS_IsUndefined(val);
        }
    } // namespace

    std::optional<ParsedResult> SbmdResultExecutor::Parse(JSContext *ctx, JSValue resultVal)
    {
        if (JS_IsUndefined(resultVal) || JS_IsNull(resultVal))
        {
            icError("result is undefined or null");
            return std::nullopt;
        }

        // Get ops array
        JSValue opsVal = JS_GetPropertyStr(ctx, resultVal, SBMD_KEY_OPS);

        if (JS_IsUndefined(opsVal) || JS_IsNull(opsVal))
        {
            icError("result has no 'ops' array");
            return std::nullopt;
        }

        // Get terminal
        JSValue termVal = JS_GetPropertyStr(ctx, resultVal, SBMD_KEY_TERMINAL);

        if (JS_IsUndefined(termVal) || JS_IsNull(termVal))
        {
            icError("result has no 'terminal' object");
            return std::nullopt;
        }

        ParsedResult result;

        // Parse ops array
        uint32_t opsLen = GetArrayLength(ctx, opsVal);

        for (uint32_t i = 0; i < opsLen; i++)
        {
            JSValue opVal = JS_GetPropertyUint32(ctx, opsVal, i);
            auto op = ParseOp(ctx, opVal);

            if (!op.has_value())
            {
                icWarn("failed to parse op at index %u, skipping", i);
                continue;
            }

            result.ops.push_back(std::move(*op));
        }

        // Parse terminal
        auto terminal = ParseTerminal(ctx, termVal);

        if (!terminal.has_value())
        {
            icError("failed to parse terminal");
            return std::nullopt;
        }

        result.terminal = std::move(*terminal);

        return result;
    }

    std::optional<ResultOp> SbmdResultExecutor::ParseOp(JSContext *ctx, JSValue opVal)
    {
        std::string opType = GetStringProp(ctx, opVal, SBMD_KEY_OP);

        if (opType == "updateResource")
        {
            ResultOp::UpdateResource data;

            if (HasProperty(ctx, opVal, SBMD_KEY_ENDPOINT))
            {
                data.endpoint = GetStringProp(ctx, opVal, SBMD_KEY_ENDPOINT);
            }

            data.resource = GetStringProp(ctx, opVal, SBMD_KEY_RESOURCE);
            data.value = GetStringProp(ctx, opVal, SBMD_KEY_VALUE);

            if (HasProperty(ctx, opVal, SBMD_KEY_METADATA))
            {
                data.metadata = GetStringProp(ctx, opVal, SBMD_KEY_METADATA);
            }

            return ResultOp{std::move(data)};
        }
        else if (opType == "setMetadata")
        {
            ResultOp::SetMetadata data;
            data.name = GetStringProp(ctx, opVal, SBMD_KEY_NAME);
            data.value = GetStringProp(ctx, opVal, SBMD_KEY_VALUE);

            return ResultOp {std::move(data)};
        }
        else if (opType == "setPersistentData")
        {
            ResultOp::SetPersistentData data;
            data.key = GetStringProp(ctx, opVal, SBMD_KEY_KEY);
            data.value = GetStringProp(ctx, opVal, SBMD_KEY_VALUE);

            return ResultOp{std::move(data)};
        }
        else if (opType == "setTransientData")
        {
            ResultOp::SetTransientData data;
            data.key = GetStringProp(ctx, opVal, SBMD_KEY_KEY);
            data.value = GetStringProp(ctx, opVal, SBMD_KEY_VALUE);
            data.ttlSecs = GetUint32Prop(ctx, opVal, SBMD_KEY_TTL_SECS);

            return ResultOp{std::move(data)};
        }
        else if (opType == "log")
        {
            ResultOp::Log data;
            data.message = GetStringProp(ctx, opVal, SBMD_KEY_MESSAGE);

            return ResultOp{std::move(data)};
        }
        else
        {
            icWarn("unknown op type '%s', skipping", opType.c_str());
            return std::nullopt;
        }
    }

    std::optional<ResultTerminal> SbmdResultExecutor::ParseTerminal(JSContext *ctx, JSValue termVal)
    {
        std::string opType = GetStringProp(ctx, termVal, SBMD_KEY_OP);

        if (opType == "success")
        {
            ResultTerminal::Success data;
            data.value = GetStringProp(ctx, termVal, SBMD_KEY_VALUE);

            return ResultTerminal {std::move(data)};
        }
        else if (opType == "error")
        {
            ResultTerminal::Error data;
            data.message = GetStringProp(ctx, termVal, SBMD_KEY_MESSAGE);

            return ResultTerminal{std::move(data)};
        }
        else if (opType == "sendCommand")
        {
            ResultTerminal::SendCommand data;
            data.clusterId = GetUint32Prop(ctx, termVal, SBMD_KEY_CLUSTER_ID);
            data.commandId = GetUint32Prop(ctx, termVal, SBMD_KEY_COMMAND_ID);
            data.tlvBase64 = GetStringProp(ctx, termVal, SBMD_KEY_TLV_BASE64);

            // options: { endpointId?, timedInvokeTimeoutMs?, successValue? }
            JSValue opts = JS_GetPropertyStr(ctx, termVal, SBMD_KEY_OPTIONS);

            if (!JS_IsUndefined(opts) && !JS_IsNull(opts))
            {
                data.endpointId = GetOptUint32Prop(ctx, opts, SBMD_KEY_ENDPOINT_ID);
                data.timedInvokeTimeoutMs = GetOptUint16Prop(ctx, opts, SBMD_KEY_TIMED_INVOKE_TIMEOUT_MS);
                data.successValue = GetStringProp(ctx, opts, SBMD_KEY_SUCCESS_VALUE);
            }

            return ResultTerminal {std::move(data)};
        }
        else if (opType == "writeAttribute")
        {
            ResultTerminal::WriteAttribute data;
            data.clusterId = GetUint32Prop(ctx, termVal, SBMD_KEY_CLUSTER_ID);
            data.attributeId = GetUint32Prop(ctx, termVal, SBMD_KEY_ATTRIBUTE_ID);
            data.tlvBase64 = GetStringProp(ctx, termVal, SBMD_KEY_TLV_BASE64);

            // options: { endpointId? }
            JSValue opts = JS_GetPropertyStr(ctx, termVal, SBMD_KEY_OPTIONS);

            if (!JS_IsUndefined(opts) && !JS_IsNull(opts))
            {
                data.endpointId = GetOptUint32Prop(ctx, opts, SBMD_KEY_ENDPOINT_ID);
            }

            return ResultTerminal {std::move(data)};
        }
        else if (opType == "requestCommand")
        {
            ResultTerminal::RequestCommand data;
            data.clusterId = GetUint32Prop(ctx, termVal, SBMD_KEY_CLUSTER_ID);
            data.commandId = GetUint32Prop(ctx, termVal, SBMD_KEY_COMMAND_ID);
            data.tlvBase64 = GetStringProp(ctx, termVal, SBMD_KEY_TLV_BASE64);

            // Deferred handler callbacks
            JSValue deferred = JS_GetPropertyStr(ctx, termVal, SBMD_KEY_DEFERRED);

            if (!JS_IsUndefined(deferred) && !JS_IsNull(deferred))
            {
                data.responseCommandId = GetUint32Prop(ctx, deferred, SBMD_KEY_RESPONSE_COMMAND_ID);
                data.onResponse = JS_GetPropertyStr(ctx, deferred, SBMD_KEY_ON_RESPONSE);
                data.onError = JS_GetPropertyStr(ctx, deferred, SBMD_KEY_ON_ERROR);
                data.timeoutMs = GetOptUint32Prop(ctx, deferred, SBMD_KEY_TIMEOUT_MS);
                data.context = JS_GetPropertyStr(ctx, deferred, SBMD_KEY_CONTEXT);
            }

            // options: { endpointId?, timedInvokeTimeoutMs? }
            JSValue opts = JS_GetPropertyStr(ctx, termVal, SBMD_KEY_OPTIONS);

            if (!JS_IsUndefined(opts) && !JS_IsNull(opts))
            {
                data.endpointId = GetOptUint32Prop(ctx, opts, SBMD_KEY_ENDPOINT_ID);
                data.timedInvokeTimeoutMs = GetOptUint16Prop(ctx, opts, SBMD_KEY_TIMED_INVOKE_TIMEOUT_MS);
            }

            return ResultTerminal{std::move(data)};
        }
        else if (opType == "readAttribute")
        {
            ResultTerminal::ReadAttribute data;
            data.clusterId = GetUint32Prop(ctx, termVal, SBMD_KEY_CLUSTER_ID);
            data.attributeId = GetUint32Prop(ctx, termVal, SBMD_KEY_ATTRIBUTE_ID);

            // Deferred handler callbacks
            JSValue deferred = JS_GetPropertyStr(ctx, termVal, SBMD_KEY_DEFERRED);

            if (!JS_IsUndefined(deferred) && !JS_IsNull(deferred))
            {
                data.onResponse = JS_GetPropertyStr(ctx, deferred, SBMD_KEY_ON_RESPONSE);
                data.onError = JS_GetPropertyStr(ctx, deferred, SBMD_KEY_ON_ERROR);
                data.timeoutMs = GetOptUint32Prop(ctx, deferred, SBMD_KEY_TIMEOUT_MS);
                data.context = JS_GetPropertyStr(ctx, deferred, SBMD_KEY_CONTEXT);
            }

            // options: { endpointId? }
            JSValue opts = JS_GetPropertyStr(ctx, termVal, SBMD_KEY_OPTIONS);

            if (!JS_IsUndefined(opts) && !JS_IsNull(opts))
            {
                data.endpointId = GetOptUint32Prop(ctx, opts, SBMD_KEY_ENDPOINT_ID);
            }

            return ResultTerminal {std::move(data)};
        }
        else
        {
            icError("unknown terminal op type '%s'", opType.c_str());
            return std::nullopt;
        }
    }

} // namespace barton
