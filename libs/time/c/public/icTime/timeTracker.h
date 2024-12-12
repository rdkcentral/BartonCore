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
 * timeTracker.h
 *
 * Track the amount of time that is spent on an operation.
 * Handy for checking if operations are taking too long
 * or for simply gathering statistics.
 *
 * This is not a threadsafe API. Threads must coordinate access
 * to timeTracker objects.
 *
 * Author: jelderton - 9/1/15
 *-----------------------------------------------*/

#ifndef FLEXCORE_TIMETRACKER_H
#define FLEXCORE_TIMETRACKER_H

#include <stdbool.h>
#include <stdint.h>


typedef enum timeTrackerUnit
{
    TIME_TRACKER_MILLIS,
    TIME_TRACKER_SECS,
    TIME_TRACKER_MINS,
    TIME_TRACKER_HOURS
} timeTrackerUnit;

// NULL array with string representations of trimeTrackerUnit (debugging)
static const char *timeTrackerUnitLabels[] = {"MILLIS", "SECS", "MINS", "HOURS", NULL};

/*
 * define object for tracking time
 */
typedef struct _timeTracker timeTracker;

/*
 * create a new time tracker.  needs to be released
 * via the timeTrackerDestroy() function
 */
timeTracker *timeTrackerCreate();

/*
 * free a time tracker
 */
void timeTrackerDestroy(timeTracker *tracker);

inline void timeTrackerDestroy__auto(timeTracker **tracker)
{
    timeTrackerDestroy(*tracker);
}

#define scoped_timeTracker AUTO_CLEAN(timeTrackerDestroy__auto) timeTracker

/*
 * starts the timer
 */
void timeTrackerStart(timeTracker *tracker, uint32_t timeoutSecs);

/*
 * starts the timer
 */
void timeTrackerStartWithUnit(timeTracker *tracker, uint64_t timeout, timeTrackerUnit timeUnit);

/*
 * stops the timer
 */
void timeTrackerStop(timeTracker *tracker);

/*
 * return is the timer is still going
 */
bool timeTrackerRunning(timeTracker *tracker);

/*
 * return if the amount of time setup for this tracker has expired
 */
bool timeTrackerExpired(timeTracker *tracker);

/*
 * if started, returns number of seconds remaining
 * before considering this timer 'expired'
 */
uint32_t timeTrackerSecondsUntilExpiration(timeTracker *tracker);

/*
 * if started, returns amount of time remaining
 * before considering this timer 'expired'
 */
uint64_t timeTrackerTimeUntilExpiration(timeTracker *tracker, timeTrackerUnit timeUnit);

/*
 * return the number of seconds the timer ran (or is running)
 */
uint32_t timeTrackerElapsedSeconds(timeTracker *tracker);

/*
 * return the amount of time the timer ran (or is running)
 */
uint64_t timeTrackerElapsedTime(timeTracker *tracker, timeTrackerUnit timeUnit);

void timeTrackerDebug(timeTracker *tracker);

#endif // FLEXCORE_TIMETRACKER_H
