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

#pragma once

#include "lib/core/DataModelTypes.h"
#include <app-common/zap-generated/cluster-objects.h>
#include <app/AttributeAccessInterface.h>
#include <app/CommandHandlerInterface.h>
#include <app/ConcreteAttributePath.h>
#include <app/reporting/reporting.h>
#include <lib/core/CHIPError.h>
#include <protocols/interaction_model/StatusCode.h>

class WebRTCTransportRequestorServerDelegate
{
public:
    virtual ~WebRTCTransportRequestorServerDelegate() {}

    virtual void
    OnAnswer(const chip::NodeId nodeId,
             const chip::app::Clusters::WebRTCTransportRequestor::Commands::Answer::DecodableType &commandData)
    {
    }

    virtual void OnICECandidates(
        const chip::NodeId nodeId,
        const chip::app::Clusters::WebRTCTransportRequestor::Commands::ICECandidates::DecodableType &commandData)
    {
    }

    virtual void OnEnd(const chip::NodeId nodeId,
                       const chip::app::Clusters::WebRTCTransportRequestor::Commands::End::DecodableType &commandData)
    {
    }
};

void MatterWebRTCTransportRequestorPluginServerSetDelegate(WebRTCTransportRequestorServerDelegate * delegate);

namespace chip {
namespace app {
namespace Clusters {

    class WebRTCTransportRequestorServer
    {
    public:
        WebRTCTransportRequestorServer() {}
        ~WebRTCTransportRequestorServer() {}

        CHIP_ERROR Init();

        static WebRTCTransportRequestorServer & Instance()
        {
            static WebRTCTransportRequestorServer instance;
            return instance;
        }

        // TODO attribute setters, getters, reading, writing, storage, etc

        void HandleAnswerCommand(
            const chip::NodeId nodeId,
            const chip::EndpointId endpointId,
            const chip::app::Clusters::WebRTCTransportRequestor::Commands::Answer::DecodableType &commandData);

        void HandleICECandidatesCommand(
            const chip::NodeId nodeId,
            const chip::EndpointId endpointId,
            const chip::app::Clusters::WebRTCTransportRequestor::Commands::ICECandidates::DecodableType &commandData);

        void HandleEndCommand(
            const chip::NodeId nodeId,
            const chip::EndpointId endpointId,
            const chip::app::Clusters::WebRTCTransportRequestor::Commands::End::DecodableType &commandData);
    };
} // namespace Clusters
} // namespace app
} // namespace chip
