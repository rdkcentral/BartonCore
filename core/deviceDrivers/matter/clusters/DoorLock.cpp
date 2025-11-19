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

// TODO we have a smattering of timeouts that need to be coordinated
#define INTERACTION_TIMEOUT_SECONDS 15

namespace barton
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

        CHIP_ERROR error = CHIP_NO_ERROR;
        using namespace chip::app::Clusters::DoorLock;
        using TypeInfo = Attributes::LockState::TypeInfo;

        chip::app::ConcreteAttributePath path(
            endpointId, chip::app::Clusters::DoorLock::Id, chip::app::Clusters::DoorLock::Attributes::LockState::Id);
        chip::TLV::TLVReader reader;
        error = deviceDataCache->GetAttributeData(path, reader);
        if (error != CHIP_NO_ERROR)
        {
            icError("Failed to read lockstate attribute: %s", error.AsString());
            return false;
        }

        TypeInfo::DecodableType value;
        error = chip::app::DataModel::Decode(reader, value);
        if (error != CHIP_NO_ERROR)
        {
            icError("Failed to decode lockstate attribute: %s", error.AsString());
            return false;
        }

        static_cast<DoorLock::EventHandler *>(eventHandler)
            ->LockStateReadComplete(deviceId, value.Value(), context);

        return true;
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
} // namespace barton
