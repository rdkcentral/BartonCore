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
 * timeTracker.c
 *
 * Track the amount of time that is spent on an operation.
 * Handy for checking if operations are taking too long
 * or for simply gathering statistics.
 *
 * Author: jelderton - 9/1/15
 *-----------------------------------------------*/

#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <icLog/logging.h>
#include <icTime/timeTracker.h>
#include <icTime/timeUtils.h>

#define LOG_TAG "TIME_TRACKER"

/**
 * Return the number of millis in the specified timeUnit
 * @param timeUnit
 * @return the number of millis in the time unit
 */
static uint64_t unitToMillisFactor(timeTrackerUnit timeUnit);

/**
 * Convert from specified unit to millis
 * @param value value to convert
 * @param timeUnit
 * @return the number of millis for value in timeUnit
 */
static uint64_t convertUnitToMillis(uint64_t value, timeTrackerUnit timeUnit);

/**
 * Convert from millis to the specified unit
 * @param valueMillis the value in millis
 * @param timeUnit
 * @param useFloor if true, floor the result, otherwise ceiling the result
 * @return the resultant value after conversion
 */
static uint64_t convertMillisToUnit(uint64_t valueMillis, timeTrackerUnit timeUnit, bool useFloor);

extern inline void timeTrackerDestroy__auto(timeTracker **tracker);

#define MILLIS_IN_SECOND (1000)
#define MILLIS_IN_MINUTE (MILLIS_IN_SECOND * 60)
#define MILLIS_IN_HOUR   (MILLIS_IN_MINUTE * 60)

/*
 * define object for tracking time
 */
struct _timeTracker
{
    uint64_t startTime;
    uint64_t timeoutMillis;
    bool running;
};

/*
 * create a new time tracker.  needs to be released
 * via the timeTrackerDestroy() function
 */
timeTracker *timeTrackerCreate()
{
    // create basic struct
    //
    timeTracker *retVal = (timeTracker *) malloc(sizeof(timeTracker));
    if (retVal != NULL)
    {
        memset(retVal, 0, sizeof(timeTracker));
        retVal->running = false;
    }
    return retVal;
}

/*
 * free a time tracker
 */
void timeTrackerDestroy(timeTracker *tracker)
{
    if (tracker != NULL)
    {
        free(tracker);
        tracker = NULL;
    }
}

/*
 * starts the timer
 */
void timeTrackerStart(timeTracker *tracker, uint32_t timeoutSecs)
{
    timeTrackerStartWithUnit(tracker, timeoutSecs, TIME_TRACKER_SECS);
}

/*
 * starts the timer
 */
void timeTrackerStartWithUnit(timeTracker *tracker, uint64_t timeout, timeTrackerUnit timeUnit)
{
    if (tracker != NULL)
    {
        tracker->timeoutMillis = convertUnitToMillis(timeout, timeUnit);
        tracker->startTime = getMonotonicMillis();
        tracker->running = true;
    }
}

/*
 * stops the timer
 */
void timeTrackerStop(timeTracker *tracker)
{
    if (tracker != NULL)
    {
        // get the elapsed time since start, and save as 'start'
        // in case something comes back around and asks for the
        // elapsed time (after it was stopped)
        //
        tracker->startTime = timeTrackerElapsedTime(tracker, TIME_TRACKER_MILLIS);

        // now set the flag
        //
        tracker->running = false;
    }
}

/*
 * return is the timer is still going
 */
bool timeTrackerRunning(timeTracker *tracker)
{
    if (tracker != NULL)
    {
        return tracker->running;
    }
    return false;
}

/*
 * return if the amount of time setup for this tracker has expired
 */
bool timeTrackerExpired(timeTracker *tracker)
{
    if (tracker != NULL && tracker->running == true)
    {
        // get the elapsed seconds, then compare to our timeout
        //
        uint64_t elapsed = timeTrackerElapsedTime(tracker, TIME_TRACKER_MILLIS);
        if (elapsed >= tracker->timeoutMillis)
        {
            return true;
        }
    }

    return false;
}

/*
 * if started, returns number of seconds remaining
 * before considering this timer 'expired'
 */
uint32_t timeTrackerSecondsUntilExpiration(timeTracker *tracker)
{
    return timeTrackerTimeUntilExpiration(tracker, TIME_TRACKER_SECS);
}

/*
 * if started, returns amount of time remaining
 * before considering this timer 'expired'
 */
uint64_t timeTrackerTimeUntilExpiration(timeTracker *tracker, timeTrackerUnit timeUnit)
{
    if (tracker != NULL && tracker->running == true)
    {
        // get the elapsed millis, then subtract from the timeout
        //
        uint64_t elapsed = timeTrackerElapsedTime(tracker, TIME_TRACKER_MILLIS);
        if (elapsed < tracker->timeoutMillis)
        {
            // Use ceiling rather than floor to ensure we don't return 0 before we have "expired"
            return convertMillisToUnit(tracker->timeoutMillis - elapsed, timeUnit, false);
        }

        // already expired, so fall through to return 0
        //
    }
    return 0;
}

/*
 * return the number of seconds the timer ran (or is running)
 */
uint32_t timeTrackerElapsedSeconds(timeTracker *tracker)
{
    return timeTrackerElapsedTime(tracker, TIME_TRACKER_SECS);
}

/*
 * return the amount of time the timer ran (or is running)
 */
uint64_t timeTrackerElapsedTime(timeTracker *tracker, timeTrackerUnit timeUnit)
{
    uint64_t retVal = 0;
    if (tracker != NULL)
    {
        // if running, diff start from now
        //
        if (tracker->running == true)
        {
            // get the number of millis between start time and now
            //
            uint64_t now = getMonotonicMillis();
            retVal = now - tracker->startTime;
        }
        else
        {
            // not running, so just return the 'startTime' as that
            // should be 0 or was set to the duration of the stop - start
            //
            retVal = tracker->startTime;
        }

        // Use floor so to be consistent with timeTrackerTimeUntilExpiration
        retVal = convertMillisToUnit(retVal, timeUnit, true);
    }

    return retVal;
}

void timeTrackerDebug(timeTracker *tracker)
{
    if (tracker == NULL)
    {
        icLogDebug(LOG_TAG, "tracker-dump: tracker is NULL");
        return;
    }

    icLogDebug(LOG_TAG,
               "tracker-dump: tracker run=%s timeout=%" PRIu64 " start=%" PRIu64 " now=%" PRIu64 " elapsed=%" PRIu64
               " remain=%" PRIu64,
               (tracker->running == true) ? "true" : "false",
               tracker->timeoutMillis,
               tracker->startTime,
               getMonotonicMillis(),
               timeTrackerElapsedTime(tracker, TIME_TRACKER_MILLIS),
               timeTrackerTimeUntilExpiration(tracker, TIME_TRACKER_MILLIS));
}

static uint64_t unitToMillisFactor(timeTrackerUnit timeUnit)
{
    uint64_t result = 1;

    switch (timeUnit)
    {
        case TIME_TRACKER_SECS:
            result = MILLIS_IN_SECOND;
            break;
        case TIME_TRACKER_MINS:
            result = MILLIS_IN_MINUTE;
            break;
        case TIME_TRACKER_HOURS:
            result = MILLIS_IN_HOUR;
            break;
        default:
        case TIME_TRACKER_MILLIS:
            break;
    }

    return result;
}

static uint64_t convertUnitToMillis(uint64_t value, timeTrackerUnit timeUnit)
{
    return value * unitToMillisFactor(timeUnit);
}

static uint64_t convertMillisToUnit(uint64_t valueMillis, timeTrackerUnit timeUnit, bool useFloor)
{
    uint64_t result = valueMillis;
    uint64_t factor = unitToMillisFactor(timeUnit);
    if (factor > 1)
    {
        double value = valueMillis / (double) factor;
        if (useFloor == true)
        {
            result = floor(value);
        }
        else
        {
            result = ceil(value);
        }
    }
    return result;
}
