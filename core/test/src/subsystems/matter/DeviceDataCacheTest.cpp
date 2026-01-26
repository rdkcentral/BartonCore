//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2024 Comcast Cable Communications Management, LLC
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

#include "subsystems/matter/DeviceDataCache.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

using namespace barton;
using namespace chip;
using namespace chip::app;

namespace
{
    class DeviceDataCacheTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            deviceUuid = "test-device-uuid-12345";
            cache = std::make_shared<DeviceDataCache>(deviceUuid, nullptr);
        }

        void TearDown() override { cache.reset(); }

        std::string deviceUuid;
        std::shared_ptr<DeviceDataCache> cache;
    };

    // Test: DeviceDataCache can be instantiated
    TEST_F(DeviceDataCacheTest, CanCreate)
    {
        ASSERT_NE(cache, nullptr);
        EXPECT_EQ(cache->GetDeviceUuid(), deviceUuid);
    }

    // Test: GetDeviceUuid returns the correct UUID
    TEST_F(DeviceDataCacheTest, GetDeviceUuid)
    {
        EXPECT_EQ(cache->GetDeviceUuid(), deviceUuid);
    }

    // Test: Initial last report timestamp is zero
    TEST_F(DeviceDataCacheTest, InitialLastReportTimestamp)
    {
        EXPECT_EQ(cache->GetLastReportCompletedTimestamp(), 0);
    }

    // Test: GetVendorName returns error when cache is not initialized
    TEST_F(DeviceDataCacheTest, GetVendorNameWhenNotInitialized)
    {
        std::string value;
        EXPECT_NE(cache->GetVendorName(value), CHIP_NO_ERROR);
    }

    // Test: GetProductName returns error when cache is not initialized
    TEST_F(DeviceDataCacheTest, GetProductNameWhenNotInitialized)
    {
        std::string value;
        EXPECT_NE(cache->GetProductName(value), CHIP_NO_ERROR);
    }

    // Test: GetHardwareVersionString returns error when cache is not initialized
    TEST_F(DeviceDataCacheTest, GetHardwareVersionStringWhenNotInitialized)
    {
        std::string value;
        EXPECT_NE(cache->GetHardwareVersionString(value), CHIP_NO_ERROR);
    }

    // Test: GetSoftwareVersionString returns error when cache is not initialized
    TEST_F(DeviceDataCacheTest, GetSoftwareVersionStringWhenNotInitialized)
    {
        std::string value;
        EXPECT_NE(cache->GetSoftwareVersionString(value), CHIP_NO_ERROR);
    }

    // Test: GetSerialNumber returns error when cache is not initialized
    TEST_F(DeviceDataCacheTest, GetSerialNumberWhenNotInitialized)
    {
        std::string value;
        EXPECT_NE(cache->GetSerialNumber(value), CHIP_NO_ERROR);
    }

    // Test: GetMacAddress returns error when cache is not initialized
    TEST_F(DeviceDataCacheTest, GetMacAddressWhenNotInitialized)
    {
        std::string value;
        EXPECT_NE(cache->GetMacAddress(value), CHIP_NO_ERROR);
    }

    // Test: GetNetworkType returns error when cache is not initialized
    TEST_F(DeviceDataCacheTest, GetNetworkTypeWhenNotInitialized)
    {
        std::string value;
        EXPECT_NE(cache->GetNetworkType(value), CHIP_NO_ERROR);
    }

    // Test: GetEndpointIds returns empty vector when cache is not initialized
    TEST_F(DeviceDataCacheTest, GetEndpointIdsWhenNotInitialized)
    {
        auto endpoints = cache->GetEndpointIds();
        EXPECT_TRUE(endpoints.empty());
    }

    // Test: EndpointHasServerCluster returns false when cache is not initialized
    TEST_F(DeviceDataCacheTest, EndpointHasServerClusterWhenNotInitialized)
    {
        EXPECT_FALSE(cache->EndpointHasServerCluster(0, 6)); // On/Off cluster
    }

    // Test: GetCacheAsJson returns valid JSON structure when not initialized
    TEST_F(DeviceDataCacheTest, GetCacheAsJsonWhenNotInitialized)
    {
        Json::Value json = cache->GetCacheAsJson();
        EXPECT_TRUE(json.isObject());
        EXPECT_TRUE(json.isMember("lastReportCompletedTimestamp"));
        EXPECT_EQ(json["lastReportCompletedTimestamp"].asUInt64(), 0);
        EXPECT_TRUE(json.isMember("endpoints"));
    }

    // Test: Multiple DeviceDataCache instances can coexist
    TEST_F(DeviceDataCacheTest, MultipleInstancesCanCoexist)
    {
        auto cache1 = std::make_shared<DeviceDataCache>("device-1", nullptr);
        auto cache2 = std::make_shared<DeviceDataCache>("device-2", nullptr);

        EXPECT_NE(cache1, nullptr);
        EXPECT_NE(cache2, nullptr);
        EXPECT_EQ(cache1->GetDeviceUuid(), "device-1");
        EXPECT_EQ(cache2->GetDeviceUuid(), "device-2");
    }

    // Test: Destructor completes without error
    TEST_F(DeviceDataCacheTest, DestructorCompletesWithoutError)
    {
        auto tempCache = std::make_shared<DeviceDataCache>("temp-device", nullptr);
        EXPECT_NO_THROW(tempCache.reset());
    }

    // Test: Cache UUID cannot be empty
    TEST_F(DeviceDataCacheTest, UuidIsNotEmpty)
    {
        EXPECT_FALSE(cache->GetDeviceUuid().empty());
    }

    // Test: Different caches have different UUIDs
    TEST_F(DeviceDataCacheTest, DifferentCachesHaveDifferentUuids)
    {
        auto cache1 = std::make_shared<DeviceDataCache>("device-1", nullptr);
        auto cache2 = std::make_shared<DeviceDataCache>("device-2", nullptr);

        EXPECT_NE(cache1->GetDeviceUuid(), cache2->GetDeviceUuid());
    }

} // namespace
