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
 * timedWait.c
 *
 *  Created on: Oct 13, 2010
 *      Author: gfaulkner
 */

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <icConcurrent/timedWait.h>
#include <icLog/logging.h>
#include <icTime/timeUtils.h>

// TODO this guard probably needs to change at some point, newer versions of droid might
// not have this, but support the standard pthread_condattr_setclock.  We should be
// functions to be using
#ifdef BUILDENV_android
// Ugly but this isn't declared in the universal headers, but the symbol exists in libc
// This way we don't get undefined symbol warnings, which are treated as errors
int pthread_cond_timedwait_monotonic_np(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime);
#endif


/**
 * initialize a conditional for use in a relative timed wait
 */
int initTimedWaitCond(pthread_cond_t *cond)
{
    pthread_condattr_t condattr;
    pthread_condattr_init(&condattr);

    // if using monotonic clock, need to tell pthread about that
    // or else the timedwait call below will not work properly
    //
#if !(defined CONFIG_OS_DARWIN) && !(defined BUILDENV_android)
    if (supportMonotonic() == true)
    {
        pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC);
    }
#endif

    return pthread_cond_init(cond, &condattr);
}

/**
 * Performs the actual timed wait using the given timespec
 * @see incrementalCondTimedWait()
 * @see granularIncrementalCondTimedWait
 */
static int doCondTimedWait(pthread_cond_t *cond, pthread_mutex_t *mtx, struct timespec *tp)
{
#ifdef BUILDENV_android
    // Android has specific function to use the motonic clock
    return pthread_cond_timedwait_monotonic_np(cond, mtx, tp);
#elif CONFIG_OS_DARWIN
    if (supportMonotonic() == true)
    {
        return pthread_cond_timedwait_relative_np(cond, mtx, tp);
    }
    else
    {
        return pthread_cond_timedwait(cond, mtx, tp);
    }
#else
    // assuming Linux or MIPS; the motonic clock is not a special function
    return pthread_cond_timedwait(cond, mtx, tp);
#endif
}

/**
 * does a timed wait based on the monotonic clock (if supported).
 * The cond parm should have been initialized using initTimedWaitCond().
 * The mutex should already be held by the caller
 *
 * @see initTimedWaitCond()
 */
int incrementalCondTimedWait(pthread_cond_t *cond, pthread_mutex_t *mtx, int timeoutSecs)
{
    struct timespec tp;

    // get the time, then add the 'timeout' seconds to it.  note that
    // we will use the monotonic clock "if supported".
    //
    getCurrentTime(&tp, supportMonotonic());
    tp.tv_sec += timeoutSecs;

    return doCondTimedWait(cond, mtx, &tp);
}

/**
 * does a timed wait based on the monotonic clock (if supported).
 * The cond parm should have been initialized using initTimedWaitCond().
 * The mutex should already be held by the caller
 *
 * @see initTimedWaitCond()
 */
int incrementalCondTimedWaitMillis(pthread_cond_t *cond, pthread_mutex_t *mtx, uint64_t timeoutMillis)
{
    struct timespec tp;

    // get the time, then add the 'timeout' millis to it.  note that
    // we will use the monotonic clock "if supported".
    //
    getCurrentTime(&tp, supportMonotonic());
    tp.tv_sec += timeoutMillis / 1000UL;
    tp.tv_nsec += (timeoutMillis % 1000UL) * 1000000UL;
    if (tp.tv_nsec >= 1000000000UL)
    {
        tp.tv_sec++;
        tp.tv_nsec = tp.tv_nsec - 1000000000UL;
    }

    return doCondTimedWait(cond, mtx, &tp);
}


/**
 * does a timed wait based on the monotonic clock.  The cond parm should have been initialized using
 * initTimedWaitCond().  The mutex should already be held by the caller.  So, these waits should be considered
 * relative.  If you want an absolute wait, you need to calculate the timeoutSecs accordingly.
 * This one allows for more granular control (includes nanoseconds)
 */
int granularIncrementalCondTimedWait(pthread_cond_t *cond, pthread_mutex_t *mtx, struct timespec *waitTime)
{
    struct timespec tp;

    // get the monotonic time, then add the 'timeout' to it
    //
    getCurrentTime(&tp, true);
    tp.tv_sec += waitTime->tv_sec;
    uint64_t totalNano = tp.tv_nsec;
    totalNano += waitTime->tv_nsec;
    while (totalNano >= 1000000000UL)
    {
        tp.tv_sec++;
        totalNano = totalNano - 1000000000UL;
    }
    tp.tv_nsec = totalNano;

    return doCondTimedWait(cond, mtx, &tp);
}
