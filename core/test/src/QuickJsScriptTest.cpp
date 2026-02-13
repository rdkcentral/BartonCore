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

        std::string mapperScript = "return {output: sbmdReadArgs.input ? 'true' : 'false'};";

        EXPECT_TRUE(script->AddAttributeReadMapper(attr, mapperScript));
    }

    // Test: AddAttributeWriteMapper returns true for valid input
    TEST_F(QuickJsScriptTest, AddAttributeWriteMapperSuccess)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        std::string mapperScript = "return {output: sbmdWriteArgs.input === 'true'};";

        EXPECT_TRUE(script->AddAttributeWriteMapper(attr, mapperScript));
    }

    // Test: AddCommandExecuteMapper returns true for valid input
    TEST_F(QuickJsScriptTest, AddCommandExecuteMapperSuccess)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0001; // On command
        cmd.name = "on";

        std::string mapperScript = "return {output: {}};";

        EXPECT_TRUE(script->AddCommandExecuteMapper(cmd, mapperScript));
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
        // sbmdReadArgs.input contains the direct TLV value (not wrapped in {"value": ...})
        std::string mapperScript = "return {output: (sbmdReadArgs.input === true) ? 'true' : 'false'};";

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
    // Note: sbmdReadArgs.input contains the direct TLV value (unwrapped from TlvToJson's {"value": ...} wrapper)
    TEST_F(QuickJsScriptTest, MapAttributeReadBooleanFalse)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // Script needs to properly handle false - compare against true explicitly
        // sbmdReadArgs.input contains the direct value (not wrapped)
        std::string mapperScript = "return {output: (sbmdReadArgs.input === true) ? 'true' : 'false'};";

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
    // Note: sbmdReadArgs.input contains the direct TLV value (unwrapped from TlvToJson's {"value": ...} wrapper)
    TEST_F(QuickJsScriptTest, MapAttributeReadIntegerConversion)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0008; // Level cluster
        attr.attributeId = 0x0000; // CurrentLevel
        attr.name = "currentLevel";
        attr.type = "uint8";

        // Script that converts Matter uint8 to percentage string
        // sbmdReadArgs.input contains the direct integer value
        std::string mapperScript = R"(
            var level = sbmdReadArgs.input;
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

    // Test: MapAttributeWrite returns false when no mapper is registered
    TEST_F(QuickJsScriptTest, MapAttributeWriteNoMapper)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        std::string inValue = "true";
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        EXPECT_FALSE(script->MapAttributeWrite(attr, inValue, buffer, encodedLength));
    }

    // Test: MapAttributeWrite verifies sbmdWriteArgs contains input and produces valid TLV
    TEST_F(QuickJsScriptTest, MapAttributeWriteHasInput)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // Script that echoes back a boolean based on the input value
        std::string mapperScript = "return {output: sbmdWriteArgs.input === 'true'};";

        ASSERT_TRUE(script->AddAttributeWriteMapper(attr, mapperScript));

        std::string inValue = "true";
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        ASSERT_TRUE(script->MapAttributeWrite(attr, inValue, buffer, encodedLength));
        EXPECT_GT(encodedLength, 0u);

        // JsonToTlv wraps in a structure, so navigate into it
        chip::TLV::TLVReader reader;
        reader.Init(buffer.Get(), encodedLength);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);  // Position at structure
        ASSERT_EQ(reader.GetType(), chip::TLV::kTLVType_Structure);

        chip::TLV::TLVType containerType;
        ASSERT_EQ(reader.EnterContainer(containerType), CHIP_NO_ERROR);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);  // Position at first element

        bool result;
        ASSERT_EQ(reader.Get(result), CHIP_NO_ERROR);
        EXPECT_TRUE(result);
    }

    // Test: MapAttributeWrite verifies sbmdWriteArgs contains deviceUuid
    TEST_F(QuickJsScriptTest, MapAttributeWriteHasDeviceUuid)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // Script that returns true if deviceUuid matches expected value
        std::string mapperScript = R"(
            return {output: sbmdWriteArgs.deviceUuid === 'test-device-uuid'};
        )";

        ASSERT_TRUE(script->AddAttributeWriteMapper(attr, mapperScript));

        std::string inValue = "true";
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        ASSERT_TRUE(script->MapAttributeWrite(attr, inValue, buffer, encodedLength));

        // JsonToTlv wraps in a structure, so navigate into it
        chip::TLV::TLVReader reader;
        reader.Init(buffer.Get(), encodedLength);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);
        ASSERT_EQ(reader.GetType(), chip::TLV::kTLVType_Structure);

        chip::TLV::TLVType containerType;
        ASSERT_EQ(reader.EnterContainer(containerType), CHIP_NO_ERROR);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);

        bool result;
        ASSERT_EQ(reader.Get(result), CHIP_NO_ERROR);
        EXPECT_TRUE(result);
    }

    // Test: MapAttributeWrite verifies sbmdWriteArgs contains clusterId
    TEST_F(QuickJsScriptTest, MapAttributeWriteHasClusterId)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0008;
        attr.attributeId = 0x0000;
        attr.name = "currentLevel";
        attr.type = "uint8";

        // Script that returns the cluster ID as a uint8
        std::string mapperScript = "return {output: sbmdWriteArgs.clusterId};";

        ASSERT_TRUE(script->AddAttributeWriteMapper(attr, mapperScript));

        std::string inValue = "50";
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        ASSERT_TRUE(script->MapAttributeWrite(attr, inValue, buffer, encodedLength));

        // JsonToTlv wraps in a structure, so navigate into it
        chip::TLV::TLVReader reader;
        reader.Init(buffer.Get(), encodedLength);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);
        ASSERT_EQ(reader.GetType(), chip::TLV::kTLVType_Structure);

        chip::TLV::TLVType containerType;
        ASSERT_EQ(reader.EnterContainer(containerType), CHIP_NO_ERROR);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);

        uint64_t result;
        ASSERT_EQ(reader.Get(result), CHIP_NO_ERROR);
        EXPECT_EQ(result, 8u);  // clusterId = 0x0008
    }

    // Test: MapAttributeWrite verifies sbmdWriteArgs contains attributeId
    TEST_F(QuickJsScriptTest, MapAttributeWriteHasAttributeId)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0003;
        attr.name = "testAttr";
        attr.type = "uint8";

        // Script that returns the attribute ID
        std::string mapperScript = "return {output: sbmdWriteArgs.attributeId};";

        ASSERT_TRUE(script->AddAttributeWriteMapper(attr, mapperScript));

        std::string inValue = "test";
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        ASSERT_TRUE(script->MapAttributeWrite(attr, inValue, buffer, encodedLength));

        // JsonToTlv wraps in a structure, so navigate into it
        chip::TLV::TLVReader reader;
        reader.Init(buffer.Get(), encodedLength);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);
        ASSERT_EQ(reader.GetType(), chip::TLV::kTLVType_Structure);

        chip::TLV::TLVType containerType;
        ASSERT_EQ(reader.EnterContainer(containerType), CHIP_NO_ERROR);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);

        uint64_t result;
        ASSERT_EQ(reader.Get(result), CHIP_NO_ERROR);
        EXPECT_EQ(result, 3u);  // attributeId = 0x0003
    }

    // Test: MapAttributeWrite with string type
    TEST_F(QuickJsScriptTest, MapAttributeWriteStringType)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "testString";
        attr.type = "string";

        // Script that returns a string value
        std::string mapperScript = "return {output: 'hello ' + sbmdWriteArgs.input};";

        ASSERT_TRUE(script->AddAttributeWriteMapper(attr, mapperScript));

        std::string inValue = "world";
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        ASSERT_TRUE(script->MapAttributeWrite(attr, inValue, buffer, encodedLength));

        // JsonToTlv wraps in a structure, so navigate into it
        chip::TLV::TLVReader reader;
        reader.Init(buffer.Get(), encodedLength);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);
        ASSERT_EQ(reader.GetType(), chip::TLV::kTLVType_Structure);

        chip::TLV::TLVType containerType;
        ASSERT_EQ(reader.EnterContainer(containerType), CHIP_NO_ERROR);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);

        chip::CharSpan result;
        ASSERT_EQ(reader.Get(result), CHIP_NO_ERROR);
        EXPECT_EQ(std::string(result.data(), result.size()), "hello world");
    }

    // Test: MapAttributeWrite fails when script doesn't return output field
    TEST_F(QuickJsScriptTest, MapAttributeWriteMissingOutputField)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // Script that returns wrong format (missing output field)
        std::string mapperScript = "return {value: {value: true}};";

        ASSERT_TRUE(script->AddAttributeWriteMapper(attr, mapperScript));

        std::string inValue = "true";
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        EXPECT_FALSE(script->MapAttributeWrite(attr, inValue, buffer, encodedLength));
    }

    // Test: MapAttributeWrite fails with script syntax error
    TEST_F(QuickJsScriptTest, MapAttributeWriteScriptSyntaxError)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // Script with syntax error
        std::string mapperScript = "return {output: this is bad syntax";

        ASSERT_TRUE(script->AddAttributeWriteMapper(attr, mapperScript));

        std::string inValue = "true";
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        EXPECT_FALSE(script->MapAttributeWrite(attr, inValue, buffer, encodedLength));
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
        // Note: sbmdReadArgs.input contains the direct value; any {"value": ...} wrapper from TlvToJson is unwrapped before scripts run
        std::string mapperScript = R"(
            var result = 'input:' + JSON.stringify(sbmdReadArgs.input) +
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
    // MapCommandExecute tests
    //==============================================================================

    // Test: MapCommandExecute returns false when no mapper is registered
    TEST_F(QuickJsScriptTest, MapCommandExecuteNoMapper)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0001;
        cmd.name = "on";

        std::vector<std::string> argumentValues;
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        EXPECT_FALSE(script->MapCommandExecute(cmd, argumentValues, buffer, encodedLength));
    }

    // Test: MapCommandExecute with no arguments produces valid TLV
    TEST_F(QuickJsScriptTest, MapCommandExecuteNoArgs)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0001; // On command
        cmd.name = "on";
        // No args

        // Script that returns empty object (no command fields)
        std::string mapperScript = "return {output: null};";

        ASSERT_TRUE(script->AddCommandExecuteMapper(cmd, mapperScript));

        std::vector<std::string> argumentValues;
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        ASSERT_TRUE(script->MapCommandExecute(cmd, argumentValues, buffer, encodedLength));
        // Even with null output, some TLV encoding happens (empty struct)
        EXPECT_GT(encodedLength, 0u);
    }

    // Test: MapCommandExecute verifies sbmdCommandArgs contains input array
    TEST_F(QuickJsScriptTest, MapCommandExecuteHasInputArray)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0008; // Level cluster
        cmd.commandId = 0x0000; // MoveToLevel
        cmd.name = "moveToLevel";
        cmd.args.push_back({"level", "uint8"});

        // Script that returns the first input argument as uint8
        std::string mapperScript = R"(
            var level = parseInt(sbmdCommandArgs.input[0]);
            return {output: level};
        )";

        ASSERT_TRUE(script->AddCommandExecuteMapper(cmd, mapperScript));

        std::vector<std::string> argumentValues = {"128"};
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        ASSERT_TRUE(script->MapCommandExecute(cmd, argumentValues, buffer, encodedLength));
        EXPECT_GT(encodedLength, 0u);

        // Verify the TLV contains the expected value
        chip::TLV::TLVReader reader;
        reader.Init(buffer.Get(), encodedLength);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);
        ASSERT_EQ(reader.GetType(), chip::TLV::kTLVType_Structure);

        chip::TLV::TLVType containerType;
        ASSERT_EQ(reader.EnterContainer(containerType), CHIP_NO_ERROR);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);

        uint64_t result;
        ASSERT_EQ(reader.Get(result), CHIP_NO_ERROR);
        EXPECT_EQ(result, 128u);
    }

    // Test: MapCommandExecute verifies sbmdCommandArgs contains deviceUuid
    TEST_F(QuickJsScriptTest, MapCommandExecuteHasDeviceUuid)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0001;
        cmd.name = "on";
        cmd.args.push_back({"flag", "bool"});

        // Script that checks deviceUuid
        std::string mapperScript = R"(
            return {output: sbmdCommandArgs.deviceUuid === 'test-device-uuid'};
        )";

        ASSERT_TRUE(script->AddCommandExecuteMapper(cmd, mapperScript));

        std::vector<std::string> argumentValues = {"true"};
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        ASSERT_TRUE(script->MapCommandExecute(cmd, argumentValues, buffer, encodedLength));

        // Verify the TLV contains true
        chip::TLV::TLVReader reader;
        reader.Init(buffer.Get(), encodedLength);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);
        ASSERT_EQ(reader.GetType(), chip::TLV::kTLVType_Structure);

        chip::TLV::TLVType containerType;
        ASSERT_EQ(reader.EnterContainer(containerType), CHIP_NO_ERROR);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);

        bool result;
        ASSERT_EQ(reader.Get(result), CHIP_NO_ERROR);
        EXPECT_TRUE(result);
    }

    // Test: MapCommandExecute verifies sbmdCommandArgs contains clusterId and commandId
    TEST_F(QuickJsScriptTest, MapCommandExecuteHasClusterAndCommandId)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0008;
        cmd.commandId = 0x0004;
        cmd.name = "moveWithOnOff";
        cmd.args.push_back({"result", "uint8"});

        // Script that returns clusterId + commandId
        std::string mapperScript = R"(
            return {output: sbmdCommandArgs.clusterId + sbmdCommandArgs.commandId};
        )";

        ASSERT_TRUE(script->AddCommandExecuteMapper(cmd, mapperScript));

        std::vector<std::string> argumentValues;
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        ASSERT_TRUE(script->MapCommandExecute(cmd, argumentValues, buffer, encodedLength));

        // Verify the TLV contains expected value (8 + 4 = 12)
        chip::TLV::TLVReader reader;
        reader.Init(buffer.Get(), encodedLength);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);
        ASSERT_EQ(reader.GetType(), chip::TLV::kTLVType_Structure);

        chip::TLV::TLVType containerType;
        ASSERT_EQ(reader.EnterContainer(containerType), CHIP_NO_ERROR);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);

        uint64_t result;
        ASSERT_EQ(reader.Get(result), CHIP_NO_ERROR);
        EXPECT_EQ(result, 12u);
    }

    // Test: MapCommandExecute with scalar uint8 argument
    TEST_F(QuickJsScriptTest, MapCommandExecuteScalarUint8)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0008;
        cmd.commandId = 0x0000;
        cmd.name = "moveToLevel";
        cmd.args.push_back({"level", "uint8"});

        // Script that converts percentage string to Matter level (0-254)
        std::string mapperScript = R"(
            var percent = parseInt(sbmdCommandArgs.input[0]);
            var level = Math.round(percent * 254 / 100);
            return {output: level};
        )";

        ASSERT_TRUE(script->AddCommandExecuteMapper(cmd, mapperScript));

        std::vector<std::string> argumentValues = {"50"}; // 50%
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        ASSERT_TRUE(script->MapCommandExecute(cmd, argumentValues, buffer, encodedLength));

        // Navigate TLV and verify value
        chip::TLV::TLVReader reader;
        reader.Init(buffer.Get(), encodedLength);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);
        ASSERT_EQ(reader.GetType(), chip::TLV::kTLVType_Structure);

        chip::TLV::TLVType containerType;
        ASSERT_EQ(reader.EnterContainer(containerType), CHIP_NO_ERROR);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);

        uint64_t result;
        ASSERT_EQ(reader.Get(result), CHIP_NO_ERROR);
        EXPECT_EQ(result, 127u); // 50% of 254 = 127
    }

    // Test: MapCommandExecute with scalar boolean argument
    TEST_F(QuickJsScriptTest, MapCommandExecuteScalarBool)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0002;
        cmd.name = "toggle";
        cmd.args.push_back({"force", "bool"});

        // Script that converts string to boolean
        std::string mapperScript = R"(
            return {output: sbmdCommandArgs.input[0] === 'true'};
        )";

        ASSERT_TRUE(script->AddCommandExecuteMapper(cmd, mapperScript));

        std::vector<std::string> argumentValues = {"true"};
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        ASSERT_TRUE(script->MapCommandExecute(cmd, argumentValues, buffer, encodedLength));

        // Navigate TLV and verify value
        chip::TLV::TLVReader reader;
        reader.Init(buffer.Get(), encodedLength);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);
        ASSERT_EQ(reader.GetType(), chip::TLV::kTLVType_Structure);

        chip::TLV::TLVType containerType;
        ASSERT_EQ(reader.EnterContainer(containerType), CHIP_NO_ERROR);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);

        bool result;
        ASSERT_EQ(reader.Get(result), CHIP_NO_ERROR);
        EXPECT_TRUE(result);
    }

    // Test: MapCommandExecute with octet string (BYTES) argument - base64 encoding
    TEST_F(QuickJsScriptTest, MapCommandExecuteOctetString)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0101; // Door Lock
        cmd.commandId = 0x0000; // LockDoor
        cmd.name = "lockDoor";
        cmd.args.push_back({"pinCode", "octstr"});

        // Script that returns an array of bytes (will be base64 encoded by formatCommandArgsForJsonToTlv)
        // PIN code "1234" -> [0x31, 0x32, 0x33, 0x34]
        std::string mapperScript = R"(
            var pin = sbmdCommandArgs.input[0];
            var bytes = [];
            for (var i = 0; i < pin.length; i++) {
                bytes.push(pin.charCodeAt(i));
            }
            return {output: bytes};
        )";

        ASSERT_TRUE(script->AddCommandExecuteMapper(cmd, mapperScript));

        std::vector<std::string> argumentValues = {"1234"};
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        ASSERT_TRUE(script->MapCommandExecute(cmd, argumentValues, buffer, encodedLength));
        EXPECT_GT(encodedLength, 0u);

        // Navigate TLV and verify the byte string
        chip::TLV::TLVReader reader;
        reader.Init(buffer.Get(), encodedLength);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);
        ASSERT_EQ(reader.GetType(), chip::TLV::kTLVType_Structure);

        chip::TLV::TLVType containerType;
        ASSERT_EQ(reader.EnterContainer(containerType), CHIP_NO_ERROR);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);

        // Should be a byte string
        ASSERT_EQ(reader.GetType(), chip::TLV::kTLVType_ByteString);

        chip::ByteSpan result;
        ASSERT_EQ(reader.Get(result), CHIP_NO_ERROR);
        ASSERT_EQ(result.size(), 4u);
        EXPECT_EQ(result.data()[0], '1'); // 0x31
        EXPECT_EQ(result.data()[1], '2'); // 0x32
        EXPECT_EQ(result.data()[2], '3'); // 0x33
        EXPECT_EQ(result.data()[3], '4'); // 0x34
    }

    // Test: MapCommandExecute with longer byte array
    TEST_F(QuickJsScriptTest, MapCommandExecuteByteArrayLonger)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0101;
        cmd.commandId = 0x0001;
        cmd.name = "testCmd";
        cmd.args.push_back({"data", "octet_string"});

        // Script that returns a byte array directly
        std::string mapperScript = R"(
            return {output: [0x00, 0x01, 0x02, 0xFF, 0xFE, 0x80, 0x7F]};
        )";

        ASSERT_TRUE(script->AddCommandExecuteMapper(cmd, mapperScript));

        std::vector<std::string> argumentValues;
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        ASSERT_TRUE(script->MapCommandExecute(cmd, argumentValues, buffer, encodedLength));

        // Navigate TLV and verify the byte string
        chip::TLV::TLVReader reader;
        reader.Init(buffer.Get(), encodedLength);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);
        ASSERT_EQ(reader.GetType(), chip::TLV::kTLVType_Structure);

        chip::TLV::TLVType containerType;
        ASSERT_EQ(reader.EnterContainer(containerType), CHIP_NO_ERROR);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);

        ASSERT_EQ(reader.GetType(), chip::TLV::kTLVType_ByteString);

        chip::ByteSpan result;
        ASSERT_EQ(reader.Get(result), CHIP_NO_ERROR);
        ASSERT_EQ(result.size(), 7u);
        EXPECT_EQ(result.data()[0], 0x00);
        EXPECT_EQ(result.data()[1], 0x01);
        EXPECT_EQ(result.data()[2], 0x02);
        EXPECT_EQ(result.data()[3], 0xFF);
        EXPECT_EQ(result.data()[4], 0xFE);
        EXPECT_EQ(result.data()[5], 0x80);
        EXPECT_EQ(result.data()[6], 0x7F);
    }

    // Test: MapCommandExecute with string argument
    TEST_F(QuickJsScriptTest, MapCommandExecuteStringArg)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0050;
        cmd.commandId = 0x0000;
        cmd.name = "setName";
        cmd.args.push_back({"name", "string"});

        // Script that uppercases the input string
        std::string mapperScript = R"(
            return {output: sbmdCommandArgs.input[0].toUpperCase()};
        )";

        ASSERT_TRUE(script->AddCommandExecuteMapper(cmd, mapperScript));

        std::vector<std::string> argumentValues = {"hello"};
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        ASSERT_TRUE(script->MapCommandExecute(cmd, argumentValues, buffer, encodedLength));

        // Navigate TLV and verify the string
        chip::TLV::TLVReader reader;
        reader.Init(buffer.Get(), encodedLength);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);
        ASSERT_EQ(reader.GetType(), chip::TLV::kTLVType_Structure);

        chip::TLV::TLVType containerType;
        ASSERT_EQ(reader.EnterContainer(containerType), CHIP_NO_ERROR);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);

        chip::CharSpan result;
        ASSERT_EQ(reader.Get(result), CHIP_NO_ERROR);
        EXPECT_EQ(std::string(result.data(), result.size()), "HELLO");
    }

    // Test: MapCommandExecute fails when script doesn't return output field
    TEST_F(QuickJsScriptTest, MapCommandExecuteMissingOutputField)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0001;
        cmd.name = "on";
        cmd.args.push_back({"value", "uint8"});

        // Script that returns wrong format (missing output field)
        std::string mapperScript = "return {value: 42};";

        ASSERT_TRUE(script->AddCommandExecuteMapper(cmd, mapperScript));

        std::vector<std::string> argumentValues = {"1"};
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        EXPECT_FALSE(script->MapCommandExecute(cmd, argumentValues, buffer, encodedLength));
    }

    // Test: MapCommandExecute fails with script syntax error
    TEST_F(QuickJsScriptTest, MapCommandExecuteScriptSyntaxError)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0001;
        cmd.name = "on";

        // Script with syntax error
        std::string mapperScript = "return {output: this is bad syntax";

        ASSERT_TRUE(script->AddCommandExecuteMapper(cmd, mapperScript));

        std::vector<std::string> argumentValues;
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        EXPECT_FALSE(script->MapCommandExecute(cmd, argumentValues, buffer, encodedLength));
    }

    // Test: MapCommandExecute with endpointId verification
    TEST_F(QuickJsScriptTest, MapCommandExecuteWithEndpointId)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0001;
        cmd.name = "on";
        cmd.resourceEndpointId = "ep2";
        cmd.args.push_back({"result", "bool"});

        // Script that checks endpointId
        std::string mapperScript = R"(
            return {output: sbmdCommandArgs.endpointId === 'ep2'};
        )";

        ASSERT_TRUE(script->AddCommandExecuteMapper(cmd, mapperScript));

        std::vector<std::string> argumentValues;
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        ASSERT_TRUE(script->MapCommandExecute(cmd, argumentValues, buffer, encodedLength));

        // Navigate TLV and verify
        chip::TLV::TLVReader reader;
        reader.Init(buffer.Get(), encodedLength);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);
        ASSERT_EQ(reader.GetType(), chip::TLV::kTLVType_Structure);

        chip::TLV::TLVType containerType;
        ASSERT_EQ(reader.EnterContainer(containerType), CHIP_NO_ERROR);
        ASSERT_EQ(reader.Next(), CHIP_NO_ERROR);

        bool result;
        ASSERT_EQ(reader.Get(result), CHIP_NO_ERROR);
        EXPECT_TRUE(result);
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
            return {output: sbmdCommandResponseArgs.input ? 'success' : 'failure'};
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
            var state = sbmdCommandResponseArgs.input;
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
            var input = sbmdCommandResponseArgs.input;
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
            return {output: 'Name: ' + sbmdCommandResponseArgs.input};
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

    // ========================================================================
    // MapWriteCommand Tests
    // ========================================================================

    // Test: Single command write without explicitly specifying command name
    TEST_F(QuickJsScriptTest, MapWriteCommandSingleAutoSelect)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0008;
        cmd.commandId = 0x0004;
        cmd.name = "MoveToLevelWithOnOff";
        SbmdArgument arg1;
        arg1.name = "Level";
        arg1.type = "uint8";
        cmd.args.push_back(arg1);
        SbmdArgument arg2;
        arg2.name = "TransitionTime";
        arg2.type = "uint16";
        cmd.args.push_back(arg2);

        std::vector<SbmdCommand> commands = {cmd};

        // Script does not specify "command" field - should auto-select the only command
        std::string mapperScript = "return {output: {Level: 100, TransitionTime: 0}};";
        ASSERT_TRUE(script->AddCommandsWriteMapper(commands, mapperScript));

        std::string selectedCommandName;
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        ASSERT_TRUE(script->MapWriteCommand(commands, "100", selectedCommandName, buffer, encodedLength));
        EXPECT_EQ(selectedCommandName, "MoveToLevelWithOnOff");
        EXPECT_GT(encodedLength, 0u);
    }

    // Test: Single command write with explicitly specifying command name (also valid)
    TEST_F(QuickJsScriptTest, MapWriteCommandSingleExplicit)
    {
        SbmdCommand cmd;
        cmd.clusterId = 0x0008;
        cmd.commandId = 0x0004;
        cmd.name = "MoveToLevelWithOnOff";
        SbmdArgument arg1;
        arg1.name = "Level";
        arg1.type = "uint8";
        cmd.args.push_back(arg1);

        std::vector<SbmdCommand> commands = {cmd};

        // Script explicitly specifies "command" field even though there's only one
        std::string mapperScript = "return {output: {Level: 50}, command: 'MoveToLevelWithOnOff'};";
        ASSERT_TRUE(script->AddCommandsWriteMapper(commands, mapperScript));

        std::string selectedCommandName;
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        ASSERT_TRUE(script->MapWriteCommand(commands, "50", selectedCommandName, buffer, encodedLength));
        EXPECT_EQ(selectedCommandName, "MoveToLevelWithOnOff");
        EXPECT_GT(encodedLength, 0u);
    }

    // Test: Multiple commands write with specifying command name (required)
    TEST_F(QuickJsScriptTest, MapWriteCommandMultipleSelectOn)
    {
        SbmdCommand cmdOff;
        cmdOff.clusterId = 0x0006;
        cmdOff.commandId = 0x0000;
        cmdOff.name = "Off";

        SbmdCommand cmdOn;
        cmdOn.clusterId = 0x0006;
        cmdOn.commandId = 0x0001;
        cmdOn.name = "On";

        std::vector<SbmdCommand> commands = {cmdOff, cmdOn};

        // Script selects "On" command based on input
        std::string mapperScript = R"(
            return {
                output: null,
                command: (sbmdWriteArgs.input === 'true') ? 'On' : 'Off'
            };
        )";
        ASSERT_TRUE(script->AddCommandsWriteMapper(commands, mapperScript));

        std::string selectedCommandName;
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        // Test selecting "On"
        ASSERT_TRUE(script->MapWriteCommand(commands, "true", selectedCommandName, buffer, encodedLength));
        EXPECT_EQ(selectedCommandName, "On");
    }

    // Test: Multiple commands write selecting different command
    TEST_F(QuickJsScriptTest, MapWriteCommandMultipleSelectOff)
    {
        SbmdCommand cmdOff;
        cmdOff.clusterId = 0x0006;
        cmdOff.commandId = 0x0000;
        cmdOff.name = "Off";

        SbmdCommand cmdOn;
        cmdOn.clusterId = 0x0006;
        cmdOn.commandId = 0x0001;
        cmdOn.name = "On";

        std::vector<SbmdCommand> commands = {cmdOff, cmdOn};

        std::string mapperScript = R"(
            return {
                output: null,
                command: (sbmdWriteArgs.input === 'true') ? 'On' : 'Off'
            };
        )";
        ASSERT_TRUE(script->AddCommandsWriteMapper(commands, mapperScript));

        std::string selectedCommandName;
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        // Test selecting "Off"
        ASSERT_TRUE(script->MapWriteCommand(commands, "false", selectedCommandName, buffer, encodedLength));
        EXPECT_EQ(selectedCommandName, "Off");
    }

    // Test: Multiple commands write without specifying command name (should fail)
    TEST_F(QuickJsScriptTest, MapWriteCommandMultipleMissingCommandFails)
    {
        SbmdCommand cmdOff;
        cmdOff.clusterId = 0x0006;
        cmdOff.commandId = 0x0000;
        cmdOff.name = "Off";

        SbmdCommand cmdOn;
        cmdOn.clusterId = 0x0006;
        cmdOn.commandId = 0x0001;
        cmdOn.name = "On";

        std::vector<SbmdCommand> commands = {cmdOff, cmdOn};

        // Script does NOT specify "command" field - should fail for multiple commands
        std::string mapperScript = "return {output: null};";
        ASSERT_TRUE(script->AddCommandsWriteMapper(commands, mapperScript));

        std::string selectedCommandName;
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        // Should fail because "command" field is missing
        EXPECT_FALSE(script->MapWriteCommand(commands, "true", selectedCommandName, buffer, encodedLength));
    }

    // Test: Multiple commands write with unknown command name (should fail)
    TEST_F(QuickJsScriptTest, MapWriteCommandMultipleUnknownCommandFails)
    {
        SbmdCommand cmdOff;
        cmdOff.clusterId = 0x0006;
        cmdOff.commandId = 0x0000;
        cmdOff.name = "Off";

        SbmdCommand cmdOn;
        cmdOn.clusterId = 0x0006;
        cmdOn.commandId = 0x0001;
        cmdOn.name = "On";

        std::vector<SbmdCommand> commands = {cmdOff, cmdOn};

        // Script specifies a command name that doesn't exist
        std::string mapperScript = "return {output: null, command: 'Toggle'};";
        ASSERT_TRUE(script->AddCommandsWriteMapper(commands, mapperScript));

        std::string selectedCommandName;
        chip::Platform::ScopedMemoryBuffer<uint8_t> buffer;
        size_t encodedLength = 0;

        // Should fail because "Toggle" is not in the available commands
        EXPECT_FALSE(script->MapWriteCommand(commands, "true", selectedCommandName, buffer, encodedLength));
    }

} // namespace
