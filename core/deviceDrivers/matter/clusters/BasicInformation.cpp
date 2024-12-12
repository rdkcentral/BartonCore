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

#define G_LOG_DOMAIN "BasicInformation"
#define LOG_TAG      G_LOG_DOMAIN
#define logFmt(fmt)  fmt

extern "C" {
#include "icLog/logging.h"
}

#include "BasicInformation.hpp"
#include "app-common/zap-generated/ids/Attributes.h"
#include "app-common/zap-generated/ids/Clusters.h"
#include "app/ClusterStateCache.h"
#include "app/ConcreteAttributePath.h"
#include "app/ReadHandler.h"
#include "glib.h"
#include "lib/core/CHIPError.h"
#include "lib/core/DataModelTypes.h"

namespace zilker
{
    std::string BasicInformation::GetFirmwareVersionString()
    {
        auto cache = clusterStateCacheRef.lock();

        if (cache == nullptr)
        {
            icDebug("Attribute cache not available");
            return "";
        }

        using TypeInfo = Attributes::SoftwareVersionString::TypeInfo;

        TypeInfo::DecodableType value;
        CHIP_ERROR error =
            cache->Get<TypeInfo>({chip::kRootEndpointId, Id, Attributes::SoftwareVersionString::Id}, value);

        if (error != CHIP_NO_ERROR)
        {
            icWarn(
                "Failed to read SoftwareVersionString attribute for device %s: %s", deviceId.c_str(), error.AsString());

            return "";
        }

        return {value.data(), value.size()};
    }

    void BasicInformation::OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                              const chip::app::ConcreteAttributePath &path)
    {
        if (path.mEndpointId != chip::kRootEndpointId || path.mClusterId != Id)
        {
            return;
        }

        switch (path.mAttributeId)
        {
            case Attributes::SoftwareVersion::Id:
            {
                using TypeInfo = Attributes::SoftwareVersion::TypeInfo;

                TypeInfo::DecodableType value;
                CHIP_ERROR error = cache->Get<TypeInfo>(path, value);

                if (error == CHIP_NO_ERROR)
                {
                    static_cast<BasicInformation::EventHandler *>(eventHandler)->OnSoftwareVersionChanged(*this, value);
                }
                else
                {
                    icWarn(
                        "Failed to read SoftwareVersion report from device %s: %s", deviceId.c_str(), error.AsString());
                }

                break;
            }

            case Attributes::SoftwareVersionString::Id:
            {
                using TypeInfo = Attributes::SoftwareVersionString::TypeInfo;

                TypeInfo::DecodableType value;
                CHIP_ERROR error = cache->Get<TypeInfo>(path, value);

                if (error == CHIP_NO_ERROR)
                {
                    static_cast<BasicInformation::EventHandler *>(eventHandler)
                        ->OnSoftwareVersionStringChanged(*this, {value.data(), value.size()});
                }
                else
                {
                    icWarn("Failed to read SoftwareVersionString report from device %s: %s",
                           deviceId.c_str(),
                           error.AsString());
                }

                break;
            }

            case Attributes::SerialNumber::Id:
            {
                using TypeInfo = Attributes::SerialNumber::TypeInfo;

                TypeInfo::DecodableType value;
                CHIP_ERROR error = cache->Get<TypeInfo>(path, value);

                if (error == CHIP_NO_ERROR)
                {
                    static_cast<BasicInformation::EventHandler *>(eventHandler)
                        ->OnSerialNumberChanged(*this, {value.data(), value.size()});
                }
                else
                {
                    icWarn("Failed to read SerialNumber report from device %s: %s", deviceId.c_str(), error.AsString());
                }

                break;
            }

            default:
                // Not interested
                break;
        }
    }
} // namespace zilker
