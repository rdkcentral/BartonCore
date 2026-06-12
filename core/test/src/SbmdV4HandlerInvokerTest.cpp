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
 * Unit tests for SbmdV4HandlerInvoker — args building, handler invocation,
 * result parsing, and non-terminal op execution.
 */

#include "deviceDrivers/matter/sbmd/mquickjs/MQuickJsRuntime.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdUtilsLoader.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdV4HandlerInvoker.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdV4Loader.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

extern "C" {
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
    };

    struct SetMetadataCall
    {
        std::string deviceUuid;
        std::string endpointId;
        std::string key;
        std::string value;
    };

    std::vector<UpdateResourceCall> g_updateResourceCalls;
    std::vector<SetMetadataCall> g_setMetadataCalls;
} // namespace

extern "C" {
void updateResource(const char *deviceUuid,
                    const char *endpointId,
                    const char *resourceId,
                    const char *newValue,
                    void *metadata)
{
    g_updateResourceCalls.push_back({deviceUuid ? deviceUuid : "",
                                     endpointId ? endpointId : "",
                                     resourceId ? resourceId : "",
                                     newValue ? newValue : ""});
}

void setMetadata(const char *deviceUuid, const char *endpointId, const char *name, const char *value)
{
    g_setMetadataCalls.push_back(
        {deviceUuid ? deviceUuid : "", endpointId ? endpointId : "", name ? name : "", value ? value : ""});
}
}

namespace
{
    class SbmdV4HandlerInvokerTest : public ::testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            ASSERT_TRUE(MQuickJsRuntime::Initialize(512 * 1024));
            auto *ctx = MQuickJsRuntime::GetSharedContext();
            ASSERT_NE(ctx, nullptr);
            ASSERT_TRUE(SbmdUtilsLoader::LoadBundle(ctx));
            ASSERT_TRUE(SbmdV4Loader::InjectCaptureFunction(ctx));
        }

        static void TearDownTestSuite()
        {
            MQuickJsRuntime::Shutdown();
        }

        void SetUp() override
        {
            g_updateResourceCalls.clear();
            g_setMetadataCalls.clear();
        }

        JSContext *Ctx()
        {
            return MQuickJsRuntime::GetSharedContext();
        }

        HandlerContext MakeContext()
        {
            HandlerContext hctx;
            hctx.deviceUuid = "test-device-uuid";
            hctx.endpointId = "1";
            hctx.clusterFeatureMaps = {{6, 0x01}, {8, 0x03}};

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

    TEST_F(SbmdV4HandlerInvokerTest, BuildAttributeArgsBasicFields)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        JSValue args = SbmdV4HandlerInvoker::BuildAttributeArgs(Ctx(), hctx, 6, 0, "AB==");
        ASSERT_FALSE(JS_IsException(args));

        EXPECT_EQ(GetStringProp(args, "deviceUuid"), "test-device-uuid");
        EXPECT_EQ(GetStringProp(args, "endpointId"), "1");

        // Check attribute trigger
        JSValue attr = JS_GetPropertyStr(Ctx(), args, "attribute");
        ASSERT_FALSE(JS_IsUndefined(attr));
        EXPECT_EQ(GetUint32Prop(attr, "clusterId"), 6u);
        EXPECT_EQ(GetUint32Prop(attr, "attributeId"), 0u);
        EXPECT_EQ(GetStringProp(attr, "tlvBase64"), "AB==");
    }

    TEST_F(SbmdV4HandlerInvokerTest, BuildAttributeArgsFeatureMaps)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        JSValue args = SbmdV4HandlerInvoker::BuildAttributeArgs(Ctx(), hctx, 6, 0, "");

        JSValue fm = JS_GetPropertyStr(Ctx(), args, "clusterFeatureMaps");
        ASSERT_FALSE(JS_IsUndefined(fm));
        EXPECT_EQ(GetUint32Prop(fm, "6"), 0x01u);
        EXPECT_EQ(GetUint32Prop(fm, "8"), 0x03u);
    }

    TEST_F(SbmdV4HandlerInvokerTest, BuildAttributeArgsEmptyTlv)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        JSValue args = SbmdV4HandlerInvoker::BuildAttributeArgs(Ctx(), hctx, 6, 0, "");

        JSValue attr = JS_GetPropertyStr(Ctx(), args, "attribute");
        JSValue tlv = JS_GetPropertyStr(Ctx(), attr, "tlvBase64");
        EXPECT_TRUE(JS_IsUndefined(tlv));
    }

    // ========================================================================
    // BuildResourceArgs
    // ========================================================================

    TEST_F(SbmdV4HandlerInvokerTest, BuildResourceArgsRead)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        JSValue args = SbmdV4HandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::nullopt);
        ASSERT_FALSE(JS_IsException(args));

        EXPECT_EQ(GetStringProp(args, "deviceUuid"), "test-device-uuid");

        JSValue resource = JS_GetPropertyStr(Ctx(), args, "resource");
        ASSERT_FALSE(JS_IsUndefined(resource));
        EXPECT_EQ(GetStringProp(resource, "resourceId"), "isOn");

        // input should be null for read
        JSValue input = JS_GetPropertyStr(Ctx(), resource, "input");
        EXPECT_TRUE(JS_IsNull(input));
    }

    TEST_F(SbmdV4HandlerInvokerTest, BuildResourceArgsWrite)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        JSValue args = SbmdV4HandlerInvoker::BuildResourceArgs(Ctx(), hctx, "dimLevel", std::string("75"));
        ASSERT_FALSE(JS_IsException(args));

        JSValue resource = JS_GetPropertyStr(Ctx(), args, "resource");
        EXPECT_EQ(GetStringProp(resource, "resourceId"), "dimLevel");
        EXPECT_EQ(GetStringProp(resource, "input"), "75");
    }

    // ========================================================================
    // InvokeHandler
    // ========================================================================

    TEST_F(SbmdV4HandlerInvokerTest, InvokeSimpleSuccessHandler)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        JSValue handler = EvalFunc("(function(args) { return SbmdUtils.result().success(); })");
        ASSERT_FALSE(JS_IsException(handler));

        JSValue args = SbmdV4HandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::nullopt);
        auto result = SbmdV4HandlerInvoker::InvokeHandler(Ctx(), handler, args);

        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(result->ops.empty());
        EXPECT_TRUE(std::holds_alternative<ResultTerminal::Success>(result->terminal.data));
    }

    TEST_F(SbmdV4HandlerInvokerTest, InvokeHandlerWithOps)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        JSValue handler = EvalFunc(
            "(function(args) {"
            "  return SbmdUtils.result()"
            "    .dataModel.updateResource(args.endpointId, 'isOn', 'true')"
            "    .success();"
            "})");
        ASSERT_FALSE(JS_IsException(handler));

        JSValue args = SbmdV4HandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::nullopt);
        auto result = SbmdV4HandlerInvoker::InvokeHandler(Ctx(), handler, args);

        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result->ops.size(), 1u);
        ASSERT_TRUE(std::holds_alternative<ResultOp::UpdateResource>(result->ops[0].data));

        auto &ur = std::get<ResultOp::UpdateResource>(result->ops[0].data);
        EXPECT_EQ(*ur.endpoint, "1"); // from args.endpointId
        EXPECT_EQ(ur.resource, "isOn");
        EXPECT_EQ(ur.value, "true");
    }

    TEST_F(SbmdV4HandlerInvokerTest, InvokeHandlerWithSendCommand)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        JSValue handler = EvalFunc(
            "(function(args) {"
            "  return SbmdUtils.result()"
            "    .device.sendCommand(6, 1);"
            "})");
        ASSERT_FALSE(JS_IsException(handler));

        JSValue args = SbmdV4HandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::string("true"));
        auto result = SbmdV4HandlerInvoker::InvokeHandler(Ctx(), handler, args);

        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::SendCommand>(result->terminal.data));

        auto &cmd = std::get<ResultTerminal::SendCommand>(result->terminal.data);
        EXPECT_EQ(cmd.clusterId, 6u);
        EXPECT_EQ(cmd.commandId, 1u);
    }

    TEST_F(SbmdV4HandlerInvokerTest, InvokeThrowingHandlerReturnsNullopt)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        JSValue handler = EvalFunc("(function(args) { throw new Error('boom'); })");
        ASSERT_FALSE(JS_IsException(handler));

        JSValue args = SbmdV4HandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::nullopt);
        auto result = SbmdV4HandlerInvoker::InvokeHandler(Ctx(), handler, args);

        EXPECT_FALSE(result.has_value());
    }

    TEST_F(SbmdV4HandlerInvokerTest, InvokeUndefinedHandlerReturnsNullopt)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto hctx = MakeContext();

        JSValue args = SbmdV4HandlerInvoker::BuildResourceArgs(Ctx(), hctx, "isOn", std::nullopt);
        auto result = SbmdV4HandlerInvoker::InvokeHandler(Ctx(), JS_UNDEFINED, args);

        EXPECT_FALSE(result.has_value());
    }

    // ========================================================================
    // ExecuteOps
    // ========================================================================

    TEST_F(SbmdV4HandlerInvokerTest, ExecuteOpsUpdateResource)
    {
        auto hctx = MakeContext();

        std::vector<ResultOp> ops;
        ResultOp::UpdateResource ur;
        ur.endpoint = "1";
        ur.resource = "isOn";
        ur.value = "true";
        ops.push_back(ResultOp{ur});

        SbmdV4HandlerInvoker::ExecuteOps(hctx, ops);

        ASSERT_EQ(g_updateResourceCalls.size(), 1u);
        EXPECT_EQ(g_updateResourceCalls[0].deviceUuid, "test-device-uuid");
        EXPECT_EQ(g_updateResourceCalls[0].endpointId, "1");
        EXPECT_EQ(g_updateResourceCalls[0].resourceId, "isOn");
        EXPECT_EQ(g_updateResourceCalls[0].value, "true");
    }

    TEST_F(SbmdV4HandlerInvokerTest, ExecuteOpsUpdateResourceUsesDefaultEndpoint)
    {
        auto hctx = MakeContext();

        std::vector<ResultOp> ops;
        ResultOp::UpdateResource ur;
        // No endpoint set — should use hctx.endpointId
        ur.resource = "isOn";
        ur.value = "false";
        ops.push_back(ResultOp{ur});

        SbmdV4HandlerInvoker::ExecuteOps(hctx, ops);

        ASSERT_EQ(g_updateResourceCalls.size(), 1u);
        EXPECT_EQ(g_updateResourceCalls[0].endpointId, "1"); // default from context
    }

    TEST_F(SbmdV4HandlerInvokerTest, ExecuteOpsSetMetadata)
    {
        auto hctx = MakeContext();

        std::vector<ResultOp> ops;
        ResultOp::SetMetadata sm;
        sm.endpoint = "1";
        sm.resource = "dimLevel";
        sm.key = "unit";
        sm.value = "percent";
        ops.push_back(ResultOp{sm});

        SbmdV4HandlerInvoker::ExecuteOps(hctx, ops);

        ASSERT_EQ(g_setMetadataCalls.size(), 1u);
        EXPECT_EQ(g_setMetadataCalls[0].deviceUuid, "test-device-uuid");
        EXPECT_EQ(g_setMetadataCalls[0].endpointId, "1");
        EXPECT_EQ(g_setMetadataCalls[0].key, "unit");
        EXPECT_EQ(g_setMetadataCalls[0].value, "percent");
    }

    TEST_F(SbmdV4HandlerInvokerTest, ExecuteOpsMultiple)
    {
        auto hctx = MakeContext();

        std::vector<ResultOp> ops;

        ResultOp::Log logOp;
        logOp.message = "updating";
        ops.push_back(ResultOp{logOp});

        ResultOp::UpdateResource ur;
        ur.endpoint = "1";
        ur.resource = "isOn";
        ur.value = "true";
        ops.push_back(ResultOp{ur});

        ResultOp::SetMetadata sm;
        sm.endpoint = "1";
        sm.resource = "isOn";
        sm.key = "source";
        sm.value = "device";
        ops.push_back(ResultOp{sm});

        SbmdV4HandlerInvoker::ExecuteOps(hctx, ops);

        // Log doesn't produce external calls, but the other two should
        EXPECT_EQ(g_updateResourceCalls.size(), 1u);
        EXPECT_EQ(g_setMetadataCalls.size(), 1u);
    }

    // ========================================================================
    // End-to-end: invoke → parse → execute ops
    // ========================================================================

    TEST_F(SbmdV4HandlerInvokerTest, EndToEndInvokeAndExecuteOps)
    {
        auto hctx = MakeContext();

        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());

        JSValue handler = EvalFunc(
            "(function(args) {"
            "  return SbmdUtils.result()"
            "    .log('attribute changed')"
            "    .dataModel.updateResource(args.endpointId, 'isOn', 'true')"
            "    .success();"
            "})");
        ASSERT_FALSE(JS_IsException(handler));

        JSValue args = SbmdV4HandlerInvoker::BuildAttributeArgs(Ctx(), hctx, 6, 0, "AB==");
        auto result = SbmdV4HandlerInvoker::InvokeHandler(Ctx(), handler, args);
        ASSERT_TRUE(result.has_value());

        // Execute ops outside the mutex (in real code) but fine in test
        SbmdV4HandlerInvoker::ExecuteOps(hctx, result->ops);

        ASSERT_EQ(g_updateResourceCalls.size(), 1u);
        EXPECT_EQ(g_updateResourceCalls[0].endpointId, "1");
        EXPECT_EQ(g_updateResourceCalls[0].resourceId, "isOn");
        EXPECT_EQ(g_updateResourceCalls[0].value, "true");
    }

} // namespace
