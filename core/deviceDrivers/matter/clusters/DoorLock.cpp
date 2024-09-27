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

#define LOG_TAG     "DoorLockCluster"
#define logFmt(fmt) "(%s): " fmt, __func__

extern "C" {
#include <icLog/logging.h>
}

#include "DoorLock.h"
#include "app-common/zap-generated/ids/Attributes.h"
#include "app-common/zap-generated/ids/Clusters.h"
#include "app/BufferedReadCallback.h"
#include "app/InteractionModelEngine.h"

//TODO we have a smattering of timeouts that need to be coordinated
#define INTERACTION_TIMEOUT_SECONDS 15

namespace zilker
{
    void DoorLock::OnAttributeChanged(chip::app::ClusterStateCache *cache, const chip::app::ConcreteAttributePath &path)
    {
        using namespace chip::app::Clusters::DoorLock;
        using TypeInfo = Attributes::LockState::TypeInfo;

        if (path.mClusterId == chip::app::Clusters::DoorLock::Id &&
            path.mAttributeId == chip::app::Clusters::DoorLock::Attributes::LockState::Id)
        {
            TypeInfo::DecodableType value;
            CHIP_ERROR error = cache->Get<TypeInfo>(path, value);
            if (error == CHIP_NO_ERROR)
            {
                static_cast<DoorLock::EventHandler *>(eventHandler)->LockStateChanged(deviceId, value.Value(), nullptr);
            }
            else
            {
                icError("Failed to decode lockstate attribute: %s", error.AsString());
            }
        }
        else
        {
            icTrace("Unexpected cluster or attribute");
        }
    }

    bool DoorLock::GetLockState(void *context,
                                const chip::Messaging::ExchangeManager &exchangeMgr,
                                const chip::SessionHandle &sessionHandle)
    {
        icDebug();

        bool result = false;
        std::shared_ptr<chip::app::ClusterStateCache> cache;
        if (!(cache = clusterStateCacheRef.lock()))
        {
            icError("Failed to get lockstate attribute because the cluster state cache expired or was never set");
            return false;
        }

        CHIP_ERROR error = CHIP_NO_ERROR;
        using namespace chip::app::Clusters::DoorLock;
        using TypeInfo = Attributes::LockState::TypeInfo;

        chip::app::ConcreteAttributePath path(endpointId,
                                              chip::app::Clusters::DoorLock::Id,
                                              chip::app::Clusters::DoorLock::Attributes::LockState::Id);
        {
            TypeInfo::DecodableType value;
            error = cache->Get<TypeInfo>(path, value);
            if (error == CHIP_NO_ERROR)
            {
                static_cast<DoorLock::EventHandler *>(eventHandler)
                    ->LockStateReadComplete(deviceId, value.Value(), context);
                result = true;
            }
        }

        if (error != CHIP_NO_ERROR)
        {
            icError("Failed to decode lockstate attribute: %s", error.AsString());
        }

        return result;
    };

    bool DoorLock::SetLocked(void *context,
                             bool locked,
                             const chip::Messaging::ExchangeManager &exchangeMgr,
                             const chip::SessionHandle &sessionHandle)
    {
        icDebug();

        // TODO: const_cast goes away with Matter 1.1+
        auto commandSender =
            new chip::app::CommandSender(this, const_cast<chip::Messaging::ExchangeManager *>(&exchangeMgr), true);
        chip::app::CommandPathParams commandPath(endpointId,
                                                 0, /* not used */
                                                 chip::app::Clusters::DoorLock::Id,
                                                 locked ? chip::app::Clusters::DoorLock::Commands::LockDoor::Id
                                                        : chip::app::Clusters::DoorLock::Commands::UnlockDoor::Id,
                                                 (chip::app::CommandPathFlags::kEndpointIdValid));

        if (locked)
        {
            chip::app::Clusters::DoorLock::Commands::LockDoor::Type data;
            commandSender->AddRequestData(
                commandPath, data, chip::Optional<uint16_t>(INTERACTION_TIMEOUT_SECONDS * 1000));
        }
        else
        {
            chip::app::Clusters::DoorLock::Commands::UnlockDoor::Type data;
            commandSender->AddRequestData(
                commandPath, data, chip::Optional<uint16_t>(INTERACTION_TIMEOUT_SECONDS * 1000));
        }

        // hand off the commandSender to the base class
        return SendCommand(commandSender, sessionHandle, context);
    };
} // namespace zilker
