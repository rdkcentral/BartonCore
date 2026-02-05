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

#include "matter/sbmd/script/QuickJsScript.h"
#include "matter/sbmd/SbmdSpec.h"
#include <gtest/gtest.h>
#include <memory>
#include <lib/core/TLVReader.h>
#include <lib/core/TLVWriter.h>
#include <lib/core/TLVTypes.h>
#include <lib/support/CHIPMem.h>

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
            script = std::make_unique<QuickJsScript>(deviceId);
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
    // Note: TlvToJson wraps scalar values in {"value": ...} format
    TEST_F(QuickJsScriptTest, MapAttributeReadBooleanPassthrough)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // Script that converts Matter boolean to Barton string
        // Note: input is now the direct value, not wrapped in {"value": ...}
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
    // Note: TlvToJson wraps scalar values in {"value": ...} format
    TEST_F(QuickJsScriptTest, MapAttributeReadBooleanFalse)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        attr.name = "onOff";
        attr.type = "bool";

        // Script needs to properly handle false - compare against undefined, not truthy
        // Note: TlvToJson produces {"value": bool} for boolean values
        std::string mapperScript = "return {output: (sbmdReadArgs.input.value === true) ? 'true' : 'false'};";

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
    // Note: TlvToJson wraps scalar values in {"value": ...} format
    TEST_F(QuickJsScriptTest, MapAttributeReadIntegerConversion)
    {
        SbmdAttribute attr;
        attr.clusterId = 0x0008; // Level cluster
        attr.attributeId = 0x0000; // CurrentLevel
        attr.name = "currentLevel";
        attr.type = "uint8";

        // Script that converts Matter uint8 to percentage string
        // Note: input is now the direct value, not wrapped in {"value": ...}
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
        // input is now the direct value, not wrapped in {"value": ...}
        EXPECT_NE(outValue.find("input:true"), std::string::npos);
        EXPECT_NE(outValue.find("device:test-device-uuid"), std::string::npos);
    }

} // namespace
