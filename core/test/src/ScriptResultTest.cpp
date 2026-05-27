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
 * Unit tests for ScriptResult::FromJsonValue().
 *
 * These tests are engine-agnostic and exercise the JSON-to-result parsing logic
 * without instantiating any JS runtime.
 */

#include "deviceDrivers/matter/sbmd/ScriptResult.h"

#include <gtest/gtest.h>
#include <json/json.h>
#include <lib/support/CHIPMem.h>

using namespace barton;

namespace
{
    // Initialize CHIP Platform memory once for all tests (needed for ScopedMemoryBuffer)
    class ChipPlatformEnvironment : public ::testing::Environment
    {
    public:
        void SetUp() override { ASSERT_EQ(chip::Platform::MemoryInit(), CHIP_NO_ERROR); }

        void TearDown() override { chip::Platform::MemoryShutdown(); }
    };

    ::testing::Environment *const chipEnv = ::testing::AddGlobalTestEnvironment(new ChipPlatformEnvironment);

    // base64 of [0x15, 0x18] — a valid TLV empty struct (start + end_container)
    static constexpr const char *kEmptyStructBase64 = "FRg=";

    // -------------------------------------------------------------------------
    // Suppress (empty object)
    // -------------------------------------------------------------------------

    TEST(ScriptResultFromJsonValue, EmptyObjectYieldsSuppressed)
    {
        Json::Value jv(Json::objectValue);
        auto result = ScriptResult::FromJsonValue(jv);

        EXPECT_FALSE(result.IsError());
        EXPECT_TRUE(result.IsSuppressed());
        EXPECT_FALSE(result.HasOperation());
    }

    // -------------------------------------------------------------------------
    // "value" → ResourceUpdate
    // -------------------------------------------------------------------------

    TEST(ScriptResultFromJsonValue, StringValueYieldsResourceUpdate)
    {
        Json::Value jv(Json::objectValue);
        jv["value"] = "hello";
        auto result = ScriptResult::FromJsonValue(jv);

        ASSERT_FALSE(result.IsError());
        ASSERT_TRUE(result.HasOperation());

        const auto &op = result.Operation();
        ASSERT_TRUE(std::holds_alternative<ScriptResult::ResourceUpdate>(op));
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(op).value, "hello");
    }

    TEST(ScriptResultFromJsonValue, NumericValueYieldsResourceUpdate)
    {
        Json::Value jv(Json::objectValue);
        jv["value"] = 42;
        auto result = ScriptResult::FromJsonValue(jv);

        ASSERT_FALSE(result.IsError());
        ASSERT_TRUE(result.HasOperation());

        const auto &op = result.Operation();
        ASSERT_TRUE(std::holds_alternative<ScriptResult::ResourceUpdate>(op));
        // JsonCpp represents integer 42 as "42"
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(op).value, "42");
    }

    TEST(ScriptResultFromJsonValue, BoolTrueValueYieldsResourceUpdate)
    {
        Json::Value jv(Json::objectValue);
        jv["value"] = true;
        auto result = ScriptResult::FromJsonValue(jv);

        ASSERT_FALSE(result.IsError());
        ASSERT_TRUE(result.HasOperation());

        const auto &op = result.Operation();
        ASSERT_TRUE(std::holds_alternative<ScriptResult::ResourceUpdate>(op));
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(op).value, "true");
    }

    TEST(ScriptResultFromJsonValue, BoolFalseValueYieldsResourceUpdate)
    {
        Json::Value jv(Json::objectValue);
        jv["value"] = false;
        auto result = ScriptResult::FromJsonValue(jv);

        ASSERT_FALSE(result.IsError());
        ASSERT_TRUE(result.HasOperation());

        const auto &op = result.Operation();
        ASSERT_TRUE(std::holds_alternative<ScriptResult::ResourceUpdate>(op));
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(op).value, "false");
    }

    // -------------------------------------------------------------------------
    // "error" → error result
    // -------------------------------------------------------------------------

    TEST(ScriptResultFromJsonValue, ErrorKeyYieldsError)
    {
        Json::Value jv(Json::objectValue);
        jv["error"] = "something went wrong";
        auto result = ScriptResult::FromJsonValue(jv);

        EXPECT_TRUE(result.IsError());
        EXPECT_FALSE(result.IsSuppressed());
        EXPECT_FALSE(result.HasOperation());
        EXPECT_EQ(result.ErrorMessage(), "something went wrong");
    }

    // -------------------------------------------------------------------------
    // "invoke" → ScriptWriteResult::Invoke
    // -------------------------------------------------------------------------

    TEST(ScriptResultFromJsonValue, InvokeYieldsInvokeOperation)
    {
        Json::Value jv(Json::objectValue);
        Json::Value invokeObj(Json::objectValue);
        invokeObj["clusterId"] = 6;
        invokeObj["commandId"] = 1;
        jv["invoke"] = invokeObj;
        auto result = ScriptResult::FromJsonValue(jv);

        ASSERT_FALSE(result.IsError()) << result.ErrorMessage();
        ASSERT_TRUE(result.HasOperation());

        const auto &op = result.Operation();
        ASSERT_TRUE(std::holds_alternative<ScriptWriteResult>(op));

        const auto &wr = std::get<ScriptWriteResult>(op);
        EXPECT_EQ(wr.type, ScriptWriteResult::OperationType::Invoke);
        EXPECT_EQ(wr.clusterId, 6u);
        EXPECT_EQ(wr.commandId, 1u);
        EXPECT_EQ(wr.tlvLength, 0u);
    }

    TEST(ScriptResultFromJsonValue, InvokeWithOptionalFields)
    {
        Json::Value jv(Json::objectValue);
        Json::Value invokeObj(Json::objectValue);
        invokeObj["clusterId"] = 0x0101;
        invokeObj["commandId"] = 0x00;
        invokeObj["endpointId"] = 1;
        invokeObj["timedInvokeTimeoutMs"] = 500;
        invokeObj["tlvBase64"] = kEmptyStructBase64;
        jv["invoke"] = invokeObj;
        auto result = ScriptResult::FromJsonValue(jv);

        ASSERT_FALSE(result.IsError()) << result.ErrorMessage();
        ASSERT_TRUE(result.HasOperation());

        const auto &op = result.Operation();
        ASSERT_TRUE(std::holds_alternative<ScriptWriteResult>(op));

        const auto &wr = std::get<ScriptWriteResult>(op);
        EXPECT_EQ(wr.type, ScriptWriteResult::OperationType::Invoke);
        EXPECT_EQ(wr.clusterId, 0x0101u);
        EXPECT_EQ(wr.commandId, 0x00u);
        ASSERT_TRUE(wr.endpointId.has_value());
        EXPECT_EQ(wr.endpointId.value(), 1u);
        ASSERT_TRUE(wr.timedInvokeTimeoutMs.has_value());
        EXPECT_EQ(wr.timedInvokeTimeoutMs.value(), 500u);
        EXPECT_GT(wr.tlvLength, 0u);
    }

    TEST(ScriptResultFromJsonValue, InvokeMissingClusterId)
    {
        Json::Value jv(Json::objectValue);
        Json::Value invokeObj(Json::objectValue);
        invokeObj["commandId"] = 1;
        jv["invoke"] = invokeObj;
        auto result = ScriptResult::FromJsonValue(jv);

        EXPECT_TRUE(result.IsError());
    }

    TEST(ScriptResultFromJsonValue, InvokeMissingCommandId)
    {
        Json::Value jv(Json::objectValue);
        Json::Value invokeObj(Json::objectValue);
        invokeObj["clusterId"] = 6;
        jv["invoke"] = invokeObj;
        auto result = ScriptResult::FromJsonValue(jv);

        EXPECT_TRUE(result.IsError());
    }

    TEST(ScriptResultFromJsonValue, InvokeNotAnObject)
    {
        Json::Value jv(Json::objectValue);
        jv["invoke"] = "not-an-object";
        auto result = ScriptResult::FromJsonValue(jv);

        EXPECT_TRUE(result.IsError());
    }

    // -------------------------------------------------------------------------
    // "write" → ScriptWriteResult::Write
    // -------------------------------------------------------------------------

    TEST(ScriptResultFromJsonValue, WriteYieldsWriteOperation)
    {
        Json::Value jv(Json::objectValue);
        Json::Value writeObj(Json::objectValue);
        writeObj["clusterId"] = 8;
        writeObj["attributeId"] = 0;
        writeObj["tlvBase64"] = kEmptyStructBase64;
        jv["write"] = writeObj;
        auto result = ScriptResult::FromJsonValue(jv);

        ASSERT_FALSE(result.IsError()) << result.ErrorMessage();
        ASSERT_TRUE(result.HasOperation());

        const auto &op = result.Operation();
        ASSERT_TRUE(std::holds_alternative<ScriptWriteResult>(op));

        const auto &wr = std::get<ScriptWriteResult>(op);
        EXPECT_EQ(wr.type, ScriptWriteResult::OperationType::Write);
        EXPECT_EQ(wr.clusterId, 8u);
        EXPECT_EQ(wr.attributeId, 0u);
        EXPECT_GT(wr.tlvLength, 0u);
    }

    TEST(ScriptResultFromJsonValue, WriteWithOptionalEndpointId)
    {
        Json::Value jv(Json::objectValue);
        Json::Value writeObj(Json::objectValue);
        writeObj["clusterId"] = 8;
        writeObj["attributeId"] = 0;
        writeObj["tlvBase64"] = kEmptyStructBase64;
        writeObj["endpointId"] = 2;
        jv["write"] = writeObj;
        auto result = ScriptResult::FromJsonValue(jv);

        ASSERT_FALSE(result.IsError()) << result.ErrorMessage();
        ASSERT_TRUE(result.HasOperation());

        const auto &op = result.Operation();
        ASSERT_TRUE(std::holds_alternative<ScriptWriteResult>(op));

        const auto &wr = std::get<ScriptWriteResult>(op);
        ASSERT_TRUE(wr.endpointId.has_value());
        EXPECT_EQ(wr.endpointId.value(), 2u);
    }

    TEST(ScriptResultFromJsonValue, WriteMissingClusterId)
    {
        Json::Value jv(Json::objectValue);
        Json::Value writeObj(Json::objectValue);
        writeObj["attributeId"] = 0;
        writeObj["tlvBase64"] = kEmptyStructBase64;
        jv["write"] = writeObj;
        auto result = ScriptResult::FromJsonValue(jv);

        EXPECT_TRUE(result.IsError());
    }

    TEST(ScriptResultFromJsonValue, WriteMissingAttributeId)
    {
        Json::Value jv(Json::objectValue);
        Json::Value writeObj(Json::objectValue);
        writeObj["clusterId"] = 8;
        writeObj["tlvBase64"] = kEmptyStructBase64;
        jv["write"] = writeObj;
        auto result = ScriptResult::FromJsonValue(jv);

        EXPECT_TRUE(result.IsError());
    }

    TEST(ScriptResultFromJsonValue, WriteMissingTlvBase64)
    {
        Json::Value jv(Json::objectValue);
        Json::Value writeObj(Json::objectValue);
        writeObj["clusterId"] = 8;
        writeObj["attributeId"] = 0;
        jv["write"] = writeObj;
        auto result = ScriptResult::FromJsonValue(jv);

        EXPECT_TRUE(result.IsError());
    }

    TEST(ScriptResultFromJsonValue, WriteNotAnObject)
    {
        Json::Value jv(Json::objectValue);
        jv["write"] = 42;
        auto result = ScriptResult::FromJsonValue(jv);

        EXPECT_TRUE(result.IsError());
    }

    // -------------------------------------------------------------------------
    // Ambiguity detection — multiple recognized keys → error
    // -------------------------------------------------------------------------

    TEST(ScriptResultFromJsonValue, AmbiguousValueAndInvoke)
    {
        Json::Value jv(Json::objectValue);
        jv["value"] = "hello";
        Json::Value invokeObj(Json::objectValue);
        invokeObj["clusterId"] = 6;
        invokeObj["commandId"] = 1;
        jv["invoke"] = invokeObj;
        auto result = ScriptResult::FromJsonValue(jv);

        EXPECT_TRUE(result.IsError());
    }

    TEST(ScriptResultFromJsonValue, AmbiguousInvokeAndWrite)
    {
        Json::Value jv(Json::objectValue);
        Json::Value invokeObj(Json::objectValue);
        invokeObj["clusterId"] = 6;
        invokeObj["commandId"] = 1;
        jv["invoke"] = invokeObj;
        Json::Value writeObj(Json::objectValue);
        writeObj["clusterId"] = 8;
        writeObj["attributeId"] = 0;
        writeObj["tlvBase64"] = kEmptyStructBase64;
        jv["write"] = writeObj;
        auto result = ScriptResult::FromJsonValue(jv);

        EXPECT_TRUE(result.IsError());
    }

    TEST(ScriptResultFromJsonValue, AmbiguousErrorAndValue)
    {
        Json::Value jv(Json::objectValue);
        jv["error"] = "oops";
        jv["value"] = "hello";
        auto result = ScriptResult::FromJsonValue(jv);

        EXPECT_TRUE(result.IsError());
    }

    // -------------------------------------------------------------------------
    // Non-object input
    // -------------------------------------------------------------------------

    TEST(ScriptResultFromJsonValue, NullInputYieldsError)
    {
        Json::Value jv(Json::nullValue);
        auto result = ScriptResult::FromJsonValue(jv);

        EXPECT_TRUE(result.IsError());
    }

    TEST(ScriptResultFromJsonValue, StringInputYieldsError)
    {
        Json::Value jv("a string");
        auto result = ScriptResult::FromJsonValue(jv);

        EXPECT_TRUE(result.IsError());
    }

    TEST(ScriptResultFromJsonValue, ArrayInputYieldsError)
    {
        Json::Value jv(Json::arrayValue);
        jv.append("item");
        auto result = ScriptResult::FromJsonValue(jv);

        EXPECT_TRUE(result.IsError());
    }

    // -------------------------------------------------------------------------
    // Helper factory methods
    // -------------------------------------------------------------------------

    TEST(ScriptResultHelpers, MakeErrorIsError)
    {
        auto r = ScriptResult::MakeError("test error");
        EXPECT_TRUE(r.IsError());
        EXPECT_EQ(r.ErrorMessage(), "test error");
    }

    TEST(ScriptResultHelpers, MakeSuppressIsSuppressed)
    {
        auto r = ScriptResult::MakeSuppress();
        EXPECT_TRUE(r.IsSuppressed());
        EXPECT_FALSE(r.IsError());
        EXPECT_FALSE(r.HasOperation());
    }

    TEST(ScriptResultHelpers, MakeResourceUpdateHasOperation)
    {
        auto r = ScriptResult::MakeResourceUpdate("42");
        EXPECT_FALSE(r.IsError());
        EXPECT_FALSE(r.IsSuppressed());
        ASSERT_TRUE(r.HasOperation());
        ASSERT_TRUE(std::holds_alternative<ScriptResult::ResourceUpdate>(r.Operation()));
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(r.Operation()).value, "42");
    }

    TEST(ScriptResultHelpers, MakeWriteResultHasOperation)
    {
        ScriptWriteResult wr;
        wr.type = ScriptWriteResult::OperationType::Invoke;
        wr.clusterId = 6;
        wr.commandId = 2;

        auto r = ScriptResult::MakeWriteResult(std::move(wr));

        EXPECT_FALSE(r.IsError());
        EXPECT_FALSE(r.IsSuppressed());
        ASSERT_TRUE(r.HasOperation());
        ASSERT_TRUE(std::holds_alternative<ScriptWriteResult>(r.Operation()));

        const auto &result = std::get<ScriptWriteResult>(r.Operation());
        EXPECT_EQ(result.type, ScriptWriteResult::OperationType::Invoke);
        EXPECT_EQ(result.clusterId, 6u);
        EXPECT_EQ(result.commandId, 2u);
    }

} // anonymous namespace
