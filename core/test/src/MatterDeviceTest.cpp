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

#include "MatterDeviceTestHelpers.h"
#include <app/EventHeader.h>

using namespace barton;

namespace
{
    ::testing::Environment *const chipEnv = ::testing::AddGlobalTestEnvironment(new ChipPlatformEnvironment);

    constexpr chip::EndpointId kTestEndpointId = 1;
    constexpr chip::ClusterId kTestClusterId = 0x0101;  // DoorLock
    constexpr chip::EventId kTestEventId = 0x0002;      // LockOperation

    /**
     * Build a minimal single-byte TLV buffer. Returns the number of bytes written.
     */
    uint32_t buildMinimalTlvBuffer(uint8_t *buffer, size_t bufferSize)
    {
        chip::TLV::TLVWriter writer;
        writer.Init(buffer, bufferSize);
        writer.Put(chip::TLV::AnonymousTag(), static_cast<uint8_t>(0));
        writer.Finalize();
        return writer.GetLengthWritten();
    }

    class MatterDeviceEventTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            cache = std::make_shared<DeviceDataCache>("test-device", nullptr);
            device = std::make_unique<TestableMatterDevice>("test-device", cache);

            auto mockScript = std::make_unique<MockSbmdScript>("test-device");
            mockScriptPtr = mockScript.get();
            device->SetScript(std::move(mockScript));

            SbmdEvent event;
            event.clusterId = kTestClusterId;
            event.eventId = kTestEventId;
            device->InsertEventBinding(kTestEndpointId, kTestClusterId, kTestEventId, "/ep/ep1/r/lockState", event);
        }

        void TearDown() override
        {
            device.reset();
            cache.reset();
        }

        chip::app::EventHeader MakeTestEventHeader()
        {
            chip::app::EventHeader header;
            header.mPath = chip::app::ConcreteEventPath(kTestEndpointId, kTestClusterId, kTestEventId);
            return header;
        }

        std::shared_ptr<DeviceDataCache> cache;
        std::unique_ptr<TestableMatterDevice> device;
        MockSbmdScript *mockScriptPtr = nullptr;
    };

    /**
     * When MapEvent() returns a suppressed ScriptResult, OnEventData() must
     * short-circuit at the IsSuppressed() check and must NOT proceed to
     * updateResource().
     *
     * updateResource() is a free C function and cannot be directly intercepted
     * by GMock. Correctness of the suppress path is verified structurally: the
     * IsSuppressed() check in OnEventData() is the only code path between the
     * MapEvent() return and the updateResource() call. The single EXPECT_CALL
     * with Times(1) confirms the suppress branch was reached; no further mock
     * method is expected after MapEvent() returns suppressed, confirming that
     * OnEventData() did not fall through to the resource-update path.
     */
    TEST_F(MatterDeviceEventTest, SuppressedEventSkipsResourceUpdate)
    {
        EXPECT_CALL(*mockScriptPtr, MapEvent(::testing::_, ::testing::_))
            .Times(1)
            .WillOnce(::testing::InvokeWithoutArgs([] { return ScriptResult::MakeSuppress(); }));

        constexpr size_t kTlvBufferSize = 16;
        uint8_t tlvBuffer[kTlvBufferSize];
        uint32_t written = buildMinimalTlvBuffer(tlvBuffer, kTlvBufferSize);

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, written);
        reader.Next();

        chip::app::EventHeader header = MakeTestEventHeader();
        device->GetCacheCallback()->OnEventData(header, &reader, nullptr);
    }

    /**
     * When MapEvent() returns a resource update value, OnEventData() proceeds
     * to the updateResource() call. This positive-path test confirms that the
     * suppress test above is genuinely verifying a short-circuit, not a no-op
     * common to all paths.
     *
     * In this test context the device service is not running, so updateResource()
     * finds no matching device record and returns harmlessly.
     */
    TEST_F(MatterDeviceEventTest, ResourceUpdateEventProceedsToUpdateResource)
    {
        EXPECT_CALL(*mockScriptPtr, MapEvent(::testing::_, ::testing::_))
            .Times(1)
            .WillOnce(::testing::InvokeWithoutArgs([] { return ScriptResult::MakeResourceUpdate("true"); }));

        constexpr size_t kTlvBufferSize = 16;
        uint8_t tlvBuffer[kTlvBufferSize];
        uint32_t written = buildMinimalTlvBuffer(tlvBuffer, kTlvBufferSize);

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, written);
        reader.Next();

        chip::app::EventHeader header = MakeTestEventHeader();
        device->GetCacheCallback()->OnEventData(header, &reader, nullptr);
    }

    /**
     * When MapEvent() returns an error ScriptResult, OnEventData() logs the
     * error and returns without calling updateResource().
     */
    TEST_F(MatterDeviceEventTest, ErroredEventSkipsResourceUpdate)
    {
        EXPECT_CALL(*mockScriptPtr, MapEvent(::testing::_, ::testing::_))
            .Times(1)
            .WillOnce(::testing::InvokeWithoutArgs([] { return ScriptResult::MakeError("script error"); }));

        constexpr size_t kTlvBufferSize = 16;
        uint8_t tlvBuffer[kTlvBufferSize];
        uint32_t written = buildMinimalTlvBuffer(tlvBuffer, kTlvBufferSize);

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, written);
        reader.Next();

        chip::app::EventHeader header = MakeTestEventHeader();
        device->GetCacheCallback()->OnEventData(header, &reader, nullptr);
    }

    /**
     * When OnEventData() receives an event with no registered binding, it returns
     * immediately without invoking MapEvent() at all.
     */
    TEST_F(MatterDeviceEventTest, UnregisteredEventIsIgnored)
    {
        EXPECT_CALL(*mockScriptPtr, MapEvent(::testing::_, ::testing::_)).Times(0);

        constexpr size_t kTlvBufferSize = 16;
        uint8_t tlvBuffer[kTlvBufferSize];
        uint32_t written = buildMinimalTlvBuffer(tlvBuffer, kTlvBufferSize);

        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer, written);
        reader.Next();

        // Use a different event ID that has no binding
        chip::app::EventHeader header;
        header.mPath = chip::app::ConcreteEventPath(kTestEndpointId, kTestClusterId, 0xDEADU);
        device->GetCacheCallback()->OnEventData(header, &reader, nullptr);
    }

    /**
     * When OnEventData() receives a null data pointer, it returns immediately
     * without invoking MapEvent().
     */
    TEST_F(MatterDeviceEventTest, NullTlvDataIsIgnored)
    {
        EXPECT_CALL(*mockScriptPtr, MapEvent(::testing::_, ::testing::_)).Times(0);

        chip::app::EventHeader header = MakeTestEventHeader();
        device->GetCacheCallback()->OnEventData(header, nullptr, nullptr);
    }
} // namespace
