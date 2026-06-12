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

#define LOG_TAG "SbmdV4HandlerInvoker"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "SbmdV4HandlerInvoker.h"
#include "MQuickJsRuntime.h"
#include "SbmdV4ResultExecutor.h"

#include <string>
#include <variant>

extern "C" {
#include <icLog/logging.h>
#include <mquickjs/mquickjs.h>
}

// Forward-declare C APIs used by ExecuteOps. These are provided by the
// main build but not available in unit tests. The test build stubs them.
extern "C" {
extern void updateResource(const char *deviceUuid,
                           const char *endpointId,
                           const char *resourceId,
                           const char *newValue,
                           void *metadata);

extern void setMetadata(const char *deviceUuid,
                        const char *endpointId,
                        const char *name,
                        const char *value);
}

namespace barton
{
    JSValue SbmdV4HandlerInvoker::BuildBaseArgs(JSContext *ctx, const HandlerContext &hctx)
    {
        JSValue args = JS_NewObject(ctx);

        JS_SetPropertyStr(ctx, args, "deviceUuid", JS_NewString(ctx, hctx.deviceUuid.c_str()));
        JS_SetPropertyStr(ctx, args, "endpointId", JS_NewString(ctx, hctx.endpointId.c_str()));

        // Build clusterFeatureMaps object
        JSValue featureMaps = JS_NewObject(ctx);

        for (const auto &[clusterId, featureMap] : hctx.clusterFeatureMaps)
        {
            JS_SetPropertyStr(ctx, featureMaps, std::to_string(clusterId).c_str(), JS_NewUint32(ctx, featureMap));
        }

        JS_SetPropertyStr(ctx, args, "clusterFeatureMaps", featureMaps);

        return args;
    }

    JSValue SbmdV4HandlerInvoker::BuildAttributeArgs(JSContext *ctx,
                                                     const HandlerContext &hctx,
                                                     uint32_t clusterId,
                                                     uint32_t attributeId,
                                                     const std::string &tlvBase64)
    {
        JSValue args = BuildBaseArgs(ctx, hctx);

        // Add trigger info
        JSValue trigger = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, trigger, "clusterId", JS_NewUint32(ctx, clusterId));
        JS_SetPropertyStr(ctx, trigger, "attributeId", JS_NewUint32(ctx, attributeId));

        if (!tlvBase64.empty())
        {
            JS_SetPropertyStr(ctx, trigger, "tlvBase64", JS_NewString(ctx, tlvBase64.c_str()));
        }

        JS_SetPropertyStr(ctx, args, "attribute", trigger);

        return args;
    }

    JSValue SbmdV4HandlerInvoker::BuildResourceArgs(JSContext *ctx,
                                                    const HandlerContext &hctx,
                                                    const std::string &resourceId,
                                                    const std::optional<std::string> &input)
    {
        JSValue args = BuildBaseArgs(ctx, hctx);

        // Add resource info
        JSValue resource = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, resource, "resourceId", JS_NewString(ctx, resourceId.c_str()));

        if (input.has_value())
        {
            JS_SetPropertyStr(ctx, resource, "input", JS_NewString(ctx, input->c_str()));
        }
        else
        {
            JS_SetPropertyStr(ctx, resource, "input", JS_NULL);
        }

        JS_SetPropertyStr(ctx, args, "resource", resource);

        return args;
    }

    std::optional<ParsedResult> SbmdV4HandlerInvoker::InvokeHandler(JSContext *ctx, JSValue handler, JSValue args)
    {
        if (JS_IsUndefined(handler))
        {
            icError("handler is undefined");
            return std::nullopt;
        }

        if (JS_StackCheck(ctx, 3))
        {
            icError("stack overflow before handler call");
            return std::nullopt;
        }

        // Stack order for JS_Call: arg, func, this
        JS_PushArg(ctx, args);
        JS_PushArg(ctx, handler);
        JS_PushArg(ctx, JS_NULL);

        // Arm the execution timeout
        MQuickJsRuntime::SetDeadline(std::chrono::steady_clock::now() + std::chrono::milliseconds(5000));

        JSValue result = JS_Call(ctx, 1);

        MQuickJsRuntime::ClearDeadline();

        if (JS_IsException(result))
        {
            std::string err;
            MQuickJsRuntime::CheckAndClearPendingException(ctx, &err);
            icError("handler threw exception: %s", err.c_str());
            return std::nullopt;
        }

        return SbmdV4ResultExecutor::Parse(ctx, result);
    }

    void SbmdV4HandlerInvoker::ExecuteOps(const HandlerContext &hctx, const std::vector<ResultOp> &ops)
    {
        for (const auto &op : ops)
        {
            if (std::holds_alternative<ResultOp::UpdateResource>(op.data))
            {
                const auto &ur = std::get<ResultOp::UpdateResource>(op.data);
                const char *epId = ur.endpoint.has_value() ? ur.endpoint->c_str() : hctx.endpointId.c_str();

                updateResource(hctx.deviceUuid.c_str(), epId, ur.resource.c_str(), ur.value.c_str(), nullptr);
            }
            else if (std::holds_alternative<ResultOp::SetMetadata>(op.data))
            {
                const auto &sm = std::get<ResultOp::SetMetadata>(op.data);
                setMetadata(hctx.deviceUuid.c_str(), sm.endpoint.c_str(), sm.key.c_str(), sm.value.c_str());
            }
            else if (std::holds_alternative<ResultOp::SetPersistentData>(op.data))
            {
                const auto &sp = std::get<ResultOp::SetPersistentData>(op.data);
                icDebug("setPersistentData('%s', '%s') — not yet implemented", sp.key.c_str(), sp.value.c_str());
                // TODO: implement persistent data storage
            }
            else if (std::holds_alternative<ResultOp::SetTransientData>(op.data))
            {
                const auto &st = std::get<ResultOp::SetTransientData>(op.data);
                icDebug("setTransientData('%s', '%s') — not yet implemented", st.key.c_str(), st.value.c_str());
                // TODO: implement transient data storage
            }
            else if (std::holds_alternative<ResultOp::Log>(op.data))
            {
                const auto &log = std::get<ResultOp::Log>(op.data);
                icInfo("sbmd: %s", log.message.c_str());
            }
        }
    }

} // namespace barton
