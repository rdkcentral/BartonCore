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

/*
 * Created by Raiyan Chowdhury on 11/27/23.
 */

#include "../MatterDeviceDriver.h"
#include "app/AttributePathParams.h"
#include "app/ConcreteAttributePath.h"
#include "app/ConcreteClusterPath.h"
#include "app/ConcreteEventPath.h"
#include "matter/clusters/MatterCluster.h"
#include "matter/subscriptions/SubscribeInteraction.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#define TEST_DEVICE_ID "0x1111"

namespace barton
{
    class MockCluster : public MatterCluster
    {
    public:
        MockCluster(EventHandler *handler, std::string deviceId, chip::EndpointId endpointId) :
            MatterCluster(handler, deviceId, endpointId)
        {
        }

        class EventHandler : public MatterCluster::EventHandler
        {
        };

        MOCK_METHOD(SubscriptionIntervalSecs, GetDesiredSubscriptionIntervalSecs, (), (override));

        MOCK_METHOD(void,
                    OnResponse,
                    (chip::app::CommandSender * apCommandSender,
                     const chip::app::ConcreteCommandPath &aPath,
                     const chip::app::StatusIB &aStatusIB,
                     chip::TLV::TLVReader *apData),
                    (override));

        MOCK_METHOD(void, OnError, (const chip::app::CommandSender *apCommandSender, CHIP_ERROR aError), (override));

        MOCK_METHOD(void,
                    OnResponse,
                    (const chip::app::WriteClient *apWriteClient,
                     const chip::app::ConcreteDataAttributePath &path,
                     chip::app::StatusIB status),
                    (override));

        MOCK_METHOD(void, OnError, (const chip::app::WriteClient *apWriteClient, CHIP_ERROR aError), (override));
    };

    class MockMatterDeviceDriver : public MatterDeviceDriver,
                                   MockCluster::EventHandler
    {
    public:
        MockMatterDeviceDriver() : MatterDeviceDriver("matterMock", "mock", 0) {}

        std::unique_ptr<MatterCluster>
        MakeCluster(std::string const &deviceUuid, chip::EndpointId endpointId, chip::ClusterId clusterId) override
        {
            return std::make_unique<MockCluster>((MockCluster::EventHandler *) this, deviceUuid, endpointId);
        }

        MOCK_METHOD(bool, ClaimDevice, (DiscoveredDeviceDetails * details), (override));

        MOCK_METHOD(uint32_t, GetCommFailTimeoutSecs, (const char *deviceUuid), (override));

        MOCK_METHOD(std::vector<MatterCluster *>, GetClustersToSubscribeTo, (const std::string &deviceId), (override));

    private:
        std::vector<MatterCluster *> GetCommonClustersToSubscribeTo(const std::string &deviceId) override { return {}; }
    };

    class MatterConfigureSubscriptionSpecsTest : public ::testing::Test
    {
    public:
        MatterConfigureSubscriptionSpecsTest() {}

        void SetUp() override { deviceId = TEST_DEVICE_ID; }

        SubscriptionIntervalSecs CalculateFinalSubscriptionIntervalSecs()
        {
            return mockDriver.CalculateFinalSubscriptionIntervalSecs(deviceId);
        }

        MockMatterDeviceDriver mockDriver;
        std::string deviceId;
    };


    using namespace ::testing;

    /**
     * @brief Tests the MatterDeviceDriver's CalculateFinalSubscriptionIntervalSecs() method.
     *        Given multiple clusters that ask for conflicting min interval floors and max interval ceilings, the
     *        driver should choose the higher floor and lower ceiling. Tests once with 2 clusters and then again
     *        with 3 clusters.
     */
    TEST_F(MatterConfigureSubscriptionSpecsTest, ConflictingIntervalParamsTest)
    {
        EXPECT_CALL(mockDriver, GetCommFailTimeoutSecs(_)).Times(2).WillRepeatedly(Return(4000));

        std::vector<MatterCluster *> clusters;

        auto heatCluster = mockDriver.MakeCluster(deviceId, 1, 1);
        SubscriptionIntervalSecs heatClusterIntervalSecs(1, 300);
        EXPECT_CALL(*(dynamic_cast<MockCluster *>(heatCluster.get())), GetDesiredSubscriptionIntervalSecs())
            .Times(2)
            .WillRepeatedly(Return(heatClusterIntervalSecs));

        auto volumeCluster = mockDriver.MakeCluster(deviceId, 1, 2);
        SubscriptionIntervalSecs volumeClusterIntervalSecs(60, 3600);
        EXPECT_CALL(*(dynamic_cast<MockCluster *>(volumeCluster.get())), GetDesiredSubscriptionIntervalSecs())
            .Times(2)
            .WillRepeatedly(Return(volumeClusterIntervalSecs));

        clusters.push_back(heatCluster.get());
        clusters.push_back(volumeCluster.get());
        EXPECT_CALL(mockDriver, GetClustersToSubscribeTo(_)).WillOnce(Return(clusters));

        auto intervalParams = CalculateFinalSubscriptionIntervalSecs();
        ASSERT_EQ(intervalParams.minIntervalFloorSecs, 60);
        ASSERT_EQ(intervalParams.maxIntervalCeilingSecs, 300);

        std::vector<MatterCluster *> clusters2;

        auto wifiCluster = mockDriver.MakeCluster(deviceId, 2, 3);
        SubscriptionIntervalSecs wifiClusterIntervalSecs(90, 3000);
        EXPECT_CALL(*(dynamic_cast<MockCluster *>(wifiCluster.get())), GetDesiredSubscriptionIntervalSecs())
            .WillOnce(Return(wifiClusterIntervalSecs));

        clusters2.push_back(heatCluster.get());
        clusters2.push_back(volumeCluster.get());
        clusters2.push_back(wifiCluster.get());
        EXPECT_CALL(mockDriver, GetClustersToSubscribeTo(_)).WillOnce(Return(clusters2));

        intervalParams = CalculateFinalSubscriptionIntervalSecs();
        ASSERT_EQ(intervalParams.minIntervalFloorSecs, 90);
        ASSERT_EQ(intervalParams.maxIntervalCeilingSecs, 300);
    }

    /**
     * @brief Tests the MatterDeviceDriver's CalculateFinalSubscriptionIntervalSecs() method.
     *        The driver must ensure that the max interval ceiling is less than the commfail timeout,
     *        even if the clusters want a higher max interval ceiling.
     */
    TEST_F(MatterConfigureSubscriptionSpecsTest, MaxIntervalCeilingLessThanCommfailTimeoutTest)
    {
        EXPECT_CALL(mockDriver, GetCommFailTimeoutSecs(_)).WillOnce(Return(4000));

        std::vector<MatterCluster *> clusters;

        auto grapeCluster = mockDriver.MakeCluster(deviceId, 1, 2);
        SubscriptionIntervalSecs grapeClusterIntervalSecs(1, 5000);
        EXPECT_CALL(*(dynamic_cast<MockCluster *>(grapeCluster.get())), GetDesiredSubscriptionIntervalSecs())
            .WillOnce(Return(grapeClusterIntervalSecs));

        auto bananaCluster = mockDriver.MakeCluster(deviceId, 2, 3);
        SubscriptionIntervalSecs bananaClusterIntervalSecs(1, 5000);
        EXPECT_CALL(*(dynamic_cast<MockCluster *>(bananaCluster.get())), GetDesiredSubscriptionIntervalSecs())
            .WillOnce(Return(bananaClusterIntervalSecs));

        clusters.push_back(grapeCluster.get());
        clusters.push_back(bananaCluster.get());
        EXPECT_CALL(mockDriver, GetClustersToSubscribeTo(_)).WillOnce(Return(clusters));

        auto intervalParams = CalculateFinalSubscriptionIntervalSecs();
        ASSERT_EQ(intervalParams.minIntervalFloorSecs, 1);
        ASSERT_LT(intervalParams.maxIntervalCeilingSecs, 4000);
    }

    /**
     * @brief Tests the MatterDeviceDriver's CalculateFinalSubscriptionIntervalSecs() method.
     *        The driver must ensure that the min interval floor is not greater than the max interval ceiling.
     *        If by some chance the highest floor from the cluster servers is greater than the lowest ceiling,
     *        then the driver must correct this somehow.
     */
    TEST_F(MatterConfigureSubscriptionSpecsTest, FloorGreaterThanCeilingTest)
    {
        EXPECT_CALL(mockDriver, GetCommFailTimeoutSecs(_)).Times(2).WillRepeatedly(Return(4000));

        std::vector<MatterCluster *> clusters;

        auto oneCluster = mockDriver.MakeCluster(deviceId, 1, 1);
        SubscriptionIntervalSecs oneClusterIntervalSecs(60, 30);
        EXPECT_CALL(*(dynamic_cast<MockCluster *>(oneCluster.get())), GetDesiredSubscriptionIntervalSecs())
            .WillOnce(Return(oneClusterIntervalSecs));

        clusters.push_back(oneCluster.get());
        EXPECT_CALL(mockDriver, GetClustersToSubscribeTo(_)).WillOnce(Return(clusters));

        auto intervalParams = CalculateFinalSubscriptionIntervalSecs();
        ASSERT_LE(intervalParams.minIntervalFloorSecs, intervalParams.maxIntervalCeilingSecs);

        std::vector<MatterCluster *> clusters2;

        auto twoCluster = mockDriver.MakeCluster(deviceId, 1, 2);
        SubscriptionIntervalSecs twoClusterIntervalSecs(1, 40);
        EXPECT_CALL(*(dynamic_cast<MockCluster *>(twoCluster.get())), GetDesiredSubscriptionIntervalSecs())
            .WillOnce(Return(twoClusterIntervalSecs));

        auto anotherCluster = mockDriver.MakeCluster(deviceId, 3, 17);
        SubscriptionIntervalSecs anotherClusterIntervalSecs(80, 300);
        EXPECT_CALL(*(dynamic_cast<MockCluster *>(anotherCluster.get())), GetDesiredSubscriptionIntervalSecs())
            .WillOnce(Return(anotherClusterIntervalSecs));

        clusters2.push_back(twoCluster.get());
        clusters2.push_back(anotherCluster.get());
        EXPECT_CALL(mockDriver, GetClustersToSubscribeTo(_)).WillOnce(Return(clusters2));

        intervalParams = CalculateFinalSubscriptionIntervalSecs();
        ASSERT_LE(intervalParams.minIntervalFloorSecs, intervalParams.maxIntervalCeilingSecs);
    }
} // namespace barton
