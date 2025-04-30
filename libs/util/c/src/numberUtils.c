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

/*-----------------------------------------------
 * numberUtils.c
 *
 * Utilities for numbers
 *
 * Author: eInfochips
 *-----------------------------------------------*/

#include <icLog/logging.h>
#include <icUtil/numberUtils.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#define LOG_TAG "NUMBERUTILS"

/*
 * Generate random number between given range
 * NOTE: caller is responsible for passing first arg value lower than second arg.
 */
bool generateRandomNumberInRange(uint32_t lowerRange, uint32_t higherRange, uint64_t *randomNumber)
{
    // if the given lowerRange is higher than higherRange then return false.
    //
    if (lowerRange > higherRange)
    {
        icLogError(LOG_TAG, "%s: Can't generate random number for given range ", __FUNCTION__);
        return false;
    }

    // get the current time
    //
    struct timespec current;

#ifdef CONFIG_OS_DARWIN
    // do this the Mac way
    //
    struct timeval now;
    gettimeofday(&now, NULL);
    current.tv_sec = now.tv_sec;
    current.tv_nsec = now.tv_usec * 1000;
#else
    // standard linux way
    //
    clock_gettime(CLOCK_REALTIME, &current);
#endif

    // seed the random value
    //
    srandom(current.tv_nsec ^ current.tv_sec);

    // Generate a random number between given range
    //
    *randomNumber = (random() % ((higherRange - lowerRange) + 1)) + lowerRange;

    return true;
}
