//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2020 Comcast
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
// Comcast Networks retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*-----------------------------------------------
 * numberUtils.c
 *
 * Utilities for numbers
 *
 * Author: eInfochips
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <icLog/logging.h>
#include <icUtil/numberUtils.h>

#define LOG_TAG    "NUMBERUTILS"

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
        icLogError(LOG_TAG, "%s: Can't generate random number for given range ",__FUNCTION__);
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
    current.tv_sec  = now.tv_sec;
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
