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
#include "deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.h"
#include "subsystems/matter/DeviceDataCache.h"
#include <app-common/zap-generated/cluster-objects.h>
#include <app/data-model/Decode.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <lib/core/TLV.h>
#include <lib/support/CHIPMem.h>
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

        using MatterDevice::GetEndpointForSbmdIndex;
        using MatterDevice::ResolveEndpointMap;

        /**
         * Populate a DeviceDataCache's internal ClusterStateCache with endpoint and device type data.
         * This simulates what would happen when a real device subscription delivers attribute reports.
         *
         * @param cache The DeviceDataCache to populate
         * @param partsList Endpoint IDs to include in the root endpoint's PartsList
         * @param endpointDeviceTypes Map of endpoint ID → device type IDs for each endpoint
         */
        static void PopulateTestCache(std::shared_ptr<DeviceDataCache> &cache,
                                      const std::vector<chip::EndpointId> &partsList,
                                      const std::map<chip::EndpointId, std::vector<uint16_t>> &endpointDeviceTypes,
                                      const std::map<chip::EndpointId, std::vector<chip::ClusterId>> &endpointServerClusters = {})
        {
            // Create the ClusterStateCache (normally done in OnDeviceConnected)
            cache->clusterStateCache = std::make_unique<chip::app::ClusterStateCache>(*cache);

            auto &cb = cache->clusterStateCache->GetBufferedCallback();
            chip::app::StatusIB okStatus;
            cb.OnReportBegin();

            // Encode PartsList for endpoint 0 (Descriptor cluster, attribute 0x0003)
            {
                uint8_t buffer[256];
                chip::TLV::TLVWriter writer;
                writer.Init(buffer, sizeof(buffer));
                chip::TLV::TLVType arrayType;
                writer.StartContainer(chip::TLV::AnonymousTag(), chip::TLV::kTLVType_Array, arrayType);
                for (auto ep : partsList)
                {
                    writer.Put(chip::TLV::AnonymousTag(), ep);
                }
                writer.EndContainer(arrayType);
                writer.Finalize();

                chip::TLV::TLVReader reader;
                reader.Init(buffer, writer.GetLengthWritten());
                reader.Next();

                chip::app::ConcreteDataAttributePath path(
                    0, chip::app::Clusters::Descriptor::Id, chip::app::Clusters::Descriptor::Attributes::PartsList::Id);
                cb.OnAttributeData(path, &reader, okStatus);
            }

            // Encode DeviceTypeList for each endpoint
            for (const auto &[endpointId, deviceTypes] : endpointDeviceTypes)
            {
                uint8_t buffer[256];
                chip::TLV::TLVWriter writer;
                writer.Init(buffer, sizeof(buffer));
                chip::TLV::TLVType arrayType;
                writer.StartContainer(chip::TLV::AnonymousTag(), chip::TLV::kTLVType_Array, arrayType);
                for (uint16_t dt : deviceTypes)
                {
                    chip::TLV::TLVType structType;
                    writer.StartContainer(chip::TLV::AnonymousTag(), chip::TLV::kTLVType_Structure, structType);
                    writer.Put(chip::TLV::ContextTag(0), static_cast<uint32_t>(dt));
                    writer.Put(chip::TLV::ContextTag(1), static_cast<uint16_t>(1));
                    writer.EndContainer(structType);
                }
                writer.EndContainer(arrayType);
                writer.Finalize();

                chip::TLV::TLVReader reader;
                reader.Init(buffer, writer.GetLengthWritten());
                reader.Next();

                chip::app::ConcreteDataAttributePath path(
                    endpointId,
                    chip::app::Clusters::Descriptor::Id,
                    chip::app::Clusters::Descriptor::Attributes::DeviceTypeList::Id);
                cb.OnAttributeData(path, &reader, okStatus);
            }

            // Inject a dummy attribute for each server cluster on each endpoint so that
            // ForEachCluster (used by EndpointHasServerCluster) will find them.
            for (const auto &[endpointId, clusters] : endpointServerClusters)
            {
                for (auto clusterId : clusters)
                {
                    uint8_t buffer[16];
                    chip::TLV::TLVWriter writer;
                    writer.Init(buffer, sizeof(buffer));
                    writer.Put(chip::TLV::AnonymousTag(), static_cast<uint16_t>(0));
                    writer.Finalize();

                    chip::TLV::TLVReader reader;
                    reader.Init(buffer, writer.GetLengthWritten());
                    reader.Next();

                    chip::app::ConcreteDataAttributePath path(endpointId, clusterId, 0x0000);
                    cb.OnAttributeData(path, &reader, okStatus);
                }
            }

            cb.OnReportEnd();
        }
    };
} // namespace barton

namespace
{
    // Initialize CHIP Platform memory once for all tests
    class ChipPlatformEnvironment : public ::testing::Environment
    {
    public:
        void SetUp() override { ASSERT_EQ(chip::Platform::MemoryInit(), CHIP_NO_ERROR); }

        void TearDown() override { chip::Platform::MemoryShutdown(); }
    };

    ::testing::Environment *const chipEnv = ::testing::AddGlobalTestEnvironment(new ChipPlatformEnvironment);

    // Matter device type IDs
    constexpr uint16_t kDimmableLightDeviceType = 0x0100;
    constexpr uint16_t kTemperatureSensorDeviceType = 0x0302;
    constexpr uint16_t kHumiditySensorDeviceType = 0x0307;

    // Matter cluster IDs
    constexpr chip::ClusterId kTemperatureMeasurementCluster = 0x0402;
    constexpr chip::ClusterId kRelativeHumidityMeasurementCluster = 0x0405;

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

    // Positive case: multi-endpoint device, skips endpoint 0 and non-matching endpoints,
    // assigns sequential SBMD indices in PartsList order
    TEST_F(MatterDeviceEndpointMapTest, ResolveEndpointMapMultipleEndpoints)
    {
        // Device has endpoints 0 (root), 1 (light 0x0100), 2 (sensor 0x000F), 3 (light 0x0100)
        std::vector<chip::EndpointId> partsList = {1, 2, 3};
        std::map<chip::EndpointId, std::vector<uint16_t>> deviceTypes = {
            {1, {0x0100}},
            {2, {0x000F}},
            {3, {0x0100}},
        };
        TestableMatterDevice::PopulateTestCache(cache, partsList, deviceTypes);

        // Resolve for device type 0x0100 (light) — should match endpoints 1 and 3
        EXPECT_TRUE(device->ResolveEndpointMap({0x0100}));

        auto &map = device->GetSbmdEndpointMap();
        ASSERT_EQ(map.size(), 2u);
        EXPECT_EQ(map[0], 1); // First match → SBMD index 0
        EXPECT_EQ(map[1], 3); // Second match → SBMD index 1
    }

    // Positive case: single matching endpoint
    TEST_F(MatterDeviceEndpointMapTest, ResolveEndpointMapSingleMatch)
    {
        std::vector<chip::EndpointId> partsList = {1, 2};
        std::map<chip::EndpointId, std::vector<uint16_t>> deviceTypes = {
            {1, {0x0100}},
            {2, {0x000F}},
        };
        TestableMatterDevice::PopulateTestCache(cache, partsList, deviceTypes);

        EXPECT_TRUE(device->ResolveEndpointMap({0x0100}));

        auto &map = device->GetSbmdEndpointMap();
        ASSERT_EQ(map.size(), 1u);
        EXPECT_EQ(map[0], 1);
    }

    // Positive case: no matching device types on any endpoint
    TEST_F(MatterDeviceEndpointMapTest, ResolveEndpointMapNoMatch)
    {
        std::vector<chip::EndpointId> partsList = {1, 2};
        std::map<chip::EndpointId, std::vector<uint16_t>> deviceTypes = {
            {1, {0x0100}},
            {2, {0x000F}},
        };
        TestableMatterDevice::PopulateTestCache(cache, partsList, deviceTypes);

        EXPECT_FALSE(device->ResolveEndpointMap({0x0302}));
        EXPECT_TRUE(device->GetSbmdEndpointMap().empty());
    }

    // Positive case: multiple supported device types match different endpoints
    TEST_F(MatterDeviceEndpointMapTest, ResolveEndpointMapMultipleDeviceTypes)
    {
        std::vector<chip::EndpointId> partsList = {1, 2, 3};
        std::map<chip::EndpointId, std::vector<uint16_t>> deviceTypes = {
            {1, {0x0100}}, // light
            {2, {0x000F}}, // sensor
            {3, {0x0302}}, // no match
        };
        TestableMatterDevice::PopulateTestCache(cache, partsList, deviceTypes);

        // Match both light and sensor types
        EXPECT_TRUE(device->ResolveEndpointMap({0x0100, 0x000F}));

        auto &map = device->GetSbmdEndpointMap();
        ASSERT_EQ(map.size(), 2u);
        EXPECT_EQ(map[0], 1); // endpoint 1 matches 0x0100
        EXPECT_EQ(map[1], 2); // endpoint 2 matches 0x000F
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

    // Endpoint-level write binding with invalid index: bind should fail and no binding created
    TEST_F(MatterDeviceEndpointMapTest, BindWriteInfoEndpointLevelBadIndex)
    {
        device->GetSbmdEndpointMap()[0] = 1;

        EXPECT_FALSE(device->BindWriteInfo("/test/write-bad", "key", "ep1", "res1", 5));
        auto &bindings = device->GetWriteBindings();
        EXPECT_EQ(bindings.find("/test/write-bad"), bindings.end());
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

    // ================================================================
    // Tests for ResolveEndpointForCluster fallback
    // ================================================================

    // Composite device: EP 1 has temperature measurement, EP 2 has humidity measurement.
    // SBMD index 0 maps to EP 1. Reading temperature resolves directly to EP 1.
    // Reading humidity falls back to EP 2 because EP 1 doesn't host that cluster.
    TEST_F(MatterDeviceEndpointMapTest, BindReadInfoFallsBackToClusterWhenNotOnMappedEndpoint)
    {
        // Simulates a temperature/humidity sensor:
        // Matter EP 1: Temperature Sensor, has Temperature Measurement cluster
        // Matter EP 2: Humidity Sensor, has Relative Humidity Measurement cluster
        std::vector<chip::EndpointId> partsList = {1, 2};
        std::map<chip::EndpointId, std::vector<uint16_t>> deviceTypes = {
            {1, {kTemperatureSensorDeviceType}},
            {2, {kHumiditySensorDeviceType}},
        };
        std::map<chip::EndpointId, std::vector<chip::ClusterId>> serverClusters = {
            {1, {kTemperatureMeasurementCluster}},
            {2, {kRelativeHumidityMeasurementCluster}},
        };
        TestableMatterDevice::PopulateTestCache(cache, partsList, deviceTypes, serverClusters);

        ASSERT_TRUE(device->ResolveEndpointMap({kTemperatureSensorDeviceType, kHumiditySensorDeviceType}));

        // Temperature with SBMD index 0 → EP 1 directly (EP 1 has the cluster)
        SbmdMapper tempMapper;
        tempMapper.hasRead = true;
        SbmdAttribute tempAttr;
        tempAttr.clusterId = kTemperatureMeasurementCluster;
        tempAttr.attributeId = 0x0000;
        tempMapper.readAttribute = tempAttr;

        EXPECT_TRUE(device->BindResourceReadInfo("/test/temperature", tempMapper, 0));
        EXPECT_EQ(device->GetReadBindings().at("/test/temperature").attributePath.mEndpointId, 1);

        // Humidity with SBMD index 0 → EP 1 doesn't have that cluster → falls back to EP 2
        SbmdMapper humMapper;
        humMapper.hasRead = true;
        SbmdAttribute humAttr;
        humAttr.clusterId = kRelativeHumidityMeasurementCluster;
        humAttr.attributeId = 0x0000;
        humMapper.readAttribute = humAttr;

        EXPECT_TRUE(device->BindResourceReadInfo("/test/humidity", humMapper, 0));
        EXPECT_EQ(device->GetReadBindings().at("/test/humidity").attributePath.mEndpointId, 2);
    }

    // Endpoint-level event binding: falls back to cluster-based lookup when
    // SBMD-mapped endpoint doesn't host the event cluster
    TEST_F(MatterDeviceEndpointMapTest, BindEventInfoFallsBackToClusterWhenNotOnMappedEndpoint)
    {
        std::vector<chip::EndpointId> partsList = {1, 2};
        std::map<chip::EndpointId, std::vector<uint16_t>> deviceTypes = {
            {1, {kTemperatureSensorDeviceType}},
            {2, {kHumiditySensorDeviceType}},
        };
        std::map<chip::EndpointId, std::vector<chip::ClusterId>> serverClusters = {
            {1, {kTemperatureMeasurementCluster}},
            {2, {kRelativeHumidityMeasurementCluster}},
        };
        TestableMatterDevice::PopulateTestCache(cache, partsList, deviceTypes, serverClusters);

        ASSERT_TRUE(device->ResolveEndpointMap({kTemperatureSensorDeviceType, kHumiditySensorDeviceType}));

        // Event on humidity cluster with SBMD index 0 → EP 1 doesn't have it → falls back to EP 2
        SbmdEvent event;
        event.clusterId = kRelativeHumidityMeasurementCluster;
        event.eventId = 0x0000;

        EXPECT_TRUE(device->BindResourceEventInfo("/test/humidity-event", event, 0));
    }

    // ================================================================
    // Tests for ClaimDevice with "any" and "all" semantics
    // ================================================================

    class ClaimDeviceTest : public ::testing::Test
    {
    protected:
        void SetUp() override { cache = std::make_shared<DeviceDataCache>("test-device", nullptr); }

        void TearDown() override { cache.reset(); }

        std::shared_ptr<SbmdSpec> MakeSpec(std::vector<uint16_t> deviceTypes, const std::string &matchMode = "any")
        {
            auto spec = std::make_shared<SbmdSpec>();
            spec->name = "test";
            spec->bartonMeta.deviceClass = "testClass";
            spec->bartonMeta.deviceClassVersion = 1;
            spec->matterMeta.deviceTypes = std::move(deviceTypes);
            spec->matterMeta.deviceTypeMatch = matchMode;
            return spec;
        }

        std::shared_ptr<DeviceDataCache> cache;
    };

    TEST_F(ClaimDeviceTest, AnyMatchSingleDeviceTypeMatch)
    {
        std::vector<chip::EndpointId> partsList = {1};
        std::map<chip::EndpointId, std::vector<uint16_t>> endpointDeviceTypes = {
            {1, {kDimmableLightDeviceType}},
        };
        TestableMatterDevice::PopulateTestCache(cache, partsList, endpointDeviceTypes);

        // MakeSpec defaults to "any" match mode
        SpecBasedMatterDeviceDriver driver(MakeSpec({kDimmableLightDeviceType}));
        EXPECT_TRUE(driver.ClaimDevice(cache.get()));
    }

    TEST_F(ClaimDeviceTest, AnyMatchNoMatch)
    {
        std::vector<chip::EndpointId> partsList = {1};
        std::map<chip::EndpointId, std::vector<uint16_t>> endpointDeviceTypes = {
            {1, {kDimmableLightDeviceType}},
        };
        TestableMatterDevice::PopulateTestCache(cache, partsList, endpointDeviceTypes);

        // MakeSpec defaults to "any" match mode
        SpecBasedMatterDeviceDriver driver(MakeSpec({kTemperatureSensorDeviceType}));
        EXPECT_FALSE(driver.ClaimDevice(cache.get()));
    }

    TEST_F(ClaimDeviceTest, AnyMatchClaimsWhenOneEndpointMatches)
    {
        std::vector<chip::EndpointId> partsList = {1, 2};
        std::map<chip::EndpointId, std::vector<uint16_t>> endpointDeviceTypes = {
            {1, {kTemperatureSensorDeviceType}},
            {2, {kHumiditySensorDeviceType}},
        };
        TestableMatterDevice::PopulateTestCache(cache, partsList, endpointDeviceTypes);

        // MakeSpec defaults to "any" match mode
        SpecBasedMatterDeviceDriver driver(MakeSpec({kTemperatureSensorDeviceType}));
        EXPECT_TRUE(driver.ClaimDevice(cache.get()));
    }

    TEST_F(ClaimDeviceTest, AllMatchBothPresent)
    {
        std::vector<chip::EndpointId> partsList = {1, 2};
        std::map<chip::EndpointId, std::vector<uint16_t>> endpointDeviceTypes = {
            {1, {kTemperatureSensorDeviceType}},
            {2, {kHumiditySensorDeviceType}},
        };
        TestableMatterDevice::PopulateTestCache(cache, partsList, endpointDeviceTypes);

        SpecBasedMatterDeviceDriver driver(MakeSpec({kTemperatureSensorDeviceType, kHumiditySensorDeviceType}, "all"));
        EXPECT_TRUE(driver.ClaimDevice(cache.get()));
    }

    TEST_F(ClaimDeviceTest, AllMatchSubsetFails)
    {
        std::vector<chip::EndpointId> partsList = {1};
        std::map<chip::EndpointId, std::vector<uint16_t>> endpointDeviceTypes = {
            {1, {kTemperatureSensorDeviceType}},
        };
        TestableMatterDevice::PopulateTestCache(cache, partsList, endpointDeviceTypes);

        SpecBasedMatterDeviceDriver driver(MakeSpec({kTemperatureSensorDeviceType, kHumiditySensorDeviceType}, "all"));
        EXPECT_FALSE(driver.ClaimDevice(cache.get()));
    }

    TEST_F(ClaimDeviceTest, AllMatchSameEndpoint)
    {
        std::vector<chip::EndpointId> partsList = {1};
        std::map<chip::EndpointId, std::vector<uint16_t>> endpointDeviceTypes = {
            {1, {kTemperatureSensorDeviceType, kHumiditySensorDeviceType}},
        };
        TestableMatterDevice::PopulateTestCache(cache, partsList, endpointDeviceTypes);

        SpecBasedMatterDeviceDriver driver(MakeSpec({kTemperatureSensorDeviceType, kHumiditySensorDeviceType}, "all"));
        EXPECT_TRUE(driver.ClaimDevice(cache.get()));
    }

    TEST_F(ClaimDeviceTest, AllMatchRootEndpointExcluded)
    {
        // Device type kHumiditySensorDeviceType only on root endpoint 0 — should be excluded
        std::vector<chip::EndpointId> partsList = {0, 1};
        std::map<chip::EndpointId, std::vector<uint16_t>> endpointDeviceTypes = {
            {0, {kHumiditySensorDeviceType}},
            {1, {kTemperatureSensorDeviceType}},
        };
        TestableMatterDevice::PopulateTestCache(cache, partsList, endpointDeviceTypes);

        SpecBasedMatterDeviceDriver driver(MakeSpec({kTemperatureSensorDeviceType, kHumiditySensorDeviceType}, "all"));
        EXPECT_FALSE(driver.ClaimDevice(cache.get()));
    }

    TEST_F(ClaimDeviceTest, ClaimDeviceNullCache)
    {
        SpecBasedMatterDeviceDriver driver(MakeSpec({kTemperatureSensorDeviceType}, "all"));
        EXPECT_FALSE(driver.ClaimDevice(nullptr));
    }

    // Negative tests for composite matching
    TEST_F(ClaimDeviceTest, AllMatchRejectsTemperatureOnly)
    {
        std::vector<chip::EndpointId> partsList = {1};
        std::map<chip::EndpointId, std::vector<uint16_t>> endpointDeviceTypes = {
            {1, {kTemperatureSensorDeviceType}},
        };
        TestableMatterDevice::PopulateTestCache(cache, partsList, endpointDeviceTypes);

        SpecBasedMatterDeviceDriver driver(MakeSpec({kTemperatureSensorDeviceType, kHumiditySensorDeviceType}, "all"));
        EXPECT_FALSE(driver.ClaimDevice(cache.get()));
    }

    TEST_F(ClaimDeviceTest, AllMatchRejectsHumidityOnly)
    {
        std::vector<chip::EndpointId> partsList = {1};
        std::map<chip::EndpointId, std::vector<uint16_t>> endpointDeviceTypes = {
            {1, {kHumiditySensorDeviceType}},
        };
        TestableMatterDevice::PopulateTestCache(cache, partsList, endpointDeviceTypes);

        SpecBasedMatterDeviceDriver driver(MakeSpec({kTemperatureSensorDeviceType, kHumiditySensorDeviceType}, "all"));
        EXPECT_FALSE(driver.ClaimDevice(cache.get()));
    }

    TEST_F(ClaimDeviceTest, AnyMatchClaimsCompositeDevice)
    {
        std::vector<chip::EndpointId> partsList = {1, 2};
        std::map<chip::EndpointId, std::vector<uint16_t>> endpointDeviceTypes = {
            {1, {kTemperatureSensorDeviceType}},
            {2, {kHumiditySensorDeviceType}},
        };
        TestableMatterDevice::PopulateTestCache(cache, partsList, endpointDeviceTypes);

        // MakeSpec defaults to "any" match mode.
        // A standalone temperature driver with "any" matches the composite device
        // because at least one endpoint has a matching device type.
        // Priority ordering in the driver factory handles this — the composite driver
        // claims first. This test just verifies "any" semantics still match.
        SpecBasedMatterDeviceDriver driver(MakeSpec({kTemperatureSensorDeviceType}));
        EXPECT_TRUE(driver.ClaimDevice(cache.get()));
    }

} // namespace
