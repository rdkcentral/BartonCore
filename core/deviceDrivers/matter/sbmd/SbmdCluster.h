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
// Created by tlea on 10/30/25
//

#pragma once

#include "../clusters/MatterCluster.h"
#include "app/BufferedReadCallback.h"
#include "app/ReadPrepareParams.h"
#include "lib/core/DataModelTypes.h"
#include <mutex>
#include <string>

namespace barton
{
    class SbmdCluster : public MatterCluster
    {
    public:
        SbmdCluster(EventHandler *handler, std::string deviceId, chip::EndpointId endpointId,
            uint16_t clusterId, std::shared_ptr<DeviceDataCache> deviceDataCache) :
            MatterCluster(handler, deviceId, endpointId, clusterId, deviceDataCache)
        {
        }

        /**
         * Base EventHandler with cluster specific additions
         */
        class EventHandler : public MatterCluster::EventHandler
        {
        public:
            virtual void AttributeChanged(std::string &deviceUuid,
                                          uint16_t clusterId,
                                          uint16_t attributeId,
                                          void *asyncContext) {};
            virtual void AttributeReadComplete(std::string &deviceUuid,
                                               uint16_t clusterId,
                                               uint16_t attributeId,
                                               chip::TLV::TLVReader &reader,
                                               void *asyncContext) {};
        };

        CHIP_ERROR ReadAttribute(void *context,
                                 uint16_t attributeId,
                                 const chip::Messaging::ExchangeManager &exchangeMgr,
                                 const chip::SessionHandle &sessionHandle);

        void OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                const chip::app::ConcreteAttributePath &path) override;

    private:
        uint16_t clusterId;
    };
} // namespace barton
