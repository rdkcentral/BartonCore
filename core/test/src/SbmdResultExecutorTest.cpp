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
 * Unit tests for SbmdResultExecutor::Parse — walks handler result JSValues
 * and extracts typed ParsedResult structures.
 */

#include "deviceDrivers/matter/sbmd/mquickjs/MQuickJsRuntime.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdUtilsLoader.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdResultExecutor.h"

#include <gtest/gtest.h>
#include <string>

extern "C" {
#include <mquickjs/mquickjs.h>
}

using namespace barton;

namespace
{
    class SbmdResultExecutorTest : public ::testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            ASSERT_TRUE(MQuickJsRuntime::Initialize(256 * 1024));
            auto *ctx = MQuickJsRuntime::GetSharedContext();
            ASSERT_NE(ctx, nullptr);
            ASSERT_TRUE(SbmdUtilsLoader::LoadBundle(ctx));
        }

        static void TearDownTestSuite()
        {
            MQuickJsRuntime::Shutdown();
        }

        /**
         * Evaluate a JS expression and return the raw JSValue.
         * Caller must hold the mutex.
         */
        JSValue Eval(const char *expr)
        {
            auto *ctx = MQuickJsRuntime::GetSharedContext();

            return JS_Eval(ctx, expr, strlen(expr), "<test>", JS_EVAL_RETVAL);
        }

        /**
         * Evaluate a JS expression, parse the result chain, and return it.
         * Takes and releases the mutex.
         */
        std::optional<ParsedResult> EvalAndParse(const char *expr)
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto *ctx = MQuickJsRuntime::GetSharedContext();

            JSValue result = Eval(expr);

            if (JS_IsException(result))
            {
                MQuickJsRuntime::CheckAndClearPendingException(ctx);
                return std::nullopt;
            }

            return SbmdResultExecutor::Parse(ctx, result);
        }
    };

    // ========================================================================
    // Basic parse: success terminal with empty ops
    // ========================================================================

    TEST_F(SbmdResultExecutorTest, ParseSuccessTerminal)
    {
        auto parsed = EvalAndParse("SbmdUtils.result().success()");
        ASSERT_TRUE(parsed.has_value());
        EXPECT_TRUE(parsed->ops.empty());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::Success>(parsed->terminal.data));
    }

    TEST_F(SbmdResultExecutorTest, ParseErrorTerminal)
    {
        auto parsed = EvalAndParse("SbmdUtils.result().error('something broke')");
        ASSERT_TRUE(parsed.has_value());
        EXPECT_TRUE(parsed->ops.empty());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::Error>(parsed->terminal.data));
        EXPECT_EQ(std::get<ResultTerminal::Error>(parsed->terminal.data).message, "something broke");
    }

    // ========================================================================
    // Non-terminal ops
    // ========================================================================

    TEST_F(SbmdResultExecutorTest, ParseLogOp)
    {
        auto parsed = EvalAndParse("SbmdUtils.result().log('hello world').success()");
        ASSERT_TRUE(parsed.has_value());
        ASSERT_EQ(parsed->ops.size(), 1u);
        ASSERT_TRUE(std::holds_alternative<ResultOp::Log>(parsed->ops[0].data));
        EXPECT_EQ(std::get<ResultOp::Log>(parsed->ops[0].data).message, "hello world");
    }

    TEST_F(SbmdResultExecutorTest, ParseUpdateResource2Arg)
    {
        auto parsed = EvalAndParse("SbmdUtils.result().dataModel.updateResource('isOn', 'true').success()");
        ASSERT_TRUE(parsed.has_value());
        ASSERT_EQ(parsed->ops.size(), 1u);
        ASSERT_TRUE(std::holds_alternative<ResultOp::UpdateResource>(parsed->ops[0].data));

        auto &ur = std::get<ResultOp::UpdateResource>(parsed->ops[0].data);
        EXPECT_FALSE(ur.endpoint.has_value());
        EXPECT_EQ(ur.resource, "isOn");
        EXPECT_EQ(ur.value, "true");
    }

    TEST_F(SbmdResultExecutorTest, ParseUpdateResource3Arg)
    {
        auto parsed = EvalAndParse("SbmdUtils.result().dataModel.updateResource('1', 'isOn', 'true').success()");
        ASSERT_TRUE(parsed.has_value());
        ASSERT_EQ(parsed->ops.size(), 1u);
        ASSERT_TRUE(std::holds_alternative<ResultOp::UpdateResource>(parsed->ops[0].data));

        auto &ur = std::get<ResultOp::UpdateResource>(parsed->ops[0].data);
        ASSERT_TRUE(ur.endpoint.has_value());
        EXPECT_EQ(*ur.endpoint, "1");
        EXPECT_EQ(ur.resource, "isOn");
        EXPECT_EQ(ur.value, "true");
    }

    TEST_F(SbmdResultExecutorTest, ParseSetMetadata)
    {
        auto parsed =
            EvalAndParse("SbmdUtils.result().dataModel.setMetadata('1', 'dimLevel', 'unit', 'percent').success()");
        ASSERT_TRUE(parsed.has_value());
        ASSERT_EQ(parsed->ops.size(), 1u);
        ASSERT_TRUE(std::holds_alternative<ResultOp::SetMetadata>(parsed->ops[0].data));

        auto &sm = std::get<ResultOp::SetMetadata>(parsed->ops[0].data);
        EXPECT_EQ(sm.endpoint, "1");
        EXPECT_EQ(sm.resource, "dimLevel");
        EXPECT_EQ(sm.key, "unit");
        EXPECT_EQ(sm.value, "percent");
    }

    TEST_F(SbmdResultExecutorTest, ParseSetPersistentData)
    {
        auto parsed = EvalAndParse("SbmdUtils.result().storage.setPersistentData('lastState', 'on').success()");
        ASSERT_TRUE(parsed.has_value());
        ASSERT_EQ(parsed->ops.size(), 1u);
        ASSERT_TRUE(std::holds_alternative<ResultOp::SetPersistentData>(parsed->ops[0].data));

        auto &sp = std::get<ResultOp::SetPersistentData>(parsed->ops[0].data);
        EXPECT_EQ(sp.key, "lastState");
        EXPECT_EQ(sp.value, "on");
    }

    TEST_F(SbmdResultExecutorTest, ParseSetTransientData)
    {
        auto parsed = EvalAndParse("SbmdUtils.result().storage.setTransientData('debounce', '1').success()");
        ASSERT_TRUE(parsed.has_value());
        ASSERT_EQ(parsed->ops.size(), 1u);
        ASSERT_TRUE(std::holds_alternative<ResultOp::SetTransientData>(parsed->ops[0].data));

        auto &st = std::get<ResultOp::SetTransientData>(parsed->ops[0].data);
        EXPECT_EQ(st.key, "debounce");
        EXPECT_EQ(st.value, "1");
    }

    // ========================================================================
    // Multiple ops before terminal
    // ========================================================================

    TEST_F(SbmdResultExecutorTest, ParseMultipleOps)
    {
        auto parsed = EvalAndParse("SbmdUtils.result()"
                                   ".log('updating')"
                                   ".dataModel.updateResource('1', 'temp', '72')"
                                   ".storage.setPersistentData('last', 'ok')"
                                   ".success()");
        ASSERT_TRUE(parsed.has_value());
        ASSERT_EQ(parsed->ops.size(), 3u);
        EXPECT_TRUE(std::holds_alternative<ResultOp::Log>(parsed->ops[0].data));
        EXPECT_TRUE(std::holds_alternative<ResultOp::UpdateResource>(parsed->ops[1].data));
        EXPECT_TRUE(std::holds_alternative<ResultOp::SetPersistentData>(parsed->ops[2].data));
        EXPECT_TRUE(std::holds_alternative<ResultTerminal::Success>(parsed->terminal.data));
    }

    // ========================================================================
    // Device terminal: sendCommand
    // ========================================================================

    TEST_F(SbmdResultExecutorTest, ParseSendCommandMinimal)
    {
        auto parsed = EvalAndParse("SbmdUtils.result().device.sendCommand(6, 1)");
        ASSERT_TRUE(parsed.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::SendCommand>(parsed->terminal.data));

        auto &cmd = std::get<ResultTerminal::SendCommand>(parsed->terminal.data);
        EXPECT_EQ(cmd.clusterId, 6u);
        EXPECT_EQ(cmd.commandId, 1u);
        EXPECT_TRUE(cmd.tlvBase64.empty());
        EXPECT_FALSE(cmd.endpointId.has_value());
        EXPECT_FALSE(cmd.timedInvokeTimeoutMs.has_value());
    }

    TEST_F(SbmdResultExecutorTest, ParseSendCommandWithPayload)
    {
        auto parsed = EvalAndParse("SbmdUtils.result().device.sendCommand(257, 0, 'AB==')");
        ASSERT_TRUE(parsed.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::SendCommand>(parsed->terminal.data));

        auto &cmd = std::get<ResultTerminal::SendCommand>(parsed->terminal.data);
        EXPECT_EQ(cmd.clusterId, 257u);
        EXPECT_EQ(cmd.commandId, 0u);
        EXPECT_EQ(cmd.tlvBase64, "AB==");
    }

    TEST_F(SbmdResultExecutorTest, ParseSendCommandWithOptions)
    {
        auto parsed = EvalAndParse(
            "SbmdUtils.result().device.sendCommand(257, 0, 'AB==', {timedInvokeTimeoutMs: 10000, endpointId: 5})");
        ASSERT_TRUE(parsed.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::SendCommand>(parsed->terminal.data));

        auto &cmd = std::get<ResultTerminal::SendCommand>(parsed->terminal.data);
        EXPECT_EQ(cmd.clusterId, 257u);
        EXPECT_EQ(cmd.commandId, 0u);
        EXPECT_EQ(cmd.tlvBase64, "AB==");
        ASSERT_TRUE(cmd.endpointId.has_value());
        EXPECT_EQ(*cmd.endpointId, 5u);
        ASSERT_TRUE(cmd.timedInvokeTimeoutMs.has_value());
        EXPECT_EQ(*cmd.timedInvokeTimeoutMs, 10000u);
    }

    // ========================================================================
    // Device terminal: writeAttribute
    // ========================================================================

    TEST_F(SbmdResultExecutorTest, ParseWriteAttribute)
    {
        auto parsed = EvalAndParse("SbmdUtils.result().device.writeAttribute(3, 0, 'AQID')");
        ASSERT_TRUE(parsed.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::WriteAttribute>(parsed->terminal.data));

        auto &wa = std::get<ResultTerminal::WriteAttribute>(parsed->terminal.data);
        EXPECT_EQ(wa.clusterId, 3u);
        EXPECT_EQ(wa.attributeId, 0u);
        EXPECT_EQ(wa.tlvBase64, "AQID");
        EXPECT_FALSE(wa.endpointId.has_value());
    }

    TEST_F(SbmdResultExecutorTest, ParseWriteAttributeWithOptions)
    {
        auto parsed = EvalAndParse("SbmdUtils.result().device.writeAttribute(3, 0, 'AQID', {endpointId: 2})");
        ASSERT_TRUE(parsed.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::WriteAttribute>(parsed->terminal.data));

        auto &wa = std::get<ResultTerminal::WriteAttribute>(parsed->terminal.data);
        EXPECT_EQ(wa.clusterId, 3u);
        EXPECT_EQ(wa.attributeId, 0u);
        EXPECT_EQ(wa.tlvBase64, "AQID");
        ASSERT_TRUE(wa.endpointId.has_value());
        EXPECT_EQ(*wa.endpointId, 2u);
    }

    // ========================================================================
    // Device terminal: requestCommand (deferred)
    // ========================================================================

    TEST_F(SbmdResultExecutorTest, ParseRequestCommand)
    {
        // Use IIFE to allow var declarations
        auto parsed = EvalAndParse(
            "(function() {"
            "  var deferred = {"
            "    responseCommandId: 42,"
            "    onResponse: function(args) { return SbmdUtils.result().success(); },"
            "    onError: function(args) { return SbmdUtils.result().error('timeout'); },"
            "    timeoutMs: 5000"
            "  };"
            "  return SbmdUtils.result().device.requestCommand(0x0101, 0, deferred, 'AB==');"
            "})()");
        ASSERT_TRUE(parsed.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::RequestCommand>(parsed->terminal.data));

        auto &rc = std::get<ResultTerminal::RequestCommand>(parsed->terminal.data);
        EXPECT_EQ(rc.clusterId, 0x0101u);
        EXPECT_EQ(rc.commandId, 0u);
        EXPECT_EQ(rc.tlvBase64, "AB==");
        EXPECT_EQ(rc.responseCommandId, 42u);
        ASSERT_TRUE(rc.timeoutMs.has_value());
        EXPECT_EQ(*rc.timeoutMs, 5000u);

        // The handlers should be JS functions (not undefined)
        EXPECT_FALSE(JS_IsUndefined(rc.onResponse));
        EXPECT_FALSE(JS_IsUndefined(rc.onError));
    }

    // ========================================================================
    // Device terminal: readAttribute (deferred)
    // ========================================================================

    TEST_F(SbmdResultExecutorTest, ParseReadAttribute)
    {
        auto parsed = EvalAndParse(
            "(function() {"
            "  var deferred = {"
            "    onResponse: function(args) { return SbmdUtils.result().success(); },"
            "    onError: function(args) { return SbmdUtils.result().error('fail'); },"
            "    timeoutMs: 3000"
            "  };"
            "  return SbmdUtils.result().device.readAttribute(0x0300, 0x0001, deferred);"
            "})()");
        ASSERT_TRUE(parsed.has_value());
        ASSERT_TRUE(std::holds_alternative<ResultTerminal::ReadAttribute>(parsed->terminal.data));

        auto &ra = std::get<ResultTerminal::ReadAttribute>(parsed->terminal.data);
        EXPECT_EQ(ra.clusterId, 0x0300u);
        EXPECT_EQ(ra.attributeId, 0x0001u);
        EXPECT_FALSE(ra.endpointId.has_value());
        ASSERT_TRUE(ra.timeoutMs.has_value());
        EXPECT_EQ(*ra.timeoutMs, 3000u);
        EXPECT_FALSE(JS_IsUndefined(ra.onResponse));
        EXPECT_FALSE(JS_IsUndefined(ra.onError));
    }

    // ========================================================================
    // Ops before device terminal
    // ========================================================================

    TEST_F(SbmdResultExecutorTest, ParseOpsBeforeDeviceTerminal)
    {
        auto parsed = EvalAndParse("SbmdUtils.result()"
                                   ".log('sending lock command')"
                                   ".storage.setPersistentData('lastLockOp', 'lock')"
                                   ".device.sendCommand(0x0101, 0)");
        ASSERT_TRUE(parsed.has_value());
        ASSERT_EQ(parsed->ops.size(), 2u);
        EXPECT_TRUE(std::holds_alternative<ResultOp::Log>(parsed->ops[0].data));
        EXPECT_TRUE(std::holds_alternative<ResultOp::SetPersistentData>(parsed->ops[1].data));
        EXPECT_TRUE(std::holds_alternative<ResultTerminal::SendCommand>(parsed->terminal.data));
    }

    // ========================================================================
    // Edge cases
    // ========================================================================

    TEST_F(SbmdResultExecutorTest, ParseNullResultReturnsNullopt)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto *ctx = MQuickJsRuntime::GetSharedContext();

        auto parsed = SbmdResultExecutor::Parse(ctx, JS_NULL);
        EXPECT_FALSE(parsed.has_value());
    }

    TEST_F(SbmdResultExecutorTest, ParseUndefinedResultReturnsNullopt)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto *ctx = MQuickJsRuntime::GetSharedContext();

        auto parsed = SbmdResultExecutor::Parse(ctx, JS_UNDEFINED);
        EXPECT_FALSE(parsed.has_value());
    }

    TEST_F(SbmdResultExecutorTest, ParseMissingTerminalReturnsNullopt)
    {
        // Construct a raw object with ops but no terminal
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto *ctx = MQuickJsRuntime::GetSharedContext();

        JSValue result = JS_Eval(ctx, "({ops: []})", 11, "<test>", JS_EVAL_RETVAL);
        ASSERT_FALSE(JS_IsException(result));

        auto parsed = SbmdResultExecutor::Parse(ctx, result);
        EXPECT_FALSE(parsed.has_value());
    }

    TEST_F(SbmdResultExecutorTest, ParseUnknownOpTypeSkipped)
    {
        // Build a raw result with an unknown op type followed by a known one
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto *ctx = MQuickJsRuntime::GetSharedContext();

        const char *code = "({"
                           "  ops: [{op: 'futureOp', foo: 'bar'}, {op: 'log', message: 'hi'}],"
                           "  terminal: {op: 'success'}"
                           "})";

        JSValue result = JS_Eval(ctx, code, strlen(code), "<test>", JS_EVAL_RETVAL);
        ASSERT_FALSE(JS_IsException(result));

        auto parsed = SbmdResultExecutor::Parse(ctx, result);
        ASSERT_TRUE(parsed.has_value());
        // Unknown op should be skipped, only the log op remains
        ASSERT_EQ(parsed->ops.size(), 1u);
        EXPECT_TRUE(std::holds_alternative<ResultOp::Log>(parsed->ops[0].data));
    }

    TEST_F(SbmdResultExecutorTest, ParseUnknownTerminalReturnsNullopt)
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto *ctx = MQuickJsRuntime::GetSharedContext();

        const char *code = "({ops: [], terminal: {op: 'unknownTerminal'}})";

        JSValue result = JS_Eval(ctx, code, strlen(code), "<test>", JS_EVAL_RETVAL);
        ASSERT_FALSE(JS_IsException(result));

        auto parsed = SbmdResultExecutor::Parse(ctx, result);
        EXPECT_FALSE(parsed.has_value());
    }

} // namespace
