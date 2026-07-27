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

#define LOG_TAG     "SbmdResultExecutor"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "SbmdResultExecutor.h"
#include "SbmdJsUtil.h"
#include "SbmdWireContract.h"

#include <string>

extern "C" {
#include <icLog/logging.h>
#include <mquickjs/mquickjs.h>
}

namespace barton
{
    using namespace mquickjs;

    std::optional<ParsedResult> SbmdResultExecutor::Parse(JSContext *ctx, JSValue resultValRaw)
    {
        // Root the incoming value and every intermediate: mquickjs uses a moving GC, so any raw
        // JSValue held across an allocating call (property read / array index) becomes stale.
        SafeJSValue resultVal(ctx, resultValRaw);

        if (JS_IsUndefined(resultVal.Get()) || JS_IsNull(resultVal.Get()))
        {
            icError("result is undefined or null");
            return std::nullopt;
        }

        // Get ops array
        SafeJSValue opsVal(ctx, JS_GetPropertyStr(ctx, resultVal.Get(), SBMD_KEY_OPS));

        if (JS_IsUndefined(opsVal.Get()) || JS_IsNull(opsVal.Get()))
        {
            icError("result has no 'ops' array");
            return std::nullopt;
        }

        // Get terminal
        SafeJSValue termVal(ctx, JS_GetPropertyStr(ctx, resultVal.Get(), SBMD_KEY_TERMINAL));

        if (JS_IsUndefined(termVal.Get()) || JS_IsNull(termVal.Get()))
        {
            icError("result has no 'terminal' object");
            return std::nullopt;
        }

        ParsedResult result;

        // Parse ops array
        uint32_t opsLen = GetArrayLength(ctx, opsVal.Get());

        for (uint32_t i = 0; i < opsLen; i++)
        {
            SafeJSValue opVal(ctx, JS_GetPropertyUint32(ctx, opsVal.Get(), i));
            auto op = ParseOp(ctx, opVal.Get());

            if (!op.has_value())
            {
                icWarn("failed to parse op at index %u, skipping", i);
                continue;
            }

            result.ops.push_back(std::move(*op));
        }

        // Parse terminal
        auto terminal = ParseTerminal(ctx, termVal.Get());

        if (!terminal.has_value())
        {
            icError("failed to parse terminal");
            return std::nullopt;
        }

        result.terminal = std::move(*terminal);

        return result;
    }

    std::optional<ResultOp> SbmdResultExecutor::ParseOp(JSContext *ctx, JSValue opValRaw)
    {
        SafeJSValue opVal(ctx, opValRaw);
        std::string opType = GetStringProp(ctx, opVal.Get(), SBMD_KEY_OP);

        if (opType == "updateResource")
        {
            ResultOp::UpdateResource data;

            if (HasProperty(ctx, opVal.Get(), SBMD_KEY_ENDPOINT))
            {
                data.endpoint = GetStringProp(ctx, opVal.Get(), SBMD_KEY_ENDPOINT);
            }

            data.resource = GetStringProp(ctx, opVal.Get(), SBMD_KEY_RESOURCE);
            data.value = GetStringProp(ctx, opVal.Get(), SBMD_KEY_VALUE);

            if (HasProperty(ctx, opVal.Get(), SBMD_KEY_METADATA))
            {
                data.metadata = GetStringProp(ctx, opVal.Get(), SBMD_KEY_METADATA);
            }

            return ResultOp {std::move(data)};
        }
        else if (opType == "setMetadata")
        {
            ResultOp::SetMetadata data;
            data.name = GetStringProp(ctx, opVal.Get(), SBMD_KEY_NAME);
            data.value = GetStringProp(ctx, opVal.Get(), SBMD_KEY_VALUE);

            return ResultOp {std::move(data)};
        }
        else if (opType == "setPersistentData")
        {
            ResultOp::SetPersistentData data;
            data.key = GetStringProp(ctx, opVal.Get(), SBMD_KEY_KEY);
            data.value = GetStringProp(ctx, opVal.Get(), SBMD_KEY_VALUE);

            return ResultOp {std::move(data)};
        }
        else if (opType == "setTransientData")
        {
            ResultOp::SetTransientData data;
            data.key = GetStringProp(ctx, opVal.Get(), SBMD_KEY_KEY);
            data.value = GetStringProp(ctx, opVal.Get(), SBMD_KEY_VALUE);
            data.ttlSecs = GetUint32Prop(ctx, opVal.Get(), SBMD_KEY_TTL_SECS, 0);

            return ResultOp {std::move(data)};
        }
        else if (opType == "log")
        {
            ResultOp::Log data;
            data.message = GetStringProp(ctx, opVal.Get(), SBMD_KEY_MESSAGE);

            return ResultOp {std::move(data)};
        }
        else
        {
            icWarn("unknown op type '%s', skipping", opType.c_str());
            return std::nullopt;
        }
    }

    std::optional<ResultTerminal> SbmdResultExecutor::ParseTerminal(JSContext *ctx, JSValue termValRaw)
    {
        SafeJSValue termVal(ctx, termValRaw);
        std::string opType = GetStringProp(ctx, termVal.Get(), SBMD_KEY_OP);

        if (opType == "success")
        {
            ResultTerminal::Success data;
            data.value = GetStringProp(ctx, termVal.Get(), SBMD_KEY_VALUE);

            return ResultTerminal {std::move(data)};
        }
        else if (opType == "error")
        {
            ResultTerminal::Error data;
            data.message = GetStringProp(ctx, termVal.Get(), SBMD_KEY_MESSAGE);

            return ResultTerminal {std::move(data)};
        }
        else if (opType == "sendCommand")
        {
            ResultTerminal::SendCommand data;
            data.clusterId = GetUint32Prop(ctx, termVal.Get(), SBMD_KEY_CLUSTER_ID, 0);
            data.commandId = GetUint32Prop(ctx, termVal.Get(), SBMD_KEY_COMMAND_ID, 0);
            data.tlvBase64 = GetStringProp(ctx, termVal.Get(), SBMD_KEY_TLV_BASE64);

            // options: { endpointId?, timedInvokeTimeoutMs?, successValue? }
            SafeJSValue opts(ctx, JS_GetPropertyStr(ctx, termVal.Get(), SBMD_KEY_OPTIONS));

            if (!JS_IsUndefined(opts.Get()) && !JS_IsNull(opts.Get()))
            {
                data.endpointId = GetOptUint32Prop(ctx, opts.Get(), SBMD_KEY_ENDPOINT_ID);
                data.timedInvokeTimeoutMs = GetOptUint16Prop(ctx, opts.Get(), SBMD_KEY_TIMED_INVOKE_TIMEOUT_MS);
                data.successValue = GetStringProp(ctx, opts.Get(), SBMD_KEY_SUCCESS_VALUE);
            }

            return ResultTerminal {std::move(data)};
        }
        else if (opType == "writeAttribute")
        {
            ResultTerminal::WriteAttribute data;
            data.clusterId = GetUint32Prop(ctx, termVal.Get(), SBMD_KEY_CLUSTER_ID, 0);
            data.attributeId = GetUint32Prop(ctx, termVal.Get(), SBMD_KEY_ATTRIBUTE_ID, 0);
            data.tlvBase64 = GetStringProp(ctx, termVal.Get(), SBMD_KEY_TLV_BASE64);

            // options: { endpointId? }
            SafeJSValue opts(ctx, JS_GetPropertyStr(ctx, termVal.Get(), SBMD_KEY_OPTIONS));

            if (!JS_IsUndefined(opts.Get()) && !JS_IsNull(opts.Get()))
            {
                data.endpointId = GetOptUint32Prop(ctx, opts.Get(), SBMD_KEY_ENDPOINT_ID);
            }

            return ResultTerminal {std::move(data)};
        }
        else if (opType == "requestCommand")
        {
            ResultTerminal::RequestCommand data;
            data.clusterId = GetUint32Prop(ctx, termVal.Get(), SBMD_KEY_CLUSTER_ID, 0);
            data.commandId = GetUint32Prop(ctx, termVal.Get(), SBMD_KEY_COMMAND_ID, 0);
            data.tlvBase64 = GetStringProp(ctx, termVal.Get(), SBMD_KEY_TLV_BASE64);

            // Deferred handler callbacks
            SafeJSValue deferred(ctx, JS_GetPropertyStr(ctx, termVal.Get(), SBMD_KEY_DEFERRED));

            if (!JS_IsUndefined(deferred.Get()) && !JS_IsNull(deferred.Get()))
            {
                data.responseCommandId = GetUint32Prop(ctx, deferred.Get(), SBMD_KEY_RESPONSE_COMMAND_ID, 0);

                SafeJSValue onResponse(ctx, JS_GetPropertyStr(ctx, deferred.Get(), SBMD_KEY_ON_RESPONSE));

                if (!JS_IsUndefined(onResponse.Get()))
                {
                    data.onResponse = SafeJSValue {ctx, onResponse.Get()};
                }

                SafeJSValue onError(ctx, JS_GetPropertyStr(ctx, deferred.Get(), SBMD_KEY_ON_ERROR));

                if (!JS_IsUndefined(onError.Get()))
                {
                    data.onError = SafeJSValue {ctx, onError.Get()};
                }

                data.timeoutMs = GetOptUint32Prop(ctx, deferred.Get(), SBMD_KEY_TIMEOUT_MS);

                SafeJSValue context(ctx, JS_GetPropertyStr(ctx, deferred.Get(), SBMD_KEY_CONTEXT));

                if (!JS_IsUndefined(context.Get()))
                {
                    data.context = SafeJSValue {ctx, context.Get()};
                }
            }

            // options: { endpointId?, timedInvokeTimeoutMs? }
            SafeJSValue opts(ctx, JS_GetPropertyStr(ctx, termVal.Get(), SBMD_KEY_OPTIONS));

            if (!JS_IsUndefined(opts.Get()) && !JS_IsNull(opts.Get()))
            {
                data.endpointId = GetOptUint32Prop(ctx, opts.Get(), SBMD_KEY_ENDPOINT_ID);
                data.timedInvokeTimeoutMs = GetOptUint16Prop(ctx, opts.Get(), SBMD_KEY_TIMED_INVOKE_TIMEOUT_MS);
            }

            return ResultTerminal {std::move(data)};
        }
        else if (opType == "readAttribute")
        {
            ResultTerminal::ReadAttribute data;
            data.clusterId = GetUint32Prop(ctx, termVal.Get(), SBMD_KEY_CLUSTER_ID, 0);
            data.attributeId = GetUint32Prop(ctx, termVal.Get(), SBMD_KEY_ATTRIBUTE_ID, 0);

            // Deferred handler callbacks
            SafeJSValue deferred(ctx, JS_GetPropertyStr(ctx, termVal.Get(), SBMD_KEY_DEFERRED));

            if (!JS_IsUndefined(deferred.Get()) && !JS_IsNull(deferred.Get()))
            {
                SafeJSValue onResponse(ctx, JS_GetPropertyStr(ctx, deferred.Get(), SBMD_KEY_ON_RESPONSE));

                if (!JS_IsUndefined(onResponse.Get()))
                {
                    data.onResponse = SafeJSValue {ctx, onResponse.Get()};
                }

                SafeJSValue onError(ctx, JS_GetPropertyStr(ctx, deferred.Get(), SBMD_KEY_ON_ERROR));

                if (!JS_IsUndefined(onError.Get()))
                {
                    data.onError = SafeJSValue {ctx, onError.Get()};
                }

                data.timeoutMs = GetOptUint32Prop(ctx, deferred.Get(), SBMD_KEY_TIMEOUT_MS);

                SafeJSValue context(ctx, JS_GetPropertyStr(ctx, deferred.Get(), SBMD_KEY_CONTEXT));

                if (!JS_IsUndefined(context.Get()))
                {
                    data.context = SafeJSValue {ctx, context.Get()};
                }
            }

            // options: { endpointId? }
            SafeJSValue opts(ctx, JS_GetPropertyStr(ctx, termVal.Get(), SBMD_KEY_OPTIONS));

            if (!JS_IsUndefined(opts.Get()) && !JS_IsNull(opts.Get()))
            {
                data.endpointId = GetOptUint32Prop(ctx, opts.Get(), SBMD_KEY_ENDPOINT_ID);
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
