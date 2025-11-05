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

namespace barton
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

        using namespace chip::app::Clusters::WindowCovering;
        using TypeInfo = Attributes::CurrentPositionLiftPercentage::TypeInfo;

        chip::app::ConcreteAttributePath path(
            endpointId,
            chip::app::Clusters::WindowCovering::Id,
            chip::app::Clusters::WindowCovering::Attributes::CurrentPositionLiftPercentage::Id);
        chip::TLV::TLVReader reader;
        CHIP_ERROR error = deviceDataCache->GetAttributeData(path, reader);
        if (error != CHIP_NO_ERROR)
        {
            icError("Failed to read current position lift percent attribute: %s", error.AsString());
            return false;
        }

        TypeInfo::DecodableType value;
        error = chip::app::DataModel::Decode(reader, value);
        if (error != CHIP_NO_ERROR)
        {
            icError("Failed to decode current position lift percent attribute from device %s: %s", deviceId.c_str(), error.AsString());
            return false;
        }

        static_cast<WindowCovering::EventHandler *>(eventHandler)
                    ->CurrentPositionLiftPercentageReadComplete(deviceId, value.Value(), context);

        return true;
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
} // namespace barton
