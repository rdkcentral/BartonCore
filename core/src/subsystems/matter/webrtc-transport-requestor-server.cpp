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
// Created by Raiyan Chowdhury on 3/4/2025.
//

#include "webrtc-transport-requestor-server.hpp"
#include "lib/core/NodeId.h"
#include <app-common/zap-generated/callback.h>

namespace
{
WebRTCTransportRequestorServerDelegate * serverDelegate;
}

void MatterWebRTCTransportRequestorPluginServerSetDelegate(WebRTCTransportRequestorServerDelegate * delegate)
{
    serverDelegate = delegate;
}

namespace chip {
namespace app {
namespace Clusters {

    CHIP_ERROR WebRTCTransportRequestorServer::Init()
    {
        return CHIP_NO_ERROR;
    }

    void WebRTCTransportRequestorServer::HandleAnswerCommand(
        const chip::NodeId nodeId,
        const chip::EndpointId endpointId,
        const chip::app::Clusters::WebRTCTransportRequestor::Commands::Answer::DecodableType &commandData)
    {
        ChipLogProgress(Zcl, "Received WebRTCTransportRequestor::Answer command from endpoint %d", endpointId);
        if (serverDelegate)
        {
            serverDelegate->OnAnswer(nodeId, commandData);
        }
    }

    void WebRTCTransportRequestorServer::HandleICECandidatesCommand(
        const chip::NodeId nodeId,
        const chip::EndpointId endpointId,
        const chip::app::Clusters::WebRTCTransportRequestor::Commands::ICECandidates::DecodableType &commandData)
    {
        ChipLogProgress(Zcl, "Received WebRTCTransportRequestor::ICECandidates command from endpoint %d", endpointId);
        if (serverDelegate)
        {
            serverDelegate->OnICECandidates(nodeId, commandData);
        }
    }

    void WebRTCTransportRequestorServer::HandleEndCommand(
        const chip::NodeId nodeId,
        const chip::EndpointId endpointId,
        const chip::app::Clusters::WebRTCTransportRequestor::Commands::End::DecodableType &commandData)
    {
        ChipLogProgress(Zcl, "Received WebRTCTransportRequestor::End command from endpoint %d", endpointId);
        if (serverDelegate)
        {
            serverDelegate->OnEnd(nodeId, commandData);
        }
    }

} // namespace Clusters
} // namespace app
} // namespace chip

using namespace chip::app::Clusters;

// TODO: Can probably try extracting the peer NodeId from the CurrentSessions attribute first (when it's implemented)
static chip::NodeId ExtractPeerNodeId(chip::app::CommandHandler *commandObj)
{
    auto exchangeContext = commandObj->GetExchangeContext();
    if (exchangeContext == nullptr)
    {
        ChipLogError(Zcl, "Failed to extract peer node id. Exchange context is null");
        return chip::kUndefinedNodeId;
    }

    auto sessionHandle = exchangeContext->GetSessionHandle();

    auto session = sessionHandle.operator->();
    if (session == nullptr)
    {
        ChipLogError(Zcl, "Failed to extract peer node id. Session is null");
        return chip::kUndefinedNodeId;
    }

    return session->GetPeer().GetNodeId();
}

bool emberAfWebRTCTransportRequestorClusterAnswerCallback(
    chip::app::CommandHandler *commandObj,
    const chip::app::ConcreteCommandPath &commandPath,
    const WebRTCTransportRequestor::Commands::Answer::DecodableType &commandData)
{
    chip::NodeId nodeId = ExtractPeerNodeId(commandObj);
    if (nodeId == chip::kUndefinedNodeId)
    {
        ChipLogError(Zcl, "Failed to handle WebRTCTransportRequestor::Answer command.");
        return false;
    }
    WebRTCTransportRequestorServer::Instance().HandleAnswerCommand(nodeId, commandPath.mEndpointId, commandData);
    return true;
}

bool emberAfWebRTCTransportRequestorClusterICECandidatesCallback(
    chip::app::CommandHandler *commandObj,
    const chip::app::ConcreteCommandPath &commandPath,
    const WebRTCTransportRequestor::Commands::ICECandidates::DecodableType &commandData)
{
    chip::NodeId nodeId = ExtractPeerNodeId(commandObj);
    if (nodeId == chip::kUndefinedNodeId)
    {
        ChipLogError(Zcl, "Failed to handle WebRTCTransportRequestor::ICECandidates command.");
        return false;
    }
    WebRTCTransportRequestorServer::Instance().HandleICECandidatesCommand(nodeId, commandPath.mEndpointId, commandData);
    return true;
}

bool emberAfWebRTCTransportRequestorClusterEndCallback(
    chip::app::CommandHandler *commandObj,
    const chip::app::ConcreteCommandPath &commandPath,
    const WebRTCTransportRequestor::Commands::End::DecodableType &commandData)
{
    chip::NodeId nodeId = ExtractPeerNodeId(commandObj);
    if (nodeId == chip::kUndefinedNodeId)
    {
        ChipLogError(Zcl, "Failed to handle WebRTCTransportRequestor::End command.");
        return false;
    }
    WebRTCTransportRequestorServer::Instance().HandleEndCommand(nodeId, commandPath.mEndpointId, commandData);
    return true;
}

void MatterWebRTCTransportRequestorPluginServerInitCallback()
{
    WebRTCTransportRequestorServer::Instance().Init();
}
