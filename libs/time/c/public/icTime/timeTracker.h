//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2015 iControl Networks, Inc.
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
// iControl Networks retains all ownership rights.
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

#include <stdint.h>
#include <stdbool.h>


typedef enum timeTrackerUnit {
    TIME_TRACKER_MILLIS,
    TIME_TRACKER_SECS,
    TIME_TRACKER_MINS,
    TIME_TRACKER_HOURS
} timeTrackerUnit;

// NULL array with string representations of trimeTrackerUnit (debugging)
static const char *timeTrackerUnitLabels[] = {
     "MILLIS",
     "SECS",
     "MINS",
     "HOURS",
     NULL
};

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
