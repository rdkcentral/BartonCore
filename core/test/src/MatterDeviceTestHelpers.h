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

//
// Created by Raiyan Chowdhury on 5/26/2026.
//

/*
 * Shared test helpers for MatterDevice unit tests.
 *
 * Provides:
 *   - TestableMatterDevice  — friend subclass that exposes private members
 *   - ChipPlatformEnvironment — GTest Environment that initialises CHIP memory
 *
 * Each test binary must register ChipPlatformEnvironment once:
 *   ::testing::Environment *const chipEnv =
 *       ::testing::AddGlobalTestEnvironment(new ChipPlatformEnvironment);
 */

#pragma once

#include "deviceDrivers/matter/MatterDevice.h"
#include "subsystems/matter/DeviceDataCache.h"
#include <app-common/zap-generated/cluster-objects.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <lib/core/TLV.h>
#include <lib/support/CHIPMem.h>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace barton
{
    /**
     * Testable subclass of MatterDevice that exposes private members needed by
     * unit tests. Named TestableMatterDevice to match the friend declaration in
     * MatterDevice.h.
     */
    class TestableMatterDevice : public MatterDevice
    {
    public:
        using MatterDevice::MatterDevice;

        // ── Endpoint-map test helpers ──────────────────────────────────────

        std::map<uint32_t, chip::EndpointId> &GetSbmdEndpointMap() { return sbmdEndpointMap; }

        CacheCallback *GetCacheCallback()
        {
            if (!deviceDataCache || !deviceDataCache->callback)
            {
                return nullptr;
            }

            return static_cast<CacheCallback *>(deviceDataCache->callback.get());
        }

        /**
         * Get the raw ClusterStateCache pointer from the DeviceDataCache.
         * Used by tests to pass to OnAttributeChanged.
         */
        chip::app::ClusterStateCache *GetClusterStateCache()
        {
            return deviceDataCache ? deviceDataCache->clusterStateCache.get() : nullptr;
        }

        /**
         * Seed the ClusterStateCache with a single uint16 attribute value.
         * Used by OnAttributeChanged tests to ensure cache->Get() returns data.
         */
        static void SeedCacheWithUint16(const std::shared_ptr<DeviceDataCache> &cache,
                                        const chip::app::ConcreteDataAttributePath &path,
                                        uint16_t value)
        {
            if (!cache->clusterStateCache)
            {
                cache->clusterStateCache = std::make_unique<chip::app::ClusterStateCache>(*cache);
            }

            constexpr size_t kTlvUint16BufferSize = 16;
            auto &cb = cache->clusterStateCache->GetBufferedCallback();
            chip::app::StatusIB okStatus;
            cb.OnReportBegin();

            {
                uint8_t buffer[kTlvUint16BufferSize];
                chip::TLV::TLVWriter writer;
                writer.Init(buffer, sizeof(buffer));
                writer.Put(chip::TLV::AnonymousTag(), value);
                writer.Finalize();

                chip::TLV::TLVReader reader;
                reader.Init(buffer, writer.GetLengthWritten());
                reader.Next();

                cb.OnAttributeData(path, &reader, okStatus);
            }

            cb.OnReportEnd();
        }

        using MatterDevice::GetEndpointForSbmdIndex;
        using MatterDevice::ResolveEndpointMap;

        /**
         * Populate a DeviceDataCache's internal ClusterStateCache with endpoint
         * and device type data. This simulates what would happen when a real
         * device subscription delivers attribute reports.
         *
         * @param cache The DeviceDataCache to populate
         * @param partsList Endpoint IDs to include in the root endpoint's PartsList
         * @param endpointDeviceTypes Map of endpoint ID → device type IDs for each endpoint
         * @param endpointServerClusters Map of endpoint ID → cluster IDs for each endpoint
         */
        static void
        PopulateTestCache(std::shared_ptr<DeviceDataCache> &cache,
                          const std::vector<chip::EndpointId> &partsList,
                          const std::map<chip::EndpointId, std::vector<uint16_t>> &endpointDeviceTypes,
                          const std::map<chip::EndpointId, std::vector<chip::ClusterId>> &endpointServerClusters = {})
        {
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
                    0,
                    chip::app::Clusters::Descriptor::Id,
                    chip::app::Clusters::Descriptor::Attributes::PartsList::Id);
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

        /**
         * Inject BasicInformation VendorID and ProductID into an existing test cache.
         * Separated from PopulateTestCache so that tests which only need endpoint/device-type
         * data don't carry unnecessary vendor identity.
         * Must be called after PopulateTestCache (which creates the ClusterStateCache).
         */
        static void InjectVendorProduct(std::shared_ptr<DeviceDataCache> &cache, uint16_t vendorId, uint16_t productId)
        {
            auto &cb = cache->clusterStateCache->GetBufferedCallback();
            chip::app::StatusIB okStatus;
            cb.OnReportBegin();

            // Encode VendorID as TLV and inject it on endpoint 0, BasicInformation cluster
            {
                uint8_t buffer[16];
                chip::TLV::TLVWriter writer;
                writer.Init(buffer, sizeof(buffer));
                writer.Put(chip::TLV::AnonymousTag(), vendorId);
                writer.Finalize();

                chip::TLV::TLVReader reader;
                reader.Init(buffer, writer.GetLengthWritten());
                reader.Next();

                chip::app::ConcreteDataAttributePath path(
                    0,
                    chip::app::Clusters::BasicInformation::Id,
                    chip::app::Clusters::BasicInformation::Attributes::VendorID::Id);
                cb.OnAttributeData(path, &reader, okStatus);
            }

            // Encode ProductID as TLV and inject it on endpoint 0, BasicInformation cluster
            {
                uint8_t buffer[16];
                chip::TLV::TLVWriter writer;
                writer.Init(buffer, sizeof(buffer));
                writer.Put(chip::TLV::AnonymousTag(), productId);
                writer.Finalize();

                chip::TLV::TLVReader reader;
                reader.Init(buffer, writer.GetLengthWritten());
                reader.Next();

                chip::app::ConcreteDataAttributePath path(
                    0,
                    chip::app::Clusters::BasicInformation::Id,
                    chip::app::Clusters::BasicInformation::Attributes::ProductID::Id);
                cb.OnAttributeData(path, &reader, okStatus);
            }

            cb.OnReportEnd();
        }

    };

} // namespace barton

/**
 * GTest Environment that initialises and shuts down CHIP Platform memory.
 *
 * Each test binary must register this once in its anonymous namespace:
 *   ::testing::Environment *const chipEnv =
 *       ::testing::AddGlobalTestEnvironment(new ChipPlatformEnvironment);
 */
class ChipPlatformEnvironment : public ::testing::Environment
{
public:
    void SetUp() override { ASSERT_EQ(chip::Platform::MemoryInit(), CHIP_NO_ERROR); }

    void TearDown() override { chip::Platform::MemoryShutdown(); }
};
