// ------------------------------ tabstop = 4 ----------------------------------
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
// ------------------------------ tabstop = 4 ----------------------------------

//
// Created by Raiyan Chowdhury on 3/4/2026.
//

#pragma once

#include "MatterCluster.h"
#include "app/ClusterStateCache.h"
#include "app/ConcreteAttributePath.h"
#include "lib/core/DataModelTypes.h"
#include <string>
#include <app-common/zap-generated/ids/Clusters.h>

namespace barton
{
    class Identify : public MatterCluster
    {
    public:
        class EventHandler : public MatterCluster::EventHandler
        {
        public:
            virtual void IdentifyTimeChanged(const std::string &deviceUuid, uint16_t identifyTimeSecs) {};
        };

        void OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                const chip::app::ConcreteAttributePath &path) override;

    protected:
        Identify(EventHandler *handler, const std::string deviceId, chip::EndpointId endpointId,
                 std::shared_ptr<DeviceDataCache> deviceDataCache) :
            MatterCluster(handler, deviceId, endpointId, chip::app::Clusters::Identify::Id, deviceDataCache)
        {
        }

    private:
        template <typename T, typename... Args>
        friend std::shared_ptr<T> MatterCluster::Create(Args&&... args);
    };
} // namespace barton

