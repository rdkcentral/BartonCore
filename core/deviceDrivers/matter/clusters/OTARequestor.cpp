//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2023 Comcast Corporation
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
