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

#include "app/AttributePathParams.h"
#include "app/ConcreteAttributePath.h"
#include "app/ConcreteClusterPath.h"
#include "app/ConcreteEventPath.h"
#include "matter/MatterDeviceDriver.h"
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

        MOCK_METHOD(SubscriptionIntervalSecs, GetDesiredSubscriptionIntervalSecs, (), (override));
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
     *        The driver must ensure that the max interval ceiling is less than the commfail timeout,
     *        even if the driver want a higher max interval ceiling.
     */
    TEST_F(MatterConfigureSubscriptionSpecsTest, MaxIntervalCeilingLessThanCommfailTimeoutTest)
    {
        EXPECT_CALL(mockDriver, GetCommFailTimeoutSecs(_)).WillOnce(Return(4000));

        std::vector<MatterCluster *> clusters;

        SubscriptionIntervalSecs desiredIntervalSecs(1, 5000);
        EXPECT_CALL(mockDriver, GetDesiredSubscriptionIntervalSecs()).WillOnce(Return(desiredIntervalSecs));

        auto intervalParams = CalculateFinalSubscriptionIntervalSecs();
        ASSERT_EQ(intervalParams.minIntervalFloorSecs, 1);
        ASSERT_LT(intervalParams.maxIntervalCeilingSecs, 4000);
    }

    /**
     * @brief Tests the MatterDeviceDriver's CalculateFinalSubscriptionIntervalSecs() method.
     *        The driver must ensure that the min interval floor is not greater than the max interval ceiling.
     *        If by some chance the highest floor is greater than the lowest ceiling, then the driver must correct this somehow.
     */
    TEST_F(MatterConfigureSubscriptionSpecsTest, FloorGreaterThanCeilingTest)
    {
        EXPECT_CALL(mockDriver, GetCommFailTimeoutSecs(_)).WillRepeatedly(Return(4000));

        std::vector<MatterCluster *> clusters;

        SubscriptionIntervalSecs desiredIntervalSecs(60, 30);
        EXPECT_CALL(mockDriver, GetDesiredSubscriptionIntervalSecs()).WillOnce(Return(desiredIntervalSecs));

        auto intervalParams = CalculateFinalSubscriptionIntervalSecs();
        ASSERT_LE(intervalParams.minIntervalFloorSecs, intervalParams.maxIntervalCeilingSecs);
    }
} // namespace barton
