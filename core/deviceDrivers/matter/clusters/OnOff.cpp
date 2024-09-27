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

#define LOG_TAG     "OnOffCluster"
#define logFmt(fmt) "(%s): " fmt, __func__

extern "C" {
#include <icLog/logging.h>
}

#include "OnOff.h"
#include "app-common/zap-generated/ids/Attributes.h"
#include "app-common/zap-generated/ids/Clusters.h"
#include "app/BufferedReadCallback.h"
#include "app/InteractionModelEngine.h"

namespace zilker
{
    void OnOff::OnAttributeChanged(chip::app::ClusterStateCache *cache, const chip::app::ConcreteAttributePath &path)
    {
        using namespace chip::app::Clusters::OnOff;
        using TypeInfo = Attributes::OnOff::TypeInfo;

        if (path.mClusterId == chip::app::Clusters::OnOff::Id &&
            path.mAttributeId == chip::app::Clusters::OnOff::Attributes::OnOff::Id)
        {
            TypeInfo::DecodableType value;
            CHIP_ERROR error = cache->Get<TypeInfo>(path, value);
            if (error == CHIP_NO_ERROR)
            {
                static_cast<OnOff::EventHandler *>(eventHandler)->OnOffChanged(deviceId, value, nullptr);
            }
            else
            {
                icError("Failed to decode onoff attribute: %s", error.AsString());
            }
        }
        else
        {
            icTrace("Unexpected cluster or attribute");
        }
    }

    bool OnOff::GetOnOff(void *context,
                         const chip::Messaging::ExchangeManager &exchangeMgr,
                         const chip::SessionHandle &sessionHandle)
    {
        icDebug();

        bool result = false;
        std::shared_ptr<chip::app::ClusterStateCache> cache;
        if (!(cache = clusterStateCacheRef.lock()))
        {
            icError("Failed to get onoff attribute because the cluster state cache expired or was never set");
            return result;
        }

        CHIP_ERROR error = CHIP_NO_ERROR;
        using namespace chip::app::Clusters::OnOff;
        using TypeInfo = Attributes::OnOff::TypeInfo;

        chip::app::ConcreteAttributePath path(endpointId,
                                              chip::app::Clusters::OnOff::Id,
                                              chip::app::Clusters::OnOff::Attributes::OnOff::Id);
        {
            TypeInfo::DecodableType value;
            error = cache->Get<TypeInfo>(path, value);
            if (error == CHIP_NO_ERROR)
            {
                static_cast<OnOff::EventHandler *>(eventHandler)->OnOffReadComplete(deviceId, value, context);
                result = true;
            }
        }

        if (error != CHIP_NO_ERROR)
        {
            icError("Failed to decode onoff attribute: %s", error.AsString());
        }

        return result;
    };

    bool OnOff::SetOnOff(void *context,
                         bool on,
                         const chip::Messaging::ExchangeManager &exchangeMgr,
                         const chip::SessionHandle &sessionHandle)
    {
        icDebug();

        // TODO: const_cast goes away with Matter 1.1+
        auto commandSender =
            new chip::app::CommandSender(this, const_cast<chip::Messaging::ExchangeManager *>(&exchangeMgr));
        chip::app::CommandPathParams commandPath(endpointId,
                                                 0, /* not used */
                                                 chip::app::Clusters::OnOff::Id,
                                                 on ? chip::app::Clusters::OnOff::Commands::On::Id
                                                    : chip::app::Clusters::OnOff::Commands::Off::Id,
                                                 (chip::app::CommandPathFlags::kEndpointIdValid));

        if (on)
        {
            chip::app::Clusters::OnOff::Commands::On::Type data;
            commandSender->AddRequestData(commandPath, data);
        }
        else
        {
            chip::app::Clusters::OnOff::Commands::Off::Type data;
            commandSender->AddRequestData(commandPath, data);
        }

        //hand off the commandSender to the base class
        return SendCommand(commandSender, sessionHandle, context);
    };
} // namespace zilker
