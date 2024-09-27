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
#include "app/ReadPrepareParams.h"
#include "lib/core/DataModelTypes.h"
#include <mutex>
#include <string>

namespace zilker
{
    class DoorLock : public MatterCluster
    {
    public:
        DoorLock(EventHandler *handler, std::string deviceId, chip::EndpointId endpointId) :
            MatterCluster(handler, deviceId, endpointId)
        {
        }

        /**
         * Base EventHandler with cluster specific additions
         */
        class EventHandler : public MatterCluster::EventHandler
        {
        public:
            virtual void LockStateChanged(std::string &deviceUuid,
                                          chip::app::Clusters::DoorLock::DlLockState state,
                                          void *asyncContext) {};
            virtual void LockStateReadComplete(std::string &deviceUuid,
                                               chip::app::Clusters::DoorLock::DlLockState state,
                                               void *asyncContext) {};
        };

        SubscriptionIntervalSecs GetDesiredSubscriptionIntervalSecs() override
        {
            SubscriptionIntervalSecs intervalSecs(1, 3600);
            return intervalSecs;
        }

        bool GetLockState(void *context,
                          const chip::Messaging::ExchangeManager &exchangeMgr,
                          const chip::SessionHandle &sessionHandle);

        bool SetLocked(void *context,
                       bool locked,
                       const chip::Messaging::ExchangeManager &exchangeMgr,
                       const chip::SessionHandle &sessionHandle);

        void OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                const chip::app::ConcreteAttributePath &path) override;
    };
} // namespace zilker
