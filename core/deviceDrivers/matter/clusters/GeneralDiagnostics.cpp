// Copyright (C) 2024 Comcast Corporation
//
// All rights reserved.
//
// This software is protected by copyright laws of the United States
// and of foreign countries. This material may also be protected by
// patent laws of the United States and of foreign countries.
//
// This software is furnished under a license agreement and/or a
// nondisclosure agreement and may only be used or copied in accordance
// with the terms of those agreements.
//
// The mere transfer of this software does not imply any licenses of trade
// secrets, proprietary technology, copyrights, patents, trademarks, or
// any other form of intellectual property whatsoever.
//
// Comcast Corporation retains all ownership rights.

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

namespace zilker
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
} // namespace zilker