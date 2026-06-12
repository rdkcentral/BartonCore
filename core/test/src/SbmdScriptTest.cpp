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
 * Created by tlea on 2/3/2026
 *
 * Unit tests for SbmdScript implementations focusing on script interfaces.
 */

#include "deviceDrivers/matter/sbmd/SbmdScript.h"
#include "deviceDrivers/matter/sbmd/SbmdSpec.h"

#if defined(BCORE_USE_MQUICKJS)
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdScriptImpl.h"
#include "deviceDrivers/matter/sbmd/mquickjs/MQuickJsRuntime.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdUtilsLoader.h"
#elif defined(BCORE_USE_QUICKJS)
#include "deviceDrivers/matter/sbmd/quickjs/SbmdScriptImpl.h"
#endif

#include <gtest/gtest.h>
#include <lib/core/TLVReader.h>
#include <lib/core/TLVTypes.h>
#include <lib/core/TLVWriter.h>
#include <lib/support/CHIPMem.h>
#include <memory>

using namespace barton;

namespace
{
    // Initialize CHIP Platform memory once for all tests
    class ChipPlatformEnvironment : public ::testing::Environment
    {
    public:
        void SetUp() override
        {
            ASSERT_EQ(chip::Platform::MemoryInit(), CHIP_NO_ERROR);
        }

        void TearDown() override
        {
            chip::Platform::MemoryShutdown();
        }
    };

    // Register the environment - it will be set up before any tests run
    ::testing::Environment* const chipEnv =
        ::testing::AddGlobalTestEnvironment(new ChipPlatformEnvironment);

    std::unique_ptr<SbmdScript> CreateScript(const std::string &deviceId)
    {
        return SbmdScriptImpl::Create(deviceId);
    }

    class SbmdScriptTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            deviceId = "test-device-uuid";
            script = CreateScript(deviceId);
            ASSERT_NE(script, nullptr) << "Failed to create SbmdScript";
        }

        void TearDown() override { script.reset(); }

        std::string deviceId;
        std::unique_ptr<SbmdScript> script;
    };

    // Test: SbmdScript can be instantiated
    TEST_F(SbmdScriptTest, CanCreate)
    {
        ASSERT_NE(script, nullptr);
    }

    // Test: AddAttributeReadMapper returns true for valid input
    TEST_F(SbmdScriptTest, AddAttributeReadMapperSuccess)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006; // On/Off cluster
        attr.attributeId = 0x0000; // OnOff attribute
        attr.name = "onOff";
        attr.type = "bool";

        std::string mapperScript =
            "var val = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64); return {value: val ? 'true' : 'false'};";

        EXPECT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));
    }

    // Test: MapAttributeRead returns false when no mapper is registered
    TEST_F(SbmdScriptTest, MapAttributeReadNoMapper)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // Create a TLV buffer with a boolean value
        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto readResult = script->MapAttributeRead(attr, reader);
        EXPECT_TRUE(readResult.IsError());
    }

    // Test: MapAttributeRead with simple boolean passthrough script
    TEST_F(SbmdScriptTest, MapAttributeReadBooleanPassthrough)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // Script that converts Matter boolean to Barton string
        // sbmdReadArgs.tlvBase64 contains base64 encoded TLV
        std::string mapperScript = "var val = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64); return {value: (val === "
                                   "true) ? 'true' : 'false'};";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        // Create a TLV buffer with a boolean value (true)
        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto readResult = script->MapAttributeRead(attr, reader);
        ASSERT_TRUE(readResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(readResult.Operation()).value, "true");
    }

    // Test: MapAttributeRead with boolean false value
    // sbmdReadArgs.tlvBase64 contains base64 encoded TLV - use SbmdUtils.Tlv.decode() to decode
    TEST_F(SbmdScriptTest, MapAttributeReadBooleanFalse)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // Script needs to properly handle false - compare against true explicitly
        // sbmdReadArgs.tlvBase64 contains base64 encoded TLV
        std::string mapperScript = "var val = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64); return {value: (val === "
                                   "true) ? 'true' : 'false'};";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        // Create a TLV buffer with a boolean value (false)
        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), false);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto readResult = script->MapAttributeRead(attr, reader);
        ASSERT_TRUE(readResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(readResult.Operation()).value, "false");
    }

    // Test: MapAttributeRead with integer value conversion
    // sbmdReadArgs.tlvBase64 contains base64 encoded TLV - use SbmdUtils.Tlv.decode() to decode
    TEST_F(SbmdScriptTest, MapAttributeReadIntegerConversion)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0008; // Level cluster
        attr.attributeId = 0x0000; // CurrentLevel
        attr.name = "currentLevel";
        attr.type = "uint8";

        // Script that converts Matter uint8 to percentage string
        // sbmdReadArgs.tlvBase64 contains base64 encoded TLV
        std::string mapperScript = R"(
            var level = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64);
            var percent = Math.round(level / 254 * 100);
            return {value: percent.toString()};
        )";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        // Create a TLV buffer with an integer value (127 = ~50%)
        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.Put(chip::TLV::AnonymousTag(), static_cast<uint8_t>(127));
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto readResult = script->MapAttributeRead(attr, reader);
        ASSERT_TRUE(readResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(readResult.Operation()).value, "50"); // 127/254 * 100 = 50%
    }

    // Test: MapAttributeRead verifies sbmdReadArgs contains deviceUuid
    TEST_F(SbmdScriptTest, MapAttributeReadHasDeviceUuid)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // Script that returns the deviceUuid
        std::string mapperScript = "return {value: sbmdReadArgs.deviceUuid};";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto readResult = script->MapAttributeRead(attr, reader);
        ASSERT_TRUE(readResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(readResult.Operation()).value, deviceId);
    }

    // Test: MapAttributeRead verifies sbmdReadArgs contains clusterId
    TEST_F(SbmdScriptTest, MapAttributeReadHasClusterId)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // Script that returns the clusterId
        std::string mapperScript = "return {value: sbmdReadArgs.clusterId.toString()};";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto readResult = script->MapAttributeRead(attr, reader);
        ASSERT_TRUE(readResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(readResult.Operation()).value, "6"); // 0x0006 = 6
    }

    // Test: MapAttributeRead verifies sbmdReadArgs contains attributeId
    TEST_F(SbmdScriptTest, MapAttributeReadHasAttributeId)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0005;
        attr.name = "testAttr";
        attr.type = "bool";

        // Script that returns the attributeId
        std::string mapperScript = "return {value: sbmdReadArgs.attributeId.toString()};";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto readResult = script->MapAttributeRead(attr, reader);
        ASSERT_TRUE(readResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(readResult.Operation()).value, "5"); // 0x0005 = 5
    }

    // Test: MapAttributeRead verifies sbmdReadArgs contains attributeName
    TEST_F(SbmdScriptTest, MapAttributeReadHasAttributeName)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "myTestAttribute";
        attr.type = "bool";

        // Script that returns the attributeName
        std::string mapperScript = "return {value: sbmdReadArgs.attributeName};";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto readResult = script->MapAttributeRead(attr, reader);
        ASSERT_TRUE(readResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(readResult.Operation()).value, "myTestAttribute");
    }

    // Test: MapAttributeRead verifies sbmdReadArgs contains attributeType
    TEST_F(SbmdScriptTest, MapAttributeReadHasAttributeType)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "boolean";

        // Script that returns the attributeType
        std::string mapperScript = "return {value: sbmdReadArgs.attributeType};";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto readResult = script->MapAttributeRead(attr, reader);
        ASSERT_TRUE(readResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(readResult.Operation()).value, "boolean");
    }

    // Test: MapAttributeRead succeeds when script returns value field (v3.0 format)
    TEST_F(SbmdScriptTest, MapAttributeReadWithValueField)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // Script returns the v3.0 "value" field — the correct format
        std::string mapperScript = "return {value: 'true'};";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto readResult = script->MapAttributeRead(attr, reader);
        ASSERT_TRUE(readResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(readResult.Operation()).value, "true");
    }

    // Test: MapAttributeRead fails with script syntax error
    TEST_F(SbmdScriptTest, MapAttributeReadScriptSyntaxError)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // Script with syntax error
        std::string mapperScript = "return {value: invalid syntax here";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto readResult = script->MapAttributeRead(attr, reader);
        EXPECT_TRUE(readResult.IsError());
    }

    // Test: MapAttributeRead with endpointId set
    TEST_F(SbmdScriptTest, MapAttributeReadWithEndpointId)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";
        attr.resourceEndpointId = "ep1";

        // Script that returns the endpointId
        std::string mapperScript = "return {value: sbmdReadArgs.endpointId};";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto readResult = script->MapAttributeRead(attr, reader);
        ASSERT_TRUE(readResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(readResult.Operation()).value, "ep1");
    }

    // Test: Multiple attribute mappers can coexist
    TEST_F(SbmdScriptTest, MultipleAttributeMappers)
    {
        SbmdAttribute attr1;
        attr1.clusterId = 0x0006;
        attr1.attributeId = 0x0000;
        attr1.name = "onOff";
        attr1.type = "bool";

        SbmdAttribute attr2;
        attr2.clusterId = 0x0008;
        attr2.attributeId = 0x0000;
        attr2.name = "currentLevel";
        attr2.type = "uint8";

        std::string script1 = "return {value: 'attr1'};";
        std::string script2 = "return {value: 'attr2'};";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr1, script1));
        ASSERT_TRUE(script->AddAttributeReadMapper(attr2, script2));

        // Test attr1
        uint8_t tlvBuffer1[32];
        chip::TLV::TLVWriter writer1;
        writer1.Init(tlvBuffer1, sizeof(tlvBuffer1));
        writer1.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer1.Finalize();

        chip::TLV::TLVReader reader1;
        reader1.Init(tlvBuffer1, writer1.GetLengthWritten());
        reader1.Next();

        auto readResult1 = script->MapAttributeRead(attr1, reader1);
        ASSERT_TRUE(readResult1.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(readResult1.Operation()).value, "attr1");

        // Test attr2
        uint8_t tlvBuffer2[32];
        chip::TLV::TLVWriter writer2;
        writer2.Init(tlvBuffer2, sizeof(tlvBuffer2));
        writer2.Put(chip::TLV::AnonymousTag(), static_cast<uint8_t>(100));
        writer2.Finalize();

        chip::TLV::TLVReader reader2;
        reader2.Init(tlvBuffer2, writer2.GetLengthWritten());
        reader2.Next();

        auto readResult2 = script->MapAttributeRead(attr2, reader2);
        ASSERT_TRUE(readResult2.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(readResult2.Operation()).value, "attr2");
    }

    // Test: Script can access complex JSON structures
    TEST_F(SbmdScriptTest, MapAttributeReadWithJsonStructure)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "testAttr";
        attr.type = "struct";

        // Script that accesses the input object
        // sbmdReadArgs.tlvBase64 contains base64 encoded TLV
        std::string mapperScript = R"(
            var val = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64);
            var result = 'input:' + JSON.stringify(val) +
                         ',device:' + sbmdReadArgs.deviceUuid;
            return {value: result};
        )";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        // Create TLV with boolean for simplicity
        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto readResult = script->MapAttributeRead(attr, reader);
        ASSERT_TRUE(readResult.HasOperation());
        const auto &readVal = std::get<ScriptResult::ResourceUpdate>(readResult.Operation()).value;
        // Verify it contains expected parts
        // Ensure the input value and device UUID appear in the output string
        EXPECT_NE(readVal.find("input:true"), std::string::npos);
        EXPECT_NE(readVal.find("device:test-device-uuid"), std::string::npos);
    }

    //==============================================================================
    // MapCommandExecuteResponse tests
    //==============================================================================

    // Test: MapCommandExecuteResponse returns false when no mapper is registered
    TEST_F(SbmdScriptTest, MapCommandExecuteResponseNoMapper)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0001;
        cmd.name = "on";

        // Create a TLV buffer with a simple value
        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto cmdResult = script->MapCommandExecuteResponse(cmd, reader);
        EXPECT_TRUE(cmdResult.IsError());
    }

    // Test: MapCommandExecuteResponse happy path with boolean response
    TEST_F(SbmdScriptTest, MapCommandExecuteResponseBooleanHappyPath)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0001;
        cmd.name = "on";

        // Script that converts Matter boolean response to string
        std::string mapperScript = R"(
            var val = SbmdUtils.Tlv.decode(sbmdCommandResponseArgs.tlvBase64);
            return {value: val ? 'success' : 'failure'};
        )";

        ASSERT_TRUE(script->AddCommandExecuteResponseMapper(cmd, mapperScript));

        // Create TLV with boolean true
        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto cmdResult = script->MapCommandExecuteResponse(cmd, reader);
        ASSERT_TRUE(cmdResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(cmdResult.Operation()).value, "success");
    }

    // Test: MapCommandExecuteResponse happy path with integer response
    TEST_F(SbmdScriptTest, MapCommandExecuteResponseIntegerHappyPath)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0101; // Door Lock
        cmd.commandId = 0x0000;
        cmd.name = "getLockState";

        // Script that converts lock state integer to string
        std::string mapperScript = R"(
            var states = ['not_fully_locked', 'locked', 'unlocked', 'unlatched'];
            var state = SbmdUtils.Tlv.decode(sbmdCommandResponseArgs.tlvBase64);
            return {value: states[state] || 'unknown'};
        )";

        ASSERT_TRUE(script->AddCommandExecuteResponseMapper(cmd, mapperScript));

        // Create TLV with integer 1 (locked)
        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.Put(chip::TLV::AnonymousTag(), static_cast<uint8_t>(1));
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto cmdResult = script->MapCommandExecuteResponse(cmd, reader);
        ASSERT_TRUE(cmdResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(cmdResult.Operation()).value, "locked");
    }

    // Test: MapCommandExecuteResponse with struct TLV response
    TEST_F(SbmdScriptTest, MapCommandExecuteResponseStructHappyPath)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0101;
        cmd.commandId = 0x0024; // GetCredentialStatusResponse
        cmd.name = "getCredentialStatus";

        // Script that extracts fields from struct response
        // TlvToJson uses context tag numbers as keys (e.g., "0", "1")
        std::string mapperScript = R"(
            var input = SbmdUtils.Tlv.decode(sbmdCommandResponseArgs.tlvBase64);
            return {value: 'exists:' + input['0'] + ',index:' + input['1']};
        )";

        ASSERT_TRUE(script->AddCommandExecuteResponseMapper(cmd, mapperScript));

        // Create TLV with struct containing boolean + uint16
        uint8_t tlvBuffer[64];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));

        chip::TLV::TLVType structType;
        writer.StartContainer(chip::TLV::AnonymousTag(), chip::TLV::kTLVType_Structure, structType);
        writer.PutBoolean(chip::TLV::ContextTag(0), true);
        writer.Put(chip::TLV::ContextTag(1), static_cast<uint16_t>(42));
        writer.EndContainer(structType);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto cmdResult = script->MapCommandExecuteResponse(cmd, reader);
        ASSERT_TRUE(cmdResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(cmdResult.Operation()).value, "exists:true,index:42");
    }

    // Test: MapCommandExecuteResponse verifies sbmdCommandResponseArgs contains deviceUuid
    TEST_F(SbmdScriptTest, MapCommandExecuteResponseHasDeviceUuid)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0001;
        cmd.name = "on";

        // Script that returns the deviceUuid
        std::string mapperScript = "return {value: sbmdCommandResponseArgs.deviceUuid};";

        ASSERT_TRUE(script->AddCommandExecuteResponseMapper(cmd, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto cmdResult = script->MapCommandExecuteResponse(cmd, reader);
        ASSERT_TRUE(cmdResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(cmdResult.Operation()).value, deviceId);
    }

    // Test: MapCommandExecuteResponse verifies sbmdCommandResponseArgs contains clusterId
    TEST_F(SbmdScriptTest, MapCommandExecuteResponseHasClusterId)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0008;
        cmd.commandId = 0x0001;
        cmd.name = "moveToLevel";

        // Script that returns the clusterId
        std::string mapperScript = "return {value: sbmdCommandResponseArgs.clusterId.toString()};";

        ASSERT_TRUE(script->AddCommandExecuteResponseMapper(cmd, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto cmdResult = script->MapCommandExecuteResponse(cmd, reader);
        ASSERT_TRUE(cmdResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(cmdResult.Operation()).value, "8"); // 0x0008 = 8
    }

    // Test: MapCommandExecuteResponse verifies sbmdCommandResponseArgs contains commandId
    TEST_F(SbmdScriptTest, MapCommandExecuteResponseHasCommandId)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0005;
        cmd.name = "testCmd";

        // Script that returns the commandId
        std::string mapperScript = "return {value: sbmdCommandResponseArgs.commandId.toString()};";

        ASSERT_TRUE(script->AddCommandExecuteResponseMapper(cmd, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto cmdResult = script->MapCommandExecuteResponse(cmd, reader);
        ASSERT_TRUE(cmdResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(cmdResult.Operation()).value, "5"); // 0x0005 = 5
    }

    // Test: MapCommandExecuteResponse verifies sbmdCommandResponseArgs contains commandName
    TEST_F(SbmdScriptTest, MapCommandExecuteResponseHasCommandName)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0001;
        cmd.name = "myTestCommand";

        // Script that returns the commandName
        std::string mapperScript = "return {value: sbmdCommandResponseArgs.commandName};";

        ASSERT_TRUE(script->AddCommandExecuteResponseMapper(cmd, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto cmdResult = script->MapCommandExecuteResponse(cmd, reader);
        ASSERT_TRUE(cmdResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(cmdResult.Operation()).value, "myTestCommand");
    }

    // Test: MapCommandExecuteResponse with endpointId
    TEST_F(SbmdScriptTest, MapCommandExecuteResponseWithEndpointId)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0001;
        cmd.name = "on";
        cmd.resourceEndpointId = "ep3";

        // Script that returns the endpointId
        std::string mapperScript = "return {value: sbmdCommandResponseArgs.endpointId};";

        ASSERT_TRUE(script->AddCommandExecuteResponseMapper(cmd, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto cmdResult = script->MapCommandExecuteResponse(cmd, reader);
        ASSERT_TRUE(cmdResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(cmdResult.Operation()).value, "ep3");
    }

    // Test: MapCommandExecuteResponse succeeds when script returns value field (v3.0 format)
    TEST_F(SbmdScriptTest, MapCommandExecuteResponseWithValueField)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0001;
        cmd.name = "on";

        // Script returns v3.0 "value" field — now the correct format
        std::string mapperScript = "return {value: 'result'};";

        ASSERT_TRUE(script->AddCommandExecuteResponseMapper(cmd, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto cmdResult = script->MapCommandExecuteResponse(cmd, reader);
        ASSERT_TRUE(cmdResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(cmdResult.Operation()).value, "result");
    }

    // Test: MapCommandExecuteResponse fails with script syntax error
    TEST_F(SbmdScriptTest, MapCommandExecuteResponseScriptSyntaxError)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0001;
        cmd.name = "on";

        // Script with syntax error
        std::string mapperScript = "return {value: this is bad syntax";

        ASSERT_TRUE(script->AddCommandExecuteResponseMapper(cmd, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto cmdResult = script->MapCommandExecuteResponse(cmd, reader);
        EXPECT_TRUE(cmdResult.IsError());
    }

    // Test: MapCommandExecuteResponse fails with runtime exception
    TEST_F(SbmdScriptTest, MapCommandExecuteResponseRuntimeException)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0001;
        cmd.name = "on";

        // Script that throws a runtime exception
        std::string mapperScript = R"(
            throw new Error('Something went wrong');
        )";

        ASSERT_TRUE(script->AddCommandExecuteResponseMapper(cmd, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto cmdResult = script->MapCommandExecuteResponse(cmd, reader);
        EXPECT_TRUE(cmdResult.IsError());
    }

    // Test: MapCommandExecuteResponse with string TLV response
    TEST_F(SbmdScriptTest, MapCommandExecuteResponseStringValue)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0050;
        cmd.commandId = 0x0001;
        cmd.name = "getName";

        // Script that processes a string response
        std::string mapperScript = R"(
            var val = SbmdUtils.Tlv.decode(sbmdCommandResponseArgs.tlvBase64);
            return {value: 'Name: ' + val};
        )";

        ASSERT_TRUE(script->AddCommandExecuteResponseMapper(cmd, mapperScript));

        // Create TLV with string value
        uint8_t tlvBuffer[64];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutString(chip::TLV::AnonymousTag(), "TestDevice");
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto cmdResult = script->MapCommandExecuteResponse(cmd, reader);
        ASSERT_TRUE(cmdResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(cmdResult.Operation()).value, "Name: TestDevice");
    }

    // Test: Multiple command response mappers can coexist
    TEST_F(SbmdScriptTest, MultipleCommandResponseMappers)
    {
        SbmdCommand cmd1;
        cmd1.clusterId = 0x0006;
        cmd1.commandId = 0x0001;
        cmd1.name = "on";

        SbmdCommand cmd2;
        cmd2.clusterId = 0x0008;
        cmd2.commandId = 0x0000;
        cmd2.name = "moveToLevel";

        std::string script1 = "return {value: 'response1'};";
        std::string script2 = "return {value: 'response2'};";

        ASSERT_TRUE(script->AddCommandExecuteResponseMapper(cmd1, script1));
        ASSERT_TRUE(script->AddCommandExecuteResponseMapper(cmd2, script2));

        // Test cmd1
        uint8_t tlvBuffer1[32];
        chip::TLV::TLVWriter writer1;
        writer1.Init(tlvBuffer1, sizeof(tlvBuffer1));
        writer1.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer1.Finalize();

        chip::TLV::TLVReader reader1;
        reader1.Init(tlvBuffer1, writer1.GetLengthWritten());
        reader1.Next();

        auto cmdResult1 = script->MapCommandExecuteResponse(cmd1, reader1);
        ASSERT_TRUE(cmdResult1.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(cmdResult1.Operation()).value, "response1");

        // Test cmd2
        uint8_t tlvBuffer2[32];
        chip::TLV::TLVWriter writer2;
        writer2.Init(tlvBuffer2, sizeof(tlvBuffer2));
        writer2.Put(chip::TLV::AnonymousTag(), static_cast<uint8_t>(100));
        writer2.Finalize();

        chip::TLV::TLVReader reader2;
        reader2.Init(tlvBuffer2, writer2.GetLengthWritten());
        reader2.Next();

        auto cmdResult2 = script->MapCommandExecuteResponse(cmd2, reader2);
        ASSERT_TRUE(cmdResult2.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(cmdResult2.Operation()).value, "response2");
    }

    //--------------------------------------------------------------------------
    // Input validation tests — invalid Base64 input
    //--------------------------------------------------------------------------

    // Test: SbmdUtils.Tlv.decode throws on invalid Base64 characters
    TEST_F(SbmdScriptTest, TlvDecodeInvalidBase64Exception)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // 'CQ!!' is a valid-length (4-char) quartet with an invalid '!' at index 2 and 3.
        std::string mapperScript = "var val = SbmdUtils.Tlv.decode('CQ!!'); return {value: 'unreachable'};";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto readResult = script->MapAttributeRead(attr, reader);
        EXPECT_TRUE(readResult.IsError());
    }

    // Test: SbmdUtils.Base64.decode throws on invalid Base64 characters
    TEST_F(SbmdScriptTest, Base64DecodeInvalidBase64Exception)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // 'AA!A' is a valid-length (4-char) quartet with an invalid '!' at index 2.
        std::string mapperScript =
            "var bytes = SbmdUtils.Base64.decode('AA!A'); return {value: bytes.length.toString()};";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto readResult = script->MapAttributeRead(attr, reader);
        EXPECT_TRUE(readResult.IsError());
    }

    //--------------------------------------------------------------------------
    // SbmdUtils.Tlv.encode tests
    //
    // These tests exercise the encode path: type argument requirement,
    // integer types with range checks, string parsing with radix, string
    // type, and round-trip encode→decode consistency.
    //--------------------------------------------------------------------------

    // Helper: run a script via an attribute read mapper (the TLV input is
    // ignored by the script – we just need a valid TLV to satisfy the API).
    // Returns the output string on success, std::nullopt on failure.
    static std::optional<std::string> RunEncodeScript(SbmdScript &script, const std::string &js)
    {
        SbmdAttribute attr;
        attr.clusterId = 0xFFFF;
        attr.attributeId = 0xFFFF;
        attr.name = "encodeTest";
        attr.type = "bool";

        if (!script.AddAttributeReadMapper(attr, js))
        {
            return std::nullopt;
        }

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto mapResult = script.MapAttributeRead(attr, reader);

        if (!mapResult.HasOperation())
        {
            return std::nullopt;
        }

        return std::get<ScriptResult::ResourceUpdate>(mapResult.Operation()).value;
    }

    // Encode uint8 and round-trip via decode
    TEST_F(SbmdScriptTest, TlvEncodeUint8RoundTrip)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode(42, 'uint8');
            var decoded = SbmdUtils.Tlv.decode(encoded);
            return {value: decoded.toString()};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "42");
    }

    // Encode uint16 and round-trip via decode
    TEST_F(SbmdScriptTest, TlvEncodeUint16RoundTrip)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode(1000, 'uint16');
            var decoded = SbmdUtils.Tlv.decode(encoded);
            return {value: decoded.toString()};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "1000");
    }

    // Encode uint32 max value
    TEST_F(SbmdScriptTest, TlvEncodeUint32MaxRoundTrip)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode(4294967295, 'uint32');
            var decoded = SbmdUtils.Tlv.decode(encoded);
            return {value: decoded.toString()};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "4294967295");
    }

    // Encode int16 negative value
    TEST_F(SbmdScriptTest, TlvEncodeInt16NegativeRoundTrip)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode(-100, 'int16');
            var decoded = SbmdUtils.Tlv.decode(encoded);
            return {value: decoded.toString()};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "-100");
    }

    // Encode int8 boundary values
    TEST_F(SbmdScriptTest, TlvEncodeInt8BoundaryRoundTrip)
    {
        auto result = RunEncodeScript(*script, R"(
            var lo = SbmdUtils.Tlv.encode(-128, 'int8');
            var hi = SbmdUtils.Tlv.encode(127, 'int8');
            var dLo = SbmdUtils.Tlv.decode(lo);
            var dHi = SbmdUtils.Tlv.decode(hi);
            return {value: dLo + ',' + dHi};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "-128,127");
    }

    // Encode enum8 round-trip
    TEST_F(SbmdScriptTest, TlvEncodeEnum8RoundTrip)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode(3, 'enum8');
            var decoded = SbmdUtils.Tlv.decode(encoded);
            return {value: decoded.toString()};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "3");
    }

    // Encode string type
    TEST_F(SbmdScriptTest, TlvEncodeStringRoundTrip)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode('hello', 'string');
            var decoded = SbmdUtils.Tlv.decode(encoded);
            return {value: decoded};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "hello");
    }

    // Encode string type coerces non-string value via String()
    TEST_F(SbmdScriptTest, TlvEncodeStringCoercesNumber)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode(99, 'string');
            var decoded = SbmdUtils.Tlv.decode(encoded);
            return {value: decoded};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "99");
    }

    // Encode boolean true
    TEST_F(SbmdScriptTest, TlvEncodeBoolTrueRoundTrip)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode(true, 'bool');
            var decoded = SbmdUtils.Tlv.decode(encoded);
            return {value: decoded === true ? 'true' : 'false'};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "true");
    }

    // Encode boolean false
    TEST_F(SbmdScriptTest, TlvEncodeBoolFalseRoundTrip)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode(false, 'bool');
            var decoded = SbmdUtils.Tlv.decode(encoded);
            return {value: decoded === false ? 'false' : 'true'};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "false");
    }

    // Parse string value as integer (decimal)
    TEST_F(SbmdScriptTest, TlvEncodeStringParsedAsDecimal)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode('200', 'uint8');
            var decoded = SbmdUtils.Tlv.decode(encoded);
            return {value: decoded.toString()};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "200");
    }

    // Parse string value as hex integer
    TEST_F(SbmdScriptTest, TlvEncodeStringParsedAsHex)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode('FF', 'uint8', 16);
            var decoded = SbmdUtils.Tlv.decode(encoded);
            return {value: decoded.toString()};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "255");
    }

    // Parse string value as binary integer
    TEST_F(SbmdScriptTest, TlvEncodeStringParsedAsBinary)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode('1010', 'uint8', 2);
            var decoded = SbmdUtils.Tlv.decode(encoded);
            return {value: decoded.toString()};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "10");
    }

    // Range check: uint8 out of range (256) returns null
    TEST_F(SbmdScriptTest, TlvEncodeUint8OutOfRangeReturnsNull)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode(256, 'uint8');
            return {value: encoded === null ? 'null' : 'not-null'};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "null");
    }

    // Range check: int8 out of range (-129) returns null
    TEST_F(SbmdScriptTest, TlvEncodeInt8BelowMinReturnsNull)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode(-129, 'int8');
            return {value: encoded === null ? 'null' : 'not-null'};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "null");
    }

    // Range check: uint16 negative returns null
    TEST_F(SbmdScriptTest, TlvEncodeUint16NegativeReturnsNull)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode(-1, 'uint16');
            return {value: encoded === null ? 'null' : 'not-null'};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "null");
    }

    // Range check: non-integer number returns null
    TEST_F(SbmdScriptTest, TlvEncodeFloatReturnsNull)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode(3.5, 'uint8');
            return {value: encoded === null ? 'null' : 'not-null'};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "null");
    }

    // Encode with missing type throws Error (script fails)
    TEST_F(SbmdScriptTest, TlvEncodeMissingTypeThrows)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode(42);
            return {value: 'unreachable'};
        )");
        // Script should fail due to uncaught exception
        EXPECT_FALSE(result.has_value());
    }

    // Encode string type with base argument throws Error
    TEST_F(SbmdScriptTest, TlvEncodeStringWithBaseThrows)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode('hello', 'string', 16);
            return {value: 'unreachable'};
        )");
        // Script should fail due to uncaught exception
        EXPECT_FALSE(result.has_value());
    }

    // Empty string input returns null for integer types
    TEST_F(SbmdScriptTest, TlvEncodeEmptyStringReturnsNull)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode('', 'uint8');
            return {value: encoded === null ? 'null' : 'not-null'};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "null");
    }

    // Non-numeric string returns null for integer types
    TEST_F(SbmdScriptTest, TlvEncodeNonNumericStringReturnsNull)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode('abc', 'uint8');
            return {value: encoded === null ? 'null' : 'not-null'};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "null");
    }

    // Invalid hex string returns null
    TEST_F(SbmdScriptTest, TlvEncodeInvalidHexStringReturnsNull)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode('GG', 'uint8', 16);
            return {value: encoded === null ? 'null' : 'not-null'};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "null");
    }

    // Invalid base returns null
    TEST_F(SbmdScriptTest, TlvEncodeInvalidBaseReturnsNull)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode('42', 'uint8', 7);
            return {value: encoded === null ? 'null' : 'not-null'};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "null");
    }

    // percent type range 0..255
    TEST_F(SbmdScriptTest, TlvEncodePercentRoundTrip)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode(100, 'percent');
            var decoded = SbmdUtils.Tlv.decode(encoded);
            return {value: decoded.toString()};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "100");
    }

    // bitmap8 type round-trip
    TEST_F(SbmdScriptTest, TlvEncodeBitmap8RoundTrip)
    {
        auto result = RunEncodeScript(*script, R"(
            var encoded = SbmdUtils.Tlv.encode(0xAB, 'bitmap8');
            var decoded = SbmdUtils.Tlv.decode(encoded);
            return {value: decoded.toString()};
        )");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "171"); // 0xAB = 171
    }

    //--------------------------------------------------------------------------
    // Out-of-memory handling tests (mquickjs-specific)
    //
    // These tests artificially restrict the mquickjs arena to verify that
    // OOM conditions are detected gracefully (return false / log errors)
    // rather than crashing.
    //--------------------------------------------------------------------------
#if defined(BCORE_USE_MQUICKJS)

    class SbmdScriptOomTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            // Shut down any existing runtime from prior tests
            MQuickJsRuntime::Shutdown();
        }

        void TearDown() override
        {
            // Always clean up the runtime so subsequent tests start fresh
            MQuickJsRuntime::Shutdown();
        }
    };

    // Arena too small even for context initialization (stdlib setup needs heap)
    TEST_F(SbmdScriptOomTest, TinyArenaFailsInitGracefully)
    {
        // 4 KB is too small for context + stdlib init
        EXPECT_FALSE(MQuickJsRuntime::Initialize(4096));
        EXPECT_FALSE(MQuickJsRuntime::IsInitialized());
    }

    // Arena large enough for context/stdlib/polyfill but too small for SBMD utils bundle
    TEST_F(SbmdScriptOomTest, SmallArenaFailsSbmdUtilsLoadGracefully)
    {
        // 16 KB: enough for init (~10KB) but SBMD utils bundle (28939 bytes)
        // needs significant heap for parsing
        ASSERT_TRUE(MQuickJsRuntime::Initialize(16384));

        // Manually try to load SBMD utils - this should fail due to OOM
        JSContext *ctx = MQuickJsRuntime::GetSharedContext();
        ASSERT_NE(ctx, nullptr);

        bool loaded = SbmdUtilsLoader::LoadBundle(ctx);
        EXPECT_FALSE(loaded);
    }

    // Arena just barely large enough for everything, then execute scripts
    // that allocate heavily to trigger OOM during script execution
    TEST_F(SbmdScriptOomTest, HeapExhaustionDuringScriptExec)
    {
        // 200KB is enough for init + SBMD utils but scripts that allocate heavily
        // will exhaust the remaining heap
        ASSERT_TRUE(MQuickJsRuntime::Initialize(200 * 1024));

        JSContext *ctx = MQuickJsRuntime::GetSharedContext();
        ASSERT_NE(ctx, nullptr);

        // Load SBMD utils (needed for scripts to work)
        ASSERT_TRUE(SbmdUtilsLoader::LoadBundle(ctx)) << "200KB should be sufficient for SBMD utils";
        JS_GC(ctx);

        auto script = SbmdScriptImpl::Create("oom-test-device");
        ASSERT_NE(script, nullptr);

        // Add a mapper with a script that allocates heavily
        SbmdAttribute attr;
        attr.clusterId = 6;
        attr.attributeId = 0;
        attr.name = "oomTest";
        attr.type = "bool";

        // Script that allocates buffers to exhaust heap quickly and deterministically
        std::string heavyScript = R"(
            var bufs = [];
            try {
                // Allocate a series of buffers until the arena is exhausted.
                // With a 200KB arena, this should OOM well before the loop completes.
                for (var i = 0; i < 2048; i++) {
                    bufs.push(new ArrayBuffer(256 * 1024));
                }
            } catch (e) {
                // Ignore out-of-memory or other allocation errors; we only care
                // that the engine handled them without crashing the host.
            }
            return { value: JSON.stringify({ value: bufs.length }) };
        )";
        script->AddAttributeReadMapper(attr, heavyScript);

        // Create a simple TLV value for the read call
        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        // The script catches the OOM error internally via try/catch, so it
        // completes successfully.  The important thing is the engine does not
        // crash.  Zero buffers should have been allocated since each request
        // (256 KB) exceeds the remaining arena.
        auto readResult = script->MapAttributeRead(attr, reader);
        ASSERT_TRUE(readResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(readResult.Operation()).value, R"({"value":0})");
    }

    // Test stack exhaustion via deeply recursive script
    TEST_F(SbmdScriptOomTest, StackExhaustionDuringScriptExec)
    {
        ASSERT_TRUE(MQuickJsRuntime::Initialize(200 * 1024)); // 200KB

        JSContext *ctx = MQuickJsRuntime::GetSharedContext();
        ASSERT_NE(ctx, nullptr);

        ASSERT_TRUE(SbmdUtilsLoader::LoadBundle(ctx)) << "200KB should be sufficient for SBMD utils";
        JS_GC(ctx);

        auto script = SbmdScriptImpl::Create("stack-oom-test");
        ASSERT_NE(script, nullptr);

        SbmdAttribute attr;
        attr.clusterId = 6;
        attr.attributeId = 0;
        attr.name = "stackTest";
        attr.type = "bool";

        // Script with infinite recursion to exhaust the stack
        std::string recursiveScript = R"(
            function recurse(n) { return recurse(n + 1); }
            return { value: JSON.stringify({value: recurse(0)}) };
        )";
        script->AddAttributeReadMapper(attr, recursiveScript);

        // Create a simple TLV value
        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        // Should fail gracefully with stack overflow, not crash
        auto readResult = script->MapAttributeRead(attr, reader);
        EXPECT_TRUE(readResult.IsError());
    }

    // After OOM during init, verify we can re-initialize with a larger size
    TEST_F(SbmdScriptOomTest, RecoveryAfterInitOom)
    {
        // First try with too-small arena
        EXPECT_FALSE(MQuickJsRuntime::Initialize(4096));
        EXPECT_FALSE(MQuickJsRuntime::IsInitialized());

        // Should be able to try again with a proper size
        MQuickJsRuntime::Shutdown(); // clean up any partial state
        EXPECT_TRUE(MQuickJsRuntime::Initialize(2097152));
        EXPECT_TRUE(MQuickJsRuntime::IsInitialized());
    }

    //--------------------------------------------------------------------------
    // Script execution timeout tests (mquickjs-specific)
    //
    // These tests verify that the interrupt handler terminates runaway scripts
    // and that the context remains usable afterward.
    //--------------------------------------------------------------------------

    class SbmdScriptTimeoutTest : public ::testing::Test
    {
    protected:
        void SetUp() override { MQuickJsRuntime::Shutdown(); }

        void TearDown() override { MQuickJsRuntime::Shutdown(); }
    };

    // An infinite loop script must be terminated by the interrupt handler
    TEST_F(SbmdScriptTimeoutTest, InfiniteLoopTerminatedByTimeout)
    {
        ASSERT_TRUE(MQuickJsRuntime::Initialize(1048576));

        JSContext *ctx = MQuickJsRuntime::GetSharedContext();
        ASSERT_NE(ctx, nullptr);
        ASSERT_TRUE(SbmdUtilsLoader::LoadBundle(ctx));

        auto script = SbmdScriptImpl::Create("timeout-test-device");
        ASSERT_NE(script, nullptr);

        SbmdAttribute attr;
        attr.clusterId = 6;
        attr.attributeId = 0;
        attr.name = "timeoutTest";
        attr.type = "bool";

        std::string infiniteScript = "while(true) {} return {value: 'never'};";
        ASSERT_TRUE(script->AddAttributeReadMapper(attr, infiniteScript));

        // Create a simple TLV boolean
        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        // Must return error (script interrupted), not hang forever
        auto readResult = script->MapAttributeRead(attr, reader);
        EXPECT_TRUE(readResult.IsError());
    }

    // A normal fast script completes successfully with timeout enabled
    TEST_F(SbmdScriptTimeoutTest, NormalScriptCompletesWithTimeoutEnabled)
    {
        ASSERT_TRUE(MQuickJsRuntime::Initialize(1048576));

        JSContext *ctx = MQuickJsRuntime::GetSharedContext();
        ASSERT_NE(ctx, nullptr);
        ASSERT_TRUE(SbmdUtilsLoader::LoadBundle(ctx));

        auto script = SbmdScriptImpl::Create("timeout-normal-device");
        ASSERT_NE(script, nullptr);

        SbmdAttribute attr;
        attr.clusterId = 6;
        attr.attributeId = 0;
        attr.name = "normalTest";
        attr.type = "bool";

        std::string normalScript =
            "var val = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64); return {value: val ? 'true' : 'false'};";
        ASSERT_TRUE(script->AddAttributeReadMapper(attr, normalScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        auto readResult = script->MapAttributeRead(attr, reader);
        ASSERT_TRUE(readResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(readResult.Operation()).value, "true");
    }

    // After a timeout, the context must remain usable for subsequent scripts
    TEST_F(SbmdScriptTimeoutTest, ContextUsableAfterTimeout)
    {
        ASSERT_TRUE(MQuickJsRuntime::Initialize(1048576));

        JSContext *ctx = MQuickJsRuntime::GetSharedContext();
        ASSERT_NE(ctx, nullptr);
        ASSERT_TRUE(SbmdUtilsLoader::LoadBundle(ctx));

        auto script = SbmdScriptImpl::Create("timeout-recovery-device");
        ASSERT_NE(script, nullptr);

        SbmdAttribute badAttr;
        badAttr.clusterId = 6;
        badAttr.attributeId = 0;
        badAttr.name = "badScript";
        badAttr.type = "bool";

        SbmdAttribute goodAttr;
        goodAttr.clusterId = 6;
        goodAttr.attributeId = 1;
        goodAttr.name = "goodScript";
        goodAttr.type = "bool";

        std::string infiniteScript = "while(true) {} return {value: 'never'};";
        std::string normalScript =
            "var val = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64); return {value: val ? 'true' : 'false'};";

        ASSERT_TRUE(script->AddAttributeReadMapper(badAttr, infiniteScript));
        ASSERT_TRUE(script->AddAttributeReadMapper(goodAttr, normalScript));

        // Create TLV boolean for both calls
        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), false);
        writer.Finalize();

        // First: run the infinite loop script — should time out
        {
            chip::TLV::TLVReader reader;
            reader.Init(tlvBuffer, writer.GetLengthWritten());
            reader.Next();

            auto readResult = script->MapAttributeRead(badAttr, reader);
            EXPECT_TRUE(readResult.IsError());
        }

        // Second: run a normal script — should succeed, proving context is OK
        {
            chip::TLV::TLVReader reader;
            reader.Init(tlvBuffer, writer.GetLengthWritten());
            reader.Next();

            auto readResult = script->MapAttributeRead(goodAttr, reader);
            ASSERT_TRUE(readResult.HasOperation());
            EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(readResult.Operation()).value, "false");
        }
    }

    // Test: MapAttributeRead with uint8 decoded to boolean (seedFrom script pattern)
    // Exercises the exact script shape used by the door-lock seedFrom mapper:
    //   - decode uint8 TLV using SbmdUtils.Tlv.decode()
    //   - return "true" for value 1 (Locked), "false" for value 2 (Unlocked)
    TEST_F(SbmdScriptTest, MapAttributeReadUint8ToBoolean)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0101;
        attr.attributeId = 0x0000;
        attr.name = "LockState";
        attr.type = "uint8";

        std::string mapperScript = "var value = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64);"
                                   "var isLocked = value === 1;"
                                   "return { value: isLocked ? 'true' : 'false' };";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        // Helper to write a uint8 TLV and run the mapper
        auto runMapper = [&](uint8_t lockStateValue, const std::string &expectedOutput) {
            uint8_t tlvBuffer[32];
            chip::TLV::TLVWriter writer;
            writer.Init(tlvBuffer, sizeof(tlvBuffer));
            writer.Put(chip::TLV::AnonymousTag(), lockStateValue);
            writer.Finalize();

            chip::TLV::TLVReader reader;
            reader.Init(tlvBuffer, writer.GetLengthWritten());
            reader.Next();

            auto readResult = script->MapAttributeRead(attr, reader);
            ASSERT_TRUE(readResult.HasOperation());
            EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(readResult.Operation()).value, expectedOutput);
        };

        runMapper(1, "true");  // DlLockState::Locked
        runMapper(2, "false"); // DlLockState::Unlocked
        runMapper(0, "false"); // DlLockState::NotFullyLocked (not == 1, so false)
    }

    //==============================================================================
    // MapEvent tests
    //
    // MapEvent has a tri-state contract (documented in SbmdScript.h):
    //   IsError()       = script error (exception, compile error, non-object return)
    //   IsSuppressed()  = suppress (script returned {} with no recognized keys)
    //   HasOperation()  = publish (script returned { value: "..." })
    //==============================================================================

    // Helper: encode a LockOperation TLV struct with a single uint8 at context tag 0.
    // The door-lock event script reads event[0] as the LockOperationType.
    //
    // NOTE: reader is initialized with sizeof(buf) — not just GetLengthWritten() — to match
    // the production code path where MapEvent receives a reader whose underlying buffer is
    // the full Matter subscription report (much larger than the struct being read).
    // MapEvent's CopyElement needs 1 extra byte of headroom beyond GetLengthWritten() due
    // to tag encoding; using the full buffer size provides that.
    static void WriteLockOperationTlv(uint8_t (&buf)[64], chip::TLV::TLVReader &reader, uint8_t lockOperationType)
    {
        chip::TLV::TLVWriter writer;
        writer.Init(buf, sizeof(buf));
        chip::TLV::TLVType structType;
        writer.StartContainer(chip::TLV::AnonymousTag(), chip::TLV::kTLVType_Structure, structType);
        writer.Put(chip::TLV::ContextTag(0), lockOperationType);
        writer.EndContainer(structType);
        writer.Finalize();

        reader.Init(buf, sizeof(buf));
        reader.Next();
    }

    // Test: MapEvent returns false when no mapper is registered
    TEST_F(SbmdScriptTest, MapEventNoMapper)
    {
        SbmdEvent event;
        event.clusterId = 0x0101;
        event.eventId = 0x0002;
        event.name = "LockOperation";

        uint8_t buf[64];
        chip::TLV::TLVReader reader;
        WriteLockOperationTlv(buf, reader, 0);

        auto eventResult = script->MapEvent(event, reader);
        EXPECT_TRUE(eventResult.IsError());
    }

    // Test: AddEventMapper returns false for an empty script
    TEST_F(SbmdScriptTest, AddEventMapperRejectsEmptyScript)
    {
        SbmdEvent event;
        event.clusterId = 0x0101;
        event.eventId = 0x0002;
        event.name = "LockOperation";

        EXPECT_FALSE(script->AddEventMapper(event, ""));
    }

    // Test: MapEvent happy path — LockOperationType 0 (Lock) → "true"
    TEST_F(SbmdScriptTest, MapEventLockOperationLock)
    {
        SbmdEvent event;
        event.clusterId = 0x0101;
        event.eventId = 0x0002;
        event.name = "LockOperation";

        // Exact script from door-lock.sbmd
        std::string mapperScript = R"(
            var event = SbmdUtils.Tlv.decode(sbmdEventArgs.tlvBase64);
            var opType = event[0];
            if (opType === 0) { return { value: 'true' }; }
            if (opType === 1) { return { value: 'false' }; }
            return {};
        )";

        ASSERT_TRUE(script->AddEventMapper(event, mapperScript));

        uint8_t buf[64];
        chip::TLV::TLVReader reader;
        WriteLockOperationTlv(buf, reader, 0 /* Lock */);

        auto eventResult = script->MapEvent(event, reader);
        ASSERT_TRUE(eventResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(eventResult.Operation()).value, "true");
    }

    // Test: MapEvent happy path — LockOperationType 1 (Unlock) → "false"
    TEST_F(SbmdScriptTest, MapEventLockOperationUnlock)
    {
        SbmdEvent event;
        event.clusterId = 0x0101;
        event.eventId = 0x0002;
        event.name = "LockOperation";

        std::string mapperScript = R"(
            var event = SbmdUtils.Tlv.decode(sbmdEventArgs.tlvBase64);
            var opType = event[0];
            if (opType === 0) { return { value: 'true' }; }
            if (opType === 1) { return { value: 'false' }; }
            return {};
        )";

        ASSERT_TRUE(script->AddEventMapper(event, mapperScript));

        uint8_t buf[64];
        chip::TLV::TLVReader reader;
        WriteLockOperationTlv(buf, reader, 1 /* Unlock */);

        auto eventResult = script->MapEvent(event, reader);
        ASSERT_TRUE(eventResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(eventResult.Operation()).value, "false");
    }

    // Test: MapEvent suppress path — LockOperationType 2 (NonAccessUserEvent) → IsSuppressed().
    // The caller checks IsSuppressed() and skips updateResource; this is the primary
    // motivation for the tri-state contract.
    TEST_F(SbmdScriptTest, MapEventSuppressOnNoOutputKey)
    {
        SbmdEvent event;
        event.clusterId = 0x0101;
        event.eventId = 0x0002;
        event.name = "LockOperation";

        std::string mapperScript = R"(
            var event = SbmdUtils.Tlv.decode(sbmdEventArgs.tlvBase64);
            var opType = event[0];
            if (opType === 0) { return { value: 'true' }; }
            if (opType === 1) { return { value: 'false' }; }
            return {};
        )";

        ASSERT_TRUE(script->AddEventMapper(event, mapperScript));

        uint8_t buf[64];
        chip::TLV::TLVReader reader;
        WriteLockOperationTlv(buf, reader, 2 /* NonAccessUserEvent */);

        // suppress: {} with no recognized keys → IsSuppressed()
        auto eventResult = script->MapEvent(event, reader);
        EXPECT_TRUE(eventResult.SkipsResourceUpdate());
    }

    // Test: MapEvent returns false when script returns a non-object (primitive string).
    // A bare string return is always a script error, not a suppress.
    TEST_F(SbmdScriptTest, MapEventFailsOnPrimitiveStringReturn)
    {
        SbmdEvent event;
        event.clusterId = 0x0101;
        event.eventId = 0x0002;
        event.name = "LockOperation";

        std::string mapperScript = "return 'true';"; // string, not object

        ASSERT_TRUE(script->AddEventMapper(event, mapperScript));

        uint8_t buf[64];
        chip::TLV::TLVReader reader;
        WriteLockOperationTlv(buf, reader, 0);

        auto eventResult = script->MapEvent(event, reader);
        EXPECT_TRUE(eventResult.IsError());
    }

    // Test: MapEvent returns error when script returns null.
    TEST_F(SbmdScriptTest, MapEventFailsOnNullReturn)
    {
        SbmdEvent event;
        event.clusterId = 0x0101;
        event.eventId = 0x0002;
        event.name = "LockOperation";

        std::string mapperScript = "return null;";

        ASSERT_TRUE(script->AddEventMapper(event, mapperScript));

        uint8_t buf[64];
        chip::TLV::TLVReader reader;
        WriteLockOperationTlv(buf, reader, 0);

        auto eventResult = script->MapEvent(event, reader);
        EXPECT_TRUE(eventResult.IsError());
    }

    // Test: MapEvent suppresses when script returns {value: null}.
    // A null value is treated as absent — the engine ignores it, resulting in suppress.
    TEST_F(SbmdScriptTest, MapEventSuppressOnValueNull)
    {
        SbmdEvent event;
        event.clusterId = 0x0101;
        event.eventId = 0x0002;
        event.name = "LockOperation";

        std::string mapperScript = "return { value: null };";

        ASSERT_TRUE(script->AddEventMapper(event, mapperScript));

        uint8_t buf[64];
        chip::TLV::TLVReader reader;
        WriteLockOperationTlv(buf, reader, 0);

        auto eventResult = script->MapEvent(event, reader);
        EXPECT_TRUE(eventResult.SkipsResourceUpdate());
    }

    // Test: MapEvent returns error when script returns undefined (missing return statement).
    TEST_F(SbmdScriptTest, MapEventFailsOnUndefinedReturn)
    {
        SbmdEvent event;
        event.clusterId = 0x0101;
        event.eventId = 0x0002;
        event.name = "LockOperation";

        std::string mapperScript = "var x = 1;"; // no return statement → undefined

        ASSERT_TRUE(script->AddEventMapper(event, mapperScript));

        uint8_t buf[64];
        chip::TLV::TLVReader reader;
        WriteLockOperationTlv(buf, reader, 0);

        auto eventResult = script->MapEvent(event, reader);
        EXPECT_TRUE(eventResult.IsError());
    }

    // Test: MapEvent returns error on script syntax error
    TEST_F(SbmdScriptTest, MapEventFailsOnSyntaxError)
    {
        SbmdEvent event;
        event.clusterId = 0x0101;
        event.eventId = 0x0002;
        event.name = "LockOperation";

        std::string mapperScript = "return {value: invalid syntax here";

        ASSERT_TRUE(script->AddEventMapper(event, mapperScript));

        uint8_t buf[64];
        chip::TLV::TLVReader reader;
        WriteLockOperationTlv(buf, reader, 0);

        auto eventResult = script->MapEvent(event, reader);
        EXPECT_TRUE(eventResult.IsError());
    }

    // Test: MapEvent exposes sbmdEventArgs.deviceUuid to the script
    TEST_F(SbmdScriptTest, MapEventHasDeviceUuid)
    {
        SbmdEvent event;
        event.clusterId = 0x0101;
        event.eventId = 0x0002;
        event.name = "LockOperation";

        ASSERT_TRUE(script->AddEventMapper(event, "return { value: sbmdEventArgs.deviceUuid };"));

        uint8_t buf[64];
        chip::TLV::TLVReader reader;
        WriteLockOperationTlv(buf, reader, 0);

        auto eventResult = script->MapEvent(event, reader);
        ASSERT_TRUE(eventResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(eventResult.Operation()).value, deviceId);
    }

    // Test: MapEvent exposes sbmdEventArgs.clusterId to the script
    TEST_F(SbmdScriptTest, MapEventHasClusterId)
    {
        SbmdEvent event;
        event.clusterId = 0x0101;
        event.eventId = 0x0002;
        event.name = "LockOperation";

        ASSERT_TRUE(script->AddEventMapper(event, "return { value: sbmdEventArgs.clusterId.toString() };"));

        uint8_t buf[64];
        chip::TLV::TLVReader reader;
        WriteLockOperationTlv(buf, reader, 0);

        auto eventResult = script->MapEvent(event, reader);
        ASSERT_TRUE(eventResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(eventResult.Operation()).value, "257"); // 0x0101 = 257
    }

    // Test: MapEvent exposes sbmdEventArgs.eventId to the script
    TEST_F(SbmdScriptTest, MapEventHasEventId)
    {
        SbmdEvent event;
        event.clusterId = 0x0101;
        event.eventId = 0x0002;
        event.name = "LockOperation";

        ASSERT_TRUE(script->AddEventMapper(event, "return { value: sbmdEventArgs.eventId.toString() };"));

        uint8_t buf[64];
        chip::TLV::TLVReader reader;
        WriteLockOperationTlv(buf, reader, 0);

        auto eventResult = script->MapEvent(event, reader);
        ASSERT_TRUE(eventResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(eventResult.Operation()).value, "2"); // 0x0002 = 2
    }

    // Test: MapEvent exposes sbmdEventArgs.eventName to the script
    TEST_F(SbmdScriptTest, MapEventHasEventName)
    {
        SbmdEvent event;
        event.clusterId = 0x0101;
        event.eventId = 0x0002;
        event.name = "LockOperation";

        ASSERT_TRUE(script->AddEventMapper(event, "return { value: sbmdEventArgs.eventName };"));

        uint8_t buf[64];
        chip::TLV::TLVReader reader;
        WriteLockOperationTlv(buf, reader, 0);

        auto eventResult = script->MapEvent(event, reader);
        ASSERT_TRUE(eventResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(eventResult.Operation()).value, "LockOperation");
    }

    // Test: MapEvent exposes sbmdEventArgs.endpointId to the script
    TEST_F(SbmdScriptTest, MapEventHasEndpointId)
    {
        SbmdEvent event;
        event.clusterId = 0x0101;
        event.eventId = 0x0002;
        event.name = "LockOperation";
        event.resourceEndpointId = "ep1";

        ASSERT_TRUE(script->AddEventMapper(event, "return { value: sbmdEventArgs.endpointId };"));

        uint8_t buf[64];
        chip::TLV::TLVReader reader;
        WriteLockOperationTlv(buf, reader, 0);

        auto eventResult = script->MapEvent(event, reader);
        ASSERT_TRUE(eventResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(eventResult.Operation()).value, "ep1");
    }

#endif // BCORE_USE_MQUICKJS

    // =========================================================================
    // MapExecute output-only tests (no Matter interaction)
    // =========================================================================

    // Test: MapExecute returns ResourceUpdate with simple value string
    TEST_F(SbmdScriptTest, MapExecuteOutputSimple)
    {
        std::string resourceKey = "camera:createSession";
        std::string executeScript = "return {value: 'hello'};";

        ASSERT_TRUE(script->AddExecuteMapper(resourceKey, executeScript, std::nullopt));

        auto result = script->MapExecute(resourceKey, "camera", "createSession", "");
        ASSERT_FALSE(result.IsError());
        ASSERT_TRUE(result.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(result.Operation()).value, "hello");
    }

    // Test: MapExecute value from SbmdUtils.Response.value()
    TEST_F(SbmdScriptTest, MapExecuteOutputViaResponseHelper)
    {
        std::string resourceKey = "camera:createSession";
        std::string executeScript = "return SbmdUtils.Response.value('42');";

        ASSERT_TRUE(script->AddExecuteMapper(resourceKey, executeScript, std::nullopt));

        auto result = script->MapExecute(resourceKey, "camera", "createSession", "");
        ASSERT_FALSE(result.IsError());
        ASSERT_TRUE(result.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(result.Operation()).value, "42");
    }

    // Test: MapExecute still supports invoke responses
    TEST_F(SbmdScriptTest, MapExecuteInvokeStillWorks)
    {
        std::string resourceKey = "ep:lock";
        std::string executeScript =
            "return {invoke: {clusterId: 0x0101, commandId: 0x0000}};";

        ASSERT_TRUE(script->AddExecuteMapper(resourceKey, executeScript, std::nullopt));

        auto result = script->MapExecute(resourceKey, "ep", "lock", "");
        ASSERT_FALSE(result.IsError());
        ASSERT_TRUE(result.HasOperation());
        const auto &writeResult = std::get<ScriptWriteResult>(result.Operation());
        EXPECT_EQ(writeResult.type, ScriptWriteResult::OperationType::Invoke);
        EXPECT_EQ(writeResult.clusterId, 0x0101u);
        EXPECT_EQ(writeResult.commandId, 0x0000u);
    }

    // Test: SessionManager.getForDevice creates per-device sessions
    TEST_F(SbmdScriptTest, MapExecuteSessionManagerCreateSession)
    {
        std::string resourceKey = "camera:createSession";
        std::string executeScript =
            "var sessions = SbmdUtils.SessionManager.getForDevice(sbmdCommandArgs.deviceUuid);"
            "var id = sessions.create({});"
            "return SbmdUtils.Response.value(id.toString());";

        ASSERT_TRUE(script->AddExecuteMapper(resourceKey, executeScript, std::nullopt));

        auto result = script->MapExecute(resourceKey, "camera", "createSession", "");
        ASSERT_FALSE(result.IsError());
        ASSERT_TRUE(result.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(result.Operation()).value, "1");

        // Second call should return 2
        auto result2 = script->MapExecute(resourceKey, "camera", "createSession", "");
        ASSERT_FALSE(result2.IsError());
        ASSERT_TRUE(result2.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(result2.Operation()).value, "2");
    }

    // Test: SessionManager get returns data for valid session
    TEST_F(SbmdScriptTest, MapExecuteSessionManagerGetValid)
    {
        std::string createKey = "camera:createSession";
        std::string createScript =
            "var sessions = SbmdUtils.SessionManager.getForDevice(sbmdCommandArgs.deviceUuid);"
            "var id = sessions.create({});"
            "return SbmdUtils.Response.value(id.toString());";

        std::string streamKey = "camera:stream";
        std::string streamScript =
            "var sessions = SbmdUtils.SessionManager.getForDevice(sbmdCommandArgs.deviceUuid);"
            "var sessionId = parseInt(sbmdCommandArgs.input, 10);"
            "var session = sessions.get(sessionId);"
            "if (!session) { return SbmdUtils.Response.error('invalidSession'); }"
            "return SbmdUtils.Response.value('ok');";

        ASSERT_TRUE(script->AddExecuteMapper(createKey, createScript, std::nullopt));
        ASSERT_TRUE(script->AddExecuteMapper(streamKey, streamScript, std::nullopt));

        // Create a session
        auto createResult = script->MapExecute(createKey, "camera", "createSession", "");
        ASSERT_FALSE(createResult.IsError());
        ASSERT_TRUE(createResult.HasOperation());
        std::string sessionId = std::get<ScriptResult::ResourceUpdate>(createResult.Operation()).value;

        // Use it
        auto streamResult = script->MapExecute(streamKey, "camera", "stream", sessionId);
        ASSERT_FALSE(streamResult.IsError());
        ASSERT_TRUE(streamResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(streamResult.Operation()).value, "ok");
    }

    // Test: SessionManager get returns error for invalid session
    TEST_F(SbmdScriptTest, MapExecuteSessionManagerGetInvalid)
    {
        std::string streamKey = "camera:stream";
        std::string streamScript =
            "var sessions = SbmdUtils.SessionManager.getForDevice(sbmdCommandArgs.deviceUuid);"
            "var sessionId = parseInt(sbmdCommandArgs.input, 10);"
            "var session = sessions.get(sessionId);"
            "if (!session) { return SbmdUtils.Response.error('invalidSession'); }"
            "return SbmdUtils.Response.value('ok');";

        ASSERT_TRUE(script->AddExecuteMapper(streamKey, streamScript, std::nullopt));

        auto result = script->MapExecute(streamKey, "camera", "stream", "999");
        ASSERT_TRUE(result.IsError());
        EXPECT_EQ(result.ErrorMessage(), "invalidSession");
    }

    // Test: SessionManager destroy removes session
    TEST_F(SbmdScriptTest, MapExecuteSessionManagerDestroy)
    {
        std::string createKey = "camera:createSession";
        std::string createScript =
            "var sessions = SbmdUtils.SessionManager.getForDevice(sbmdCommandArgs.deviceUuid);"
            "var id = sessions.create({});"
            "return SbmdUtils.Response.value(id.toString());";

        std::string destroyKey = "camera:destroySession";
        std::string destroyScript =
            "var sessions = SbmdUtils.SessionManager.getForDevice(sbmdCommandArgs.deviceUuid);"
            "var sessionId = parseInt(sbmdCommandArgs.input, 10);"
            "if (!sessions.destroy(sessionId)) { return SbmdUtils.Response.error('invalidSession'); }"
            "return SbmdUtils.Response.value('ok');";

        std::string streamKey = "camera:stream";
        std::string streamScript =
            "var sessions = SbmdUtils.SessionManager.getForDevice(sbmdCommandArgs.deviceUuid);"
            "var sessionId = parseInt(sbmdCommandArgs.input, 10);"
            "var session = sessions.get(sessionId);"
            "if (!session) { return SbmdUtils.Response.error('invalidSession'); }"
            "return SbmdUtils.Response.value('ok');";

        ASSERT_TRUE(script->AddExecuteMapper(createKey, createScript, std::nullopt));
        ASSERT_TRUE(script->AddExecuteMapper(destroyKey, destroyScript, std::nullopt));
        ASSERT_TRUE(script->AddExecuteMapper(streamKey, streamScript, std::nullopt));

        // Create session
        auto createResult = script->MapExecute(createKey, "camera", "createSession", "");
        ASSERT_FALSE(createResult.IsError());
        ASSERT_TRUE(createResult.HasOperation());
        std::string sessionId = std::get<ScriptResult::ResourceUpdate>(createResult.Operation()).value;

        // Destroy it
        auto destroyResult = script->MapExecute(destroyKey, "camera", "destroySession", sessionId);
        ASSERT_FALSE(destroyResult.IsError());
        ASSERT_TRUE(destroyResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(destroyResult.Operation()).value, "ok");

        // Trying to use it after destroy should fail
        auto streamResult = script->MapExecute(streamKey, "camera", "stream", sessionId);
        ASSERT_TRUE(streamResult.IsError());
        EXPECT_EQ(streamResult.ErrorMessage(), "invalidSession");

        // Destroying again should also fail
        auto destroyResult2 = script->MapExecute(destroyKey, "camera", "destroySession", sessionId);
        ASSERT_TRUE(destroyResult2.IsError());
        EXPECT_EQ(destroyResult2.ErrorMessage(), "invalidSession");
    }

    // Test: SessionManager getForDevice isolates sessions between devices
    TEST_F(SbmdScriptTest, MapExecuteSessionManagerPerDevice)
    {
        // Create two script instances for different devices
        auto script2 = CreateScript("other-device-uuid");
        ASSERT_NE(script2, nullptr);

        std::string createKey = "camera:createSession";
        std::string createScript =
            "var sessions = SbmdUtils.SessionManager.getForDevice(sbmdCommandArgs.deviceUuid);"
            "var id = sessions.create({});"
            "return SbmdUtils.Response.value(id.toString());";

        std::string streamKey = "camera:stream";
        std::string streamScript =
            "var sessions = SbmdUtils.SessionManager.getForDevice(sbmdCommandArgs.deviceUuid);"
            "var sessionId = parseInt(sbmdCommandArgs.input, 10);"
            "var session = sessions.get(sessionId);"
            "if (!session) { return SbmdUtils.Response.error('invalidSession'); }"
            "return SbmdUtils.Response.value('ok');";

        ASSERT_TRUE(script->AddExecuteMapper(createKey, createScript, std::nullopt));
        ASSERT_TRUE(script->AddExecuteMapper(streamKey, streamScript, std::nullopt));
        ASSERT_TRUE(script2->AddExecuteMapper(createKey, createScript, std::nullopt));
        ASSERT_TRUE(script2->AddExecuteMapper(streamKey, streamScript, std::nullopt));

        // Create session on device 1
        auto createResult = script->MapExecute(createKey, "camera", "createSession", "");
        ASSERT_FALSE(createResult.IsError());
        ASSERT_TRUE(createResult.HasOperation());
        std::string sessionId = std::get<ScriptResult::ResourceUpdate>(createResult.Operation()).value;

        // Device 1 can use it
        auto streamResult = script->MapExecute(streamKey, "camera", "stream", sessionId);
        ASSERT_FALSE(streamResult.IsError());
        ASSERT_TRUE(streamResult.HasOperation());
        EXPECT_EQ(std::get<ScriptResult::ResourceUpdate>(streamResult.Operation()).value, "ok");

        // Device 2 cannot use device 1's session (different deviceUuid in args)
        auto streamResult2 = script2->MapExecute(streamKey, "camera", "stream", sessionId);
        ASSERT_TRUE(streamResult2.IsError());
        EXPECT_EQ(streamResult2.ErrorMessage(), "invalidSession");
    }

} // namespace
