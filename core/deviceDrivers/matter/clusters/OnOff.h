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
// Created by tlea on 11/7/22.
//

#pragma once

#include "MatterCluster.h"
#include "app/BufferedReadCallback.h"
#include "lib/core/DataModelTypes.h"
#include <mutex>
#include <string>

namespace zilker
{
    class OnOff : public MatterCluster
    {
    public:
        OnOff(EventHandler *handler, std::string deviceId, chip::EndpointId endpointId) :
            MatterCluster(handler, deviceId, endpointId)
        {
        }

        /**
         * Base EventHandler with cluster specific additions
         */
        class EventHandler : public MatterCluster::EventHandler
        {
        public:
            virtual void OnOffChanged(std::string &deviceUuid, bool isOn, void *asyncContext) {};
            virtual void OnOffReadComplete(std::string &deviceUuid, bool isOn, void *asyncContext) {};
        };

        SubscriptionIntervalSecs GetDesiredSubscriptionIntervalSecs() override
        {
            SubscriptionIntervalSecs intervalSecs(1, 3600);
            return intervalSecs;
        }

        bool GetOnOff(void *context,
                      const chip::Messaging::ExchangeManager &exchangeMgr,
                      const chip::SessionHandle &sessionHandle);

        bool SetOnOff(void *context,
                      bool on,
                      const chip::Messaging::ExchangeManager &exchangeMgr,
                      const chip::SessionHandle &sessionHandle);

        void OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                const chip::app::ConcreteAttributePath &path) override;
    };
} // namespace zilker
