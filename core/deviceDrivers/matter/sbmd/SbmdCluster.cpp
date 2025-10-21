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
// Created by tlea on 10/30/25
//

#include "lib/core/CHIPError.h"
#define LOG_TAG     "SbmdCluster"
#define logFmt(fmt) "(%s): " fmt, __func__

extern "C" {
#include <icLog/logging.h>
}

#include "SbmdCluster.h"
#include "lib/core/TLVReader.h"
#include "app-common/zap-generated/ids/Attributes.h"
#include "app-common/zap-generated/ids/Clusters.h"
#include "app/BufferedReadCallback.h"
#include "app/InteractionModelEngine.h"

// TODO we have a smattering of timeouts that need to be coordinated
#define INTERACTION_TIMEOUT_SECONDS 15

namespace barton
{
    void SbmdCluster::OnAttributeChanged(chip::app::ClusterStateCache *cache, const chip::app::ConcreteAttributePath &path)
    {
        // make sure path.mClusterId and path.mAttributeId are for us
        if(true)
        {
            // TypeInfo::DecodableType value;
            // CHIP_ERROR error = cache->Get<TypeInfo>(path, value);
            // if (error == CHIP_NO_ERROR)
            // {
            //     static_cast<DoorLock::EventHandler *>(eventHandler)->LockStateChanged(deviceId, value.Value(), nullptr);
            // }
            // else
            // {
            //     icError("Failed to decode lockstate attribute: %s", error.AsString());
            // }
        }
        else
        {
            icTrace("Unexpected cluster or attribute");
        }
    }

    CHIP_ERROR SbmdCluster::ReadAttribute(void *context,
                                    uint16_t attributeId,
                                    const chip::Messaging::ExchangeManager &exchangeMgr,
                                    const chip::SessionHandle &sessionHandle)
    {
        CHIP_ERROR result = CHIP_NO_ERROR;

        icDebug();

        std::shared_ptr<chip::app::ClusterStateCache> cache;
        if (!(cache = clusterStateCacheRef.lock()))
        {
            icError("Failed to read attribute because the cluster state cache expired or was never set");
            return CHIP_ERROR_INTERNAL;
        }

        chip::app::ConcreteAttributePath path(endpointId, clusterId, attributeId);
        {
            chip::TLV::TLVReader reader;
            result = cache->Get(path, reader);
            if (result == CHIP_NO_ERROR)
            {
                static_cast<SbmdCluster::EventHandler *>(eventHandler)
                    ->AttributeReadComplete(deviceId,
                                            clusterId,
                                            attributeId,
                                            reader,
                                            context);
            }
        }

        return result;
#if 0
    ReadPrepareParams params(device->GetSecureSession().Value());
    params.mpEventPathParamsList        = nullptr;
    params.mEventPathParamsListSize     = 0;
    params.mpAttributePathParamsList    = pathsConfig.attributePathParams.get();
    params.mAttributePathParamsListSize = pathsConfig.count;

    if (mFabricFiltered.HasValue())
    {
        params.mIsFabricFiltered = mFabricFiltered.Value();
    }

    if (mDataVersions.HasValue())
    {
        params.mpDataVersionFilterList    = pathsConfig.dataVersionFilter.get();
        params.mDataVersionFilterListSize = pathsConfig.count;
    }

    if (interactionType == ReadClient::InteractionType::Subscribe)
    {
        params.mMinIntervalFloorSeconds   = mMinInterval;
        params.mMaxIntervalCeilingSeconds = mMaxInterval;
        if (mKeepSubscriptions.HasValue())
        {
            params.mKeepSubscriptions = mKeepSubscriptions.Value();
        }
        params.mIsPeerLIT = mIsPeerLIT;
    }

    auto client = std::make_unique<ReadClient>(InteractionModelEngine::GetInstance(), device->GetExchangeManager(),
                                               mBufferedReadAdapter, interactionType);

    ReturnErrorOnFailure(client->SendRequest(params));
#endif
#if 0
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

        chip::app::ConcreteAttributePath path(
            endpointId, chip::app::Clusters::DoorLock::Id, chip::app::Clusters::DoorLock::Attributes::LockState::Id);
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
#endif
    };

} // namespace barton
