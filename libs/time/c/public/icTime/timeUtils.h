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
 * timeUtils.h
 *
 * Provide common mechanism for getting and comparting time
 * Necessary as we have different mechanisms for time based
 * on the target platform.
 *
 * Author: jelderton - 6/23/15
 *-----------------------------------------------*/

#ifndef IC_TIMEUTILS_H
#define IC_TIMEUTILS_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/*
 * returns if this platform supports the monotonic clock
 * primarily used for getCurrentTime() and incrementalCondTimedWait()
 */
bool supportMonotonic();

/*
 * gets the current time (using the platform specific mechanism)
 * and populates 'spec'.  If 'useMonotonic' is false, then the
 * "system real-time" clock will be used.
 */
void getCurrentTime(struct timespec *spec, bool useMonotonic);

/*
 * same as getCurrentTime, but returns as a time_t
 */
time_t getCurrentTime_t(bool useMonotonic);

/*
 * convert from 'struct timespec' to 'time_t'
 */
time_t convertTimespecToTime_t(struct timespec *spec);

/**
 * Convert the timespec to milliseconds since 1970-01-01T00:00:00Z
 */
uint64_t convertTimespecToUnixTimeMillis(const struct timespec *spec);

/**
 * Convert milliseconds since 1970-01-01T00:00:00Z to timespec
 */
void convertUnixTimeMillisToTimespec(uint64_t millis, struct timespec *spec);

/**
 * Convert milliseconds since 1970-01-01T00:00:00Z to time_t
 */
time_t convertUnixTimeMillisToTime_t(uint64_t millis);

/**
 * Convert seconnds since 1970-01-01T00:00:00Z to milliseconds since 1970-01-01T00:00:00Z
 */
uint64_t convertTime_tToUnixTimeMillis(time_t time);

/*
 * Compute the end - beginning and store in diff
 */
void timespecDiff(struct timespec *end, struct timespec *beginning, struct timespec *diff);

/*
 * Computer first + second and store in output
 */
void timespecAdd(struct timespec *first, struct timespec *second, struct timespec *output);


/**
 * Get the current system timestamp in milliseconds since 1970-01-01T00:00:00Z
 */
uint64_t getCurrentUnixTimeMillis(void);

/**
 * Get the current monotonic clock in milliseconds elapsed
 * @return the monotonic clock as milliseconds
 */
uint64_t getMonotonicMillis(void);

/**
 * Convert a unix timestamp in milliseconds since epoch to ISO8601 format
 * @param millis the timestamp in millis
 * @return the string, caller must free
 */
char *unixTimeMillisToISO8601(uint64_t millis);

/**
 * converts a string representing a LOCAL ISO8601 time to a struct tm
 *
 * @param inputString
 * @param outputTM
 * @return
 */
bool convertLocalISO8601ToTM(const char *inputString, struct tm *outputTM);

/**
 * converts a string representing a LOCAL ISO8601 time to milliseconds since the epoch
 * this assumed the incoming string is the local time, not UTC. Output is a parameter,
 * instead of a return value, because technically (uint64_t)0 is a valid number of
 * milliseconds since the epoch
 *
 * @param inputString the input ISO8601 string to convert
 * @param outputMillis the output milliseconds since the epoch
 * @return true if successful, false otherwise
 */
bool convertLocalISO8601ToMillis(const char *inputString, uint64_t *outputMillis);

/**
 * Check if the current system time looks valid (not close to Unix epoch)
 *
 * @return true if it appears that the current system time is valid
 */
bool isSystemTimeValid();

#endif // IC_TIMEUTILS_H
