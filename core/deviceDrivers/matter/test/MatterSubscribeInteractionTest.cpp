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

/*
 * Created by Raiyan Chowdhury on 4/29/24.
 */

#define TEST_DEVICE_ID               "0x1111"
#define RESUBSCRIBE_ATTEMPTS_TO_TEST 100

#include "matter/subscriptions/SubscribeInteraction.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace zilker
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
} // namespace zilker
