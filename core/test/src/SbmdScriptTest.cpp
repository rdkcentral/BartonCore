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
            "var val = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64); return {output: val ? 'true' : 'false'};";

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

        std::string outValue;
        EXPECT_FALSE(script->MapAttributeRead(attr, reader, outValue));
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
    TEST_F(SbmdScriptTest, MapAttributeReadBooleanFalse)
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
    TEST_F(SbmdScriptTest, MapAttributeReadHasDeviceUuid)
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
    TEST_F(SbmdScriptTest, MapAttributeReadHasClusterId)
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
    TEST_F(SbmdScriptTest, MapAttributeReadHasAttributeId)
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
    TEST_F(SbmdScriptTest, MapAttributeReadHasAttributeName)
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
    TEST_F(SbmdScriptTest, MapAttributeReadHasAttributeType)
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
    TEST_F(SbmdScriptTest, MapAttributeReadMissingOutputField)
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
    TEST_F(SbmdScriptTest, MapAttributeReadScriptSyntaxError)
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
    TEST_F(SbmdScriptTest, MapAttributeReadWithEndpointId)
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

        std::string outValue;
        EXPECT_FALSE(script->MapCommandExecuteResponse(cmd, reader, outValue));
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
    TEST_F(SbmdScriptTest, MapCommandExecuteResponseHasDeviceUuid)
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
    TEST_F(SbmdScriptTest, MapCommandExecuteResponseHasClusterId)
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
    TEST_F(SbmdScriptTest, MapCommandExecuteResponseHasCommandId)
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
    TEST_F(SbmdScriptTest, MapCommandExecuteResponseHasCommandName)
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
    TEST_F(SbmdScriptTest, MapCommandExecuteResponseWithEndpointId)
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
    TEST_F(SbmdScriptTest, MapCommandExecuteResponseMissingOutputField)
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
    TEST_F(SbmdScriptTest, MapCommandExecuteResponseScriptSyntaxError)
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

        std::string outValue;
        EXPECT_FALSE(script->MapCommandExecuteResponse(cmd, reader, outValue));
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

        // '!!!!' is a valid-length (4-char) quartet with all-invalid Base64 characters.
        std::string mapperScript = "var val = SbmdUtils.Tlv.decode('!!!!'); return {output: 'unreachable'};";

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
            "var bytes = SbmdUtils.Base64.decode('AA!A'); return {output: bytes.length.toString()};";

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
            return { output: JSON.stringify({ value: bufs.length }) };
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

        std::string outValue;
        // The script catches the OOM error internally via try/catch, so it
        // completes successfully.  The important thing is the engine does not
        // crash.  Zero buffers should have been allocated since each request
        // (256 KB) exceeds the remaining arena.
        bool result = script->MapAttributeRead(attr, reader, outValue);
        EXPECT_TRUE(result);
        EXPECT_EQ(outValue, R"({"value":0})");
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
            return { output: JSON.stringify({value: recurse(0)}) };
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

        std::string outValue;
        // Should fail gracefully with stack overflow, not crash
        bool result = script->MapAttributeRead(attr, reader, outValue);
        EXPECT_FALSE(result);
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

#endif // BCORE_USE_MQUICKJS

} // namespace
