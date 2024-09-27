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
// Created by tlea on 4/6/2023
//

#pragma once

#include "MatterCluster.h"
#include "app/BufferedReadCallback.h"
#include "lib/core/DataModelTypes.h"
#include <mutex>
#include <string>

namespace zilker
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

        SubscriptionIntervalSecs GetDesiredSubscriptionIntervalSecs() override
        {
            SubscriptionIntervalSecs intervalSecs(1, 3600);
            return intervalSecs;
        }

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
} // namespace zilker
