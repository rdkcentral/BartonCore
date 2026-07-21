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
    // Observability metric handles
    ObservabilityHistogram *SbmdHandlerInvoker::handlerDurationHisto = nullptr;
    ObservabilityHistogram *SbmdHandlerInvoker::heapDeltaHisto = nullptr;
    ObservabilityCounter *SbmdHandlerInvoker::handlerOutcomeCounter = nullptr;

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

    std::optional<ParsedResult> SbmdHandlerInvoker::InvokeHandler(JSContext *ctx,
                                                                  JSValue handler,
                                                                  const SafeJSValue &args,
                                                                  const OperationContext *opCtx)
    {
        // Root the handler function: JS_StackCheck below may allocate (grow the JS stack) and the
        // moving GC would then relocate an unrooted handler, leaving the pushed argument stale.
        SafeJSValue handlerRooted(ctx, handler);

        if (JS_IsUndefined(handlerRooted.Get()))
        {
            icError("handler is undefined");
            return std::nullopt;
        }

        if (JS_StackCheck(ctx, 3)) // args, handler, this
        {
            icError("stack overflow before handler call");
            RecordOutcomeError(opCtx ? opCtx->driverName.c_str() : nullptr,
                               opCtx ? opCtx->opType.c_str() : nullptr,
                               (opCtx && !opCtx->resourceId.empty()) ? opCtx->resourceId.c_str() : nullptr,
                               "stack_overflow");
            return std::nullopt;
        }

        // Capture pre-call heap state
        JSMemoryUsage usageBefore = {};
        JS_GetMemoryUsage(ctx, &usageBefore, 0);
        auto callStart = std::chrono::steady_clock::now();

        // Stack order for JS_Call: arg, func, this
        JS_PushArg(ctx, args.Get());
        JS_PushArg(ctx, handlerRooted.Get());
        JS_PushArg(ctx, JS_NULL);

        // Arm the execution timeout
        MQuickJsRuntime::SetDeadline(std::chrono::steady_clock::now() +
                                     std::chrono::milliseconds(BARTON_CONFIG_SBMD_SCRIPT_TIMEOUT_MS));

        JSValue result = JS_Call(ctx, 1);

        // Capture the deadline before clearing it (needed for timeout detection)
        auto deadlineAtCall = MQuickJsRuntime::GetDeadline();
        MQuickJsRuntime::ClearDeadline();

        // Capture post-call state
        JSMemoryUsage usageAfter = {};
        JS_GetMemoryUsage(ctx, &usageAfter, 0);

        auto callEnd = std::chrono::steady_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(callEnd - callStart).count();
        double heapDelta = static_cast<double>(usageAfter.heap_used) - static_cast<double>(usageBefore.heap_used);

        // Extract context fields once — used for histogram attrs and outcome counter below.
        const char *outDriver = opCtx ? opCtx->driverName.c_str() : nullptr;
        const char *outOpType = opCtx ? opCtx->opType.c_str() : nullptr;
        const char *outResourceId = (opCtx && !opCtx->resourceId.empty()) ? opCtx->resourceId.c_str() : nullptr;

        // Record duration and heap-delta histograms.
        // Both histograms use the same attribute set, so share one lambda.
        auto recordHisto = [&](ObservabilityHistogram *histo, double value) {
            if (!opCtx)
            {
                observabilityHistogramRecord(histo, value);
                return;
            }

            if (outResourceId)
            {
                observabilityHistogramRecordWithAttrs(histo,
                                                      value,
                                                      "driver",
                                                      outDriver ? outDriver : "",
                                                      "op_type",
                                                      outOpType ? outOpType : "",
                                                      "resource_id",
                                                      outResourceId,
                                                      nullptr);
            }
            else
            {
                observabilityHistogramRecordWithAttrs(
                    histo, value, "driver", outDriver ? outDriver : "", "op_type", outOpType ? outOpType : "", nullptr);
            }
        };
        recordHisto(handlerDurationHisto, durationMs);
        recordHisto(heapDeltaHisto, heapDelta);

        // Update running heap snapshot (ctx is live; caller holds JS mutex)
        MQuickJsRuntime::RecordHeapSnapshot(usageAfter, JS_GetGCRootCount(ctx));
        MQuickJsRuntime::TickleSampler();

        if (JS_IsException(result))
        {
            std::string err;
            MQuickJsRuntime::CheckAndClearPendingException(ctx, &err);
            icError("handler threw exception: %s", err.c_str());

            // Distinguish timeout from handler exception
            bool isTimeout = (std::chrono::steady_clock::now() > deadlineAtCall) &&
                             (deadlineAtCall != std::chrono::steady_clock::time_point {});
            RecordOutcomeError(outDriver, outOpType, outResourceId, isTimeout ? "timeout" : "exception");

            return std::nullopt;
        }

        auto parsed = SbmdResultExecutor::Parse(ctx, result);

        if (parsed.has_value())
        {
            bool isError = std::holds_alternative<ResultTerminal::Error>(parsed->terminal.data);
            RecordOutcomeError(outDriver, outOpType, outResourceId, isError ? "error" : "success");
        }

        return parsed;
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

    void SbmdHandlerInvoker::AddSupplements(JSContext *ctx,
                                            SafeJSValue &args,
                                            const SbmdSupplements &declared,
                                            const FetchedSupplements &fetched)
    {
        // Contract: every supplement a driver DECLARES is materialized as a JS property on
        // args.supplements at handler invocation. The declaration -- not the fetched data --
        // is the source of truth, so a declared key is always defined: it holds the fetched
        // value when available, otherwise null. This holds even when the prefetch was skipped
        // (e.g. no device) or a fetch bug failed to populate it. Categories and keys that were
        // not declared are omitted.
        bool anyDeclared = !declared.attributes.empty() || !declared.resources.empty() ||
                           !declared.persistentData.empty() || !declared.transientData.empty();

        if (!anyDeclared)
        {
            return;
        }

        // Nested nodes are created-and-attached atomically via AddObject, then populated
        // through the reachable child. args is already a SafeJSValue owned by the caller.
        SafeJSValue supObj = args.AddObject(SBMD_KEY_SUPPLEMENTS);

        auto populate = [](SafeJSValue &parent,
                           const char *categoryKey,
                           const std::vector<std::string> &declaredKeys,
                           const std::vector<FetchedSupplement> &fetchedEntries) {
            if (declaredKeys.empty())
            {
                return;
            }

            SafeJSValue obj = parent.AddObject(categoryKey);

            for (const auto &declaredKey : declaredKeys)
            {
                const std::string *value = nullptr;

                for (const auto &entry : fetchedEntries)
                {
                    if (entry.key == declaredKey && entry.value.has_value())
                    {
                        value = &entry.value.value();
                        break;
                    }
                }

                if (value != nullptr)
                {
                    obj.SetString(declaredKey.c_str(), value->c_str());
                }
                else
                {
                    obj.SetNull(declaredKey.c_str());
                }
            }
        };

        populate(supObj, SBMD_KEY_ATTRIBUTES, declared.attributes, fetched.attributes);
        populate(supObj, SBMD_KEY_RESOURCES, declared.resources, fetched.resources);
        populate(supObj, SBMD_KEY_PERSISTENT_DATA, declared.persistentData, fetched.persistentData);
        populate(supObj, SBMD_KEY_TRANSIENT_DATA, declared.transientData, fetched.transientData);
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

                // If the op names an endpoint, update that endpoint resource; otherwise the update
                // targets a device-level resource (see updateResource: a null endpoint is the
                // device root).
                const char *epId = ur.endpoint.has_value() ? ur.endpoint->c_str() : nullptr;

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
        // Root handlerContext: the arg-building allocations below can relocate it under the
        // moving GC before it is attached.
        SafeJSValue handlerContextRooted(ctx, handlerContext);
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

        if (!JS_IsUndefined(handlerContextRooted.Get()))
        {
            args.SetValue(SBMD_KEY_HANDLER_CONTEXT, handlerContextRooted.Get());
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
        SafeJSValue handlerContextRooted(ctx, handlerContext);
        SafeJSValue args = BuildBaseArgs(ctx, hctx);

        SafeJSValue attribute = args.AddObject(SBMD_KEY_ATTRIBUTE);
        attribute.SetUint32(SBMD_KEY_CLUSTER_ID, clusterId);
        attribute.SetUint32(SBMD_KEY_ATTRIBUTE_ID, attributeId);
        attribute.SetString(SBMD_KEY_VALUE, tlvBase64.c_str());

        if (!JS_IsUndefined(handlerContextRooted.Get()))
        {
            args.SetValue(SBMD_KEY_HANDLER_CONTEXT, handlerContextRooted.Get());
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
        SafeJSValue handlerContextRooted(ctx, handlerContext);
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

        if (!JS_IsUndefined(handlerContextRooted.Get()))
        {
            args.SetValue(SBMD_KEY_HANDLER_CONTEXT, handlerContextRooted.Get());
        }
        else
        {
            args.SetNull(SBMD_KEY_HANDLER_CONTEXT);
        }

        return args;
    }

    void SbmdHandlerInvoker::InitializeMetrics()
    {
        handlerDurationHisto = observabilityHistogramCreate(
            "sbmd.handler.duration_ms", "Time from JS_Call entry to return for each handler invocation", "ms");
        heapDeltaHisto = observabilityHistogramCreate(
            "sbmd.handler.heap_delta_bytes", "Change in heap_used across a single handler call", "By");
        handlerOutcomeCounter = observabilityCounterCreate(
            "sbmd.handler.outcome", "Count of handler invocations by outcome", "{invocation}");
    }

    void SbmdHandlerInvoker::ShutdownMetrics()
    {
        observabilityHistogramRelease(handlerDurationHisto);
        handlerDurationHisto = nullptr;
        observabilityHistogramRelease(heapDeltaHisto);
        heapDeltaHisto = nullptr;
        observabilityCounterRelease(handlerOutcomeCounter);
        handlerOutcomeCounter = nullptr;
    }

    void SbmdHandlerInvoker::RecordOutcomeError(const char *driver,
                                                const char *opType,
                                                const char *resourceId,
                                                const char *outcome)
    {
        if (!handlerOutcomeCounter)
        {
            return;
        }

        if (driver || opType || resourceId)
        {
            if (resourceId)
            {
                observabilityCounterAddWithAttrs(handlerOutcomeCounter,
                                                 1,
                                                 "driver",
                                                 driver ? driver : "",
                                                 "op_type",
                                                 opType ? opType : "",
                                                 "resource_id",
                                                 resourceId,
                                                 "outcome",
                                                 outcome,
                                                 nullptr);
            }
            else
            {
                observabilityCounterAddWithAttrs(handlerOutcomeCounter,
                                                 1,
                                                 "driver",
                                                 driver ? driver : "",
                                                 "op_type",
                                                 opType ? opType : "",
                                                 "outcome",
                                                 outcome,
                                                 nullptr);
            }
        }
        else
        {
            observabilityCounterAddWithAttrs(handlerOutcomeCounter, 1, "outcome", outcome, nullptr);
        }
    }

} // namespace barton
