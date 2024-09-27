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

#define LOG_TAG     "WindowCoveringCluster"
#define logFmt(fmt) "(%s): " fmt, __func__

extern "C" {
#include <icLog/logging.h>
}

#include "WindowCovering.h"
#include "app-common/zap-generated/ids/Attributes.h"
#include "app-common/zap-generated/ids/Clusters.h"
#include "app/BufferedReadCallback.h"
#include "app/InteractionModelEngine.h"

namespace zilker
{
    void WindowCovering::OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                            const chip::app::ConcreteAttributePath &path)
    {
        using namespace chip::app::Clusters::WindowCovering;
        using TypeInfo = Attributes::CurrentPositionLiftPercentage::TypeInfo;

        if (path.mClusterId == chip::app::Clusters::WindowCovering::Id &&
            path.mAttributeId == chip::app::Clusters::WindowCovering::Attributes::CurrentPositionLiftPercentage::Id)
        {
            TypeInfo::DecodableType value;
            CHIP_ERROR error = cache->Get<TypeInfo>(path, value);
            if (error == CHIP_NO_ERROR)
            {
                static_cast<WindowCovering::EventHandler *>(eventHandler)
                    ->CurrentPositionLiftPercentageChanged(deviceId, value.Value(), nullptr);
            }
            else
            {
                icError("Failed to decode current position lift percent attribute: %s", error.AsString());
            }
        }
        else
        {
            icTrace("Unexpected cluster or attribute");
        }
    }

    bool WindowCovering::GetCurrentPositionLiftPercentage(void *context,
                                                          const chip::Messaging::ExchangeManager &exchangeMgr,
                                                          const chip::SessionHandle &sessionHandle)
    {
        icDebug();

        bool result = false;
        std::shared_ptr<chip::app::ClusterStateCache> cache;
        if (!(cache = clusterStateCacheRef.lock()))
        {
            icError("Failed to get current position lift percent because the cluster state cache expired or was "
                    "never set");
            return result;
        }

        CHIP_ERROR error = CHIP_NO_ERROR;
        using namespace chip::app::Clusters::WindowCovering;
        using TypeInfo = Attributes::CurrentPositionLiftPercentage::TypeInfo;

        chip::app::ConcreteAttributePath path(
            endpointId,
            chip::app::Clusters::WindowCovering::Id,
            chip::app::Clusters::WindowCovering::Attributes::CurrentPositionLiftPercentage::Id);
        {
            TypeInfo::DecodableType value;
            error = cache->Get<TypeInfo>(path, value);
            if (error == CHIP_NO_ERROR)
            {
                static_cast<WindowCovering::EventHandler *>(eventHandler)
                    ->CurrentPositionLiftPercentageReadComplete(deviceId, value.Value(), context);
                result = true;
            }
        }

        if (error != CHIP_NO_ERROR)
        {
            icError("Failed to decode current position lift percent attribute: %s", error.AsString());
        }

        return result;
    };

    bool WindowCovering::GoToLiftPercentage(void *context,
                                            uint8_t percent,
                                            const chip::Messaging::ExchangeManager &exchangeMgr,
                                            const chip::SessionHandle &sessionHandle)
    {
        icDebug();

        // TODO: const_cast goes away with Matter 1.1+
        auto commandSender =
            new chip::app::CommandSender(this, const_cast<chip::Messaging::ExchangeManager *>(&exchangeMgr));
        chip::app::CommandPathParams commandPath(endpointId,
                                                 0, /* not used */
                                                 chip::app::Clusters::WindowCovering::Id,
                                                 chip::app::Clusters::WindowCovering::Commands::GoToLiftPercentage::Id,
                                                 chip::app::CommandPathFlags::kEndpointIdValid);

        chip::app::Clusters::WindowCovering::Commands::GoToLiftPercentage::Type data;
        data.liftPercent100thsValue = percent * 100;
        commandSender->AddRequestData(commandPath, data);

        // hand off the commandSender to the base class
        return SendCommand(commandSender, sessionHandle, context);
    };
} // namespace zilker
