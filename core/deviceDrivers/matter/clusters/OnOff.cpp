//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2024 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
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

namespace barton
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

        chip::app::ConcreteAttributePath path(
            endpointId, chip::app::Clusters::OnOff::Id, chip::app::Clusters::OnOff::Attributes::OnOff::Id);
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

        // hand off the commandSender to the base class
        return SendCommand(commandSender, sessionHandle, context);
    };
} // namespace barton
