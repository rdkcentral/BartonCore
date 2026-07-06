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

#define LOG_TAG     "SbmdHandlerInvoker"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "SbmdHandlerInvoker.h"
#include "MQuickJsRuntime.h"
#include "SbmdResultExecutor.h"
#include "SbmdWireContract.h"
#include "matter/sbmd/SafeJSValue.h"

#include <string>
#include <variant>

extern "C" {
#include <cjson/cJSON.h>
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

extern void setMetadata(const char *deviceUuid, const char *endpointId, const char *name, const char *value);

extern bool deviceServiceSetMetadata(const char *uri, const char *value);
}

namespace barton
{
    SafeJSValue SbmdHandlerInvoker::BuildBaseArgs(JSContext *ctx, const HandlerContext &hctx)
    {
        // Root args for the whole build: subsequent allocations (JS_NewString, JS_NewObject)
        // may trigger the mquickjs moving GC, which would otherwise sweep the unrooted args.
        SafeJSValue args {ctx, JS_NewObject(ctx)};

        args.SetString(SBMD_KEY_DEVICE_UUID, hctx.deviceUuid.c_str());
        args.SetString(SBMD_KEY_ENDPOINT_ID, hctx.endpointId.c_str());

        // clusterFeatureMaps is created-and-attached atomically, then populated through the
        // reachable child node.
        SafeJSValue featureMaps = args.AddObject(SBMD_KEY_CLUSTER_FEATURE_MAPS);

        for (const auto &[clusterId, featureMap] : hctx.clusterFeatureMaps)
        {
            featureMaps.SetUint32(std::to_string(clusterId).c_str(), featureMap);
        }

        return args;
    }

    SafeJSValue SbmdHandlerInvoker::BuildAttributeArgs(JSContext *ctx,
                                                       const HandlerContext &hctx,
                                                       uint32_t clusterId,
                                                       uint32_t attributeId,
                                                       const std::string &tlvBase64)
    {
        SafeJSValue args = BuildBaseArgs(ctx, hctx);

        SafeJSValue trigger = args.AddObject(SBMD_KEY_ATTRIBUTE);
        trigger.SetUint32(SBMD_KEY_CLUSTER_ID, clusterId);
        trigger.SetUint32(SBMD_KEY_ATTRIBUTE_ID, attributeId);

        if (!tlvBase64.empty())
        {
            trigger.SetString(SBMD_KEY_TLV_BASE64, tlvBase64.c_str());
        }

        return args;
    }

    SafeJSValue SbmdHandlerInvoker::BuildEventArgs(JSContext *ctx,
                                                   const HandlerContext &hctx,
                                                   uint32_t clusterId,
                                                   uint32_t eventId,
                                                   const std::string &tlvBase64)
    {
        SafeJSValue args = BuildBaseArgs(ctx, hctx);

        SafeJSValue trigger = args.AddObject(SBMD_KEY_EVENT);
        trigger.SetUint32(SBMD_KEY_CLUSTER_ID, clusterId);
        trigger.SetUint32(SBMD_KEY_EVENT_ID, eventId);

        if (!tlvBase64.empty())
        {
            trigger.SetString(SBMD_KEY_TLV_BASE64, tlvBase64.c_str());
        }

        return args;
    }

    SafeJSValue SbmdHandlerInvoker::BuildCommandArgs(JSContext *ctx,
                                                     const HandlerContext &hctx,
                                                     uint32_t clusterId,
                                                     uint32_t commandId,
                                                     const std::string &tlvBase64)
    {
        SafeJSValue args = BuildBaseArgs(ctx, hctx);

        SafeJSValue trigger = args.AddObject(SBMD_KEY_COMMAND);
        trigger.SetUint32(SBMD_KEY_CLUSTER_ID, clusterId);
        trigger.SetUint32(SBMD_KEY_COMMAND_ID, commandId);

        if (!tlvBase64.empty())
        {
            trigger.SetString(SBMD_KEY_TLV_BASE64, tlvBase64.c_str());
        }

        return args;
    }

    SafeJSValue SbmdHandlerInvoker::BuildResourceArgs(JSContext *ctx,
                                                      const HandlerContext &hctx,
                                                      const std::string &resourceId,
                                                      const std::optional<std::string> &input)
    {
        SafeJSValue args = BuildBaseArgs(ctx, hctx);

        SafeJSValue resource = args.AddObject(SBMD_KEY_RESOURCE);
        resource.SetString(SBMD_KEY_RESOURCE_ID, resourceId.c_str());

        if (input.has_value())
        {
            resource.SetString(SBMD_KEY_INPUT, input->c_str());
        }
        else
        {
            resource.SetNull(SBMD_KEY_INPUT);
        }

        return args;
    }

    std::optional<ParsedResult>
    SbmdHandlerInvoker::InvokeHandler(JSContext *ctx, JSValue handler, const SafeJSValue &args)
    {
        if (JS_IsUndefined(handler))
        {
            icError("handler is undefined");
            return std::nullopt;
        }

        if (JS_StackCheck(ctx, 3)) // args, handler, this
        {
            icError("stack overflow before handler call");
            return std::nullopt;
        }


        // Stack order for JS_Call: arg, func, this
        JS_PushArg(ctx, args.Get());
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

        return SbmdResultExecutor::Parse(ctx, result);
    }

    FetchedSupplements SbmdHandlerInvoker::PrefetchSupplements(const SbmdSupplements &supplements,
                                                               const AttributeSupplementFetcher &attrFetcher,
                                                               const ResourceSupplementFetcher &resFetcher,
                                                               const PersistentDataFetcher &persistFetcher,
                                                               const TransientDataFetcher &transientFetcher)
    {
        FetchedSupplements fetched;

        for (const auto &aliasName : supplements.attributes)
        {
            fetched.attributes.push_back({aliasName, attrFetcher(aliasName)});
        }

        for (const auto &path : supplements.resources)
        {
            fetched.resources.push_back({path, resFetcher(path)});
        }

        for (const auto &key : supplements.persistentData)
        {
            fetched.persistentData.push_back({key, persistFetcher(key)});
        }

        for (const auto &key : supplements.transientData)
        {
            fetched.transientData.push_back({key, transientFetcher(key)});
        }

        return fetched;
    }

    void SbmdHandlerInvoker::AddSupplements(JSContext *ctx, SafeJSValue &args, const FetchedSupplements &fetched)
    {
        if (fetched.empty())
        {
            return;
        }

        // Nested nodes are created-and-attached atomically via AddObject, then populated
        // through the reachable child. args is already a SafeJSValue owned by the caller.
        SafeJSValue supObj = args.AddObject(SBMD_KEY_SUPPLEMENTS);

        auto populate = [](SafeJSValue &parent, const char *key, const std::vector<FetchedSupplement> &entries) {
            if (entries.empty())
            {
                return;
            }

            SafeJSValue obj = parent.AddObject(key);

            for (const auto &entry : entries)
            {
                if (entry.value.has_value())
                {
                    obj.SetString(entry.key.c_str(), entry.value->c_str());
                }
                else
                {
                    obj.SetNull(entry.key.c_str());
                }
            }
        };

        populate(supObj, SBMD_KEY_ATTRIBUTES, fetched.attributes);
        populate(supObj, SBMD_KEY_RESOURCES, fetched.resources);
        populate(supObj, SBMD_KEY_PERSISTENT_DATA, fetched.persistentData);
        populate(supObj, SBMD_KEY_TRANSIENT_DATA, fetched.transientData);
    }

    void SbmdHandlerInvoker::ExecuteOps(const HandlerContext &hctx,
                                        const std::vector<ResultOp> &ops,
                                        const TransientDataSetter &transientSetter)
    {
        for (const auto &op : ops)
        {
            if (std::holds_alternative<ResultOp::UpdateResource>(op.data))
            {
                const auto &ur = std::get<ResultOp::UpdateResource>(op.data);

                // If the op names an endpoint explicitly, write there; otherwise fall back to the
                // endpoint that triggered this handler. The fallback is the common case: most ops
                // update a resource on the same endpoint that reported the change.
                const char *epId = ur.endpoint.has_value() ? ur.endpoint->c_str() : hctx.endpointId.c_str();

                cJSON *meta = nullptr;

                if (ur.metadata.has_value())
                {
                    meta = cJSON_Parse(ur.metadata->c_str());
                }

                updateResource(hctx.deviceUuid.c_str(), epId, ur.resource.c_str(), ur.value.c_str(), meta);

                if (meta != nullptr)
                {
                    cJSON_Delete(meta);
                }
            }
            else if (std::holds_alternative<ResultOp::SetMetadata>(op.data))
            {
                const auto &sm = std::get<ResultOp::SetMetadata>(op.data);
                setMetadata(hctx.deviceUuid.c_str(), nullptr, sm.name.c_str(), sm.value.c_str());
            }
            else if (std::holds_alternative<ResultOp::SetPersistentData>(op.data))
            {
                const auto &sp = std::get<ResultOp::SetPersistentData>(op.data);
                std::string uri = "/devices/" + hctx.deviceUuid + "/metadata/sbmd." + sp.key;

                if (!deviceServiceSetMetadata(uri.c_str(), sp.value.c_str()))
                {
                    icError("failed to set persistent data '%s'", sp.key.c_str());
                }
            }
            else if (std::holds_alternative<ResultOp::SetTransientData>(op.data))
            {
                const auto &st = std::get<ResultOp::SetTransientData>(op.data);

                if (transientSetter)
                {
                    transientSetter(st.key, st.value, st.ttlSecs);
                }
                else
                {
                    icWarn("setTransientData('%s') called but no transient setter provided", st.key.c_str());
                }
            }
            else if (std::holds_alternative<ResultOp::Log>(op.data))
            {
                const auto &log = std::get<ResultOp::Log>(op.data);
                icInfo("sbmd: %s", log.message.c_str());
            }
        }
    }

    SafeJSValue SbmdHandlerInvoker::BuildCommandResponseArgs(JSContext *ctx,
                                                             const HandlerContext &hctx,
                                                             uint32_t clusterId,
                                                             uint32_t commandId,
                                                             const std::string &tlvBase64,
                                                             JSValue handlerContext)
    {
        SafeJSValue args = BuildBaseArgs(ctx, hctx);

        SafeJSValue response = args.AddObject(SBMD_KEY_RESPONSE);
        response.SetUint32(SBMD_KEY_CLUSTER_ID, clusterId);
        response.SetUint32(SBMD_KEY_COMMAND_ID, commandId);

        if (!tlvBase64.empty())
        {
            response.SetString(SBMD_KEY_DATA, tlvBase64.c_str());
        }
        else
        {
            response.SetNull(SBMD_KEY_DATA);
        }

        if (!JS_IsUndefined(handlerContext))
        {
            args.SetValue(SBMD_KEY_HANDLER_CONTEXT, handlerContext);
        }
        else
        {
            args.SetNull(SBMD_KEY_HANDLER_CONTEXT);
        }

        return args;
    }

    SafeJSValue SbmdHandlerInvoker::BuildAttributeReadResponseArgs(JSContext *ctx,
                                                                   const HandlerContext &hctx,
                                                                   uint32_t clusterId,
                                                                   uint32_t attributeId,
                                                                   const std::string &tlvBase64,
                                                                   JSValue handlerContext)
    {
        SafeJSValue args = BuildBaseArgs(ctx, hctx);

        SafeJSValue attribute = args.AddObject(SBMD_KEY_ATTRIBUTE);
        attribute.SetUint32(SBMD_KEY_CLUSTER_ID, clusterId);
        attribute.SetUint32(SBMD_KEY_ATTRIBUTE_ID, attributeId);
        attribute.SetString(SBMD_KEY_VALUE, tlvBase64.c_str());

        if (!JS_IsUndefined(handlerContext))
        {
            args.SetValue(SBMD_KEY_HANDLER_CONTEXT, handlerContext);
        }
        else
        {
            args.SetNull(SBMD_KEY_HANDLER_CONTEXT);
        }

        return args;
    }

    SafeJSValue SbmdHandlerInvoker::BuildDeferredErrorArgs(JSContext *ctx,
                                                           const HandlerContext &hctx,
                                                           const std::string &errorType,
                                                           const std::string &errorMessage,
                                                           int32_t matterCode,
                                                           JSValue handlerContext)
    {
        SafeJSValue args = BuildBaseArgs(ctx, hctx);

        SafeJSValue error = args.AddObject(SBMD_KEY_ERROR);
        error.SetString(SBMD_KEY_TYPE, errorType.c_str());
        error.SetString(SBMD_KEY_MESSAGE, errorMessage.c_str());

        if (matterCode >= 0)
        {
            error.SetInt32(SBMD_KEY_MATTER_CODE, matterCode);
        }
        else
        {
            error.SetNull(SBMD_KEY_MATTER_CODE);
        }

        if (!JS_IsUndefined(handlerContext))
        {
            args.SetValue(SBMD_KEY_HANDLER_CONTEXT, handlerContext);
        }
        else
        {
            args.SetNull(SBMD_KEY_HANDLER_CONTEXT);
        }

        return args;
    }

} // namespace barton
