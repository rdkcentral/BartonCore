// ------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2025 Comcast Cable Communications Management, LLC
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
// ------------------------------ tabstop = 4 ----------------------------------

//
// Created by Raiyan Chowdhury on 8/12/24.
//

#define LOG_TAG     "BooleanStateCluster"
#define logFmt(fmt) "(%s): " fmt, __func__

extern "C" {
#include <icLog/logging.h>
}

#include "BooleanState.h"
#include "app-common/zap-generated/ids/Attributes.h"
#include "app-common/zap-generated/ids/Clusters.h"

namespace barton
{
    void BooleanState::OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                          const chip::app::ConcreteAttributePath &path)
    {
        using namespace chip::app;
        using namespace chip::app::Clusters::BooleanState;

        if (path.mClusterId == Clusters::BooleanState::Id && path.mAttributeId == Attributes::StateValue::Id)
        {
            using TypeInfo = Attributes::StateValue::TypeInfo;
            TypeInfo::DecodableType value;
            CHIP_ERROR error = cache->Get<TypeInfo>(path, value);
            if (error == CHIP_NO_ERROR)
            {
                static_cast<BooleanState::EventHandler *>(eventHandler)->StateValueChanged(deviceId, value);
            }
            else
            {
                icError("Failed to decode state value attribute: %s", error.AsString());
            }
        }
        else
        {
            icTrace("Unexpected cluster or attribute");
        }
    }

    bool BooleanState::GetStateValue(void *context,
                                     const chip::Messaging::ExchangeManager &exchangeMgr,
                                     const chip::SessionHandle &sessionHandle)
    {
        icDebug();

        CHIP_ERROR error = CHIP_NO_ERROR;
        using namespace chip::app;
        using namespace chip::app::Clusters::BooleanState;
        using TypeInfo = Attributes::StateValue::TypeInfo;

        chip::app::ConcreteAttributePath path(endpointId, Clusters::BooleanState::Id, Attributes::StateValue::Id);
        chip::TLV::TLVReader reader;
        error = deviceDataCache->GetAttributeData(path, reader);
        if (error != CHIP_NO_ERROR)
        {
            icError("Failed to read state value attribute: %s", error.AsString());
            return false;
        }

        TypeInfo::DecodableType value;
        error = chip::app::DataModel::Decode(reader, value);
        if (error != CHIP_NO_ERROR)
        {
            icError("Failed to decode state value attribute: %s", error.AsString());
            return false;
        }

        static_cast<BooleanState::EventHandler *>(eventHandler)->StateValueReadComplete(deviceId, value, context);

        return true;
    }
}
