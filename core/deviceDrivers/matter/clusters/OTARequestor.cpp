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
// Created by vramac391 on 12/1/22.
//

#include "app-common/zap-generated/cluster-enums.h"
#include <memory>
#define LOG_TAG     "OTARequestorCluster"
#define logFmt(fmt) "(%s): " fmt, __func__

extern "C" {
#include <icLog/logging.h>
}

#include "OTARequestor.h"
#include "app-common/zap-generated/ids/Attributes.h"
#include "app-common/zap-generated/ids/Clusters.h"
#include "app/BufferedReadCallback.h"
#include "app/InteractionModelEngine.h"
#include "controller/WriteInteraction.h"
#include <cinttypes>
#include <cstdint>

using namespace chip::app::Clusters::OtaSoftwareUpdateRequestor;

namespace zilker
{
    void OTARequestor::OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                          const chip::app::ConcreteAttributePath &path)
    {
        if (path.mClusterId != OtaSoftwareUpdateRequestor::Id)
        {
            return;
        }

        switch (path.mAttributeId)
        {
            case Attributes::UpdateStateProgress::Id:
            {
                using StateInfo = Attributes::UpdateState::TypeInfo;
                StateInfo::DecodableType state;

                chip::app::ConcreteAttributePath statePath(
                    path.mEndpointId, path.mClusterId, Attributes::UpdateState::Id);

                CHIP_ERROR error = cache->Get<StateInfo>(statePath, state);

                if (error != CHIP_NO_ERROR)
                {
                    icWarn("Failed to get update state: %s", error.AsString());
                    break;
                }

                if (state != OTAUpdateStateEnum::kDownloading)
                {
                    break;
                }

                using PercentInfo = Attributes::UpdateStateProgress::TypeInfo;
                PercentInfo::DecodableType percent;
                error = cache->Get<PercentInfo>(path, percent);

                if (error != CHIP_NO_ERROR)
                {
                    icWarn("Failed to get download progress: %s", error.AsString());
                    break;
                }

                if (percent.IsNull())
                {
                    break;
                }

                icDebug("%s download progress: %" PRIu8 "%%", GetDeviceId().c_str(), percent.Value());

                // TODO: probably tell event handler about this change (state, progress)
                break;
            }

            default:
                // don't make noise about unimportant changes
                break;
        }
    }

    void OTARequestor::OnEventDataReceived(SubscribeInteraction &subscriber,
                                           const chip::app::EventHeader &aEventHeader,
                                           chip::TLV::TLVReader *apData,
                                           const chip::app::StatusIB *apStatus)
    {
        if (!aEventHeader.mPath.IsValidConcreteClusterPath())
        {
            icError("Received event with invalid path, dropping it!");
            return;
        }

        if (eventHandler == nullptr)
        {
            return;
        }

        icDebug("Endpoint: %u Cluster: " ChipLogFormatMEI " Event " ChipLogFormatMEI,
                aEventHeader.mPath.mEndpointId,
                ChipLogValueMEI(aEventHeader.mPath.mClusterId),
                ChipLogValueMEI(aEventHeader.mPath.mEventId));

        switch (aEventHeader.mPath.mEventId)
        {
            case Events::StateTransition::Id:
            {

                Events::StateTransition::DecodableType value;
                ReturnOnFailure(chip::app::DataModel::Decode(*apData, value));

                static_cast<OTARequestor::EventHandler *>(eventHandler)
                    ->OnStateTransition(*this,
                                        value.previousState,
                                        value.newState,
                                        value.reason,
                                        value.targetSoftwareVersion,
                                        subscriber);

                break;
            }


            case Events::DownloadError::Id:
            {
                Events::DownloadError::DecodableType value;
                ReturnOnFailure(chip::app::DataModel::Decode(*apData, value));

                static_cast<OTARequestor::EventHandler *>(eventHandler)
                    ->OnDownloadError(*this,
                                      value.softwareVersion,
                                      value.bytesDownloaded,
                                      value.progressPercent,
                                      value.platformCode,
                                      subscriber);
                break;
            }


            case Events::VersionApplied::Id:
            {
                Events::VersionApplied::DecodableType value;
                ReturnOnFailure(chip::app::DataModel::Decode(*apData, value));

                static_cast<OTARequestor::EventHandler *>(eventHandler)
                    ->OnVersionApplied(*this, value.softwareVersion, value.productID, subscriber);
                break;
            }

            default:
                break;
                // Logging unexpected events we don't care about can be noisy, and
                // unsolicited reports from devices is not controllable from here.
        }
    }

    bool OTARequestor::SetDefaultOTAProviders(void *context,
                                              std::vector<Structs::ProviderLocation::Type> &otaProviderList,
                                              const chip::Messaging::ExchangeManager &exchangeMgr,
                                              const chip::SessionHandle &sessionHandle)
    {
        icDebug();

        // FIXME: exchangeMgr const_cast goes away with Matter 1.1+
        auto writeClient = new chip::app::WriteClient(
            const_cast<chip::Messaging::ExchangeManager *>(&exchangeMgr), this, chip::Optional<uint16_t>::Missing());

        chip::app::AttributePathParams attributePathParams;
        Attributes::DefaultOTAProviders::TypeInfo::Type defaultOtaProvidersListAttr(otaProviderList.data(),
                                                                                    otaProviderList.size());

        attributePathParams.mEndpointId = endpointId;
        attributePathParams.mClusterId = OtaSoftwareUpdateRequestor::Id;
        attributePathParams.mAttributeId = Attributes::DefaultOTAProviders::Id;

        CHIP_ERROR err = writeClient->EncodeAttribute(attributePathParams, defaultOtaProvidersListAttr);

        if (err != CHIP_NO_ERROR)
        {
            icError("Failed to encode attribute");

            delete writeClient;
            return false;
        }

        return SendWriteRequest(writeClient, sessionHandle, context);
    }
} // namespace zilker
