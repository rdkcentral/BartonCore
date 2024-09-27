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

#define LOG_TAG     "LevelControlCluster"
#define logFmt(fmt) "(%s): " fmt, __func__

extern "C" {
#include <icLog/logging.h>
}

#include "LevelControl.h"
#include "app-common/zap-generated/ids/Attributes.h"
#include "app-common/zap-generated/ids/Clusters.h"
#include "app/BufferedReadCallback.h"
#include "app/InteractionModelEngine.h"

namespace zilker
{
    void LevelControl::OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                          const chip::app::ConcreteAttributePath &path)
    {
        using namespace chip::app::Clusters::LevelControl;
        using TypeInfo = Attributes::CurrentLevel::TypeInfo;

        if (path.mClusterId == chip::app::Clusters::LevelControl::Id &&
            path.mAttributeId == chip::app::Clusters::LevelControl::Attributes::CurrentLevel::Id)
        {
            TypeInfo::DecodableType value;
            CHIP_ERROR error = cache->Get<TypeInfo>(path, value);
            if (error == CHIP_NO_ERROR)
            {
                static_cast<LevelControl::EventHandler *>(eventHandler)
                    ->CurrentLevelChanged(deviceId, value.Value(), nullptr);
            }
            else
            {
                icError("Failed to decode current level attribute: %s", error.AsString());
            }
        }
        else
        {
            icTrace("Unexpected cluster or attribute");
        }
    }

    bool LevelControl::GetCurrentLevel(void *context,
                                       const chip::Messaging::ExchangeManager &exchangeMgr,
                                       const chip::SessionHandle &sessionHandle)
    {
        icDebug();

        bool result = false;
        std::shared_ptr<chip::app::ClusterStateCache> cache;
        if (!(cache = clusterStateCacheRef.lock()))
        {
            icError("Failed to get current level because the cluster state cache expired or was never set");
            return false;
        }

        CHIP_ERROR error = CHIP_NO_ERROR;
        using namespace chip::app::Clusters::LevelControl;
        using TypeInfo = Attributes::CurrentLevel::TypeInfo;

        chip::app::ConcreteAttributePath path(endpointId,
                                              chip::app::Clusters::LevelControl::Id,
                                              chip::app::Clusters::LevelControl::Attributes::CurrentLevel::Id);
        {
            TypeInfo::DecodableType value;
            error = cache->Get<TypeInfo>(path, value);
            if (error == CHIP_NO_ERROR)
            {
                static_cast<LevelControl::EventHandler *>(eventHandler)
                    ->CurrentLevelReadComplete(deviceId, value.Value(), context);
                result = true;
            }
        }

        if (error != CHIP_NO_ERROR)
        {
            icError("Failed to decode current level attribute: %s", error.AsString());
        }

        return result;
    };

    bool LevelControl::MoveToLevel(void *context,
                                   uint8_t level,
                                   const chip::Messaging::ExchangeManager &exchangeMgr,
                                   const chip::SessionHandle &sessionHandle)
    {
        icDebug();

        // TODO: const_cast will go away with Matter 1.1+
        auto commandSender =
            new chip::app::CommandSender(this, const_cast<chip::Messaging::ExchangeManager *>(&exchangeMgr));
        chip::app::CommandPathParams commandPath(endpointId,
                                                 0, /* not used */
                                                 chip::app::Clusters::LevelControl::Id,
                                                 chip::app::Clusters::LevelControl::Commands::MoveToLevel::Id,
                                                 chip::app::CommandPathFlags::kEndpointIdValid);

        chip::app::Clusters::LevelControl::Commands::MoveToLevel::Type data;
        data.level = level;
        commandSender->AddRequestData(commandPath, data);

        // hand off the commandSender to the base class
        return SendCommand(commandSender, sessionHandle, context);
    };
} // namespace zilker
