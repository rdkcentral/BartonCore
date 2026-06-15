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
 * Unit tests for the Sbmd.result() builder (result chain).
 *
 * These tests initialize the mquickjs runtime, load sbmd-utils.js,
 * then evaluate JS expressions to verify the builder API produces
 * the expected {ops, terminal} structures.
 */

#include "deviceDrivers/matter/sbmd/mquickjs/MQuickJsRuntime.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdBundleLoader.h"

#include <gtest/gtest.h>
#include <string>

extern "C" {
#include <mquickjs/mquickjs.h>
}

using namespace barton;

namespace
{
    class ResultBuilderTest : public ::testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            ASSERT_TRUE(MQuickJsRuntime::Initialize(256 * 1024));
            auto *ctx = MQuickJsRuntime::GetSharedContext();
            ASSERT_NE(ctx, nullptr);
            ASSERT_TRUE(SbmdBundleLoader::LoadBundle(ctx));
        }

        static void TearDownTestSuite()
        {
            MQuickJsRuntime::Shutdown();
        }

        /**
         * Evaluate a JS expression and return the result as a JSON string.
         * The expression is wrapped in JSON.stringify() automatically.
         */
        std::string EvalAsJson(const char *expr)
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto *ctx = MQuickJsRuntime::GetSharedContext();

            std::string code = std::string("JSON.stringify(") + expr + ")";

            JSValue result = JS_Eval(ctx, code.c_str(), code.size(), "<test>", JS_EVAL_RETVAL);

            if (JS_IsException(result))
            {
                std::string msg;
                MQuickJsRuntime::CheckAndClearPendingException(ctx, &msg);
                return "EXCEPTION: " + msg;
            }

            JSCStringBuf buf;
            const char *str = JS_ToCString(ctx, result, &buf);
            std::string jsonStr(str ? str : "null");

            return jsonStr;
        }

        /**
         * Evaluate a JS expression and return true if it threw an exception.
         */
        bool EvalThrows(const char *expr)
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto *ctx = MQuickJsRuntime::GetSharedContext();

            JSValue result = JS_Eval(ctx, expr, strlen(expr), "<test>", JS_EVAL_RETVAL);

            if (JS_IsException(result))
            {
                MQuickJsRuntime::CheckAndClearPendingException(ctx);
                return true;
            }

            return false;
        }
    };

    // ========================================================================
    // Basic builder creation
    // ========================================================================

    TEST_F(ResultBuilderTest, SuccessTerminalEmptyOps)
    {
        auto json = EvalAsJson("Sbmd.result().success()");
        EXPECT_EQ(json, R"({"ops":[],"terminal":{"op":"success"}})");
    }

    TEST_F(ResultBuilderTest, ErrorTerminal)
    {
        auto json = EvalAsJson("Sbmd.result().error('something failed')");
        EXPECT_EQ(json, R"({"ops":[],"terminal":{"op":"error","message":"something failed"}})");
    }

    // ========================================================================
    // Non-terminal operations
    // ========================================================================

    TEST_F(ResultBuilderTest, LogOperation)
    {
        auto json = EvalAsJson("Sbmd.result().log('hello').success()");
        EXPECT_EQ(json, R"({"ops":[{"op":"log","message":"hello"}],"terminal":{"op":"success"}})");
    }

    TEST_F(ResultBuilderTest, UpdateResourceTwoArgs)
    {
        auto json = EvalAsJson("Sbmd.result().dataModel.updateResource('isOn', 'true').success()");
        EXPECT_EQ(json,
                  R"({"ops":[{"op":"updateResource","resource":"isOn","value":"true"}],"terminal":{"op":"success"}})");
    }

    TEST_F(ResultBuilderTest, UpdateResourceThreeArgs)
    {
        auto json = EvalAsJson("Sbmd.result().dataModel.updateResource('1', 'isOn', 'true').success()");
        EXPECT_EQ(
            json,
            R"({"ops":[{"op":"updateResource","endpoint":"1","resource":"isOn","value":"true"}],"terminal":{"op":"success"}})");
    }

    TEST_F(ResultBuilderTest, UpdateResourceFourArgs)
    {
        auto json =
            EvalAsJson("Sbmd.result().dataModel.updateResource('1', 'isOn', 'true', {source: 'device'}).success()");
        EXPECT_EQ(
            json,
            R"({"ops":[{"op":"updateResource","endpoint":"1","resource":"isOn","value":"true","metadata":"{\"source\":\"device\"}"}],"terminal":{"op":"success"}})");
    }

    TEST_F(ResultBuilderTest, SetMetadata)
    {
        auto json = EvalAsJson("Sbmd.result().dataModel.setMetadata('label', 'On/Off').success()");
        EXPECT_EQ(json,
                  R"({"ops":[{"op":"setMetadata","name":"label","value":"On/Off"}],"terminal":{"op":"success"}})");
    }

    TEST_F(ResultBuilderTest, SetPersistentData)
    {
        auto json = EvalAsJson("Sbmd.result().storage.setPersistentData('lastState', 'on').success()");
        EXPECT_EQ(
            json,
            R"({"ops":[{"op":"setPersistentData","key":"lastState","value":"on"}],"terminal":{"op":"success"}})");
    }

    TEST_F(ResultBuilderTest, SetTransientData)
    {
        auto json = EvalAsJson("Sbmd.result().storage.setTransientData('cache', '42').success()");
        EXPECT_EQ(json,
                  R"({"ops":[{"op":"setTransientData","key":"cache","value":"42"}],"terminal":{"op":"success"}})");
    }

    // ========================================================================
    // Linear chaining — multiple operations
    // ========================================================================

    TEST_F(ResultBuilderTest, MultipleOpsBeforeTerminal)
    {
        auto json = EvalAsJson("Sbmd.result()"
                               ".dataModel.updateResource('1', 'isOn', 'true')"
                               ".log('updated isOn')"
                               ".storage.setPersistentData('last', 'on')"
                               ".success()");
        EXPECT_EQ(json,
                  R"({"ops":[{"op":"updateResource","endpoint":"1","resource":"isOn","value":"true"},)"
                  R"({"op":"log","message":"updated isOn"},)"
                  R"({"op":"setPersistentData","key":"last","value":"on"}],)"
                  R"("terminal":{"op":"success"}})");
    }

    TEST_F(ResultBuilderTest, OpsBeforeErrorTerminal)
    {
        auto json = EvalAsJson("Sbmd.result().log('diagnostic').error('failed')");
        EXPECT_EQ(
            json,
            R"({"ops":[{"op":"log","message":"diagnostic"}],"terminal":{"op":"error","message":"failed"}})");
    }

    // ========================================================================
    // Device terminals
    // ========================================================================

    TEST_F(ResultBuilderTest, SendCommandMinimal)
    {
        auto json = EvalAsJson("Sbmd.result().device.sendCommand(6, 1)");
        EXPECT_EQ(json, R"({"ops":[],"terminal":{"op":"sendCommand","clusterId":6,"commandId":1}})");
    }

    TEST_F(ResultBuilderTest, SendCommandWithPayload)
    {
        auto json = EvalAsJson("Sbmd.result().device.sendCommand(8, 4, 'AQID')");
        EXPECT_EQ(json,
                  R"({"ops":[],"terminal":{"op":"sendCommand","clusterId":8,"commandId":4,"tlvBase64":"AQID"}})");
    }

    TEST_F(ResultBuilderTest, SendCommandWithOptions)
    {
        auto json = EvalAsJson("Sbmd.result().device.sendCommand(257, 0, 'AB==', {timedInvokeTimeoutMs: 10000})");
        EXPECT_EQ(
            json,
            R"({"ops":[],"terminal":{"op":"sendCommand","clusterId":257,"commandId":0,"tlvBase64":"AB==","options":{"timedInvokeTimeoutMs":10000}}})");
    }

    TEST_F(ResultBuilderTest, WriteAttribute)
    {
        auto json = EvalAsJson("Sbmd.result().device.writeAttribute(3, 0, 'AQID')");
        EXPECT_EQ(
            json,
            R"({"ops":[],"terminal":{"op":"writeAttribute","clusterId":3,"attributeId":0,"tlvBase64":"AQID"}})");
    }

    TEST_F(ResultBuilderTest, WriteAttributeWithOptions)
    {
        auto json = EvalAsJson("Sbmd.result().device.writeAttribute(3, 0, 'AQID', {endpointId: 2})");
        EXPECT_EQ(
            json,
            R"({"ops":[],"terminal":{"op":"writeAttribute","clusterId":3,"attributeId":0,"tlvBase64":"AQID","options":{"endpointId":2}}})");
    }

    TEST_F(ResultBuilderTest, RequestCommand)
    {
        // Note: deferred.onResponse and onError are functions — they won't serialize to JSON.
        // We test the structural properties that do serialize.
        auto json =
            EvalAsJson("(function() { var r = Sbmd.result().device.requestCommand(257, 0, "
                       "{ responseCommandId: 26, timeoutMs: 5000 });"
                       "return { ops: r.ops, terminalOp: r.terminal.op, clusterId: r.terminal.clusterId, "
                       "   commandId: r.terminal.commandId, responseCommandId: r.terminal.deferred.responseCommandId, "
                       "   timeoutMs: r.terminal.deferred.timeoutMs }; })()");
        EXPECT_EQ(
            json,
            R"({"ops":[],"terminalOp":"requestCommand","clusterId":257,"commandId":0,"responseCommandId":26,"timeoutMs":5000})");
    }

    TEST_F(ResultBuilderTest, ReadAttribute)
    {
        auto json =
            EvalAsJson("(function() { var r = Sbmd.result().device.readAttribute(6, 0, { timeoutMs: 3000 });"
                       "return { ops: r.ops, terminalOp: r.terminal.op, clusterId: r.terminal.clusterId, "
                       "   attributeId: r.terminal.attributeId, timeoutMs: r.terminal.deferred.timeoutMs }; })()");
        EXPECT_EQ(json, R"({"ops":[],"terminalOp":"readAttribute","clusterId":6,"attributeId":0,"timeoutMs":3000})");
    }

    TEST_F(ResultBuilderTest, OpsBeforeDeviceTerminal)
    {
        auto json = EvalAsJson("Sbmd.result()"
                               ".dataModel.updateResource('1', 'isOn', 'true')"
                               ".log('sending command')"
                               ".device.sendCommand(6, 1)");
        EXPECT_EQ(json,
                  R"({"ops":[{"op":"updateResource","endpoint":"1","resource":"isOn","value":"true"},)"
                  R"({"op":"log","message":"sending command"}],)"
                  R"("terminal":{"op":"sendCommand","clusterId":6,"commandId":1}})");
    }

    // ========================================================================
    // Terminal sealing — prevents further operations
    // ========================================================================

    TEST_F(ResultBuilderTest, TerminalCutsOffChaining)
    {
        // After success(), the returned raw object has no .log method
        EXPECT_TRUE(EvalThrows("Sbmd.result().success().log('after')"));
    }

    TEST_F(ResultBuilderTest, StoredBuilderThrowsAfterTerminal)
    {
        // Store builder reference, call terminal, then try to add ops
        EXPECT_TRUE(EvalThrows("(function() { var b = Sbmd.result(); b.success(); b.log('after'); })()"));
    }

    TEST_F(ResultBuilderTest, StoredBuilderThrowsAfterTerminalViaDataModel)
    {
        EXPECT_TRUE(
            EvalThrows("(function() { var b = Sbmd.result(); b.success(); b.dataModel.updateResource('x', 'y'); })()"));
    }

    TEST_F(ResultBuilderTest, DoubleTerminalThrows)
    {
        EXPECT_TRUE(EvalThrows("(function() { var b = Sbmd.result(); b.success(); b.error('fail'); })()"));
    }

} // namespace
