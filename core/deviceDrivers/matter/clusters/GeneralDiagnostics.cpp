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

namespace barton
{
    std::string GeneralDiagnostics::GetMacAddress()
    {
        auto cache = clusterStateCacheRef.lock();

        if (cache == nullptr)
        {
            icDebug("Attribute cache not available");
            return "";
        }

        auto interfaceInfo = GetInterfaceInfo(cache.get());
        if (!interfaceInfo.HasValue())
        {
            icWarn("No MAC address for the operational network is found for the device %s", deviceId.c_str());
            return "";
        };

        return interfaceInfo.Value().macAddress;
    }

    std::string GeneralDiagnostics::GetNetworkType()
    {
        auto cache = clusterStateCacheRef.lock();

        if (cache == nullptr)
        {
            icDebug("Attribute cache not available");
            return "";
        }

        auto interfaceInfo = GetInterfaceInfo(cache.get());
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
                auto interfaceInfo = GetInterfaceInfo(cache);
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

    chip::Optional<NetworkUtils::NetworkInterfaceInfo>
    GeneralDiagnostics::GetInterfaceInfo(chip::app::ClusterStateCache *cache)
    {
        using TypeInfo = Attributes::NetworkInterfaces::TypeInfo;
        TypeInfo::DecodableType value;
        CHIP_ERROR error = cache->Get<TypeInfo>({chip::kRootEndpointId, Id, Attributes::NetworkInterfaces::Id}, value);
        if (error == CHIP_NO_ERROR)
        {
            auto sessionMgr =
                chip::app::InteractionModelEngine::GetInstance()->GetExchangeManager()->GetSessionManager();
            chip::NodeId nodeId = GetNodeId();
            chip::Transport::Session *foundSession = nullptr;

            sessionMgr->GetSecureSessions().ForEachSession([nodeId, &foundSession](auto *session) {
                if (session && session->GetPeer().GetNodeId() == nodeId)
                {
                    foundSession = session;
                    return chip::Loop::Break;
                }
                return chip::Loop::Continue;
            });

            if (foundSession)
            {
                char nodeIpv6Addr[INET6_ADDRSTRLEN] = {};
                foundSession->AsSecureSession()->GetPeerAddress().GetIPAddress().ToString(nodeIpv6Addr);
                return NetworkUtils::ExtractOperationalInterfaceInfo(value, nodeIpv6Addr);
            }
            else
            {
                icError("No session found for device %s", deviceId.c_str());
            }
        }
        else
        {
            icWarn("Failed to read network interfaces report from device %s: %s", deviceId.c_str(), error.AsString());
        }

        return chip::NullOptional;
    }
} // namespace barton
