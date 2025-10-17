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
// Created by Raiyan Chowdhury on 8/12/24.
//

#pragma once

#include "MatterCluster.h"
#include "lib/core/DataModelTypes.h"
#include <string>

namespace barton
{
    class BooleanState : public MatterCluster
    {
    public:
        BooleanState(EventHandler *handler, std::string deviceId, chip::EndpointId endpointId) :
            MatterCluster(handler, deviceId, endpointId)
        {
        }

        class EventHandler : public MatterCluster::EventHandler
        {
        public:
            virtual void StateValueChanged(std::string &deviceUuid, bool state) {};
            virtual void StateValueReadComplete(std::string &deviceUuid, bool state, void *asyncContext) {};
        };

        SubscriptionIntervalSecs GetDesiredSubscriptionIntervalSecs() override
        {
            SubscriptionIntervalSecs intervalSecs(1, kSubscriptionMaxIntervalPublisherLimit);
            return intervalSecs;
        }

        bool GetStateValue(void *context,
                           const chip::Messaging::ExchangeManager &exchangeMgr,
                           const chip::SessionHandle &sessionHandle);

        void OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                const chip::app::ConcreteAttributePath &path) override;
    };
}
