//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2024 Comcast Corporation
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
// Created by tlea on 11/15/22.
//

#pragma once

#include "MatterCluster.h"
#include "app/BufferedReadCallback.h"
#include "app/ReadPrepareParams.h"
#include "lib/core/DataModelTypes.h"
#include <mutex>
#include <string>

#define CHIME_CLUSTER_ID                  0x111dfd00
#define CHIME_CLUSTER_PLAY_URL_COMMAND_ID 0x111d0000

namespace zilker
{
    class ComcastChime : public MatterCluster
    {
    public:
        ComcastChime(EventHandler *handler, std::string deviceId, chip::EndpointId endpointId) :
            MatterCluster(handler, deviceId, endpointId)
        {
        }

        class EventHandler : public MatterCluster::EventHandler
        {
        public:
            virtual void AudioAssetsChanged(const std::string &deviceUuid,
                                            std::unique_ptr<std::string> assets,
                                            void *asyncContext) {};

            virtual void AudioAssetsReadComplete(const std::string &deviceUuid,
                                                 std::unique_ptr<std::string> assets,
                                                 void *asyncContext) {};
        };

        SubscriptionIntervalSecs GetDesiredSubscriptionIntervalSecs() override
        {
            SubscriptionIntervalSecs intervalSecs(1, UINT16_MAX);
            return intervalSecs;
        }

        void OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                const chip::app::ConcreteAttributePath &path) override;

        bool PlayUrl(void *context,
                     const std::string &url,
                     chip::Messaging::ExchangeManager &exchangeMgr,
                     const chip::SessionHandle &sessionHandle);

        bool StopPlay(void *context,
                      chip::Messaging::ExchangeManager &exchangeMgr,
                      const chip::SessionHandle &sessionHandle);

        bool GetAudioAssets(void *context,
                            const chip::Messaging::ExchangeManager &exchangeMgr,
                            const chip::SessionHandle &sessionHandle);
    };
} // namespace zilker
