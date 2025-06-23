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

#include "app-common/zap-generated/ids/Attributes.h"
#include "app-common/zap-generated/ids/Clusters.h"
#include "app-common/zap-generated/ids/Events.h"
#include "app/AttributePathParams.h"
#include "lib/core/DataModelTypes.h"
#include "lib/core/ScopedNodeId.h"
#include "messaging/ReliableMessageProtocolConfig.h"
#include "protocols/secure_channel/CASESession.h"
#include "subsystems/matter/ReadPrepareParamsBuilder.hpp"
#include "subsystems/matter/ScopedReadPrepareParams.hpp"
#include "system/SystemClock.h"
#include "transport/SecureSession.h"
#include "transport/Session.h"
#include <cassert>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

using namespace barton;

class FakeSession : public chip::Transport::Session
{
public:
    FakeSession() {}

    SessionType GetSessionType() const override { return SessionType::kSecure; }

    void Release() override {}

    void Retain() override {}

    bool IsActiveSession() const override { return true; }

    chip::ScopedNodeId GetPeer() const override { return {}; }

    chip::ScopedNodeId GetLocalScopedNodeId() const override { return {}; };

    chip::Access::SubjectDescriptor GetSubjectDescriptor() const override { return {}; }

    bool AllowsMRP() const override { return false; }

    bool AllowsLargePayload() const override { return false; }

    const chip::SessionParameters &GetRemoteSessionParameters() const override { return remoteSessionParams; }

    chip::System::Clock::Timestamp GetMRPBaseTimeout() const override { return chip::System::Clock::kZero; }

    chip::System::Clock::Milliseconds32 GetAckTimeout() const override { return chip::System::Clock::kZero; }

    chip::System::Clock::Milliseconds32
    GetMessageReceiptTimeout(chip::System::Clock::Timestamp ourLastActivity) const override
    {
        return chip::System::Clock::kZero;
    }

private:
    chip::SessionParameters remoteSessionParams;
};

class MatterReadPrepareParamsTest : public ::testing::Test
{
protected:
    MatterReadPrepareParamsTest() : fakeHandle(fakeSession) {}

    FakeSession fakeSession;
    const chip::SessionHandle fakeHandle;
    std::vector<chip::app::EventPathParams> events;
    std::vector<chip::app::AttributePathParams> attributes;

    void SetUp() override
    {
        events.clear();
        attributes.clear();
    }
};

namespace
{
    TEST_F(MatterReadPrepareParamsTest, MakesEmptyParams)
    {
        auto params = std::make_unique<ScopedReadPrepareParams>(attributes, events, fakeHandle);

        ASSERT_NE(params, nullptr);
        auto &sdkParams = params->Get();

        ASSERT_EQ(sdkParams.mpEventPathParamsList, nullptr);
        ASSERT_EQ(sdkParams.mEventPathParamsListSize, 0);

        ASSERT_EQ(sdkParams.mpAttributePathParamsList, nullptr);
        ASSERT_EQ(sdkParams.mAttributePathParamsListSize, 0);
    }

    TEST_F(MatterReadPrepareParamsTest, MakesAttributeOnlyParams)
    {

        attributes.push_back({0,
                              chip::app::Clusters::BasicInformation::Id,
                              chip::app::Clusters::BasicInformation::Attributes::HardwareVersion::Id});

        attributes.push_back({0,
                              chip::app::Clusters::BasicInformation::Id,
                              chip::app::Clusters::BasicInformation::Attributes::SoftwareVersion::Id});

        auto params = std::make_unique<ScopedReadPrepareParams>(attributes, events, fakeHandle);
        ASSERT_NE(params, nullptr);

        auto &sdkParams = params->Get();
        ASSERT_EQ(sdkParams.mEventPathParamsListSize, 0);
        ASSERT_EQ(sdkParams.mAttributePathParamsListSize, attributes.size());

        for (size_t i = 0; i < attributes.size(); i++)
        {
            ASSERT_TRUE(sdkParams.mpAttributePathParamsList[i].IsValidAttributePath());
            ASSERT_EQ(sdkParams.mpAttributePathParamsList[i].mEndpointId, 0);
            ASSERT_EQ(sdkParams.mpAttributePathParamsList[i].mClusterId, chip::app::Clusters::BasicInformation::Id);
        }

        ASSERT_EQ(sdkParams.mpAttributePathParamsList[0].mAttributeId,
                  chip::app::Clusters::BasicInformation::Attributes::HardwareVersion::Id);

        ASSERT_EQ(sdkParams.mpAttributePathParamsList[1].mAttributeId,
                  chip::app::Clusters::BasicInformation::Attributes::SoftwareVersion::Id);
    }

    TEST_F(MatterReadPrepareParamsTest, MakesEventOnlyParams)
    {
        events.push_back(
            {0, chip::app::Clusters::BasicInformation::Id, chip::app::Clusters::BasicInformation::Events::StartUp::Id});

        events.push_back({0,
                          chip::app::Clusters::BasicInformation::Id,
                          chip::app::Clusters::BasicInformation::Events::ShutDown::Id});

        auto params = std::make_unique<ScopedReadPrepareParams>(attributes, events, fakeHandle);
        ASSERT_NE(params, nullptr);

        auto &sdkParams = params->Get();
        ASSERT_EQ(sdkParams.mAttributePathParamsListSize, 0);
        ASSERT_EQ(sdkParams.mEventPathParamsListSize, events.size());

        for (size_t i = 0; i < events.size(); i++)
        {
            ASSERT_TRUE(sdkParams.mpEventPathParamsList[i].IsValidEventPath());
            ASSERT_EQ(sdkParams.mpEventPathParamsList[i].mEndpointId, 0);
            ASSERT_EQ(sdkParams.mpEventPathParamsList[i].mClusterId, chip::app::Clusters::BasicInformation::Id);
        }

        ASSERT_EQ(sdkParams.mpEventPathParamsList[0].mEventId,
                  chip::app::Clusters::BasicInformation::Events::StartUp::Id);

        ASSERT_EQ(sdkParams.mpEventPathParamsList[1].mEventId,
                  chip::app::Clusters::BasicInformation::Events::ShutDown::Id);
    }

    TEST_F(MatterReadPrepareParamsTest, MakesEventAttrParams)
    {
        events.push_back(
            {0, chip::app::Clusters::BasicInformation::Id, chip::app::Clusters::BasicInformation::Events::StartUp::Id});

        events.push_back({0,
                          chip::app::Clusters::BasicInformation::Id,
                          chip::app::Clusters::BasicInformation::Events::ShutDown::Id});

        attributes.push_back({0,
                              chip::app::Clusters::BasicInformation::Id,
                              chip::app::Clusters::BasicInformation::Attributes::HardwareVersion::Id});

        attributes.push_back({0,
                              chip::app::Clusters::BasicInformation::Id,
                              chip::app::Clusters::BasicInformation::Attributes::SoftwareVersion::Id});

        auto params = std::make_unique<ScopedReadPrepareParams>(attributes, events, fakeHandle);
        ASSERT_NE(params, nullptr);

        auto &sdkParams = params->Get();

        ASSERT_EQ(sdkParams.mAttributePathParamsListSize, attributes.size());
        ASSERT_EQ(sdkParams.mEventPathParamsListSize, events.size());
    }

    TEST_F(MatterReadPrepareParamsTest, BuildsValidAttrParams)
    {
        ReadPrepareParamsBuilder builder(0, chip::app::Clusters::BasicInformation::Id);


        auto params = builder.AddAttribute(chip::app::Clusters::BasicInformation::Attributes::HardwareVersion::Id)
                          .AddAttribute(chip::app::Clusters::BasicInformation::Attributes::SoftwareVersion::Id)
                          .Build(fakeHandle);

        ASSERT_NE(params, nullptr);

        ASSERT_EQ(params->Get().mAttributePathParamsListSize, 2);
        ASSERT_EQ(params->Get().mEventPathParamsListSize, 0);
    }

    TEST_F(MatterReadPrepareParamsTest, BuildsValidEventParams)
    {
        ReadPrepareParamsBuilder builder(0, chip::app::Clusters::BasicInformation::Id);


        auto params = builder.AddEvent(chip::app::Clusters::BasicInformation::Events::StartUp::Id)
                          .AddEvent(chip::app::Clusters::BasicInformation::Events::ShutDown::Id)
                          .Build(fakeHandle);

        ASSERT_NE(params, nullptr);

        ASSERT_EQ(params->Get().mAttributePathParamsListSize, 0);
        ASSERT_EQ(params->Get().mEventPathParamsListSize, 2);
    }

    TEST_F(MatterReadPrepareParamsTest, RejectsEmptySet)
    {
        ReadPrepareParamsBuilder builder(0, chip::app::Clusters::BasicInformation::Id);
        ASSERT_EQ(builder.Build(fakeHandle), nullptr);
    }

    TEST_F(MatterReadPrepareParamsTest, RejectsBadFloor)
    {
        ReadPrepareParamsBuilder builder(0, chip::app::Clusters::BasicInformation::Id);
        builder.AddAttribute(chip::app::Clusters::BasicInformation::Attributes::HardwareVersion::Id, 0, 120);
        ASSERT_EQ(builder.Build(fakeHandle), nullptr);
    }

    TEST_F(MatterReadPrepareParamsTest, RejectsBadCeiling)
    {
        ReadPrepareParamsBuilder builder(0, chip::app::Clusters::BasicInformation::Id);
        builder.AddAttribute(chip::app::Clusters::BasicInformation::Attributes::HardwareVersion::Id, 120, 60);
        ASSERT_EQ(builder.Build(fakeHandle), nullptr);
    }

    TEST_F(MatterReadPrepareParamsTest, BuildsValidIntervalAttr)
    {
        ReadPrepareParamsBuilder builder(0, chip::app::Clusters::BasicInformation::Id);
        builder.AddAttribute(chip::app::Clusters::BasicInformation::Attributes::HardwareVersion::Id, 1, 900);
        auto params = builder.Build(fakeHandle);
        ASSERT_NE(params, nullptr);

        ASSERT_EQ(params->Get().mMinIntervalFloorSeconds, 1);
        ASSERT_EQ(params->Get().mMaxIntervalCeilingSeconds, 900);
    }

    TEST_F(MatterReadPrepareParamsTest, BuildsValidIntervalEvent)
    {
        ReadPrepareParamsBuilder builder(0, chip::app::Clusters::BasicInformation::Id);
        builder.AddEvent(chip::app::Clusters::BasicInformation::Events::StartUp::Id, 1, 900);
        auto params = builder.Build(fakeHandle);
        ASSERT_NE(params, nullptr);

        ASSERT_EQ(params->Get().mMinIntervalFloorSeconds, 1);
        ASSERT_EQ(params->Get().mMaxIntervalCeilingSeconds, 900);
    }

    TEST_F(MatterReadPrepareParamsTest, RatchetsInterval)
    {
        ReadPrepareParamsBuilder builder(0, chip::app::Clusters::BasicInformation::Id);
        builder.AddAttribute(chip::app::Clusters::BasicInformation::Attributes::Location::Id, 10, 300)
            .AddEvent(chip::app::Clusters::BasicInformation::Events::ShutDown::Id, 1, 3600)
            .AddEvent(chip::app::Clusters::BasicInformation::Events::StartUp::Id, 5, 900);


        auto params = builder.Build(fakeHandle);
        ASSERT_NE(params, nullptr);

        ASSERT_EQ(params->Get().mMinIntervalFloorSeconds, 1);
        ASSERT_EQ(params->Get().mMaxIntervalCeilingSeconds, 3600);
    }
}; // namespace
