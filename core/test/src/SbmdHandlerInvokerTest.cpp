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
 * Unit tests for SbmdHandlerInvoker — args building, handler invocation,
 * result parsing, and non-terminal op execution.
 */

#include "deviceDrivers/matter/sbmd/mquickjs/SbmdHandlerInvoker.h"
#include "deviceDrivers/matter/sbmd/mquickjs/MQuickJsRuntime.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdBundleLoader.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdLoader.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

extern "C" {
#include <cjson/cJSON.h>
#include <mquickjs/mquickjs.h>
}

using namespace barton;

// ============================================================================
// Test stubs for C APIs called by ExecuteOps
// ============================================================================

namespace
{
    struct UpdateResourceCall
    {
        std::string deviceUuid;
        std::string endpointId;
        std::string resourceId;
        std::string value;
        std::string metadata; // JSON string, empty if null
    };

    struct SetMetadataCall
    {
        std::string deviceUuid;
        std::string endpointId;
        std::string key;
        std::string value;
    };

    struct SetPersistentDataCall
    {
        std::string uri;
        std::string value;
    };

    std::vector<UpdateResourceCall> g_updateResourceCalls;
    std::vector<SetMetadataCall> g_setMetadataCalls;
    std::vector<SetPersistentDataCall> g_setPersistentDataCalls;
} // namespace

extern "C" {
void updateResource(const char *deviceUuid,
                    const char *endpointId,
                    const char *resourceId,
                    const char *newValue,
                    void *metadata)
{
    std::string metaStr;

    if (metadata != nullptr)
    {
        char *printed = cJSON_PrintUnformatted(static_cast<cJSON *>(metadata));

        if (printed != nullptr)
        {
            metaStr = printed;
            free(printed);
        }
    }

    g_updateResourceCalls.push_back({deviceUuid ? deviceUuid : "",
                                     endpointId ? endpointId : "",
                                     resourceId ? resourceId : "",
                                     newValue ? newValue : "",
                                     metaStr});
}

void setMetadata(const char *deviceUuid, const char *endpointId, const char *name, const char *value)
{
    g_setMetadataCalls.push_back(
        {deviceUuid ? deviceUuid : "", endpointId ? endpointId : "", name ? name : "", value ? value : ""});
}

bool deviceServiceSetMetadata(const char *uri, const char *value)
{
    g_setPersistentDataCalls.push_back({uri ? uri : "", value ? value : ""});

    return true;
}
}

namespace
{
    // Test helper mirroring the pre-split AddSupplements(fetchers...) signature:
    // resolves the declared supplements via PrefetchSupplements, then attaches
    // them with AddSupplements. Keeps these tests exercising the full
    // fetch-then-attach path through the two-phase seam.
    template<typename AttrFetcher, typename ResFetcher, typename PersistFetcher, typename TransientFetcher>
    void FetchAndAddSupplements(JSContext *ctx,
                                SafeJSValue &args,
                                const SbmdSupplements &supplements,
                                AttrFetcher attrFetcher,
                                ResFetcher resFetcher,
                                PersistFetcher persistFetcher,
                                TransientFetcher transientFetcher)
    {
        FetchedSupplements fetched = SbmdHandlerInvoker::PrefetchSupplements(
            supplements, attrFetcher, resFetcher, persistFetcher, transientFetcher);
        SbmdHandlerInvoker::AddSupplements(ctx, args, supplements, fetched);
    }

    class SbmdHandlerInvokerTest : public ::testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            ASSERT_TRUE(MQuickJsRuntime::Initialize(512 * 1024));
            auto *ctx = MQuickJsRuntime::GetSharedContext();
            ASSERT_NE(ctx, nullptr);
            ASSERT_TRUE(SbmdBundleLoader::LoadBundle(ctx));
            ASSERT_TRUE(SbmdLoader::InjectCaptureFunction(ctx));
        }

        static void TearDownTestSuite() { MQuickJsRuntime::Shutdown(); }

        void SetUp() override
        {
            g_updateResourceCalls.clear();
            g_setMetadataCalls.clear();
            g_setPersistentDataCalls.clear();
        }

        JSContext *Ctx() { return MQuickJsRuntime::GetSharedContext(); }

        HandlerContext MakeContext()
        {
            HandlerContext hctx;
            hctx.deviceUuid = "test-device-uuid";
            hctx.endpointId = "1";
            hctx.clusterFeatureMaps = {
                {6, 0x01},
                {8, 0x03}
            };

            return hctx;
        }

        /**
         * Evaluate a JS expression and return it as a function JSValue.
         */
        JSValue EvalFunc(const char *expr)
        {
            auto *ctx = Ctx();

            return JS_Eval(ctx, expr, strlen(expr), "<test>", JS_EVAL_RETVAL);
        }

        /**
         * Get a string property from a JSValue object.
         */
        std::string GetStringProp(JSValue obj, const char *name)
        {
            auto *ctx = Ctx();
            JSValue val = JS_GetPropertyStr(ctx, obj, name);

            if (JS_IsUndefined(val) || JS_IsNull(val))
            {
                return "";
            }

            JSCStringBuf buf;
            const char *str = JS_ToCString(ctx, val, &buf);

            return str ? std::string(str) : "";
        }

        uint32_t GetUint32Prop(JSValue obj, const char *name)
        {
            auto *ctx = Ctx();
            JSValue val = JS_GetPropertyStr(ctx, obj, name);

            if (JS_IsUndefined(val))
            {
                return 0;
            }

            uint32_t result = 0;
            JS_ToUint32(ctx, &result, val);

            return result;
        }
    };

    // ========================================================================
    // BuildAttributeArgs
    // ========================================================================

    TEST_F(SbmdHandlerInvokerTest, BuildAttributeArgsBasicFields)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        SafeJSValue args = SbmdHandlerInvoker::BuildAttributeArgs(Ctx(), hctx, 6, 0, "AB==");
        ASSERT_FALSE(JS_IsException(args));

        EXPECT_EQ(GetStringProp(args, "deviceUuid"), "test-device-uuid");
        EXPECT_EQ(GetStringProp(args, "endpointId"), "1");

        // Check attribute trigger
        SafeJSValue attr(Ctx(), JS_GetPropertyStr(Ctx(), args, "attribute"));
        ASSERT_FALSE(JS_IsUndefined(attr.Get()));
        EXPECT_EQ(GetUint32Prop(attr.Get(), "clusterId"), 6u);
        EXPECT_EQ(GetUint32Prop(attr.Get(), "attributeId"), 0u);
        EXPECT_EQ(GetStringProp(attr.Get(), "tlvBase64"), "AB==");
    }

    TEST_F(SbmdHandlerInvokerTest, BuildAttributeArgsFeatureMaps)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        SafeJSValue args = SbmdHandlerInvoker::BuildAttributeArgs(Ctx(), hctx, 6, 0, "");

        JSValue fm = JS_GetPropertyStr(Ctx(), args, "clusterFeatureMaps");
        ASSERT_FALSE(JS_IsUndefined(fm));
        EXPECT_EQ(GetUint32Prop(fm, "6"), 0x01u);
        EXPECT_EQ(GetUint32Prop(fm, "8"), 0x03u);
    }
    TEST_F(SbmdHandlerInvokerTest, BuildAttributeArgsEmptyTlv)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        SafeJSValue args = SbmdHandlerInvoker::BuildAttributeArgs(Ctx(), hctx, 6, 0, "");

        JSValue attr = JS_GetPropertyStr(Ctx(), args, "attribute");
        JSValue tlv = JS_GetPropertyStr(Ctx(), attr, "tlvBase64");
        EXPECT_TRUE(JS_IsUndefined(tlv));
    }

    // ========================================================================
    // BuildResourceArgs
    // ========================================================================

    TEST_F(SbmdHandlerInvokerTest, BuildResourceArgsRead)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::nullopt);
        ASSERT_FALSE(JS_IsException(args));

        EXPECT_EQ(GetStringProp(args, "deviceUuid"), "test-device-uuid");

        SafeJSValue resource(Ctx(), JS_GetPropertyStr(Ctx(), args, "resource"));
        ASSERT_FALSE(JS_IsUndefined(resource.Get()));
        EXPECT_EQ(GetStringProp(resource.Get(), "resourceId"), "isOn");

        // input should be null for read
        SafeJSValue input(Ctx(), JS_GetPropertyStr(Ctx(), resource.Get(), "input"));
        EXPECT_TRUE(JS_IsNull(input.Get()));
    }

    TEST_F(SbmdHandlerInvokerTest, BuildResourceArgsWrite)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(Ctx(), hctx, "dimLevel", std::string("75"));
        ASSERT_FALSE(JS_IsException(args));

        SafeJSValue resource(Ctx(), JS_GetPropertyStr(Ctx(), args, "resource"));
        EXPECT_EQ(GetStringProp(resource.Get(), "resourceId"), "dimLevel");
        EXPECT_EQ(GetStringProp(resource.Get(), "input"), "75");
    }

    // ========================================================================
    // InvokeHandler
    // ========================================================================

    TEST_F(SbmdHandlerInvokerTest, InvokeSimpleSuccessHandler)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        SafeJSValue handler(Ctx(), EvalFunc("(function(args) { return Sbmd.result().success(); })"));
        ASSERT_FALSE(JS_IsException(handler.Get()));

        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::nullopt);
        auto result = SbmdHandlerInvoker::InvokeHandler(Ctx(), handler.Get(), args);

        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(result->ops.empty());
        EXPECT_TRUE(std::holds_alternative<ResultTerminal::Success>(result->terminal.data));
    }

    TEST_F(SbmdHandlerInvokerTest, InvokeHandlerWithOps)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        SafeJSValue handler(Ctx(),
                            EvalFunc("(function(args) {"
                                     "  return Sbmd.result()"
                                     "    .dataModel.updateResource(args.endpointId, 'isOn', 'true')"
                                     "    .success();"
                                     "})"));
        ASSERT_FALSE(JS_IsException(handler.Get()));

        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::nullopt);
        auto result = SbmdHandlerInvoker::InvokeHandler(Ctx(), handler.Get(), args);

        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result->ops.size(), 1u);
        ASSERT_TRUE(std::holds_alternative<ResultOp::UpdateResource>(result->ops[0].data));

        auto &ur = std::get<ResultOp::UpdateResource>(result->ops[0].data);
        EXPECT_EQ(*ur.endpoint, "1"); // from args.endpointId
        EXPECT_EQ(ur.resource, "isOn");
        EXPECT_EQ(ur.value, "true");
    }

    TEST_F(SbmdHandlerInvokerTest, InvokeHandlerWithSendCommand)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        SafeJSValue handler(Ctx(),
                            EvalFunc("(function(args) {"
                                     "  return Sbmd.result()"
                                     "    .device.sendCommand(6, 1);"
                                     "})"));
        ASSERT_FALSE(JS_IsException(handler.Get()));

        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::string("true"));
        auto result = SbmdHandlerInvoker::InvokeHandler(Ctx(), handler.Get(), args);

        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::SendCommand>(result->terminal.data));

        auto &cmd = std::get<ResultTerminal::SendCommand>(result->terminal.data);
        EXPECT_EQ(cmd.clusterId, 6u);
        EXPECT_EQ(cmd.commandId, 1u);
    }

    TEST_F(SbmdHandlerInvokerTest, InvokeThrowingHandlerReturnsNullopt)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        SafeJSValue handler(Ctx(), EvalFunc("(function(args) { throw new Error('boom'); })"));
        ASSERT_FALSE(JS_IsException(handler.Get()));

        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::nullopt);
        auto result = SbmdHandlerInvoker::InvokeHandler(Ctx(), handler.Get(), args);

        EXPECT_FALSE(result.has_value());
    }

    TEST_F(SbmdHandlerInvokerTest, InvokeUndefinedHandlerReturnsNullopt)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::nullopt);
        auto result = SbmdHandlerInvoker::InvokeHandler(Ctx(), JS_UNDEFINED, args);

        EXPECT_FALSE(result.has_value());
    }

    // ========================================================================
    // ExecuteOps
    // ========================================================================

    TEST_F(SbmdHandlerInvokerTest, ExecuteOpsUpdateResource)
    {
        auto hctx = MakeContext();

        std::vector<ResultOp> ops;
        ResultOp::UpdateResource ur;
        ur.endpoint = "1";
        ur.resource = "isOn";
        ur.value = "true";
        ops.push_back(ResultOp {ur});

        SbmdHandlerInvoker::ExecuteOps(hctx, ops);

        ASSERT_EQ(g_updateResourceCalls.size(), 1u);
        EXPECT_EQ(g_updateResourceCalls[0].deviceUuid, "test-device-uuid");
        EXPECT_EQ(g_updateResourceCalls[0].endpointId, "1");
        EXPECT_EQ(g_updateResourceCalls[0].resourceId, "isOn");
        EXPECT_EQ(g_updateResourceCalls[0].value, "true");
    }

    TEST_F(SbmdHandlerInvokerTest, ExecuteOpsUpdateResourceDefaultsToDeviceLevel)
    {
        auto hctx = MakeContext();

        // The handler was triggered on endpoint "1". A no-endpoint update must still
        // target a device-level resource (NULL endpoint) rather than falling back to
        // this trigger endpoint.
        ASSERT_EQ(hctx.endpointId, "1");

        std::vector<ResultOp> ops;
        ResultOp::UpdateResource ur;
        // No endpoint set — the stub records a NULL endpoint as "".
        ur.resource = "isOn";
        ur.value = "false";
        ops.push_back(ResultOp {ur});

        SbmdHandlerInvoker::ExecuteOps(hctx, ops);

        ASSERT_EQ(g_updateResourceCalls.size(), 1u);
        EXPECT_EQ(g_updateResourceCalls[0].endpointId, ""); // device-level, not the trigger endpoint "1"
    }

    TEST_F(SbmdHandlerInvokerTest, ExecuteOpsUpdateResourceWithMetadata)
    {
        auto hctx = MakeContext();

        std::vector<ResultOp> ops;
        ResultOp::UpdateResource ur;
        ur.endpoint = "1";
        ur.resource = "isOn";
        ur.value = "true";
        ur.metadata = R"({"source":"matter"})";
        ops.push_back(ResultOp {ur});

        SbmdHandlerInvoker::ExecuteOps(hctx, ops);

        ASSERT_EQ(g_updateResourceCalls.size(), 1u);
        EXPECT_EQ(g_updateResourceCalls[0].value, "true");
        EXPECT_EQ(g_updateResourceCalls[0].metadata, R"({"source":"matter"})");
    }

    TEST_F(SbmdHandlerInvokerTest, ExecuteOpsUpdateResourceWithoutMetadata)
    {
        auto hctx = MakeContext();

        std::vector<ResultOp> ops;
        ResultOp::UpdateResource ur;
        ur.endpoint = "1";
        ur.resource = "isOn";
        ur.value = "false";
        // No metadata set
        ops.push_back(ResultOp {ur});

        SbmdHandlerInvoker::ExecuteOps(hctx, ops);

        ASSERT_EQ(g_updateResourceCalls.size(), 1u);
        EXPECT_TRUE(g_updateResourceCalls[0].metadata.empty());
    }

    TEST_F(SbmdHandlerInvokerTest, ExecuteOpsSetMetadata)
    {
        auto hctx = MakeContext();

        std::vector<ResultOp> ops;
        ResultOp::SetMetadata sm;
        sm.name = "unit";
        sm.value = "percent";
        ops.push_back(ResultOp {sm});

        SbmdHandlerInvoker::ExecuteOps(hctx, ops);

        ASSERT_EQ(g_setMetadataCalls.size(), 1u);
        EXPECT_EQ(g_setMetadataCalls[0].deviceUuid, "test-device-uuid");
        EXPECT_EQ(g_setMetadataCalls[0].endpointId, "");
        EXPECT_EQ(g_setMetadataCalls[0].key, "unit");
        EXPECT_EQ(g_setMetadataCalls[0].value, "percent");
    }

    TEST_F(SbmdHandlerInvokerTest, ExecuteOpsMultiple)
    {
        auto hctx = MakeContext();

        std::vector<ResultOp> ops;

        ResultOp::Log logOp;
        logOp.message = "updating";
        ops.push_back(ResultOp {logOp});

        ResultOp::UpdateResource ur;
        ur.endpoint = "1";
        ur.resource = "isOn";
        ur.value = "true";
        ops.push_back(ResultOp {ur});

        ResultOp::SetMetadata sm;
        sm.name = "source";
        sm.value = "device";
        ops.push_back(ResultOp {sm});

        SbmdHandlerInvoker::ExecuteOps(hctx, ops);

        // Log doesn't produce external calls, but the other two should
        EXPECT_EQ(g_updateResourceCalls.size(), 1u);
        EXPECT_EQ(g_setMetadataCalls.size(), 1u);
    }

    // ========================================================================
    // End-to-end: invoke → parse → execute ops
    // ========================================================================

    TEST_F(SbmdHandlerInvokerTest, EndToEndInvokeAndExecuteOps)
    {
        auto hctx = MakeContext();

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());

        SafeJSValue handler(Ctx(),
                            EvalFunc("(function(args) {"
                                     "  return Sbmd.result()"
                                     "    .log('attribute changed')"
                                     "    .dataModel.updateResource(args.endpointId, 'isOn', 'true')"
                                     "    .success();"
                                     "})"));
        ASSERT_FALSE(JS_IsException(handler.Get()));

        SafeJSValue args = SbmdHandlerInvoker::BuildAttributeArgs(Ctx(), hctx, 6, 0, "AB==");
        auto result = SbmdHandlerInvoker::InvokeHandler(Ctx(), handler.Get(), args);
        ASSERT_TRUE(result.has_value());

        // Execute ops outside the mutex (in real code) but fine in test
        SbmdHandlerInvoker::ExecuteOps(hctx, result->ops);

        ASSERT_EQ(g_updateResourceCalls.size(), 1u);
        EXPECT_EQ(g_updateResourceCalls[0].endpointId, "1");
        EXPECT_EQ(g_updateResourceCalls[0].resourceId, "isOn");
        EXPECT_EQ(g_updateResourceCalls[0].value, "true");
    }

    // ================================================================
    // AddSupplements
    // ================================================================

    TEST_F(SbmdHandlerInvokerTest, AddSupplementsEmpty)
    {
        auto hctx = MakeContext();

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());

        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::nullopt);

        SbmdSupplements empty;
        FetchAndAddSupplements(
            Ctx(),
            args,
            empty,
            [](const std::string &) { return std::nullopt; },
            [](const std::string &) { return std::nullopt; },
            [](const std::string &) { return std::nullopt; },
            [](const std::string &) { return std::nullopt; });

        // No supplements property should be added
        JSValue sup = JS_GetPropertyStr(Ctx(), args, "supplements");
        EXPECT_TRUE(JS_IsUndefined(sup));
    }

    TEST_F(SbmdHandlerInvokerTest, AddSupplementsAttributesOnly)
    {
        auto hctx = MakeContext();

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());

        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::nullopt);

        SbmdSupplements sup;
        sup.attributes = {"onOff", "currentLevel"};

        FetchAndAddSupplements(
            Ctx(),
            args,
            sup,
            [](const std::string &alias) -> std::optional<std::string> {
                if (alias == "onOff")
                {
                    return "AQ==";
                }

                if (alias == "currentLevel")
                {
                    return "Zg==";
                }

                return std::nullopt;
            },
            [](const std::string &) { return std::nullopt; },
            [](const std::string &) { return std::nullopt; },
            [](const std::string &) { return std::nullopt; });

        SafeJSValue supObj(Ctx(), JS_GetPropertyStr(Ctx(), args, "supplements"));
        ASSERT_FALSE(JS_IsUndefined(supObj.Get()));

        SafeJSValue attrs(Ctx(), JS_GetPropertyStr(Ctx(), supObj.Get(), "attributes"));
        ASSERT_FALSE(JS_IsUndefined(attrs.Get()));

        EXPECT_EQ(GetStringProp(attrs.Get(), "onOff"), "AQ==");
        EXPECT_EQ(GetStringProp(attrs.Get(), "currentLevel"), "Zg==");

        // No resources key
        SafeJSValue res(Ctx(), JS_GetPropertyStr(Ctx(), supObj.Get(), "resources"));
        EXPECT_TRUE(JS_IsUndefined(res.Get()));
    }

    TEST_F(SbmdHandlerInvokerTest, AddSupplementsResourcesOnly)
    {
        auto hctx = MakeContext();

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());

        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::nullopt);

        SbmdSupplements sup;
        sup.resources = {"1/isOn", "firmwareVersion"};

        FetchAndAddSupplements(
            Ctx(),
            args,
            sup,
            [](const std::string &) { return std::nullopt; },
            [](const std::string &path) -> std::optional<std::string> {
                if (path == "1/isOn")
                {
                    return "true";
                }

                if (path == "firmwareVersion")
                {
                    return "1.2.3";
                }

                return std::nullopt;
            },
            [](const std::string &) { return std::nullopt; },
            [](const std::string &) { return std::nullopt; });

        SafeJSValue supObj(Ctx(), JS_GetPropertyStr(Ctx(), args, "supplements"));
        ASSERT_FALSE(JS_IsUndefined(supObj.Get()));

        SafeJSValue res(Ctx(), JS_GetPropertyStr(Ctx(), supObj.Get(), "resources"));
        ASSERT_FALSE(JS_IsUndefined(res.Get()));

        EXPECT_EQ(GetStringProp(res.Get(), "1/isOn"), "true");
        EXPECT_EQ(GetStringProp(res.Get(), "firmwareVersion"), "1.2.3");

        // No attributes key
        SafeJSValue attrs(Ctx(), JS_GetPropertyStr(Ctx(), supObj.Get(), "attributes"));
        EXPECT_TRUE(JS_IsUndefined(attrs.Get()));
    }

    TEST_F(SbmdHandlerInvokerTest, AddSupplementsBothAttributesAndResources)
    {
        auto hctx = MakeContext();

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());

        SafeJSValue args = SbmdHandlerInvoker::BuildAttributeArgs(Ctx(), hctx, 6, 0, "AQ==");

        SbmdSupplements sup;
        sup.attributes = {"lockState"};
        sup.resources = {"1/locked"};

        FetchAndAddSupplements(
            Ctx(),
            args,
            sup,
            [](const std::string &alias) -> std::optional<std::string> {
                if (alias == "lockState")
                {
                    return "Ag==";
                }

                return std::nullopt;
            },
            [](const std::string &path) -> std::optional<std::string> {
                if (path == "1/locked")
                {
                    return "true";
                }

                return std::nullopt;
            },
            [](const std::string &) { return std::nullopt; },
            [](const std::string &) { return std::nullopt; });

        SafeJSValue supObj(Ctx(), JS_GetPropertyStr(Ctx(), args, "supplements"));
        ASSERT_FALSE(JS_IsUndefined(supObj.Get()));

        SafeJSValue attrs(Ctx(), JS_GetPropertyStr(Ctx(), supObj.Get(), "attributes"));
        EXPECT_EQ(GetStringProp(attrs.Get(), "lockState"), "Ag==");

        SafeJSValue res(Ctx(), JS_GetPropertyStr(Ctx(), supObj.Get(), "resources"));
        EXPECT_EQ(GetStringProp(res.Get(), "1/locked"), "true");
    }

    TEST_F(SbmdHandlerInvokerTest, AddSupplementsMissingValuesAreNull)
    {
        auto hctx = MakeContext();

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());

        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::nullopt);

        SbmdSupplements sup;
        sup.attributes = {"missingAlias"};
        sup.resources = {"1/missingResource"};

        FetchAndAddSupplements(
            Ctx(),
            args,
            sup,
            [](const std::string &) { return std::nullopt; },
            [](const std::string &) { return std::nullopt; },
            [](const std::string &) { return std::nullopt; },
            [](const std::string &) { return std::nullopt; });

        SafeJSValue supObj(Ctx(), JS_GetPropertyStr(Ctx(), args, "supplements"));
        ASSERT_FALSE(JS_IsUndefined(supObj.Get()));

        SafeJSValue attrs(Ctx(), JS_GetPropertyStr(Ctx(), supObj.Get(), "attributes"));
        SafeJSValue missingAttr(Ctx(), JS_GetPropertyStr(Ctx(), attrs.Get(), "missingAlias"));
        EXPECT_TRUE(JS_IsNull(missingAttr.Get()));

        SafeJSValue res(Ctx(), JS_GetPropertyStr(Ctx(), supObj.Get(), "resources"));
        SafeJSValue missingRes(Ctx(), JS_GetPropertyStr(Ctx(), res.Get(), "1/missingResource"));
        EXPECT_TRUE(JS_IsNull(missingRes.Get()));
    }

    TEST_F(SbmdHandlerInvokerTest, SupplementsAccessibleFromHandler)
    {
        auto hctx = MakeContext();

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());

        SafeJSValue handler(Ctx(),
                            EvalFunc("(function(args) {"
                                     "  var onOff = args.supplements.attributes.onOff;"
                                     "  var locked = args.supplements.resources['1/locked'];"
                                     "  return Sbmd.result()"
                                     "    .dataModel.updateResource('1', 'combined', onOff + ':' + locked)"
                                     "    .success();"
                                     "})"));
        ASSERT_FALSE(JS_IsException(handler.Get()));

        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::nullopt);

        SbmdSupplements sup;
        sup.attributes = {"onOff"};
        sup.resources = {"1/locked"};

        FetchAndAddSupplements(
            Ctx(),
            args,
            sup,
            [](const std::string &alias) -> std::optional<std::string> {
                if (alias == "onOff")
                {
                    return "AQ==";
                }

                return std::nullopt;
            },
            [](const std::string &path) -> std::optional<std::string> {
                if (path == "1/locked")
                {
                    return "true";
                }

                return std::nullopt;
            },
            [](const std::string &) { return std::nullopt; },
            [](const std::string &) { return std::nullopt; });

        auto result = SbmdHandlerInvoker::InvokeHandler(Ctx(), handler.Get(), args);
        ASSERT_TRUE(result.has_value());

        SbmdHandlerInvoker::ExecuteOps(hctx, result->ops);

        ASSERT_EQ(g_updateResourceCalls.size(), 1u);
        EXPECT_EQ(g_updateResourceCalls[0].resourceId, "combined");
        EXPECT_EQ(g_updateResourceCalls[0].value, "AQ==:true");
    }

    TEST_F(SbmdHandlerInvokerTest, SupplementsNullHandledByHandler)
    {
        auto hctx = MakeContext();

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());

        // Handler checks for null supplement gracefully
        SafeJSValue handler(Ctx(),
                            EvalFunc("(function(args) {"
                                     "  var val = args.supplements.attributes.missing;"
                                     "  var out = (val === null) ? 'was-null' : 'had-value';"
                                     "  return Sbmd.result()"
                                     "    .dataModel.updateResource('1', 'result', out)"
                                     "    .success();"
                                     "})"));
        ASSERT_FALSE(JS_IsException(handler.Get()));

        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(Ctx(), hctx, "test", std::nullopt);

        SbmdSupplements sup;
        sup.attributes = {"missing"};

        FetchAndAddSupplements(
            Ctx(),
            args,
            sup,
            [](const std::string &) { return std::nullopt; },
            [](const std::string &) { return std::nullopt; },
            [](const std::string &) { return std::nullopt; },
            [](const std::string &) { return std::nullopt; });

        auto result = SbmdHandlerInvoker::InvokeHandler(Ctx(), handler.Get(), args);
        ASSERT_TRUE(result.has_value());

        SbmdHandlerInvoker::ExecuteOps(hctx, result->ops);

        ASSERT_EQ(g_updateResourceCalls.size(), 1u);
        EXPECT_EQ(g_updateResourceCalls[0].value, "was-null");
    }

    // ================================================================
    // Declared-supplement contract: every DECLARED supplement key is
    // always a defined JS property (its value, or null) at invocation --
    // never undefined -- even when the prefetch was skipped or a fetch
    // bug failed to populate it. These drive AddSupplements directly with
    // a declaration plus empty/partial fetched data to isolate the
    // contract from the prefetch phase.
    // ================================================================

    TEST_F(SbmdHandlerInvokerTest, DeclaredSupplementsPresentWhenPrefetchSkipped)
    {
        auto hctx = MakeContext();

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());

        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::nullopt);

        SbmdSupplements declared;
        declared.attributes = {"lockState"};
        declared.resources = {"1/locked"};
        declared.persistentData = {"lastOp"};
        declared.transientData = {"debounce"};

        // Simulate the prefetch being skipped entirely (e.g. no device available):
        // fetched is empty, yet every declared key must still be present as null.
        FetchedSupplements empty;
        SbmdHandlerInvoker::AddSupplements(Ctx(), args, declared, empty);

        SafeJSValue supObj(Ctx(), JS_GetPropertyStr(Ctx(), args, "supplements"));
        ASSERT_FALSE(JS_IsUndefined(supObj.Get()));

        SafeJSValue attrs(Ctx(), JS_GetPropertyStr(Ctx(), supObj.Get(), "attributes"));
        ASSERT_FALSE(JS_IsUndefined(attrs.Get()));
        SafeJSValue lockState(Ctx(), JS_GetPropertyStr(Ctx(), attrs.Get(), "lockState"));
        EXPECT_FALSE(JS_IsUndefined(lockState.Get()));
        EXPECT_TRUE(JS_IsNull(lockState.Get()));

        SafeJSValue res(Ctx(), JS_GetPropertyStr(Ctx(), supObj.Get(), "resources"));
        ASSERT_FALSE(JS_IsUndefined(res.Get()));
        SafeJSValue locked(Ctx(), JS_GetPropertyStr(Ctx(), res.Get(), "1/locked"));
        EXPECT_FALSE(JS_IsUndefined(locked.Get()));
        EXPECT_TRUE(JS_IsNull(locked.Get()));

        SafeJSValue pd(Ctx(), JS_GetPropertyStr(Ctx(), supObj.Get(), "persistentData"));
        ASSERT_FALSE(JS_IsUndefined(pd.Get()));
        SafeJSValue lastOp(Ctx(), JS_GetPropertyStr(Ctx(), pd.Get(), "lastOp"));
        EXPECT_FALSE(JS_IsUndefined(lastOp.Get()));
        EXPECT_TRUE(JS_IsNull(lastOp.Get()));

        SafeJSValue td(Ctx(), JS_GetPropertyStr(Ctx(), supObj.Get(), "transientData"));
        ASSERT_FALSE(JS_IsUndefined(td.Get()));
        SafeJSValue debounce(Ctx(), JS_GetPropertyStr(Ctx(), td.Get(), "debounce"));
        EXPECT_FALSE(JS_IsUndefined(debounce.Get()));
        EXPECT_TRUE(JS_IsNull(debounce.Get()));
    }

    TEST_F(SbmdHandlerInvokerTest, DeclaredSupplementsPresentWhenFetchDropsKey)
    {
        auto hctx = MakeContext();

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());

        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::nullopt);

        SbmdSupplements declared;
        declared.attributes = {"present", "dropped"};

        // Simulate a fetch bug: only one of the two declared keys made it into fetched.
        FetchedSupplements fetched;
        fetched.attributes.push_back({"present", std::string("AQ==")});

        SbmdHandlerInvoker::AddSupplements(Ctx(), args, declared, fetched);

        SafeJSValue supObj(Ctx(), JS_GetPropertyStr(Ctx(), args, "supplements"));
        SafeJSValue attrs(Ctx(), JS_GetPropertyStr(Ctx(), supObj.Get(), "attributes"));

        // The present key carries its value.
        EXPECT_EQ(GetStringProp(attrs.Get(), "present"), "AQ==");

        // The dropped key is still DEFINED as null, never undefined.
        SafeJSValue dropped(Ctx(), JS_GetPropertyStr(Ctx(), attrs.Get(), "dropped"));
        EXPECT_FALSE(JS_IsUndefined(dropped.Get()));
        EXPECT_TRUE(JS_IsNull(dropped.Get()));
    }

    TEST_F(SbmdHandlerInvokerTest, DeclaredSupplementSafeInHandlerWhenPrefetchSkipped)
    {
        auto hctx = MakeContext();

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());

        // Mirrors a seed handler that reads a declared attribute. Even with no fetched
        // data, args.supplements.attributes.lockState must be null (defined), so the
        // handler runs without a "cannot read property of undefined" crash.
        SafeJSValue handler(Ctx(),
                            EvalFunc("(function(args) {"
                                     "  var v = args.supplements.attributes.lockState;"
                                     "  var out = (v === null) ? 'null' : String(v);"
                                     "  return Sbmd.result()"
                                     "    .dataModel.updateResource('1', 'locked', out)"
                                     "    .success();"
                                     "})"));
        ASSERT_FALSE(JS_IsException(handler.Get()));

        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(Ctx(), hctx, "locked", std::nullopt);

        SbmdSupplements declared;
        declared.attributes = {"lockState"};

        FetchedSupplements empty; // prefetch skipped
        SbmdHandlerInvoker::AddSupplements(Ctx(), args, declared, empty);

        auto result = SbmdHandlerInvoker::InvokeHandler(Ctx(), handler.Get(), args);
        ASSERT_TRUE(result.has_value());

        SbmdHandlerInvoker::ExecuteOps(hctx, result->ops);

        ASSERT_EQ(g_updateResourceCalls.size(), 1u);
        EXPECT_EQ(g_updateResourceCalls[0].value, "null");
    }

    // ================================================================
    // Tests for persistent/transient data supplements
    // ================================================================

    TEST_F(SbmdHandlerInvokerTest, AddSupplementsPersistentDataOnly)
    {
        auto hctx = MakeContext();

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());

        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::nullopt);

        SbmdSupplements sup;
        sup.persistentData = {"lastOp", "counter"};

        FetchAndAddSupplements(
            Ctx(),
            args,
            sup,
            [](const std::string &) { return std::nullopt; },
            [](const std::string &) { return std::nullopt; },
            [](const std::string &key) -> std::optional<std::string> {
                if (key == "lastOp")
                {
                    return "lock";
                }

                return std::nullopt;
            },
            [](const std::string &) { return std::nullopt; });

        SafeJSValue supObj(Ctx(), JS_GetPropertyStr(Ctx(), args, "supplements"));
        ASSERT_FALSE(JS_IsUndefined(supObj.Get()));

        SafeJSValue pd(Ctx(), JS_GetPropertyStr(Ctx(), supObj.Get(), "persistentData"));
        ASSERT_FALSE(JS_IsUndefined(pd.Get()));

        EXPECT_EQ(GetStringProp(pd.Get(), "lastOp"), "lock");

        SafeJSValue counter(Ctx(), JS_GetPropertyStr(Ctx(), pd.Get(), "counter"));
        EXPECT_TRUE(JS_IsNull(counter.Get()));
    }

    TEST_F(SbmdHandlerInvokerTest, AddSupplementsTransientDataOnly)
    {
        auto hctx = MakeContext();

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());

        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::nullopt);

        SbmdSupplements sup;
        sup.transientData = {"debounce"};

        FetchAndAddSupplements(
            Ctx(),
            args,
            sup,
            [](const std::string &) { return std::nullopt; },
            [](const std::string &) { return std::nullopt; },
            [](const std::string &) { return std::nullopt; },
            [](const std::string &key) -> std::optional<std::string> {
                if (key == "debounce")
                {
                    return "1";
                }

                return std::nullopt;
            });

        SafeJSValue supObj(Ctx(), JS_GetPropertyStr(Ctx(), args, "supplements"));
        ASSERT_FALSE(JS_IsUndefined(supObj.Get()));

        SafeJSValue td(Ctx(), JS_GetPropertyStr(Ctx(), supObj.Get(), "transientData"));
        ASSERT_FALSE(JS_IsUndefined(td.Get()));

        EXPECT_EQ(GetStringProp(td.Get(), "debounce"), "1");
    }

    TEST_F(SbmdHandlerInvokerTest, StorageSupplementsAccessibleFromHandler)
    {
        auto hctx = MakeContext();

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());

        SafeJSValue handler(Ctx(),
                            EvalFunc("(function(args) {"
                                     "  var p = args.supplements.persistentData.lastOp;"
                                     "  var t = args.supplements.transientData.debounce;"
                                     "  return Sbmd.result()"
                                     "    .dataModel.updateResource('1', 'combined', p + ':' + t)"
                                     "    .success();"
                                     "})"));
        ASSERT_FALSE(JS_IsException(handler.Get()));

        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::nullopt);

        SbmdSupplements sup;
        sup.persistentData = {"lastOp"};
        sup.transientData = {"debounce"};

        FetchAndAddSupplements(
            Ctx(),
            args,
            sup,
            [](const std::string &) { return std::nullopt; },
            [](const std::string &) { return std::nullopt; },
            [](const std::string &) -> std::optional<std::string> { return "lock"; },
            [](const std::string &) -> std::optional<std::string> { return "1"; });

        auto result = SbmdHandlerInvoker::InvokeHandler(Ctx(), handler.Get(), args);
        ASSERT_TRUE(result.has_value());

        SbmdHandlerInvoker::ExecuteOps(hctx, result->ops);

        ASSERT_EQ(g_updateResourceCalls.size(), 1u);
        EXPECT_EQ(g_updateResourceCalls[0].value, "lock:1");
    }

    // ================================================================
    // Tests for storage op execution
    // ================================================================

    TEST_F(SbmdHandlerInvokerTest, ExecuteOpsPersistentData)
    {
        auto hctx = MakeContext();
        ResultOp::SetPersistentData sp;
        sp.key = "lastAction";
        sp.value = "unlock";
        std::vector<ResultOp> ops = {ResultOp {sp}};

        SbmdHandlerInvoker::ExecuteOps(hctx, ops);

        ASSERT_EQ(g_setPersistentDataCalls.size(), 1u);
        EXPECT_EQ(g_setPersistentDataCalls[0].uri, "/devices/test-device-uuid/metadata/sbmd.lastAction");
        EXPECT_EQ(g_setPersistentDataCalls[0].value, "unlock");
    }

    TEST_F(SbmdHandlerInvokerTest, ExecuteOpsTransientDataWithSetter)
    {
        auto hctx = MakeContext();
        ResultOp::SetTransientData st;
        st.key = "debounce";
        st.value = "active";
        st.ttlSecs = 30;
        std::vector<ResultOp> ops = {ResultOp {st}};

        std::string capturedKey;
        std::string capturedValue;
        uint32_t capturedTtl = 0;
        TransientDataSetter setter = [&](const std::string &k, const std::string &v, uint32_t t) {
            capturedKey = k;
            capturedValue = v;
            capturedTtl = t;
        };

        SbmdHandlerInvoker::ExecuteOps(hctx, ops, setter);

        EXPECT_EQ(capturedKey, "debounce");
        EXPECT_EQ(capturedValue, "active");
        EXPECT_EQ(capturedTtl, 30u);
    }

    // ================================================================
    // Tests for deferred operation args builders
    // ================================================================

    TEST_F(SbmdHandlerInvokerTest, BuildCommandResponseArgsHasResponseFields)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();
        SafeJSValue args = SbmdHandlerInvoker::BuildCommandResponseArgs(Ctx(), hctx, 0x0101, 42, "AQID");

        // Check base fields
        EXPECT_EQ(GetStringProp(args, "deviceUuid"), "test-device-uuid");
        EXPECT_EQ(GetStringProp(args, "endpointId"), "1");

        // Check response object
        SafeJSValue response(Ctx(), JS_GetPropertyStr(Ctx(), args, "response"));
        ASSERT_FALSE(JS_IsUndefined(response.Get()));
        EXPECT_EQ(GetUint32Prop(response.Get(), "clusterId"), 0x0101u);
        EXPECT_EQ(GetUint32Prop(response.Get(), "commandId"), 42u);
        EXPECT_EQ(GetStringProp(response.Get(), "data"), "AQID");
    }

    TEST_F(SbmdHandlerInvokerTest, BuildCommandResponseArgsNullData)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();
        SafeJSValue args = SbmdHandlerInvoker::BuildCommandResponseArgs(Ctx(), hctx, 0x0006, 1, "");

        SafeJSValue response(Ctx(), JS_GetPropertyStr(Ctx(), args, "response"));
        ASSERT_FALSE(JS_IsUndefined(response.Get()));

        // Empty tlvBase64 → null data
        SafeJSValue data(Ctx(), JS_GetPropertyStr(Ctx(), response.Get(), "data"));
        EXPECT_TRUE(JS_IsNull(data.Get()));
    }

    TEST_F(SbmdHandlerInvokerTest, BuildCommandResponseArgsWithHandlerContext)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        // Create a context object
        SafeJSValue context(Ctx(), JS_Eval(Ctx(), "({requestId: 42})", 18, "<test>", JS_EVAL_RETVAL));
        ASSERT_FALSE(JS_IsException(context.Get()));

        SafeJSValue args = SbmdHandlerInvoker::BuildCommandResponseArgs(Ctx(), hctx, 0x0101, 26, "AQID", context.Get());

        SafeJSValue hc(Ctx(), JS_GetPropertyStr(Ctx(), args, "handlerContext"));
        ASSERT_FALSE(JS_IsUndefined(hc.Get()));
        ASSERT_FALSE(JS_IsNull(hc.Get()));
        EXPECT_EQ(GetUint32Prop(hc.Get(), "requestId"), 42u);
    }

    TEST_F(SbmdHandlerInvokerTest, BuildAttributeReadResponseArgs)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();
        SafeJSValue args = SbmdHandlerInvoker::BuildAttributeReadResponseArgs(Ctx(), hctx, 0x0300, 7, "AB==");

        EXPECT_EQ(GetStringProp(args, "deviceUuid"), "test-device-uuid");

        SafeJSValue attribute(Ctx(), JS_GetPropertyStr(Ctx(), args, "attribute"));
        ASSERT_FALSE(JS_IsUndefined(attribute.Get()));
        EXPECT_EQ(GetUint32Prop(attribute.Get(), "clusterId"), 0x0300u);
        EXPECT_EQ(GetUint32Prop(attribute.Get(), "attributeId"), 7u);
        EXPECT_EQ(GetStringProp(attribute.Get(), "value"), "AB==");
    }

    TEST_F(SbmdHandlerInvokerTest, BuildAttributeReadResponseArgsWithHandlerContext)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        SafeJSValue context(Ctx(), JS_Eval(Ctx(), "('read-ctx')", 12, "<test>", JS_EVAL_RETVAL));
        ASSERT_FALSE(JS_IsException(context.Get()));

        SafeJSValue args =
            SbmdHandlerInvoker::BuildAttributeReadResponseArgs(Ctx(), hctx, 0x0300, 7, "AB==", context.Get());

        SafeJSValue hc(Ctx(), JS_GetPropertyStr(Ctx(), args, "handlerContext"));
        ASSERT_FALSE(JS_IsUndefined(hc.Get()));
        ASSERT_FALSE(JS_IsNull(hc.Get()));
        EXPECT_EQ(GetStringProp(args, "handlerContext"), "read-ctx");
    }

    TEST_F(SbmdHandlerInvokerTest, BuildDeferredErrorArgs)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();
        SafeJSValue args =
            SbmdHandlerInvoker::BuildDeferredErrorArgs(Ctx(), hctx, "timeout", "Operation timed out after 5000ms");

        EXPECT_EQ(GetStringProp(args, "deviceUuid"), "test-device-uuid");

        SafeJSValue error(Ctx(), JS_GetPropertyStr(Ctx(), args, "error"));
        ASSERT_FALSE(JS_IsUndefined(error.Get()));
        EXPECT_EQ(GetStringProp(error.Get(), "type"), "timeout");
        EXPECT_EQ(GetStringProp(error.Get(), "message"), "Operation timed out after 5000ms");
    }

    TEST_F(SbmdHandlerInvokerTest, BuildDeferredErrorArgsCommandFailed)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();
        SafeJSValue args =
            SbmdHandlerInvoker::BuildDeferredErrorArgs(Ctx(), hctx, "commandFailed", "CHIP Error 0x00000032");

        SafeJSValue error(Ctx(), JS_GetPropertyStr(Ctx(), args, "error"));
        EXPECT_EQ(GetStringProp(error.Get(), "type"), "commandFailed");
        EXPECT_EQ(GetStringProp(error.Get(), "message"), "CHIP Error 0x00000032");
    }

    TEST_F(SbmdHandlerInvokerTest, BuildDeferredErrorArgsWithMatterCode)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();
        SafeJSValue args = SbmdHandlerInvoker::BuildDeferredErrorArgs(Ctx(), hctx, "commandFailed", "CHIP Error", 0x32);

        SafeJSValue error(Ctx(), JS_GetPropertyStr(Ctx(), args, "error"));
        EXPECT_EQ(GetStringProp(error.Get(), "type"), "commandFailed");
        EXPECT_EQ(GetStringProp(error.Get(), "message"), "CHIP Error");

        // matterCode should be present as a number
        SafeJSValue mc(Ctx(), JS_GetPropertyStr(Ctx(), error.Get(), "matterCode"));
        ASSERT_FALSE(JS_IsNull(mc.Get()));
        ASSERT_FALSE(JS_IsUndefined(mc.Get()));
        int32_t code = 0;
        JS_ToInt32(Ctx(), &code, mc.Get());
        EXPECT_EQ(code, 0x32);
    }

    TEST_F(SbmdHandlerInvokerTest, BuildDeferredErrorArgsMatterCodeNullWhenNotProvided)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();
        // matterCode = -1 means "not available" → should be null
        SafeJSValue args = SbmdHandlerInvoker::BuildDeferredErrorArgs(Ctx(), hctx, "timeout", "timed out", -1);

        SafeJSValue error(Ctx(), JS_GetPropertyStr(Ctx(), args, "error"));
        SafeJSValue mc(Ctx(), JS_GetPropertyStr(Ctx(), error.Get(), "matterCode"));
        EXPECT_TRUE(JS_IsNull(mc.Get()));
    }

    TEST_F(SbmdHandlerInvokerTest, BuildDeferredErrorArgsWithHandlerContext)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        SafeJSValue context(Ctx(), JS_Eval(Ctx(), "({retryCount: 3})", 18, "<test>", JS_EVAL_RETVAL));
        ASSERT_FALSE(JS_IsException(context.Get()));

        SafeJSValue args =
            SbmdHandlerInvoker::BuildDeferredErrorArgs(Ctx(), hctx, "timeout", "timed out", -1, context.Get());

        SafeJSValue hc(Ctx(), JS_GetPropertyStr(Ctx(), args, "handlerContext"));
        ASSERT_FALSE(JS_IsUndefined(hc.Get()));
        ASSERT_FALSE(JS_IsNull(hc.Get()));
        EXPECT_EQ(GetUint32Prop(hc.Get(), "retryCount"), 3u);
    }

    TEST_F(SbmdHandlerInvokerTest, InvokeDeferredOnResponseHandler)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        // Create a deferred onResponse handler that reads the response data
        SafeJSValue handler(Ctx(),
                            EvalFunc("(function(args) {"
                                     "  return Sbmd.result()"
                                     "    .log('response cmd=' + args.response.commandId)"
                                     "    .success();"
                                     "})"));
        ASSERT_FALSE(JS_IsException(handler.Get()));

        // Build response args and invoke
        SafeJSValue args = SbmdHandlerInvoker::BuildCommandResponseArgs(Ctx(), hctx, 0x0101, 26, "AQID");
        auto result = SbmdHandlerInvoker::InvokeHandler(Ctx(), handler.Get(), args);
        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::Success>(result->terminal.data));

        // Verify the log op captured the response data
        ASSERT_EQ(result->ops.size(), 1u);
        const auto &logOp = std::get<ResultOp::Log>(result->ops[0].data);
        EXPECT_EQ(logOp.message, "response cmd=26");
    }

    TEST_F(SbmdHandlerInvokerTest, InvokeDeferredOnResponseHandlerWithContext)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        // Handler that reads handlerContext
        SafeJSValue handler(Ctx(),
                            EvalFunc("(function(args) {"
                                     "  return Sbmd.result()"
                                     "    .log('ctx=' + JSON.stringify(args.handlerContext))"
                                     "    .success();"
                                     "})"));
        ASSERT_FALSE(JS_IsException(handler.Get()));

        SafeJSValue context(Ctx(), JS_Eval(Ctx(), "({id: 99})", 10, "<test>", JS_EVAL_RETVAL));
        ASSERT_FALSE(JS_IsException(context.Get()));

        SafeJSValue args = SbmdHandlerInvoker::BuildCommandResponseArgs(Ctx(), hctx, 0x0101, 26, "AQID", context.Get());
        auto result = SbmdHandlerInvoker::InvokeHandler(Ctx(), handler.Get(), args);
        ASSERT_TRUE(result.has_value());

        ASSERT_EQ(result->ops.size(), 1u);
        const auto &logOp = std::get<ResultOp::Log>(result->ops[0].data);
        EXPECT_EQ(logOp.message, "ctx={\"id\":99}");
    }

    TEST_F(SbmdHandlerInvokerTest, InvokeDeferredOnErrorHandler)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        // Create an onError handler that reads the error type
        SafeJSValue handler(Ctx(),
                            EvalFunc("(function(args) {"
                                     "  return Sbmd.result()"
                                     "    .log('error type=' + args.error.type)"
                                     "    .error(args.error.message);"
                                     "})"));
        ASSERT_FALSE(JS_IsException(handler.Get()));

        SafeJSValue args = SbmdHandlerInvoker::BuildDeferredErrorArgs(Ctx(), hctx, "timeout", "5s elapsed");
        auto result = SbmdHandlerInvoker::InvokeHandler(Ctx(), handler.Get(), args);
        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::Error>(result->terminal.data));

        const auto &err = std::get<ResultTerminal::Error>(result->terminal.data);
        EXPECT_EQ(err.message, "5s elapsed");

        ASSERT_EQ(result->ops.size(), 1u);
        const auto &logOp = std::get<ResultOp::Log>(result->ops[0].data);
        EXPECT_EQ(logOp.message, "error type=timeout");
    }

    TEST_F(SbmdHandlerInvokerTest, InvokeDeferredOnResponseReturnsRequestCommand)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        // onResponse handler that returns another requestCommand (chaining)
        SafeJSValue handler(Ctx(),
                            EvalFunc("(function(args) {"
                                     "  return Sbmd.result()"
                                     "    .device.requestCommand(0x0101, 5, {"
                                     "      responseCommandId: 6,"
                                     "      onResponse: function(a) { return Sbmd.result().success(); },"
                                     "      onError: function(a) { return Sbmd.result().error('fail'); }"
                                     "    });"
                                     "})"));
        ASSERT_FALSE(JS_IsException(handler.Get()));

        SafeJSValue args = SbmdHandlerInvoker::BuildCommandResponseArgs(Ctx(), hctx, 0x0101, 26, "");
        auto result = SbmdHandlerInvoker::InvokeHandler(Ctx(), handler.Get(), args);
        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::RequestCommand>(result->terminal.data));

        const auto &rc = std::get<ResultTerminal::RequestCommand>(result->terminal.data);
        EXPECT_EQ(rc.clusterId, 0x0101u);
        EXPECT_EQ(rc.commandId, 5u);
        EXPECT_EQ(rc.responseCommandId, 6u);
    }

    TEST_F(SbmdHandlerInvokerTest, InvokeDeferredReadAttributeResponse)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        // onResponse handler that reads attribute value from args
        SafeJSValue handler(Ctx(),
                            EvalFunc("(function(args) {"
                                     "  return Sbmd.result()"
                                     "    .dataModel.updateResource('result', args.attribute.value)"
                                     "    .success();"
                                     "})"));
        ASSERT_FALSE(JS_IsException(handler.Get()));

        SafeJSValue args = SbmdHandlerInvoker::BuildAttributeReadResponseArgs(Ctx(), hctx, 0x0300, 7, "QUJD");
        auto result = SbmdHandlerInvoker::InvokeHandler(Ctx(), handler.Get(), args);
        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::Success>(result->terminal.data));

        ASSERT_EQ(result->ops.size(), 1u);
        const auto &ur = std::get<ResultOp::UpdateResource>(result->ops[0].data);
        EXPECT_EQ(ur.resource, "result");
        EXPECT_EQ(ur.value, "QUJD");
    }

    // ================================================================
    // Tests for event args builder
    // ================================================================

    TEST_F(SbmdHandlerInvokerTest, BuildEventArgsHasEventFields)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();
        SafeJSValue args = SbmdHandlerInvoker::BuildEventArgs(Ctx(), hctx, 0x0101, 0x02, "AQID");

        // Check base fields
        EXPECT_EQ(GetStringProp(args, "deviceUuid"), "test-device-uuid");
        EXPECT_EQ(GetStringProp(args, "endpointId"), "1");

        // Check event object
        SafeJSValue event(Ctx(), JS_GetPropertyStr(Ctx(), args, "event"));
        ASSERT_FALSE(JS_IsUndefined(event.Get()));
        EXPECT_EQ(GetUint32Prop(event.Get(), "clusterId"), 0x0101u);
        EXPECT_EQ(GetUint32Prop(event.Get(), "eventId"), 0x02u);
        EXPECT_EQ(GetStringProp(event.Get(), "tlvBase64"), "AQID");
    }

    TEST_F(SbmdHandlerInvokerTest, BuildEventArgsEmptyTlv)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();
        SafeJSValue args = SbmdHandlerInvoker::BuildEventArgs(Ctx(), hctx, 0x0006, 0, "");

        SafeJSValue event(Ctx(), JS_GetPropertyStr(Ctx(), args, "event"));
        ASSERT_FALSE(JS_IsUndefined(event.Get()));
        EXPECT_EQ(GetUint32Prop(event.Get(), "clusterId"), 0x0006u);
        EXPECT_EQ(GetUint32Prop(event.Get(), "eventId"), 0u);

        // tlvBase64 should be absent (not set when empty)
        SafeJSValue tlv(Ctx(), JS_GetPropertyStr(Ctx(), event.Get(), "tlvBase64"));
        EXPECT_TRUE(JS_IsUndefined(tlv.Get()));
    }

    TEST_F(SbmdHandlerInvokerTest, InvokeEventHandler)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        SafeJSValue handler(Ctx(),
                            EvalFunc("(function(args) {"
                                     "  return Sbmd.result()"
                                     "    .log('event=' + args.event.eventId)"
                                     "    .success();"
                                     "})"));
        ASSERT_FALSE(JS_IsException(handler.Get()));

        SafeJSValue args = SbmdHandlerInvoker::BuildEventArgs(Ctx(), hctx, 0x0101, 5, "AQID");
        auto result = SbmdHandlerInvoker::InvokeHandler(Ctx(), handler.Get(), args);
        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::Success>(result->terminal.data));

        ASSERT_EQ(result->ops.size(), 1u);
        const auto &logOp = std::get<ResultOp::Log>(result->ops[0].data);
        EXPECT_EQ(logOp.message, "event=5");
    }

    // ================================================================
    // Tests for command args builder
    // ================================================================

    TEST_F(SbmdHandlerInvokerTest, BuildCommandArgsHasCommandFields)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();
        SafeJSValue args = SbmdHandlerInvoker::BuildCommandArgs(Ctx(), hctx, 0x0101, 0x1C, "AQID");

        // Check base fields
        EXPECT_EQ(GetStringProp(args, "deviceUuid"), "test-device-uuid");
        EXPECT_EQ(GetStringProp(args, "endpointId"), "1");

        // Check command object
        SafeJSValue command(Ctx(), JS_GetPropertyStr(Ctx(), args, "command"));
        ASSERT_FALSE(JS_IsUndefined(command.Get()));
        EXPECT_EQ(GetUint32Prop(command.Get(), "clusterId"), 0x0101u);
        EXPECT_EQ(GetUint32Prop(command.Get(), "commandId"), 0x1Cu);
        EXPECT_EQ(GetStringProp(command.Get(), "tlvBase64"), "AQID");
    }

    TEST_F(SbmdHandlerInvokerTest, BuildCommandArgsEmptyTlv)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();
        SafeJSValue args = SbmdHandlerInvoker::BuildCommandArgs(Ctx(), hctx, 0x0006, 1, "");

        SafeJSValue command(Ctx(), JS_GetPropertyStr(Ctx(), args, "command"));
        ASSERT_FALSE(JS_IsUndefined(command.Get()));
        EXPECT_EQ(GetUint32Prop(command.Get(), "clusterId"), 0x0006u);
        EXPECT_EQ(GetUint32Prop(command.Get(), "commandId"), 1u);

        // tlvBase64 should be absent (not set when empty)
        SafeJSValue tlv(Ctx(), JS_GetPropertyStr(Ctx(), command.Get(), "tlvBase64"));
        EXPECT_TRUE(JS_IsUndefined(tlv.Get()));
    }

    TEST_F(SbmdHandlerInvokerTest, InvokeCommandHandler)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        SafeJSValue handler(Ctx(),
                            EvalFunc("(function(args) {"
                                     "  return Sbmd.result()"
                                     "    .log('cmd=' + args.command.commandId)"
                                     "    .success();"
                                     "})"));
        ASSERT_FALSE(JS_IsException(handler.Get()));

        SafeJSValue args = SbmdHandlerInvoker::BuildCommandArgs(Ctx(), hctx, 0x0101, 0x1C, "AQID");
        auto result = SbmdHandlerInvoker::InvokeHandler(Ctx(), handler.Get(), args);
        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::Success>(result->terminal.data));

        ASSERT_EQ(result->ops.size(), 1u);
        const auto &logOp = std::get<ResultOp::Log>(result->ops[0].data);
        EXPECT_EQ(logOp.message, "cmd=28");
    }

} // namespace
