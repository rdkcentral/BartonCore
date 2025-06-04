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

#pragma once

#include "MatterCluster.h"
#include "lib/core/DataModelTypes.h"
#include "lib/support/Span.h"
#include <mutex>
#include <string>
#include <vector>

namespace barton
{
    class WebRTCTransportProvider : public MatterCluster
    {
    public:
        WebRTCTransportProvider(EventHandler *handler, std::string deviceId, chip::EndpointId endpointId) :
            MatterCluster(handler, deviceId, endpointId)
        {
        }

        class EventHandler : public MatterCluster::EventHandler
        {
        public:
            virtual void HandleProvideOfferResponse(uint16_t webRTCSessionID, std::string &deviceId) {}
        };

        SubscriptionIntervalSecs GetDesiredSubscriptionIntervalSecs() override
        {
            SubscriptionIntervalSecs intervalSecs(1, 3600);
            return intervalSecs;
        }

        bool SendProvideOffer(void *context,
                              const chip::app::DataModel::Nullable<uint16_t> webRTCSessionID,
                              const std::string &sdp,
                              chip::app::Clusters::WebRTCTransportProvider::StreamUsageEnum streamUsage,
                              chip::Messaging::ExchangeManager &exchangeMgr,
                              const chip::SessionHandle &sessionHandle);

        bool SendProvideICECandidate(void *context,
                                     const uint16_t webRTCSessionID,
                                     const std::vector<chip::CharSpan> &iceCandidates,
                                     chip::Messaging::ExchangeManager &exchangeMgr,
                                     const chip::SessionHandle &sessionHandle);

        bool SendEndSessionCommand(void *context,
                                   const uint16_t webRTCSessionID,
                                   const chip::app::Clusters::WebRTCTransportProvider::WebRTCEndReasonEnum reason,
                                   chip::Messaging::ExchangeManager &exchangeMgr,
                                   const chip::SessionHandle &sessionHandle);

        void OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                const chip::app::ConcreteAttributePath &path) override;

        // CommandSender callback
        void OnResponse(chip::app::CommandSender *apCommandSender,
                        const chip::app::ConcreteCommandPath &aPath,
                        const chip::app::StatusIB &aStatusIB,
                        chip::TLV::TLVReader *apData) override;
    };
} // namespace barton
