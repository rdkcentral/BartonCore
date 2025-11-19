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

#define G_LOG_DOMAIN "GeneralDiagnostics"
#define LOG_TAG      G_LOG_DOMAIN
#define logFmt(fmt)  fmt

extern "C" {
#include "icLog/logging.h"
}

#include "GeneralDiagnostics.h"

using namespace chip::app::Clusters::GeneralDiagnostics;

namespace barton
{
    std::string GeneralDiagnostics::GetMacAddress()
    {
        auto interfaceInfo = deviceDataCache->GetInterfaceInfo();
        if (!interfaceInfo.HasValue())
        {
            icWarn("No MAC address for the operational network is found for the device %s", deviceId.c_str());
            return "";
        };

        return interfaceInfo.Value().macAddress;
    }

    std::string GeneralDiagnostics::GetNetworkType()
    {
        auto interfaceInfo = deviceDataCache->GetInterfaceInfo();
        if (!interfaceInfo.HasValue())
        {
            icWarn("No type for the operational network is found for the device %s", deviceId.c_str());
            return "";
        };

        return interfaceInfo.Value().networkType;
    }

    void GeneralDiagnostics::OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                                const chip::app::ConcreteAttributePath &path)
    {
        if (path.mEndpointId != chip::kRootEndpointId || path.mClusterId != Id)
        {
            return;
        }

        switch (path.mAttributeId)
        {
            case Attributes::NetworkInterfaces::Id:
            {
                auto interfaceInfo = deviceDataCache->GetInterfaceInfo();
                if (interfaceInfo.HasValue())
                {
                    static_cast<GeneralDiagnostics::EventHandler *>(eventHandler)
                        ->OnNetworkInterfacesChanged(*this, interfaceInfo.Value());
                }
                else
                {
                    icError("Failed to find any operational network for device %s", deviceId.c_str());
                };
            }
            default:
                break;
        }
    }


} // namespace barton
