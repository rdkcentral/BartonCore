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

//
// Created by nkhan599 on 3/27/24.
//

#pragma once

#include "MatterCluster.h"
#include "app-common/zap-generated/ids/Attributes.h"
#include "app-common/zap-generated/ids/Clusters.h"
#include "app/ClusterStateCache.h"
#include "app/ConcreteAttributePath.h"
#include "app/ReadHandler.h"
#include "lib/core/CHIPError.h"
#include "lib/core/DataModelTypes.h"
#include "subsystems/matter/MatterCommon.h"

namespace barton
{
    class GeneralDiagnostics : public MatterCluster
    {
    public:
        GeneralDiagnostics(EventHandler *handler, const std::string deviceId, chip::EndpointId endpointId,
                          std::shared_ptr<DeviceDataCache> deviceDataCache) :
            MatterCluster(handler, deviceId, endpointId, chip::app::Clusters::GeneralDiagnostics::Id, deviceDataCache)
        {
        }

        class EventHandler : public MatterCluster::EventHandler
        {
        public:
            virtual void OnNetworkInterfacesChanged(GeneralDiagnostics &source,
                                                    NetworkUtils::NetworkInterfaceInfo info) {};

            virtual void OnHardwareFaultsChanged(
                GeneralDiagnostics &source,
                const std::vector<chip::app::Clusters::GeneralDiagnostics::HardwareFaultEnum> &currentFaults) {};
        };

        void OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                const chip::app::ConcreteAttributePath &path) override;

        std::string GetMacAddress();
        std::string GetNetworkType();
    };
} // namespace barton
