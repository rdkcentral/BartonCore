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

#pragma once

#include "MatterCluster.h"
#include "app-common/zap-generated/ids/Attributes.h"
#include "app-common/zap-generated/ids/Clusters.h"
#include "app/ClusterStateCache.h"
#include "app/ConcreteAttributePath.h"
#include "app/ReadHandler.h"
#include "lib/core/CHIPError.h"
#include "lib/core/DataModelTypes.h"

namespace barton
{
    using namespace chip::app::Clusters::BasicInformation;
    class BasicInformation : public MatterCluster
    {
    public:
        BasicInformation(EventHandler *handler, const std::string deviceId, chip::EndpointId endpointId) :
            MatterCluster(handler, deviceId, endpointId)
        {
        }

        class EventHandler : public MatterCluster::EventHandler
        {
        public:
            virtual void OnSoftwareVersionChanged(BasicInformation &source, uint32_t softwareVersion) {};
            virtual void OnSoftwareVersionStringChanged(BasicInformation &source,
                                                        const std::string &softwareVersionString) {};
            virtual void OnSerialNumberChanged(BasicInformation &source, const std::string &serialNumber) {};
        };

        SubscriptionIntervalSecs GetDesiredSubscriptionIntervalSecs() override
        {
            return {1, kSubscriptionMaxIntervalPublisherLimit};
        }

        std::string GetFirmwareVersionString();

        void OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                const chip::app::ConcreteAttributePath &path) override;
    };
} // namespace barton
