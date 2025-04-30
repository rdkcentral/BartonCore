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

/*
 * Created by Raiyan Chowdhury on 4/29/24.
 */

#define TEST_DEVICE_ID               "0x1111"
#define RESUBSCRIBE_ATTEMPTS_TO_TEST 100

#include "matter/subscriptions/SubscribeInteraction.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace barton
{
    class MatterBackoffAlgorithmTest : public ::testing::Test
    {
    public:
        MatterBackoffAlgorithmTest()
        {
            auto subscribeInteractionEventHandler = std::make_unique<SubscribeInteraction::EventHandler>();
            auto subInteractionPtr =
                new SubscribeInteraction(std::move(subscribeInteractionEventHandler), TEST_DEVICE_ID, fakePromise);
            subscribeInteraction = std::unique_ptr<SubscribeInteraction>(subInteractionPtr);
        }

        void SetUp() override { subscribeInteraction->mOnResubscriptionsAttempted = 0; }

        uint32_t CustomComputeTimeTillNextSubscription()
        {
            subscribeInteraction->mOnResubscriptionsAttempted++;
            return subscribeInteraction->CustomComputeTimeTillNextSubscription();
        }

        std::unique_ptr<SubscribeInteraction> subscribeInteraction;
        std::promise<bool> fakePromise;
    };

    using namespace ::testing;

    /**
     * @brief Our backoff algorithm is mostly identical to the backoff algorithm used by default in the Matter SDK,
     *        which uses a Fibonacci sequence to calculate an exponentially longer backoff range on each resubscribe
     *        attempt. We can surmise that their logic works as expected; the only thing we have changed is that our
     *        backoff ceiling is 10 minutes and 24 seconds, as opposed to the SDK's ~1.5 hours, so this test will just
     *        verify that as it is our main concern with this algorithm.
     */
    TEST_F(MatterBackoffAlgorithmTest, BackoffAlgorithmCeilingTest)
    {
        for (int i = 0; i < RESUBSCRIBE_ATTEMPTS_TO_TEST; i++)
        {
            auto timeTilNextResub = CustomComputeTimeTillNextSubscription();
            ASSERT_LE(timeTilNextResub, CUSTOM_RESUBSCRIBE_MAX_RETRY_WAIT_INTERVAL_MS);
        }
    }
} // namespace barton
