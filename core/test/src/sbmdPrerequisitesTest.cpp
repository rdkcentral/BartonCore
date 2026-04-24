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
 * Unit tests for SpecBasedMatterDeviceDriver::CheckPrerequisites()
 *
 * Verifies that device resources are conditionally registered based on whether the
 * commissioning device's data cache contains the required clusters and/or attributes.
 */

#include "deviceDrivers/matter/MatterDevice.h"
#include "deviceDrivers/matter/sbmd/SbmdSpec.h"
#include "deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.h"
#include "subsystems/matter/DeviceDataCache.h"
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/ConcreteAttributePath.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <lib/core/TLV.h>
#include <lib/support/CHIPMem.h>
#include <memory>

using namespace barton;

namespace barton
{
    /**
     * Exposes the private static CheckPrerequisites() method for direct unit testing.
     */
    class TestableSpecBasedMatterDeviceDriver : public SpecBasedMatterDeviceDriver
    {
    public:
        static bool CheckPrerequisites(const SbmdResource &resource, const MatterDevice &device)
        {
            return SpecBasedMatterDeviceDriver::CheckPrerequisites(resource, device);
        }
    };

    /**
     * Provides cache-population helpers for prerequisite tests.
     */
    class SbmdPrerequisitesTestHelper
    {
    public:
        /**
         * Initialise a DeviceDataCache with a ClusterStateCache and populate its PartsList
         * on endpoint 0 so that GetEndpointIds() returns [0].
         * No cluster data is written — the caller must call AddClusterAttribute() separately.
         */
        static void InitCache(std::shared_ptr<DeviceDataCache> &cache)
        {
            cache->clusterStateCache = std::make_unique<chip::app::ClusterStateCache>(*cache);

            auto &cb = cache->clusterStateCache->GetBufferedCallback();
            chip::app::StatusIB okStatus;
            cb.OnReportBegin();

            // Encode PartsList on ep0 as an empty array — GetEndpointIds() will still return [0]
            {
                uint8_t buf[32];
                chip::TLV::TLVWriter w;
                w.Init(buf, sizeof(buf));
                chip::TLV::TLVType t;
                ASSERT_EQ(w.StartContainer(chip::TLV::AnonymousTag(), chip::TLV::kTLVType_Array, t), CHIP_NO_ERROR);
                ASSERT_EQ(w.EndContainer(t), CHIP_NO_ERROR);
                ASSERT_EQ(w.Finalize(), CHIP_NO_ERROR);

                chip::TLV::TLVReader r;
                r.Init(buf, w.GetLengthWritten());
                r.Next();

                chip::app::ConcreteDataAttributePath path(
                    0, chip::app::Clusters::Descriptor::Id, chip::app::Clusters::Descriptor::Attributes::PartsList::Id);
                cb.OnAttributeData(path, &r, okStatus);
            }

            cb.OnReportEnd();
        }

        /**
         * Add a single boolean attribute to the cache for the given endpoint/cluster/attribute triple.
         * This makes EndpointHasServerCluster(endpointId, clusterId) return true, and
         * GetAttributeData(path, reader) return CHIP_NO_ERROR for that path.
         */
        static void AddClusterAttribute(std::shared_ptr<DeviceDataCache> &cache,
                                        chip::EndpointId endpointId,
                                        chip::ClusterId clusterId,
                                        chip::AttributeId attributeId)
        {
            auto &cb = cache->clusterStateCache->GetBufferedCallback();
            chip::app::StatusIB okStatus;
            cb.OnReportBegin();

            uint8_t buf[32];
            chip::TLV::TLVWriter w;
            w.Init(buf, sizeof(buf));
            ASSERT_EQ(w.PutBoolean(chip::TLV::AnonymousTag(), false), CHIP_NO_ERROR);
            ASSERT_EQ(w.Finalize(), CHIP_NO_ERROR);

            chip::TLV::TLVReader r;
            r.Init(buf, w.GetLengthWritten());
            r.Next();

            chip::app::ConcreteDataAttributePath path(endpointId, clusterId, attributeId);
            cb.OnAttributeData(path, &r, okStatus);

            cb.OnReportEnd();
        }
    };

} // namespace barton

namespace
{
    class ChipPlatformEnvironment : public ::testing::Environment
    {
    public:
        void SetUp() override { ASSERT_EQ(chip::Platform::MemoryInit(), CHIP_NO_ERROR); }

        void TearDown() override { chip::Platform::MemoryShutdown(); }
    };

    ::testing::Environment *const chipEnv = ::testing::AddGlobalTestEnvironment(new ChipPlatformEnvironment);

    class SbmdPrerequisitesTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            cache = std::make_shared<DeviceDataCache>("test-device", nullptr);
            device = std::make_unique<MatterDevice>("test-device", cache);
        }

        void TearDown() override
        {
            device.reset();
            cache.reset();
        }

        /** Build a resource with an explicit-form prerequisite on the given cluster. */
        static SbmdResource MakeResourceWithClusterPrereq(uint32_t clusterId)
        {
            SbmdResource resource;
            resource.id = "testResource";
            resource.type = "boolean";

            SbmdPrerequisite prereq;
            prereq.clusterId = clusterId;
            resource.prerequisites = std::vector<SbmdPrerequisite> {prereq};

            return resource;
        }

        /**
         * Build a resource with an explicit-form prerequisite on the given cluster + attribute.
         */
        static SbmdResource MakeResourceWithAttributePrereq(uint32_t clusterId, uint32_t attributeId)
        {
            SbmdResource resource;
            resource.id = "testResource";
            resource.type = "boolean";

            SbmdPrerequisite prereq;
            prereq.clusterId = clusterId;
            prereq.attributeIds = {attributeId};
            resource.prerequisites = std::vector<SbmdPrerequisite> {prereq};

            return resource;
        }

        std::shared_ptr<DeviceDataCache> cache;
        std::unique_ptr<MatterDevice> device;
    };

    // -----------------------------------------------------------------------
    // 4.1 — cluster absent → resource skipped
    // -----------------------------------------------------------------------
    TEST_F(SbmdPrerequisitesTest, ClusterAbsentGatesResource)
    {
        // No cache data at all — GetEndpointIds() returns [] so no cluster can be found
        auto resource = MakeResourceWithClusterPrereq(0x0405);

        EXPECT_FALSE(TestableSpecBasedMatterDeviceDriver::CheckPrerequisites(resource, *device));
    }

    // -----------------------------------------------------------------------
    // 4.2 — cluster present (no attribute check) → resource registered
    // -----------------------------------------------------------------------
    TEST_F(SbmdPrerequisitesTest, ClusterPresentPassesGate)
    {
        SbmdPrerequisitesTestHelper::InitCache(cache);
        // Add any attribute on cluster 0x0405 — makes EndpointHasServerCluster(0, 0x0405) true
        SbmdPrerequisitesTestHelper::AddClusterAttribute(cache, 0, 0x0405, 0x0000);

        auto resource = MakeResourceWithClusterPrereq(0x0405);

        EXPECT_TRUE(TestableSpecBasedMatterDeviceDriver::CheckPrerequisites(resource, *device));
    }

    // -----------------------------------------------------------------------
    // 4.3 — cluster present but required attribute absent → resource skipped
    // -----------------------------------------------------------------------
    TEST_F(SbmdPrerequisitesTest, AttributeAbsentGatesResource)
    {
        SbmdPrerequisitesTestHelper::InitCache(cache);
        // Add cluster 0x0405 but only attribute 0x0000 — attribute 0x0003 is absent
        SbmdPrerequisitesTestHelper::AddClusterAttribute(cache, 0, 0x0405, 0x0000);

        auto resource = MakeResourceWithAttributePrereq(0x0405, 0x0003);

        EXPECT_FALSE(TestableSpecBasedMatterDeviceDriver::CheckPrerequisites(resource, *device));
    }

    // -----------------------------------------------------------------------
    // 4.4 — cluster and required attribute present → resource registered
    // -----------------------------------------------------------------------
    TEST_F(SbmdPrerequisitesTest, ClusterAndAttributePresentPassesGate)
    {
        SbmdPrerequisitesTestHelper::InitCache(cache);
        SbmdPrerequisitesTestHelper::AddClusterAttribute(cache, 0, 0x0405, 0x0003);

        auto resource = MakeResourceWithAttributePrereq(0x0405, 0x0003);

        EXPECT_TRUE(TestableSpecBasedMatterDeviceDriver::CheckPrerequisites(resource, *device));
    }

    // -----------------------------------------------------------------------
    // 4.5 — alias-resolved prerequisite (clusterId + attributeId) registers when present
    // -----------------------------------------------------------------------
    TEST_F(SbmdPrerequisitesTest, AliasResolvedPrerequisitePassesGate)
    {
        SbmdPrerequisitesTestHelper::InitCache(cache);
        SbmdPrerequisitesTestHelper::AddClusterAttribute(cache, 0, 0x0405, 0x0000);

        SbmdResource resource;
        resource.id = "testResource";
        resource.type = "boolean";

        // Prerequisite already resolved at parse time (from alias): clusterId + attributeId
        SbmdPrerequisite prereq;
        prereq.clusterId = 0x0405;
        prereq.attributeIds = {0x0000};
        resource.prerequisites = std::vector<SbmdPrerequisite> {prereq};

        EXPECT_TRUE(TestableSpecBasedMatterDeviceDriver::CheckPrerequisites(resource, *device));
    }

    // -----------------------------------------------------------------------
    // 4.6 — prerequisites: none (empty vector) → always register, even with empty cache
    // -----------------------------------------------------------------------
    TEST_F(SbmdPrerequisitesTest, PrerequisiteNoneNeverGates)
    {
        // Empty cache — but empty prerequisites vector means always register
        SbmdResource resource;
        resource.id = "testResource";
        resource.type = "boolean";
        resource.prerequisites = std::vector<SbmdPrerequisite> {}; // empty = none

        EXPECT_TRUE(TestableSpecBasedMatterDeviceDriver::CheckPrerequisites(resource, *device));
    }

    // -----------------------------------------------------------------------
    // 4.7 — multiple prerequisites: all must be satisfied; only first is → skipped
    // -----------------------------------------------------------------------
    TEST_F(SbmdPrerequisitesTest, PartialPrerequisitesSatisfiedGatesResource)
    {
        SbmdPrerequisitesTestHelper::InitCache(cache);
        // Cluster 0x0405 present, cluster 0x0406 absent
        SbmdPrerequisitesTestHelper::AddClusterAttribute(cache, 0, 0x0405, 0x0000);

        SbmdResource resource;
        resource.id = "testResource";
        resource.type = "boolean";

        SbmdPrerequisite prereq1;
        prereq1.clusterId = 0x0405;

        SbmdPrerequisite prereq2;
        prereq2.clusterId = 0x0406; // absent

        resource.prerequisites = std::vector<SbmdPrerequisite> {prereq1, prereq2};

        EXPECT_FALSE(TestableSpecBasedMatterDeviceDriver::CheckPrerequisites(resource, *device));
    }

    // -----------------------------------------------------------------------
    // 4.9 — CheckPrerequisites() returns false regardless of the optional flag;
    //        interpreting that result as a hard abort vs. a silent skip is the
    //        responsibility of the AddDevice() callsite, not this function.
    // -----------------------------------------------------------------------
    TEST_F(SbmdPrerequisitesTest, UnmetPrerequisitesGateRegardlessOfOptionality)
    {
        // Empty cache — cluster absent
        auto resource = MakeResourceWithClusterPrereq(0x0405);
        resource.optional = false; // explicitly required (default, but clear)

        // CheckPrerequisites returns false regardless of optional flag —
        // AddDevice() is responsible for interpreting that as a skip or a hard abort.
        EXPECT_FALSE(TestableSpecBasedMatterDeviceDriver::CheckPrerequisites(resource, *device));
    }

} // namespace
