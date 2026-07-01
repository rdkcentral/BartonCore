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
    JSValue SbmdHandlerInvoker::BuildBaseArgs(JSContext *ctx, const HandlerContext &hctx)
    {
        JSValue args = JS_NewObject(ctx);

        // GC-root args during construction: subsequent allocations (JS_NewString,
        // JS_NewObject) may trigger mquickjs GC which would sweep the unrooted args.
        JSGCRef argsRef {};
        JS_AddGCRef(ctx, &argsRef);
        argsRef.val = args;


        JS_SetPropertyStr(ctx, args, SBMD_KEY_DEVICE_UUID, JS_NewString(ctx, hctx.deviceUuid.c_str()));
        JS_SetPropertyStr(ctx, args, SBMD_KEY_ENDPOINT_ID, JS_NewString(ctx, hctx.endpointId.c_str()));

        // Build clusterFeatureMaps object — attach to args BEFORE populating
        // so that featureMaps is reachable from the GC-rooted args during the loop.
        JSValue featureMaps = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, args, SBMD_KEY_CLUSTER_FEATURE_MAPS, featureMaps);

        for (const auto &[clusterId, featureMap] : hctx.clusterFeatureMaps)
        {
            JS_SetPropertyStr(ctx, featureMaps, std::to_string(clusterId).c_str(), JS_NewUint32(ctx, featureMap));
        }

        JS_DeleteGCRef(ctx, &argsRef);

        return args;
    }

    JSValue SbmdHandlerInvoker::BuildAttributeArgs(JSContext *ctx,
                                                   const HandlerContext &hctx,
                                                   uint32_t clusterId,
                                                   uint32_t attributeId,
                                                   const std::string &tlvBase64)
    {
        JSValue args = BuildBaseArgs(ctx, hctx);

        JSGCRef argsRef {};
        JS_AddGCRef(ctx, &argsRef);
        argsRef.val = args;


        // Add trigger info — attach to args first so trigger is reachable from GC root
        JSValue trigger = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, args, SBMD_KEY_ATTRIBUTE, trigger);
        JS_SetPropertyStr(ctx, trigger, SBMD_KEY_CLUSTER_ID, JS_NewUint32(ctx, clusterId));
        JS_SetPropertyStr(ctx, trigger, SBMD_KEY_ATTRIBUTE_ID, JS_NewUint32(ctx, attributeId));

        if (!tlvBase64.empty())
        {
            JS_SetPropertyStr(ctx, trigger, SBMD_KEY_TLV_BASE64, JS_NewString(ctx, tlvBase64.c_str()));
        }

        JS_DeleteGCRef(ctx, &argsRef);

        return args;
    }

    JSValue SbmdHandlerInvoker::BuildEventArgs(JSContext *ctx,
                                               const HandlerContext &hctx,
                                               uint32_t clusterId,
                                               uint32_t eventId,
                                               const std::string &tlvBase64)
    {
        JSValue args = BuildBaseArgs(ctx, hctx);

        JSGCRef argsRef {};
        JS_AddGCRef(ctx, &argsRef);
        argsRef.val = args;


        // Add trigger info — attach to args first so trigger is reachable from GC root
        JSValue trigger = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, args, SBMD_KEY_EVENT, trigger);
        JS_SetPropertyStr(ctx, trigger, SBMD_KEY_CLUSTER_ID, JS_NewUint32(ctx, clusterId));
        JS_SetPropertyStr(ctx, trigger, SBMD_KEY_EVENT_ID, JS_NewUint32(ctx, eventId));

        if (!tlvBase64.empty())
        {
            JS_SetPropertyStr(ctx, trigger, SBMD_KEY_TLV_BASE64, JS_NewString(ctx, tlvBase64.c_str()));
        }

        JS_DeleteGCRef(ctx, &argsRef);

        return args;
    }

    JSValue SbmdHandlerInvoker::BuildCommandArgs(JSContext *ctx,
                                                 const HandlerContext &hctx,
                                                 uint32_t clusterId,
                                                 uint32_t commandId,
                                                 const std::string &tlvBase64)
    {
        JSValue args = BuildBaseArgs(ctx, hctx);

        JSGCRef argsRef {};
        JS_AddGCRef(ctx, &argsRef);
        argsRef.val = args;


        // Add trigger info — attach to args first so trigger is reachable from GC root
        JSValue trigger = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, args, SBMD_KEY_COMMAND, trigger);
        JS_SetPropertyStr(ctx, trigger, SBMD_KEY_CLUSTER_ID, JS_NewUint32(ctx, clusterId));
        JS_SetPropertyStr(ctx, trigger, SBMD_KEY_COMMAND_ID, JS_NewUint32(ctx, commandId));

        if (!tlvBase64.empty())
        {
            JS_SetPropertyStr(ctx, trigger, SBMD_KEY_TLV_BASE64, JS_NewString(ctx, tlvBase64.c_str()));
        }

        JS_DeleteGCRef(ctx, &argsRef);

        return args;
    }

    JSValue SbmdHandlerInvoker::BuildResourceArgs(JSContext *ctx,
                                                  const HandlerContext &hctx,
                                                  const std::string &resourceId,
                                                  const std::optional<std::string> &input)
    {
        JSValue args = BuildBaseArgs(ctx, hctx);

        JSGCRef argsRef {};
        JS_AddGCRef(ctx, &argsRef);
        argsRef.val = args;


        // Add resource info — attach to args first so resource is reachable from GC root
        JSValue resource = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, args, SBMD_KEY_RESOURCE, resource);
        JS_SetPropertyStr(ctx, resource, SBMD_KEY_RESOURCE_ID, JS_NewString(ctx, resourceId.c_str()));

        if (input.has_value())
        {
            JS_SetPropertyStr(ctx, resource, SBMD_KEY_INPUT, JS_NewString(ctx, input->c_str()));
        }
        else
        {
            JS_SetPropertyStr(ctx, resource, SBMD_KEY_INPUT, JS_NULL);
        }

        JS_DeleteGCRef(ctx, &argsRef);

        return args;
    }

    std::optional<ParsedResult> SbmdHandlerInvoker::InvokeHandler(JSContext *ctx, JSValue handler, JSValue args)
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

        return SbmdResultExecutor::Parse(ctx, result);
    }

    void SbmdHandlerInvoker::AddSupplements(JSContext *ctx,
                                            JSValue args,
                                            const SbmdSupplements &supplements,
                                            const AttributeSupplementFetcher &attrFetcher,
                                            const ResourceSupplementFetcher &resFetcher,
                                            const PersistentDataFetcher &persistFetcher,
                                            const TransientDataFetcher &transientFetcher)
    {
        if (supplements.attributes.empty() && supplements.resources.empty() && supplements.persistentData.empty() &&
            supplements.transientData.empty())
        {
            return;
        }

        // Attach supObj to args immediately so it is reachable from the caller's
        // GC-rooted args.  Subsequent allocations (JS_NewObject, JS_NewString) may
        // trigger GC; without this attachment supObj would be swept.
        JSValue supObj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, args, SBMD_KEY_SUPPLEMENTS, supObj);

        if (!supplements.attributes.empty())
        {
            JSValue attrsObj = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, supObj, SBMD_KEY_ATTRIBUTES, attrsObj);

            for (const auto &aliasName : supplements.attributes)
            {
                auto value = attrFetcher(aliasName);

                if (value.has_value())
                {
                    JS_SetPropertyStr(ctx, attrsObj, aliasName.c_str(), JS_NewString(ctx, value->c_str()));
                }
                else
                {
                    JS_SetPropertyStr(ctx, attrsObj, aliasName.c_str(), JS_NULL);
                }
            }
        }

        if (!supplements.resources.empty())
        {
            JSValue resObj = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, supObj, SBMD_KEY_RESOURCES, resObj);

            for (const auto &path : supplements.resources)
            {
                auto value = resFetcher(path);

                if (value.has_value())
                {
                    JS_SetPropertyStr(ctx, resObj, path.c_str(), JS_NewString(ctx, value->c_str()));
                }
                else
                {
                    JS_SetPropertyStr(ctx, resObj, path.c_str(), JS_NULL);
                }
            }
        }

        if (!supplements.persistentData.empty())
        {
            JSValue pdObj = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, supObj, SBMD_KEY_PERSISTENT_DATA, pdObj);

            for (const auto &key : supplements.persistentData)
            {
                auto value = persistFetcher(key);

                if (value.has_value())
                {
                    JS_SetPropertyStr(ctx, pdObj, key.c_str(), JS_NewString(ctx, value->c_str()));
                }
                else
                {
                    JS_SetPropertyStr(ctx, pdObj, key.c_str(), JS_NULL);
                }
            }
        }

        if (!supplements.transientData.empty())
        {
            JSValue tdObj = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, supObj, SBMD_KEY_TRANSIENT_DATA, tdObj);

            for (const auto &key : supplements.transientData)
            {
                auto value = transientFetcher(key);

                if (value.has_value())
                {
                    JS_SetPropertyStr(ctx, tdObj, key.c_str(), JS_NewString(ctx, value->c_str()));
                }
                else
                {
                    JS_SetPropertyStr(ctx, tdObj, key.c_str(), JS_NULL);
                }
            }
        }
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

    JSValue SbmdHandlerInvoker::BuildCommandResponseArgs(JSContext *ctx,
                                                         const HandlerContext &hctx,
                                                         uint32_t clusterId,
                                                         uint32_t commandId,
                                                         const std::string &tlvBase64,
                                                         JSValue handlerContext)
    {
        JSValue args = BuildBaseArgs(ctx, hctx);

        JSGCRef argsRef {};
        JS_AddGCRef(ctx, &argsRef);
        argsRef.val = args;


        JSValue response = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, response, SBMD_KEY_CLUSTER_ID, JS_NewUint32(ctx, clusterId));
        JS_SetPropertyStr(ctx, response, SBMD_KEY_COMMAND_ID, JS_NewUint32(ctx, commandId));

        if (!tlvBase64.empty())
        {
            JS_SetPropertyStr(ctx, response, SBMD_KEY_DATA, JS_NewString(ctx, tlvBase64.c_str()));
        }
        else
        {
            JS_SetPropertyStr(ctx, response, SBMD_KEY_DATA, JS_NULL);
        }

        JS_SetPropertyStr(ctx, args, SBMD_KEY_RESPONSE, response);

        if (!JS_IsUndefined(handlerContext))
        {
            JS_SetPropertyStr(ctx, args, SBMD_KEY_HANDLER_CONTEXT, handlerContext);
        }
        else
        {
            JS_SetPropertyStr(ctx, args, SBMD_KEY_HANDLER_CONTEXT, JS_NULL);
        }

        JS_DeleteGCRef(ctx, &argsRef);

        return args;
    }

    JSValue SbmdHandlerInvoker::BuildAttributeReadResponseArgs(JSContext *ctx,
                                                               const HandlerContext &hctx,
                                                               uint32_t clusterId,
                                                               uint32_t attributeId,
                                                               const std::string &tlvBase64,
                                                               JSValue handlerContext)
    {
        JSValue args = BuildBaseArgs(ctx, hctx);

        JSGCRef argsRef {};
        JS_AddGCRef(ctx, &argsRef);
        argsRef.val = args;


        JSValue attribute = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, attribute, SBMD_KEY_CLUSTER_ID, JS_NewUint32(ctx, clusterId));
        JS_SetPropertyStr(ctx, attribute, SBMD_KEY_ATTRIBUTE_ID, JS_NewUint32(ctx, attributeId));
        JS_SetPropertyStr(ctx, attribute, SBMD_KEY_VALUE, JS_NewString(ctx, tlvBase64.c_str()));
        JS_SetPropertyStr(ctx, args, SBMD_KEY_ATTRIBUTE, attribute);

        if (!JS_IsUndefined(handlerContext))
        {
            JS_SetPropertyStr(ctx, args, SBMD_KEY_HANDLER_CONTEXT, handlerContext);
        }
        else
        {
            JS_SetPropertyStr(ctx, args, SBMD_KEY_HANDLER_CONTEXT, JS_NULL);
        }

        JS_DeleteGCRef(ctx, &argsRef);

        return args;
    }

    JSValue SbmdHandlerInvoker::BuildDeferredErrorArgs(JSContext *ctx,
                                                       const HandlerContext &hctx,
                                                       const std::string &errorType,
                                                       const std::string &errorMessage,
                                                       int32_t matterCode,
                                                       JSValue handlerContext)
    {
        JSValue args = BuildBaseArgs(ctx, hctx);

        JSGCRef argsRef {};
        JS_AddGCRef(ctx, &argsRef);
        argsRef.val = args;


        JSValue error = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, error, SBMD_KEY_TYPE, JS_NewString(ctx, errorType.c_str()));
        JS_SetPropertyStr(ctx, error, SBMD_KEY_MESSAGE, JS_NewString(ctx, errorMessage.c_str()));

        if (matterCode >= 0)
        {
            JS_SetPropertyStr(ctx, error, SBMD_KEY_MATTER_CODE, JS_NewInt32(ctx, matterCode));
        }
        else
        {
            JS_SetPropertyStr(ctx, error, SBMD_KEY_MATTER_CODE, JS_NULL);
        }

        JS_SetPropertyStr(ctx, args, SBMD_KEY_ERROR, error);

        if (!JS_IsUndefined(handlerContext))
        {
            JS_SetPropertyStr(ctx, args, SBMD_KEY_HANDLER_CONTEXT, handlerContext);
        }
        else
        {
            JS_SetPropertyStr(ctx, args, SBMD_KEY_HANDLER_CONTEXT, JS_NULL);
        }

        JS_DeleteGCRef(ctx, &argsRef);

        return args;
    }

} // namespace barton
