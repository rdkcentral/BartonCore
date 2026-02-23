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
 * Unit tests for QuickJsScript module focusing on script interfaces.
 */

#include "deviceDrivers/matter/sbmd/script/QuickJsScript.h"
#include "deviceDrivers/matter/sbmd/SbmdSpec.h"
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

    class QuickJsScriptTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            deviceId = "test-device-uuid";
            script = QuickJsScript::Create(deviceId);
            ASSERT_NE(script, nullptr) << "Failed to create QuickJsScript";
        }

        void TearDown() override { script.reset(); }

        std::string deviceId;
        std::unique_ptr<QuickJsScript> script;
    };

    // Test: QuickJsScript can be instantiated
    TEST_F(QuickJsScriptTest, CanCreate)
    {
        ASSERT_NE(script, nullptr);
    }

    // Test: AddAttributeReadMapper returns true for valid input
    TEST_F(QuickJsScriptTest, AddAttributeReadMapperSuccess)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006; // On/Off cluster
        attr.attributeId = 0x0000; // OnOff attribute
        attr.name = "onOff";
        attr.type = "bool";

        std::string mapperScript =
            "var val = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64); return {output: val ? 'true' : 'false'};";

        EXPECT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));
    }

    // Test: MapAttributeRead returns false when no mapper is registered
    TEST_F(QuickJsScriptTest, MapAttributeReadNoMapper)
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

        std::string outValue;
        EXPECT_FALSE(script->MapAttributeRead(attr, reader, outValue));
    }

    // Test: MapAttributeRead with simple boolean passthrough script
    TEST_F(QuickJsScriptTest, MapAttributeReadBooleanPassthrough)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // Script that converts Matter boolean to Barton string
        // sbmdReadArgs.tlvBase64 contains base64 encoded TLV
        std::string mapperScript = "var val = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64); return {output: (val === "
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

        std::string outValue;
        ASSERT_TRUE(script->MapAttributeRead(attr, reader, outValue));
        EXPECT_EQ(outValue, "true");
    }

    // Test: MapAttributeRead with boolean false value
    // sbmdReadArgs.tlvBase64 contains base64 encoded TLV - use SbmdUtils.Tlv.decode() to decode
    TEST_F(QuickJsScriptTest, MapAttributeReadBooleanFalse)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // Script needs to properly handle false - compare against true explicitly
        // sbmdReadArgs.tlvBase64 contains base64 encoded TLV
        std::string mapperScript = "var val = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64); return {output: (val === "
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

        std::string outValue;
        ASSERT_TRUE(script->MapAttributeRead(attr, reader, outValue));
        EXPECT_EQ(outValue, "false");
    }

    // Test: MapAttributeRead with integer value conversion
    // sbmdReadArgs.tlvBase64 contains base64 encoded TLV - use SbmdUtils.Tlv.decode() to decode
    TEST_F(QuickJsScriptTest, MapAttributeReadIntegerConversion)
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
            return {output: percent.toString()};
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

        std::string outValue;
        ASSERT_TRUE(script->MapAttributeRead(attr, reader, outValue));
        EXPECT_EQ(outValue, "50"); // 127/254 * 100 = 50%
    }

    // Test: MapAttributeRead verifies sbmdReadArgs contains deviceUuid
    TEST_F(QuickJsScriptTest, MapAttributeReadHasDeviceUuid)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // Script that returns the deviceUuid
        std::string mapperScript = "return {output: sbmdReadArgs.deviceUuid};";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        std::string outValue;
        ASSERT_TRUE(script->MapAttributeRead(attr, reader, outValue));
        EXPECT_EQ(outValue, deviceId);
    }

    // Test: MapAttributeRead verifies sbmdReadArgs contains clusterId
    TEST_F(QuickJsScriptTest, MapAttributeReadHasClusterId)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // Script that returns the clusterId
        std::string mapperScript = "return {output: sbmdReadArgs.clusterId.toString()};";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        std::string outValue;
        ASSERT_TRUE(script->MapAttributeRead(attr, reader, outValue));
        EXPECT_EQ(outValue, "6"); // 0x0006 = 6
    }

    // Test: MapAttributeRead verifies sbmdReadArgs contains attributeId
    TEST_F(QuickJsScriptTest, MapAttributeReadHasAttributeId)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0005;
        attr.name = "testAttr";
        attr.type = "bool";

        // Script that returns the attributeId
        std::string mapperScript = "return {output: sbmdReadArgs.attributeId.toString()};";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        std::string outValue;
        ASSERT_TRUE(script->MapAttributeRead(attr, reader, outValue));
        EXPECT_EQ(outValue, "5"); // 0x0005 = 5
    }

    // Test: MapAttributeRead verifies sbmdReadArgs contains attributeName
    TEST_F(QuickJsScriptTest, MapAttributeReadHasAttributeName)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "myTestAttribute";
        attr.type = "bool";

        // Script that returns the attributeName
        std::string mapperScript = "return {output: sbmdReadArgs.attributeName};";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        std::string outValue;
        ASSERT_TRUE(script->MapAttributeRead(attr, reader, outValue));
        EXPECT_EQ(outValue, "myTestAttribute");
    }

    // Test: MapAttributeRead verifies sbmdReadArgs contains attributeType
    TEST_F(QuickJsScriptTest, MapAttributeReadHasAttributeType)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "boolean";

        // Script that returns the attributeType
        std::string mapperScript = "return {output: sbmdReadArgs.attributeType};";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        std::string outValue;
        ASSERT_TRUE(script->MapAttributeRead(attr, reader, outValue));
        EXPECT_EQ(outValue, "boolean");
    }

    // Test: MapAttributeRead fails when script doesn't return output field
    TEST_F(QuickJsScriptTest, MapAttributeReadMissingOutputField)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // Script that returns wrong format (missing output field)
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

        std::string outValue;
        EXPECT_FALSE(script->MapAttributeRead(attr, reader, outValue));
    }

    // Test: MapAttributeRead fails with script syntax error
    TEST_F(QuickJsScriptTest, MapAttributeReadScriptSyntaxError)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // Script with syntax error
        std::string mapperScript = "return {output: invalid syntax here";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        std::string outValue;
        EXPECT_FALSE(script->MapAttributeRead(attr, reader, outValue));
    }

    // Test: MapAttributeRead with endpointId set
    TEST_F(QuickJsScriptTest, MapAttributeReadWithEndpointId)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";
        attr.resourceEndpointId = "ep1";

        // Script that returns the endpointId
        std::string mapperScript = "return {output: sbmdReadArgs.endpointId};";

        ASSERT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        std::string outValue;
        ASSERT_TRUE(script->MapAttributeRead(attr, reader, outValue));
        EXPECT_EQ(outValue, "ep1");
    }

    // Test: Multiple attribute mappers can coexist
    TEST_F(QuickJsScriptTest, MultipleAttributeMappers)
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

        std::string script1 = "return {output: 'attr1'};";
        std::string script2 = "return {output: 'attr2'};";

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

        std::string outValue1;
        ASSERT_TRUE(script->MapAttributeRead(attr1, reader1, outValue1));
        EXPECT_EQ(outValue1, "attr1");

        // Test attr2
        uint8_t tlvBuffer2[32];
        chip::TLV::TLVWriter writer2;
        writer2.Init(tlvBuffer2, sizeof(tlvBuffer2));
        writer2.Put(chip::TLV::AnonymousTag(), static_cast<uint8_t>(100));
        writer2.Finalize();

        chip::TLV::TLVReader reader2;
        reader2.Init(tlvBuffer2, writer2.GetLengthWritten());
        reader2.Next();

        std::string outValue2;
        ASSERT_TRUE(script->MapAttributeRead(attr2, reader2, outValue2));
        EXPECT_EQ(outValue2, "attr2");
    }

    // Test: Script can access complex JSON structures
    TEST_F(QuickJsScriptTest, MapAttributeReadWithJsonStructure)
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
            return {output: result};
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

        std::string outValue;
        ASSERT_TRUE(script->MapAttributeRead(attr, reader, outValue));
        // Verify it contains expected parts
        // Ensure the input value and device UUID appear in the output string
        EXPECT_NE(outValue.find("input:true"), std::string::npos);
        EXPECT_NE(outValue.find("device:test-device-uuid"), std::string::npos);
    }

    //==============================================================================
    // MapCommandExecuteResponse tests
    //==============================================================================

    // Test: MapCommandExecuteResponse returns false when no mapper is registered
    TEST_F(QuickJsScriptTest, MapCommandExecuteResponseNoMapper)
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

        std::string outValue;
        EXPECT_FALSE(script->MapCommandExecuteResponse(cmd, reader, outValue));
    }

    // Test: MapCommandExecuteResponse happy path with boolean response
    TEST_F(QuickJsScriptTest, MapCommandExecuteResponseBooleanHappyPath)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0001;
        cmd.name = "on";

        // Script that converts Matter boolean response to string
        std::string mapperScript = R"(
            var val = SbmdUtils.Tlv.decode(sbmdCommandResponseArgs.tlvBase64);
            return {output: val ? 'success' : 'failure'};
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

        std::string outValue;
        ASSERT_TRUE(script->MapCommandExecuteResponse(cmd, reader, outValue));
        EXPECT_EQ(outValue, "success");
    }

    // Test: MapCommandExecuteResponse happy path with integer response
    TEST_F(QuickJsScriptTest, MapCommandExecuteResponseIntegerHappyPath)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0101; // Door Lock
        cmd.commandId = 0x0000;
        cmd.name = "getLockState";

        // Script that converts lock state integer to string
        std::string mapperScript = R"(
            var states = ['not_fully_locked', 'locked', 'unlocked', 'unlatched'];
            var state = SbmdUtils.Tlv.decode(sbmdCommandResponseArgs.tlvBase64);
            return {output: states[state] || 'unknown'};
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

        std::string outValue;
        ASSERT_TRUE(script->MapCommandExecuteResponse(cmd, reader, outValue));
        EXPECT_EQ(outValue, "locked");
    }

    // Test: MapCommandExecuteResponse with struct TLV response
    TEST_F(QuickJsScriptTest, MapCommandExecuteResponseStructHappyPath)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0101;
        cmd.commandId = 0x0024; // GetCredentialStatusResponse
        cmd.name = "getCredentialStatus";

        // Script that extracts fields from struct response
        // TlvToJson uses context tag numbers as keys (e.g., "0", "1")
        std::string mapperScript = R"(
            var input = SbmdUtils.Tlv.decode(sbmdCommandResponseArgs.tlvBase64);
            return {output: 'exists:' + input['0'] + ',index:' + input['1']};
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

        std::string outValue;
        ASSERT_TRUE(script->MapCommandExecuteResponse(cmd, reader, outValue));
        EXPECT_EQ(outValue, "exists:true,index:42");
    }

    // Test: MapCommandExecuteResponse verifies sbmdCommandResponseArgs contains deviceUuid
    TEST_F(QuickJsScriptTest, MapCommandExecuteResponseHasDeviceUuid)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0001;
        cmd.name = "on";

        // Script that returns the deviceUuid
        std::string mapperScript = "return {output: sbmdCommandResponseArgs.deviceUuid};";

        ASSERT_TRUE(script->AddCommandExecuteResponseMapper(cmd, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        std::string outValue;
        ASSERT_TRUE(script->MapCommandExecuteResponse(cmd, reader, outValue));
        EXPECT_EQ(outValue, deviceId);
    }

    // Test: MapCommandExecuteResponse verifies sbmdCommandResponseArgs contains clusterId
    TEST_F(QuickJsScriptTest, MapCommandExecuteResponseHasClusterId)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0008;
        cmd.commandId = 0x0001;
        cmd.name = "moveToLevel";

        // Script that returns the clusterId
        std::string mapperScript = "return {output: sbmdCommandResponseArgs.clusterId.toString()};";

        ASSERT_TRUE(script->AddCommandExecuteResponseMapper(cmd, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        std::string outValue;
        ASSERT_TRUE(script->MapCommandExecuteResponse(cmd, reader, outValue));
        EXPECT_EQ(outValue, "8"); // 0x0008 = 8
    }

    // Test: MapCommandExecuteResponse verifies sbmdCommandResponseArgs contains commandId
    TEST_F(QuickJsScriptTest, MapCommandExecuteResponseHasCommandId)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0005;
        cmd.name = "testCmd";

        // Script that returns the commandId
        std::string mapperScript = "return {output: sbmdCommandResponseArgs.commandId.toString()};";

        ASSERT_TRUE(script->AddCommandExecuteResponseMapper(cmd, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        std::string outValue;
        ASSERT_TRUE(script->MapCommandExecuteResponse(cmd, reader, outValue));
        EXPECT_EQ(outValue, "5"); // 0x0005 = 5
    }

    // Test: MapCommandExecuteResponse verifies sbmdCommandResponseArgs contains commandName
    TEST_F(QuickJsScriptTest, MapCommandExecuteResponseHasCommandName)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0001;
        cmd.name = "myTestCommand";

        // Script that returns the commandName
        std::string mapperScript = "return {output: sbmdCommandResponseArgs.commandName};";

        ASSERT_TRUE(script->AddCommandExecuteResponseMapper(cmd, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        std::string outValue;
        ASSERT_TRUE(script->MapCommandExecuteResponse(cmd, reader, outValue));
        EXPECT_EQ(outValue, "myTestCommand");
    }

    // Test: MapCommandExecuteResponse with endpointId
    TEST_F(QuickJsScriptTest, MapCommandExecuteResponseWithEndpointId)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0001;
        cmd.name = "on";
        cmd.resourceEndpointId = "ep3";

        // Script that returns the endpointId
        std::string mapperScript = "return {output: sbmdCommandResponseArgs.endpointId};";

        ASSERT_TRUE(script->AddCommandExecuteResponseMapper(cmd, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        std::string outValue;
        ASSERT_TRUE(script->MapCommandExecuteResponse(cmd, reader, outValue));
        EXPECT_EQ(outValue, "ep3");
    }

    // Test: MapCommandExecuteResponse fails when script doesn't return output field
    TEST_F(QuickJsScriptTest, MapCommandExecuteResponseMissingOutputField)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0001;
        cmd.name = "on";

        // Script that returns wrong format (missing output field)
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

        std::string outValue;
        EXPECT_FALSE(script->MapCommandExecuteResponse(cmd, reader, outValue));
    }

    // Test: MapCommandExecuteResponse fails with script syntax error
    TEST_F(QuickJsScriptTest, MapCommandExecuteResponseScriptSyntaxError)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0001;
        cmd.name = "on";

        // Script with syntax error
        std::string mapperScript = "return {output: this is bad syntax";

        ASSERT_TRUE(script->AddCommandExecuteResponseMapper(cmd, mapperScript));

        uint8_t tlvBuffer[32];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuffer, sizeof(tlvBuffer));
        writer.PutBoolean(chip::TLV::AnonymousTag(), true);
        writer.Finalize();

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, writer.GetLengthWritten());
        reader.Next();

        std::string outValue;
        EXPECT_FALSE(script->MapCommandExecuteResponse(cmd, reader, outValue));
    }

    // Test: MapCommandExecuteResponse fails with runtime exception
    TEST_F(QuickJsScriptTest, MapCommandExecuteResponseRuntimeException)
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

        std::string outValue;
        EXPECT_FALSE(script->MapCommandExecuteResponse(cmd, reader, outValue));
    }

    // Test: MapCommandExecuteResponse with string TLV response
    TEST_F(QuickJsScriptTest, MapCommandExecuteResponseStringValue)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0050;
        cmd.commandId = 0x0001;
        cmd.name = "getName";

        // Script that processes a string response
        std::string mapperScript = R"(
            var val = SbmdUtils.Tlv.decode(sbmdCommandResponseArgs.tlvBase64);
            return {output: 'Name: ' + val};
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

        std::string outValue;
        ASSERT_TRUE(script->MapCommandExecuteResponse(cmd, reader, outValue));
        EXPECT_EQ(outValue, "Name: TestDevice");
    }

    // Test: Multiple command response mappers can coexist
    TEST_F(QuickJsScriptTest, MultipleCommandResponseMappers)
    {
        SbmdCommand cmd1;
        cmd1.clusterId = 0x0006;
        cmd1.commandId = 0x0001;
        cmd1.name = "on";

        SbmdCommand cmd2;
        cmd2.clusterId = 0x0008;
        cmd2.commandId = 0x0000;
        cmd2.name = "moveToLevel";

        std::string script1 = "return {output: 'response1'};";
        std::string script2 = "return {output: 'response2'};";

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

        std::string outValue1;
        ASSERT_TRUE(script->MapCommandExecuteResponse(cmd1, reader1, outValue1));
        EXPECT_EQ(outValue1, "response1");

        // Test cmd2
        uint8_t tlvBuffer2[32];
        chip::TLV::TLVWriter writer2;
        writer2.Init(tlvBuffer2, sizeof(tlvBuffer2));
        writer2.Put(chip::TLV::AnonymousTag(), static_cast<uint8_t>(100));
        writer2.Finalize();

        chip::TLV::TLVReader reader2;
        reader2.Init(tlvBuffer2, writer2.GetLengthWritten());
        reader2.Next();

        std::string outValue2;
        ASSERT_TRUE(script->MapCommandExecuteResponse(cmd2, reader2, outValue2));
        EXPECT_EQ(outValue2, "response2");
    }

} // namespace
