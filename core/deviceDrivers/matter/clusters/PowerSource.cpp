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

#define LOG_TAG     "PowerSourceCluster"
#define logFmt(fmt) "(%s): " fmt, __func__

extern "C" {
#include <icLog/logging.h>
}

#include "PowerSource.h"
#include "app/ConcreteAttributePath.h"
#include "lib/core/CHIPError.h"

namespace barton
{
    bool PowerSource::IsBatteryPowerSource(CHIP_ERROR &err)
    {
        bool isBattery = false;
        auto featureMap = GetFeatureMap(err);
        if (err == CHIP_NO_ERROR)
        {
            isBattery = featureMap & pwrSrcFeatures::BAT;
        }
        return isBattery;
    }

    bool PowerSource::IsWiredPowerSource(CHIP_ERROR &err)
    {
        bool isWired = false;
        auto featureMap = GetFeatureMap(err);
        if (err == CHIP_NO_ERROR)
        {
            isWired = featureMap & pwrSrcFeatures::WIRED;
        }
        return isWired;
    }

    uint32_t PowerSource::GetFeatureMap(CHIP_ERROR &err)
    {
        err = CHIP_ERROR_INTERNAL;

        std::shared_ptr<chip::app::ClusterStateCache> cache;
        if (!(cache = clusterStateCacheRef.lock()))
        {
            icError("Cluster state cache not available");
            return 0;
        }

        using namespace chip::app;
        using TypeInfo = Attributes::FeatureMap::TypeInfo;

        TypeInfo::DecodableType value;
        chip::app::ConcreteAttributePath path(endpointId, Clusters::PowerSource::Id, Attributes::FeatureMap::Id);
        err = cache->Get<TypeInfo>(path, value);
        if (err != CHIP_NO_ERROR)
        {
            icWarn("Failed to read FeatureMap attribute for device %s: %s", deviceId.c_str(), err.AsString());
        }

        return value;
    }

    // The battery percentage is measured in half percent units (0-200), where e.g. 50 = 25%
    CHIP_ERROR PowerSource::GetBatPercentRemaining(uint8_t &halfPercent)
    {
        icDebug();

        CHIP_ERROR err = CHIP_ERROR_INTERNAL;
        std::shared_ptr<chip::app::ClusterStateCache> cache;
        if (!(cache = clusterStateCacheRef.lock()))
        {
            icError("Cluster state cache not available");
            return err;
        }

        using namespace chip::app;
        using TypeInfo = Attributes::BatPercentRemaining::TypeInfo;

        TypeInfo::DecodableType value;
        chip::app::ConcreteAttributePath path(
            endpointId, Clusters::PowerSource::Id, Attributes::BatPercentRemaining::Id);
        err = cache->Get<TypeInfo>(path, value);
        if (err == CHIP_NO_ERROR)
        {
            halfPercent = value.Value();
        }
        else
        {
            // Optional for battery-powered devices to support per Matter 1.4 11.7.7, so no need to warn.
            icInfo("Failed to read BatPercentRemaining attribute for device %s: %s", deviceId.c_str(), err.AsString());
        }

        return err;
    }

    BatChargeLevelEnum PowerSource::GetBatChargeLevel()
    {
        icDebug();

        BatChargeLevelEnum chargeLevel = BatChargeLevelEnum::kUnknownEnumValue;

        std::shared_ptr<chip::app::ClusterStateCache> cache;
        if (!(cache = clusterStateCacheRef.lock()))
        {
            icError("Cluster state cache not available");
            return chargeLevel;
        }

        using namespace chip::app;
        using TypeInfo = Attributes::BatChargeLevel::TypeInfo;

        TypeInfo::DecodableType value;
        chip::app::ConcreteAttributePath path(endpointId, Clusters::PowerSource::Id, Attributes::BatChargeLevel::Id);
        CHIP_ERROR err = cache->Get<TypeInfo>(path, value);
        if (err == CHIP_NO_ERROR)
        {
            chargeLevel = value;
        }
        else
        {
            // Mandatory for battery-powered devices to support per Matter 1.4 11.7.7, so we warn.
            icWarn("Failed to read BatChargeLevel attribute for device %s: %s", deviceId.c_str(), err.AsString());
        }

        return chargeLevel;
    }

    void PowerSource::OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                         const chip::app::ConcreteAttributePath &path)
    {
        using namespace chip::app;

        if (path.mEndpointId != endpointId || path.mClusterId != Clusters::PowerSource::Id)
        {
            return;
        }

        switch (path.mAttributeId)
        {
            case Attributes::BatChargeLevel::Id:
            {
                using TypeInfo = Attributes::BatChargeLevel::TypeInfo;

                TypeInfo::DecodableType value;
                CHIP_ERROR error = cache->Get<TypeInfo>(path, value);

                if (error == CHIP_NO_ERROR)
                {
                    static_cast<PowerSource::EventHandler *>(eventHandler)->BatChargeLevelChanged(deviceId, value);
                }
                else
                {
                    icWarn("Failed to read BatChargeLevel report from device %s: %s", deviceId.c_str(),
                                                                                      error.AsString());
                }

                break;
            }

            case Attributes::BatPercentRemaining::Id:
            {
                using TypeInfo = Attributes::BatPercentRemaining::TypeInfo;

                TypeInfo::DecodableType value;
                CHIP_ERROR error = cache->Get<TypeInfo>(path, value);

                if (error == CHIP_NO_ERROR)
                {
                    static_cast<PowerSource::EventHandler *>(eventHandler)->
                        BatPercentRemainingChanged(deviceId, value.Value());
                }
                else
                {
                    icWarn("Failed to read BatPercentRemaining report from device %s: %s", deviceId.c_str(),
                                                                                           error.AsString());
                }

                break;
            }

            default:
                break;
        }
    }
} // namespace barton
