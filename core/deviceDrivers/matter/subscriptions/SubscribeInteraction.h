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

/**
 * Created by Raiyan Chowdhury on 12/05/23.
 */

#pragma once

#include "app/ClusterStateCache.h"
#include "app/InteractionModelEngine.h"
#include "app/ReadClient.h"
#include <future>

/**
 * Per Matter 1.1 Section 2.11.2.2, a publisher only needs to support
 * at least 3 subscribe interactions, each of which only need to support
 * at least 3 event/attribute paths.
 */
#define MAX_SUBSCRIPTIONS                             3
#define MAX_PATHS_PER_SUBSCRIBE                       3
#define MAX_PATHS_PER_PUBLISHER                       (MAX_SUBSCRIPTIONS * MAX_PATHS_PER_SUBSCRIBE)

/*
 * A ceiling of 10m24s is used for the fibonacci-based backoff algorithm, rather than the SDK's default of 92m18s.
 * When the retry count exceeds the SDK's default step index, the backoff algo would average ~60 minutes between
 * each retry, which is a very long time to wait if a device happens comes back online e.g. 58 minutes before the
 * next attempt, whereas with this ceiling we average a more reasonable 6m45s.
 * @see CustomComputeTimeTillNextSubscription for more details.
 */
#define CUSTOM_RESUBSCRIBE_MAX_FIBONACCI_STEP_INDEX   10
#define CUSTOM_RESUBSCRIBE_MAX_RETRY_WAIT_INTERVAL_MS 624000

namespace zilker
{

    class SubscribeInteraction : public chip::app::ClusterStateCache::Callback
    {
    public:
        class EventHandler
        {
        public:
            virtual ~EventHandler() = default;
            virtual void OnSubscriptionEstablished(chip::SubscriptionId aSubscriptionId,
                                                   std::promise<bool> &subscriptionPromise,
                                                   std::shared_ptr<chip::app::ClusterStateCache> &clusterStateCache)
            {
            }
            virtual void AbandonSubscription(std::promise<bool> &subscriptionPromise) {}
            virtual void OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                            const chip::app::ConcreteAttributePath &path,
                                            const std::string deviceId)
            {
            }
            virtual void OnEventData(const chip::app::EventHeader &aEventHeader,
                                     chip::TLV::TLVReader *apData,
                                     const chip::app::StatusIB *apStatus,
                                     const std::string deviceId,
                                     SubscribeInteraction &outerSubscriber)
            {
            }
        };

        SubscribeInteraction(std::unique_ptr<EventHandler> handler,
                             std::string deviceId,
                             chip::Messaging::ExchangeManager &exchangeMgr,
                             std::promise<bool> &promise) :
            eventHandler(std::move(handler)), deviceUuid(deviceId), subscriptionPromise(promise)
        {
            clusterStateCache = std::make_shared<chip::app::ClusterStateCache>(*this);
            readClient = std::make_unique<chip::app::ReadClient>(chip::app::InteractionModelEngine::GetInstance(),
                                                                 &exchangeMgr,
                                                                 clusterStateCache->GetBufferedCallback(),
                                                                 chip::app::ReadClient::InteractionType::Subscribe);
        }

        CHIP_ERROR Send(chip::app::ReadPrepareParams &&params)
        {
            return readClient->SendAutoResubscribeRequest(std::move(params));
        }

        void OnSubscriptionEstablished(chip::SubscriptionId aSubscriptionId) override;

        CHIP_ERROR OnResubscriptionNeeded(chip::app::ReadClient *apReadClient, CHIP_ERROR aTerminationCause) override;

        void OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                const chip::app::ConcreteAttributePath &path) override;

        void OnEventData(const chip::app::EventHeader &aEventHeader,
                         chip::TLV::TLVReader *apData,
                         const chip::app::StatusIB *apStatus) override;

        /* has no real implementation anywhere, probably ignore for now */
        void OnDone(chip::app::ReadClient *apReadClient) override {}

        /**
         * @brief Delete allocated path parameters within ReadPrepareParms
         * @note Always keep the implementation of this method synchronized
         *       with the manner in which the MatterDeviceDriver allocates
         *       params for subscriptions
         * @note *Important* Define an explicit destructor that releases all
         *        protected ReadClient instances when overriding.
         */
        void OnDeallocatePaths(chip::app::ReadPrepareParams &&params) override;

        void AbandonSubscription();

        /**
         * @brief Set the liveness check interval, in milliseconds. This will immediately cancel
         *        any pending checks and schedule a new one at the new interval.
         *
         * @param livenessMillis Any nonzero value, use zero to use device's maxInterval
         * @return true when this cluster has an active subscription.
         */
        bool SetLivenessCheckMillis(uint32_t livenessMillis);

        /**
         * @brief Backoff algorithm that computes how long Matter should wait before attempting to
         *        resubscribe again, when a resubscribe is called for. Initially, the reattempts are
         *        aggressively fast. If these early attempts fail, then the algorithm exponentionally
         *        backs off to a less aggressive frequency of retries before eventually hitting a
         *        moderate ceiling.
         */
        uint32_t CustomComputeTimeTillNextSubscription();

    private:
        friend class MatterBackoffAlgorithmTest;
        std::unique_ptr<EventHandler> eventHandler;
        std::shared_ptr<chip::app::ClusterStateCache> clusterStateCache;
        std::unique_ptr<chip::app::ReadClient> readClient;
        std::string deviceUuid;
        std::promise<bool> &subscriptionPromise;
        uint32_t mOnResubscriptionsAttempted = 0;

        /**
         * @brief This private constructor is strictly for testing units that do not require the ReadClient
         *        or ClusterStateCache objects.
         */
        SubscribeInteraction(std::unique_ptr<EventHandler> handler, std::string deviceId, std::promise<bool> &promise) :
            eventHandler(std::move(handler)), deviceUuid(deviceId), subscriptionPromise(promise)
        {
        }
    };

    struct SubscriptionIntervalSecs
    {
        SubscriptionIntervalSecs(uint16_t floor, uint16_t ceiling) :
            minIntervalFloorSecs(floor), maxIntervalCeilingSecs(ceiling)
        {
        }
        uint16_t minIntervalFloorSecs;
        uint16_t maxIntervalCeilingSecs;
    };
} // namespace zilker
