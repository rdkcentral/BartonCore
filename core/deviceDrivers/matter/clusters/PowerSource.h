// ------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2025 Comcast Cable Communications Management, LLC
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
// Created by Raiyan Chowdhury on 8/16/24.
//

#pragma once

#include "MatterCluster.h"
#include "app/ClusterStateCache.h"
#include "app/ConcreteAttributePath.h"
#include "lib/core/CHIPError.h"
#include "lib/core/DataModelTypes.h"
#include <string>

namespace barton
{
    using namespace chip::app::Clusters::PowerSource;

    class PowerSource : public MatterCluster
    {
    public:
        PowerSource(EventHandler *handler, const std::string deviceId, chip::EndpointId endpointId) :
            MatterCluster(handler, deviceId, endpointId)
        {
        }

        class EventHandler : public MatterCluster::EventHandler
        {
        public:
            virtual void BatChargeLevelChanged(std::string &deviceUuid, BatChargeLevelEnum chargeLevel) {};
            virtual void BatPercentRemainingChanged(std::string &deviceUuid, uint8_t halfPercent) {};
        };

        bool IsWiredPowerSource(CHIP_ERROR &err);

        bool IsBatteryPowerSource(CHIP_ERROR &err);

        BatChargeLevelEnum GetBatChargeLevel();

        CHIP_ERROR GetBatPercentRemaining(uint8_t &halfIntPercent);

        void OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                const chip::app::ConcreteAttributePath &path) override;

    private:
        uint32_t GetFeatureMap(CHIP_ERROR &err);

        // Matter 1.4 section 11.7.4.
        typedef enum
        {
            WIRED = 1,
            BAT = WIRED << 1,
            RECHG = BAT << 1,
            REPLC = RECHG << 1
        } pwrSrcFeatures;
    };
} // namespace barton
