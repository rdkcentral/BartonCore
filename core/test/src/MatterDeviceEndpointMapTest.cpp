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

#include "deviceDrivers/matter/MatterDevice.h"
#include "subsystems/matter/DeviceDataCache.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

using namespace barton;

namespace barton
{
    /**
     * Testable subclass of MatterDevice that exposes the endpoint map for direct
     * manipulation and verification in unit tests.
     */
    class TestableMatterDevice : public MatterDevice
    {
    public:
        using MatterDevice::MatterDevice;

        // Expose private members for test manipulation and verification
        std::map<uint32_t, chip::EndpointId> &GetSbmdEndpointMap() { return sbmdEndpointMap; }
        const std::map<std::string, ResourceBinding> &GetReadBindings() { return resourceReadBindings; }
        const std::map<std::string, ResourceBinding> &GetWriteBindings() { return resourceWriteBindings; }
        const std::map<std::string, ResourceBinding> &GetExecuteBindings() { return resourceExecuteBindings; }

        using MatterDevice::ResolveEndpointMap;
        using MatterDevice::GetEndpointForSbmdIndex;
    };
} // namespace barton

namespace
{
    class MatterDeviceEndpointMapTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            cache = std::make_shared<DeviceDataCache>("test-device", nullptr);
            device = std::make_unique<TestableMatterDevice>("test-device", cache);
        }

        void TearDown() override
        {
            device.reset();
            cache.reset();
        }

        std::shared_ptr<DeviceDataCache> cache;
        std::unique_ptr<TestableMatterDevice> device;
    };

    // ================================================================
    // Tests for ResolveEndpointMap()
    // ================================================================

    TEST_F(MatterDeviceEndpointMapTest, ResolveEndpointMapNoData)
    {
        // Cache has no ClusterStateCache, so GetEndpointIds() returns empty
        EXPECT_FALSE(device->ResolveEndpointMap({0x0100}));
    }

    TEST_F(MatterDeviceEndpointMapTest, ResolveEndpointMapEmptyDeviceTypes)
    {
        EXPECT_FALSE(device->ResolveEndpointMap({}));
    }

    TEST(MatterDeviceEndpointMapNoCacheTest, ResolveEndpointMapNullCache)
    {
        TestableMatterDevice device("test-device", nullptr);
        EXPECT_FALSE(device.ResolveEndpointMap({0x0100}));
    }

    TEST_F(MatterDeviceEndpointMapTest, ResolveEndpointMapNoDataMultipleTypes)
    {
        EXPECT_FALSE(device->ResolveEndpointMap({0x0100, 0x000F}));
    }

    // ================================================================
    // Tests for GetEndpointForSbmdIndex()
    // ================================================================

    TEST_F(MatterDeviceEndpointMapTest, GetEndpointForSbmdIndexValid)
    {
        device->GetSbmdEndpointMap()[0] = 1;
        device->GetSbmdEndpointMap()[1] = 3;

        chip::EndpointId endpointId;

        EXPECT_TRUE(device->GetEndpointForSbmdIndex(0, endpointId));
        EXPECT_EQ(endpointId, 1);

        EXPECT_TRUE(device->GetEndpointForSbmdIndex(1, endpointId));
        EXPECT_EQ(endpointId, 3);
    }

    TEST_F(MatterDeviceEndpointMapTest, GetEndpointForSbmdIndexOutOfRange)
    {
        device->GetSbmdEndpointMap()[0] = 1;

        chip::EndpointId endpointId;
        EXPECT_FALSE(device->GetEndpointForSbmdIndex(1, endpointId));
        EXPECT_FALSE(device->GetEndpointForSbmdIndex(99, endpointId));
    }

    TEST_F(MatterDeviceEndpointMapTest, GetEndpointForSbmdIndexEmptyMap)
    {
        chip::EndpointId endpointId;
        EXPECT_FALSE(device->GetEndpointForSbmdIndex(0, endpointId));
    }

    // ================================================================
    // Tests for multiple SBMD endpoints
    // ================================================================

    TEST_F(MatterDeviceEndpointMapTest, FirstSbmdEndpointMapsCorrectly)
    {
        device->GetSbmdEndpointMap()[0] = 1;
        device->GetSbmdEndpointMap()[1] = 3;

        chip::EndpointId endpointId;
        EXPECT_TRUE(device->GetEndpointForSbmdIndex(0, endpointId));
        EXPECT_EQ(endpointId, 1);
    }

    TEST_F(MatterDeviceEndpointMapTest, SecondSbmdEndpointMapsCorrectly)
    {
        device->GetSbmdEndpointMap()[0] = 1;
        device->GetSbmdEndpointMap()[1] = 3;

        chip::EndpointId endpointId;
        EXPECT_TRUE(device->GetEndpointForSbmdIndex(1, endpointId));
        EXPECT_EQ(endpointId, 3);
    }

    // ================================================================
    // Tests for BindResourceReadInfo
    // ================================================================

    // Endpoint-level read binding: uses endpoint map
    TEST_F(MatterDeviceEndpointMapTest, BindReadInfoEndpointLevelUsesMap)
    {
        device->GetSbmdEndpointMap()[0] = 1;
        device->GetSbmdEndpointMap()[1] = 3;

        SbmdMapper mapper;
        mapper.hasRead = true;
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        mapper.readAttribute = attr;

        // SBMD index 0 → Matter endpoint 1
        EXPECT_TRUE(device->BindResourceReadInfo("/test/read0", mapper, 0));
        auto &bindings = device->GetReadBindings();
        ASSERT_NE(bindings.find("/test/read0"), bindings.end());
        EXPECT_EQ(bindings.at("/test/read0").attributePath.mEndpointId, 1);

        // SBMD index 1 → Matter endpoint 3
        EXPECT_TRUE(device->BindResourceReadInfo("/test/read1", mapper, 1));
        EXPECT_EQ(bindings.at("/test/read1").attributePath.mEndpointId, 3);
    }

    // Endpoint-level read binding: invalid index fails
    TEST_F(MatterDeviceEndpointMapTest, BindReadInfoEndpointLevelBadIndex)
    {
        device->GetSbmdEndpointMap()[0] = 1;

        SbmdMapper mapper;
        mapper.hasRead = true;
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        mapper.readAttribute = attr;

        EXPECT_FALSE(device->BindResourceReadInfo("/test/read-bad", mapper, 5));
    }

    // Device-level read binding (nullopt): falls back to GetEndpointForCluster.
    // With no real cache data, cluster lookup fails → bind fails.
    TEST_F(MatterDeviceEndpointMapTest, BindReadInfoDeviceLevelFallsBackToCluster)
    {
        SbmdMapper mapper;
        mapper.hasRead = true;
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        mapper.readAttribute = attr;

        // No endpoint map entry, no cache data → GetEndpointForCluster fails
        EXPECT_FALSE(device->BindResourceReadInfo("/test/read-dev", mapper, std::nullopt));
    }

    // Device-level read binding: command path also falls back to cluster lookup
    TEST_F(MatterDeviceEndpointMapTest, BindReadInfoDeviceLevelCommandFallsBackToCluster)
    {
        SbmdMapper mapper;
        mapper.hasRead = true;
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0000;
        cmd.name = "test-cmd";
        mapper.readCommand = cmd;

        EXPECT_FALSE(device->BindResourceReadInfo("/test/read-cmd-dev", mapper, std::nullopt));
    }

    // Endpoint-level read binding with command: uses endpoint map
    TEST_F(MatterDeviceEndpointMapTest, BindReadInfoEndpointLevelCommand)
    {
        device->GetSbmdEndpointMap()[0] = 2;

        SbmdMapper mapper;
        mapper.hasRead = true;
        SbmdCommand cmd;
        cmd.clusterId = 0x0006;
        cmd.commandId = 0x0000;
        cmd.name = "test-cmd";
        mapper.readCommand = cmd;

        EXPECT_TRUE(device->BindResourceReadInfo("/test/read-cmd0", mapper, 0));
    }

    // ================================================================
    // Tests for BindResourceEventInfo
    // ================================================================

    // Endpoint-level event binding: uses endpoint map
    TEST_F(MatterDeviceEndpointMapTest, BindEventInfoEndpointLevelUsesMap)
    {
        device->GetSbmdEndpointMap()[0] = 1;

        SbmdEvent event;
        event.clusterId = 0x0006;
        event.eventId = 0x0000;

        EXPECT_TRUE(device->BindResourceEventInfo("/test/event0", event, 0));
    }

    // Endpoint-level event binding: invalid index fails
    TEST_F(MatterDeviceEndpointMapTest, BindEventInfoEndpointLevelBadIndex)
    {
        device->GetSbmdEndpointMap()[0] = 1;

        SbmdEvent event;
        event.clusterId = 0x0006;
        event.eventId = 0x0000;

        EXPECT_FALSE(device->BindResourceEventInfo("/test/event-bad", event, 5));
    }

    // Device-level event binding (nullopt): falls back to GetEndpointForCluster.
    // With no cache data, cluster lookup fails → bind fails.
    TEST_F(MatterDeviceEndpointMapTest, BindEventInfoDeviceLevelFallsBackToCluster)
    {
        SbmdEvent event;
        event.clusterId = 0x0006;
        event.eventId = 0x0000;

        EXPECT_FALSE(device->BindResourceEventInfo("/test/event-dev", event, std::nullopt));
    }

    // ================================================================
    // Tests for BindWriteInfo - endpoint resolution at bind time
    // ================================================================

    // Endpoint-level write binding: resolves endpoint at bind time
    TEST_F(MatterDeviceEndpointMapTest, BindWriteInfoEndpointLevelResolvesAtBind)
    {
        device->GetSbmdEndpointMap()[0] = 1;
        device->GetSbmdEndpointMap()[1] = 3;

        EXPECT_TRUE(device->BindWriteInfo("/test/write0", "key0", "ep1", "res1", 0));
        auto &bindings = device->GetWriteBindings();
        ASSERT_NE(bindings.find("/test/write0"), bindings.end());
        ASSERT_TRUE(bindings.at("/test/write0").resolvedEndpointId.has_value());
        EXPECT_EQ(bindings.at("/test/write0").resolvedEndpointId.value(), 1);

        EXPECT_TRUE(device->BindWriteInfo("/test/write1", "key1", "ep1", "res1", 1));
        ASSERT_TRUE(bindings.at("/test/write1").resolvedEndpointId.has_value());
        EXPECT_EQ(bindings.at("/test/write1").resolvedEndpointId.value(), 3);
    }

    // Endpoint-level write binding with invalid index: succeeds but no resolvedEndpointId
    TEST_F(MatterDeviceEndpointMapTest, BindWriteInfoEndpointLevelBadIndex)
    {
        device->GetSbmdEndpointMap()[0] = 1;

        EXPECT_TRUE(device->BindWriteInfo("/test/write-bad", "key", "ep1", "res1", 5));
        auto &bindings = device->GetWriteBindings();
        EXPECT_FALSE(bindings.at("/test/write-bad").resolvedEndpointId.has_value());
    }

    // Device-level write binding (nullopt): no resolvedEndpointId (script provides at runtime)
    TEST_F(MatterDeviceEndpointMapTest, BindWriteInfoDeviceLevelNoResolvedEndpoint)
    {
        device->GetSbmdEndpointMap()[0] = 1;

        EXPECT_TRUE(device->BindWriteInfo("/test/write-dev", "key", "", "res1", std::nullopt));
        auto &bindings = device->GetWriteBindings();
        EXPECT_FALSE(bindings.at("/test/write-dev").resolvedEndpointId.has_value());
    }

    // ================================================================
    // Tests for BindExecuteInfo - endpoint resolution at bind time
    // ================================================================

    // Endpoint-level execute binding: resolves endpoint at bind time
    TEST_F(MatterDeviceEndpointMapTest, BindExecuteInfoEndpointLevelResolvesAtBind)
    {
        device->GetSbmdEndpointMap()[0] = 1;
        device->GetSbmdEndpointMap()[1] = 3;

        EXPECT_TRUE(device->BindExecuteInfo("/test/exec0", "key0", "ep1", "res1", 0));
        auto &bindings = device->GetExecuteBindings();
        ASSERT_NE(bindings.find("/test/exec0"), bindings.end());
        ASSERT_TRUE(bindings.at("/test/exec0").resolvedEndpointId.has_value());
        EXPECT_EQ(bindings.at("/test/exec0").resolvedEndpointId.value(), 1);

        EXPECT_TRUE(device->BindExecuteInfo("/test/exec1", "key1", "ep1", "res1", 1));
        ASSERT_TRUE(bindings.at("/test/exec1").resolvedEndpointId.has_value());
        EXPECT_EQ(bindings.at("/test/exec1").resolvedEndpointId.value(), 3);
    }

    // Endpoint-level execute binding with invalid index: binding fails and no binding is created
    TEST_F(MatterDeviceEndpointMapTest, BindExecuteInfoEndpointLevelBadIndexFails)
    {
        device->GetSbmdEndpointMap()[0] = 1;

        EXPECT_FALSE(device->BindExecuteInfo("/test/exec-bad", "key", "ep1", "res1", 5));
        auto &bindings = device->GetExecuteBindings();
        EXPECT_EQ(bindings.find("/test/exec-bad"), bindings.end());
    }

    // Device-level execute binding (nullopt): no resolvedEndpointId (script provides at runtime)
    TEST_F(MatterDeviceEndpointMapTest, BindExecuteInfoDeviceLevelNoResolvedEndpoint)
    {
        device->GetSbmdEndpointMap()[0] = 1;

        EXPECT_TRUE(device->BindExecuteInfo("/test/exec-dev", "key", "", "res1", std::nullopt));
        auto &bindings = device->GetExecuteBindings();
        EXPECT_FALSE(bindings.at("/test/exec-dev").resolvedEndpointId.has_value());
    }

    // ================================================================
    // Tests for null URI edge cases
    // ================================================================

    TEST_F(MatterDeviceEndpointMapTest, BindReadInfoNullUriFails)
    {
        SbmdMapper mapper;
        mapper.hasRead = true;
        SbmdAttribute attr;
        attr.clusterId = 0x0006;
        attr.attributeId = 0x0000;
        mapper.readAttribute = attr;

        EXPECT_FALSE(device->BindResourceReadInfo(nullptr, mapper, 0));
    }

    TEST_F(MatterDeviceEndpointMapTest, BindWriteInfoNullUriFails)
    {
        EXPECT_FALSE(device->BindWriteInfo(nullptr, "key", "ep1", "res1", 0));
    }

    TEST_F(MatterDeviceEndpointMapTest, BindExecuteInfoNullUriFails)
    {
        EXPECT_FALSE(device->BindExecuteInfo(nullptr, "key", "ep1", "res1", 0));
    }

    TEST_F(MatterDeviceEndpointMapTest, BindEventInfoNullUriFails)
    {
        SbmdEvent event;
        event.clusterId = 0x0006;
        event.eventId = 0x0000;

        EXPECT_FALSE(device->BindResourceEventInfo(nullptr, event, 0));
    }
} // namespace
