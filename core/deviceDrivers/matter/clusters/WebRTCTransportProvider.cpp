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
// Created by Raiyan Chowdhury on 3/3/2025.
//

#include "lib/core/DataModelTypes.h"
#define LOG_TAG     "WebRTCTransportProviderCluster"
#define logFmt(fmt) "(%s): " fmt, __func__

extern "C" {
#include "subsystems/zigbee/zigbeeCommonIds.h"
#include <icLog/logging.h>
}

#include "WebRTCTransportProvider.h"
#include "app-common/zap-generated/attributes/Accessors.h"
#include "app-common/zap-generated/cluster-objects.h"
#include "app-common/zap-generated/ids/Attributes.h"
#include "app-common/zap-generated/ids/Clusters.h"
#include "app/BufferedReadCallback.h"
#include "app/InteractionModelEngine.h"
#include "subsystems/matter/webrtc-transport-requestor-server.hpp"

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::Globals;
using namespace chip::app::Clusters::WebRTCTransportProvider;

#define PROVIDE_OFFER_TIMEOUT_SECONDS 15
#define PROVIDE_ICE_CANDIDATE_TIMEOUT_SECONDS 15

namespace barton
{
    bool WebRTCTransportProvider::SendProvideOffer(void *context,
                                                   const DataModel::Nullable<uint16_t> webRTCSessionID,
                                                   const std::string &sdp,
                                                   StreamUsageEnum streamUsage,
                                                   Messaging::ExchangeManager &exchangeMgr,
                                                   const SessionHandle &sessionHandle)
    {
        auto commandSender = new CommandSender(this, &exchangeMgr, true);
        CommandPathParams commandPath(endpointId,
                                      0, /* not used */
                                      Id,
                                      Commands::ProvideOffer::Id,
                                      (CommandPathFlags::kEndpointIdValid));

        Commands::ProvideOffer::Type data;
        data.webRTCSessionID = webRTCSessionID;
        data.sdp = CharSpan::fromCharString(sdp.c_str());
        data.streamUsage = streamUsage;
        commandSender->AddRequestData(
            commandPath, data, Optional<uint16_t>(PROVIDE_OFFER_TIMEOUT_SECONDS * 1000));

        return SendCommand(commandSender, sessionHandle, context);
    }

    bool WebRTCTransportProvider::SendProvideICECandidate(void *context,
                                                          const uint16_t webRTCSessionID,
                                                          const std::vector<CharSpan> &iceCandidates,
                                                          Messaging::ExchangeManager &exchangeMgr,
                                                          const SessionHandle &sessionHandle)
    {
        auto commandSender = new CommandSender(this, &exchangeMgr, true);
        CommandPathParams commandPath(endpointId,
                                      0, /* not used */
                                      Id,
                                      Commands::ProvideICECandidates::Id,
                                      (CommandPathFlags::kEndpointIdValid));

        std::vector<Globals::Structs::ICECandidateStruct::Type> iceCandidateStructs;
        std::transform(
            iceCandidates.begin(),
            iceCandidates.end(),
            std::back_inserter(iceCandidateStructs),
            [](const CharSpan &candidate) { return Globals::Structs::ICECandidateStruct::Type {candidate}; });

        Commands::ProvideICECandidates::Type data;
        data.webRTCSessionID = webRTCSessionID;
        data.ICECandidates = DataModel::List<const Globals::Structs::ICECandidateStruct::Type>(
            iceCandidateStructs.data(), iceCandidateStructs.size());
        commandSender->AddRequestData(
            commandPath, data, Optional<uint16_t>(PROVIDE_ICE_CANDIDATE_TIMEOUT_SECONDS * 1000));

        return SendCommand(commandSender, sessionHandle, context);
    }

    bool WebRTCTransportProvider::SendEndSessionCommand(void *context,
                                                        const uint16_t webRTCSessionID,
                                                        const WebRTCEndReasonEnum reason,
                                                        Messaging::ExchangeManager &exchangeMgr,
                                                        const SessionHandle &sessionHandle)
    {
        auto commandSender = new CommandSender(this, &exchangeMgr, true);
        CommandPathParams commandPath(endpointId,
                                      0, /* not used */
                                      Id,
                                      Commands::EndSession::Id,
                                      (CommandPathFlags::kEndpointIdValid));

        Commands::EndSession::Type data;
        data.webRTCSessionID = webRTCSessionID;
        data.reason = reason;
        commandSender->AddRequestData(commandPath, data, Optional<uint16_t>(PROVIDE_OFFER_TIMEOUT_SECONDS * 1000));

        return SendCommand(commandSender, sessionHandle, context);
    }

    void WebRTCTransportProvider::OnResponse(chip::app::CommandSender *apCommandSender,
                                             const chip::app::ConcreteCommandPath &aPath,
                                             const chip::app::StatusIB &aStatusIB,
                                             chip::TLV::TLVReader *apData)
    {
        icDebug();

        if (apData != nullptr)
        {
            switch (aPath.mCommandId)
            {
                case Commands::ProvideOfferResponse::Id:
                {
                    Commands::ProvideOfferResponse::DecodableType value;
                    if (chip::app::DataModel::Decode(*apData, value) == CHIP_NO_ERROR)
                    {
                        icDebug("ProvideOfferResponse: webRTCSessionID %d", value.webRTCSessionID);
                        // Ignoring the videoStreamID and audioStreamID fields for now
                        static_cast<WebRTCTransportProvider::EventHandler *>(eventHandler)
                            ->HandleProvideOfferResponse(value.webRTCSessionID, deviceId);
                        /*
                         * TODO:
                         * Matter spec 11.5.6.4.:
                         * Upon receiving, the client SHALL create a new WebRTCSessionStruct populated with the values
                         * from this command, along with the accessing Peer Node ID and fabric-index stored in the
                         * Secure Session Context, as the PeerNodeID and PeerFabricIndex values, and store in the
                         * Requestor clusters CurrentSessions attribute. At some point, we'll hook into the WebRTC
                         * requestor cluster server's attribute setters and do this stuff.
                         */
                    }
                    else
                    {
                        icError("Failed to decode ProvideOfferResponse");
                    }
                    break;
                }

                default:
                    icTrace("Unhandled command response ID %d", aPath.mCommandId);
                    break;
            }
        }

        // TODO: fix "call super" anti-pattern
        MatterCluster::OnResponse(apCommandSender, aPath, aStatusIB, apData);
    }

    void WebRTCTransportProvider::OnAttributeChanged(ClusterStateCache *cache, const ConcreteAttributePath &path)
    {
        //TODO implement
        icDebug("Unimplemented");
    }
} // namespace barton
