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
// Created by Christian Leithner on 12/9/20
//

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>
#include <icLog/logging.h>
#include <unistd.h>
#include <asm/errno.h>
#include <icConcurrent/icDebounce.h>
#include <icConcurrent/threadUtils.h>
#include <icConcurrent/timedWait.h>
#include <icTime/timeUtils.h>
#include <pthread.h>

#define LOG_TAG "debounceTest"

static pthread_mutex_t mtx = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static void debounceDefaultExpirationFunction(void *context)
{
    LOCK_SCOPE(mtx);
    int *expireCount = (int *) context;
    if (expireCount != NULL)
    {
        *expireCount = *expireCount + 1;
    }
    pthread_cond_signal(&cond);
}

static void test_createDestroy(void **state)
{
    (void) state;

    int *expireCount = malloc(sizeof(int));
    *expireCount = 0;
    // First try with NULL for our destroy handler. context should not get freed
    icDebounce *debouncer = debounceCreate(debounceDefaultExpirationFunction, expireCount, 100);
    assert_non_null(debouncer);
    debounceDestroy(debouncer);
    debouncer = NULL;
    assert_int_equal(*expireCount, 0);

    // Next with a destroy handler (test via asan)
    debouncer = debounceCreate(debounceDefaultExpirationFunction, expireCount, 100);
    assert_non_null(debouncer);
    debounceDestroy(debouncer);

    free(expireCount);
}

static void test_debounceExpire(void **state)
{
    static size_t timeoutErrorMarginMultiplier = 10;

    (void) state;

    int *expireCount = malloc(sizeof(int));
    *expireCount = 0;
    uint64_t timeoutMillis = 5;
    icDebounce *debouncer = debounceCreate(debounceDefaultExpirationFunction, expireCount, timeoutMillis);

    int localExpireCount;

    // Test re-usability (multiple timer expirations)
    for (int i = 0; i < 5; i++)
    {
        assert_true(debounce(debouncer));
        mutexLock(&mtx);
        while (debounceIsRunning(debouncer) == true)
        {
            struct timespec now, later, diff;
            getCurrentTime(&now, supportMonotonic());
            int result = incrementalCondTimedWaitMillis(&cond, &mtx, timeoutMillis * timeoutErrorMarginMultiplier);
            getCurrentTime(&later, supportMonotonic());
            timespecDiff(&later, &now, &diff);
            if (result == ETIMEDOUT)
            {
                break;
            }
        }
        localExpireCount = *expireCount;
        mutexUnlock(&mtx);
        assert_int_equal(localExpireCount, i + 1);
    }
    debounceDestroy(debouncer);

    *expireCount = 0;
    timeoutMillis = 100;
    debouncer = debounceCreate(debounceDefaultExpirationFunction, expireCount, timeoutMillis);

    // Test usual debounce (one timer expiration, multiple events)
    assert_true(debounce(debouncer));
    for (int i = 0; i < 5; i++)
    {
        mutexLock(&mtx);
        // Wait some amount of time, but don't let the timer expire.
        incrementalCondTimedWaitMillis(&cond, &mtx, timeoutMillis / 2);
        mutexUnlock(&mtx);

        debounce(debouncer);
    }

    // Now just wait for the latest timer refresh to end
    mutexLock(&mtx);
    while (debounceIsRunning(debouncer) == true)
    {
        int result = incrementalCondTimedWaitMillis(&cond, &mtx, timeoutMillis * timeoutErrorMarginMultiplier);
        if (result == 0 || result == ETIMEDOUT)
        {
            break;
        }
    }

    // Now check that only one expiration occurred
    localExpireCount = *expireCount;
    mutexUnlock(&mtx);
    assert_int_equal(localExpireCount, 1);

    debounceDestroy(debouncer);
    free(expireCount);
}

static void test_debounceDestroy__auto(void **state)
{
    (void) state;

    scoped_icDebounce *debouncer = debounceCreate(debounceDefaultExpirationFunction, NULL, 100);
}

int main(int argc, const char **argv)
{
    initTimedWaitCond(&cond);
    const struct CMUnitTest tests[] = {cmocka_unit_test(test_createDestroy),
                                       cmocka_unit_test(test_debounceExpire),
                                       cmocka_unit_test(test_debounceDestroy__auto)};

    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    return retval;
}
