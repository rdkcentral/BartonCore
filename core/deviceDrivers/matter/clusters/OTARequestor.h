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
// Created by vramac391 on 12/1/22.
//

#pragma once

#include "app-common/zap-generated/cluster-objects.h"
#include "app/BufferedReadCallback.h"
#include "lib/core/DataModelTypes.h"
#include "matter/clusters/MatterCluster.h"
#include <mutex>
#include <string>
#include <vector>

using namespace chip::app::Clusters;
using namespace chip::app::DataModel;

namespace barton
{
    class OTARequestor : public MatterCluster
    {
    public:
        OTARequestor(EventHandler *handler, std::string deviceId, chip::EndpointId endpointId) :
            MatterCluster(handler, deviceId, endpointId)
        {
        }

        class EventHandler : public MatterCluster::EventHandler
        {
        public:
            /**
             * @brief This is called whenever the requestor server's UpdateState attribute changes
             * @ref Matter 1.0 11.19.7.8
             *
             * @param source
             * @param oldState
             * @param newState
             * @param reason
             * @param targetSoftwareVersion
             * @param subscriber
             */
            virtual void OnStateTransition(OTARequestor &source,
                                           OtaSoftwareUpdateRequestor::OTAUpdateStateEnum oldState,
                                           OtaSoftwareUpdateRequestor::OTAUpdateStateEnum newState,
                                           OtaSoftwareUpdateRequestor::OTAChangeReasonEnum reason,
                                           Nullable<uint32_t> targetSoftwareVersion,
                                           SubscribeInteraction &subscriber) {};

            /**
             * @brief This is called whenever a download fails to complete
             * @ref Matter 1.0 11.19.7.8
             * @param source
             * @param softwareVersion
             * @param bytesDownloaded
             * @param percentDownloaded
             * @param platformCode
             * @param subscriber
             */
            virtual void OnDownloadError(OTARequestor &source,
                                         uint32_t softwareVersion,
                                         uint64_t bytesDownloaded,
                                         Nullable<uint8_t> percentDownloaded,
                                         Nullable<int64_t> platformCode,
                                         SubscribeInteraction &subscriber) {};

            /**
             * @brief This is called whenever a new version is applied and will match the SoftwareVersion
             *        attribute in the Basic Information cluster.
             * @ref Matter 1.0 11.19.7.8
             * @param source
             * @param softwareVersion
             * @param productId
             * @param subscriber
             */
            virtual void OnVersionApplied(OTARequestor &source,
                                          uint32_t softwareVersion,
                                          uint16_t productId,
                                          SubscribeInteraction &subscriber) {};
        };

        void OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                const chip::app::ConcreteAttributePath &path) override;

        void OnEventDataReceived(SubscribeInteraction &subscriber,
                                 const chip::app::EventHeader &aEventHeader,
                                 chip::TLV::TLVReader *apData,
                                 const chip::app::StatusIB *apStatus) override;

        bool SetDefaultOTAProviders(
            void *context,
            std::vector<OtaSoftwareUpdateRequestor::Structs::ProviderLocation::Type> &otaProviderList,
            const chip::Messaging::ExchangeManager &exchangeMgr,
            const chip::SessionHandle &sessionHandle);
    };
} // namespace barton
