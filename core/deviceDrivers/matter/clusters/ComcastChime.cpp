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

#include "lib/core/DataModelTypes.h"
#define LOG_TAG     "ChimeCluster"
#define logFmt(fmt) "(%s): " fmt, __func__

extern "C" {
#include "subsystems/zigbee/zigbeeCommonIds.h"
#include <icLog/logging.h>
}

#include "ComcastChime.h"
#include "app-common/zap-generated/attributes/Accessors.h"
#include "app-common/zap-generated/cluster-objects.h"
#include "app-common/zap-generated/ids/Attributes.h"
#include "app-common/zap-generated/ids/Clusters.h"
#include "app/BufferedReadCallback.h"
#include "app/InteractionModelEngine.h"
#include "matter/zap-generated/CHIPClusters.h"

#define PLAY_SOUND_TIMEOUT_SECONDS 15
#define STOP_SOUND_TIMEOUT_SECONDS PLAY_SOUND_TIMEOUT_SECONDS

namespace zilker
{
    bool ComcastChime::PlayUrl(void *context,
                               const std::string &url,
                               chip::Messaging::ExchangeManager &exchangeMgr,
                               const chip::SessionHandle &sessionHandle)
    {
        auto commandSender = new chip::app::CommandSender(this, &exchangeMgr, true);
        chip::app::CommandPathParams commandPath(endpointId,
                                                 0, /* group id - not used */
                                                 CHIME_CLUSTER_ID,
                                                 CHIME_CLUSTER_PLAY_URL_COMMAND_ID,
                                                 (chip::app::CommandPathFlags::kEndpointIdValid));

        chip::app::Clusters::Chime::Commands::PlayUrl::Type request;
        request.url = chip::CharSpan::fromCharString(url.c_str());
        commandSender->AddRequestData(
            commandPath, request, chip::Optional<uint16_t>(PLAY_SOUND_TIMEOUT_SECONDS * 1000));

        // hand off the commandSender to the base class
        return SendCommand(commandSender, sessionHandle, context);
    }

    bool ComcastChime::StopPlay(void *context,
                                chip::Messaging::ExchangeManager &exchangeMgr,
                                const chip::SessionHandle &sessionHandle)
    {
        using namespace chip::app::Clusters::Chime::Commands;

        auto commandSender = new chip::app::CommandSender(this, &exchangeMgr, true);
        chip::app::CommandPathParams commandPath(endpointId,
                                                 0, /* group id - not used */
                                                 CHIME_CLUSTER_ID,
                                                 StopPlay::Id,
                                                 (chip::app::CommandPathFlags::kEndpointIdValid));

        StopPlay::Type data;
        commandSender->AddRequestData(commandPath, data, chip::Optional<uint16_t>(STOP_SOUND_TIMEOUT_SECONDS * 1000));

        // hand off the commandSender to the base class
        return SendCommand(commandSender, sessionHandle, context);
    }

    void ComcastChime::OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                          const chip::app::ConcreteAttributePath &path)
    {
        using namespace chip::app::Clusters::Chime;
        using TypeInfo = Attributes::AudioAssets::TypeInfo;

        if (path.mClusterId == chip::app::Clusters::Chime::Id &&
            path.mAttributeId == chip::app::Clusters::Chime::Attributes::AudioAssets::Id)
        {
            TypeInfo::DecodableType value;
            CHIP_ERROR error = cache->Get<TypeInfo>(path, value);
            if (error == CHIP_NO_ERROR)
            {
                auto newValue = std::make_unique<std::string>(value.Value().begin(), value.Value().size());
                static_cast<ComcastChime::EventHandler *>(eventHandler)
                    ->AudioAssetsChanged(deviceId, std::move(newValue), nullptr);
            }
            else
            {
                icError("Failed to decode audio assets attribute: %s", error.AsString());
            }
        }
        else
        {
            icTrace("Unexpected cluster or attribute");
        }
    }

    bool ComcastChime::GetAudioAssets(void *context,
                                      const chip::Messaging::ExchangeManager &exchangeMgr,
                                      const chip::SessionHandle &sessionHandle)
    {
        icDebug();

        bool result = false;
        std::shared_ptr<chip::app::ClusterStateCache> cache;
        if (!(cache = clusterStateCacheRef.lock()))
        {
            icError("Failed to get audio assets because the cluster state cache expired or was never set");
            return result;
        }

        CHIP_ERROR error = CHIP_NO_ERROR;
        using namespace chip::app::Clusters::Chime;
        using TypeInfo = Attributes::AudioAssets::TypeInfo;

        chip::app::ConcreteAttributePath path(
            endpointId, chip::app::Clusters::Chime::Id, chip::app::Clusters::Chime::Attributes::AudioAssets::Id);
        {
            TypeInfo::DecodableType value;
            error = cache->Get<TypeInfo>(path, value);
            if (error == CHIP_NO_ERROR)
            {
                auto newValue = std::make_unique<std::string>(value.Value().begin(), value.Value().size());
                static_cast<ComcastChime::EventHandler *>(eventHandler)
                    ->AudioAssetsReadComplete(deviceId, std::move(newValue), context);
                result = true;
            }
        }

        if (error != CHIP_NO_ERROR)
        {
            icError("Failed to decode audio assets attribute: %s", error.AsString());
        }

        return result;
    }
} // namespace zilker
