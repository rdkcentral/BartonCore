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
// Created by tlea on 4/6/2023
//

#pragma once

#include "MatterCluster.h"
#include "app/BufferedReadCallback.h"
#include "lib/core/DataModelTypes.h"
#include <mutex>
#include <string>

namespace barton
{
    class LevelControl : public MatterCluster
    {
    public:
        LevelControl(EventHandler *handler, const std::string deviceId, chip::EndpointId endpointId) :
            MatterCluster(handler, deviceId, endpointId)
        {
        }

        /**
         * Base EventHandler with cluster specific additions
         */
        class EventHandler : public MatterCluster::EventHandler
        {
        public:
            virtual void CurrentLevelChanged(const std::string &deviceUuid, uint8_t level, void *asyncContext) {};
            virtual void CurrentLevelReadComplete(const std::string &deviceUuid, uint8_t level, void *asyncContext) {};
        };

        bool GetCurrentLevel(void *context,
                             const chip::Messaging::ExchangeManager &exchangeMgr,
                             const chip::SessionHandle &sessionHandle);

        bool MoveToLevel(void *context,
                         uint8_t level,
                         const chip::Messaging::ExchangeManager &exchangeMgr,
                         const chip::SessionHandle &sessionHandle);

        void OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                const chip::app::ConcreteAttributePath &path) override;

    private:
    };
} // namespace barton
