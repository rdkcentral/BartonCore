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

/**
 * Created by Raiyan Chowdhury on 12/05/23.
 */

#include "SubscribeInteraction.h"
#include <lib/support/FibonacciUtils.h>

using namespace zilker;

void SubscribeInteraction::OnSubscriptionEstablished(chip::SubscriptionId aSubscriptionId)
{
    mOnResubscriptionsAttempted = 0;
    eventHandler->OnSubscriptionEstablished(aSubscriptionId, subscriptionPromise, clusterStateCache);
}

CHIP_ERROR SubscribeInteraction::OnResubscriptionNeeded(chip::app::ReadClient *apReadClient,
                                                        CHIP_ERROR aTerminationCause)
{
    mOnResubscriptionsAttempted++;
    auto timeTillNextResubscription = CustomComputeTimeTillNextSubscription();

    ChipLogProgress(DataManagement,
                    "Will try to resubscribe to %02x:" ChipLogFormatX64 " at retry index %" PRIu32 " after %" PRIu32
                    "ms due to error %" CHIP_ERROR_FORMAT,
                    apReadClient->GetFabricIndex(),
                    ChipLogValueX64(apReadClient->GetPeerNodeId()),
                    mOnResubscriptionsAttempted,
                    timeTillNextResubscription,
                    aTerminationCause.Format());

    return apReadClient->ScheduleResubscription(
        timeTillNextResubscription, chip::NullOptional, aTerminationCause == CHIP_ERROR_TIMEOUT);
}

uint32_t SubscribeInteraction::CustomComputeTimeTillNextSubscription()
{
    uint32_t maxWaitTimeInMsec = 0;
    uint32_t waitTimeInMsec = 0;
    uint32_t minWaitTimeInMsec = 0;

    if (mOnResubscriptionsAttempted <= CUSTOM_RESUBSCRIBE_MAX_FIBONACCI_STEP_INDEX)
    {
        maxWaitTimeInMsec =
            chip::GetFibonacciForIndex(mOnResubscriptionsAttempted) * CHIP_RESUBSCRIBE_WAIT_TIME_MULTIPLIER_MS;
    }
    else
    {
        maxWaitTimeInMsec = CUSTOM_RESUBSCRIBE_MAX_RETRY_WAIT_INTERVAL_MS;
    }

    if (maxWaitTimeInMsec != 0)
    {
        minWaitTimeInMsec = (CHIP_RESUBSCRIBE_MIN_WAIT_TIME_INTERVAL_PERCENT_PER_STEP * maxWaitTimeInMsec) / 100;
        waitTimeInMsec = minWaitTimeInMsec + (chip::Crypto::GetRandU32() % (maxWaitTimeInMsec - minWaitTimeInMsec));
    }

    return waitTimeInMsec;
}

void SubscribeInteraction::AbandonSubscription()
{
    eventHandler->AbandonSubscription(subscriptionPromise);
}

bool SubscribeInteraction::SetLivenessCheckMillis(uint32_t livenessMillis)
{
    bool ok = false;

    if (readClient->GetSubscriptionId().HasValue())
    {
        readClient->OverrideLivenessTimeout(chip::System::Clock::Milliseconds32(livenessMillis));
        ok = true;
    }

    return ok;
}

void SubscribeInteraction::OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                              const chip::app::ConcreteAttributePath &path)
{
    eventHandler->OnAttributeChanged(cache, path, deviceUuid);
}

void SubscribeInteraction::OnEventData(const chip::app::EventHeader &aEventHeader,
                                       chip::TLV::TLVReader *apData,
                                       const chip::app::StatusIB *apStatus)
{
    eventHandler->OnEventData(aEventHeader, apData, apStatus, deviceUuid, *this);
}

void SubscribeInteraction::OnDeallocatePaths(chip::app::ReadPrepareParams &&params)
{
    delete params.mpEventPathParamsList;
    delete params.mpAttributePathParamsList;
}
