//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2023 Comcast Corporation
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

namespace zilker
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

        SubscriptionIntervalSecs GetDesiredSubscriptionIntervalSecs() override
        {
            SubscriptionIntervalSecs intervalSecs(1, 3600);
            return intervalSecs;
        }

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
} // namespace zilker
