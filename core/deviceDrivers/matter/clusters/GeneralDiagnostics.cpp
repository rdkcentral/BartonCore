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
#include "lib/core/CHIPError.h"
#include "lib/support/CodeUtils.h"

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

    CHIP_ERROR GeneralDiagnostics::GetCurrentActiveHardwareFaults(std::vector<HardwareFaultEnum> &currentFaults)
    {
        CHIP_ERROR err = CHIP_NO_ERROR;
        currentFaults.clear();

        auto cache = clusterStateCacheRef.lock();
        if (cache == nullptr)
        {
            icDebug("Attribute cache not available");
            return CHIP_ERROR_INCORRECT_STATE;
        }

        using TypeInfo = Attributes::ActiveHardwareFaults::TypeInfo;
        TypeInfo::DecodableType value;
        err = cache->Get<TypeInfo>({chip::kRootEndpointId, Id, Attributes::ActiveHardwareFaults::Id}, value);
        if (err == CHIP_NO_ERROR)
        {
            auto currIter = value.begin();

            while (currIter.Next())
            {
                currentFaults.push_back(currIter.GetValue());
            }

            if ((err = currIter.GetStatus()) != CHIP_NO_ERROR)
            {
                icError("Failed to decode active hardware faults list: %s", err.AsString());
                return err;
            }

            currentFaults.push_back(chip::app::Clusters::GeneralDiagnostics::HardwareFaultEnum::kUnspecified);
        }
        else
        {
            // This attribute is optional to support as per Matter 1.4 spec 11.12.6, so no need to warn.
            icInfo(
                "Could not get active hardware faults attribute from device %s: %s", deviceId.c_str(), err.AsString());
        }

        return err;
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

    void GeneralDiagnostics::OnEventDataReceived(SubscribeInteraction &subscriber,
                                                 const chip::app::EventHeader &aEventHeader,
                                                 chip::TLV::TLVReader *apData,
                                                 const chip::app::StatusIB *apStatus)
    {
        if (!aEventHeader.mPath.IsValidConcreteClusterPath())
        {
            icError("Received event with invalid path, dropping it!");
            return;
        }

        if (eventHandler == nullptr)
        {
            return;
        }

        icDebug("Endpoint: %u Cluster: " ChipLogFormatMEI " Event " ChipLogFormatMEI,
                aEventHeader.mPath.mEndpointId,
                ChipLogValueMEI(aEventHeader.mPath.mClusterId),
                ChipLogValueMEI(aEventHeader.mPath.mEventId));

        switch (aEventHeader.mPath.mEventId)
        {
            case Events::HardwareFaultChange::Id:
            {
                Events::HardwareFaultChange::DecodableType value;
                VerifyOrReturn(chip::app::DataModel::Decode(*apData, value) == CHIP_NO_ERROR,
                               icError("Failed to decode HardwareFaultChange event"));

                std::vector<chip::app::Clusters::GeneralDiagnostics::HardwareFaultEnum> currentFaults;
                auto currIter = value.current.begin();

                while (currIter.Next())
                {
                    currentFaults.push_back(currIter.GetValue());
                }

                VerifyOrReturn(currIter.GetStatus() == CHIP_NO_ERROR,
                               icError("Failed to decode all active hardware faults"));

                static_cast<GeneralDiagnostics::EventHandler *>(eventHandler)
                    ->OnHardwareFaultsChanged(*this, currentFaults);
                break;
            }

            default:
                break;
                // Logging unexpected events we don't care about can be noisy, and
                // unsolicited reports from devices is not controllable from here.
        }
    }

} // namespace barton
