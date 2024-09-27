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

#include <time.h>
#include <stdbool.h>
#include <stdint.h>

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

/*
 * Provide compatibilty for gingerbread which doesn't have timegm or timelocal only timegm64 and timelocal64
 */
#ifdef CONFIG_PRODUCT_TCA203
time_t timegm(struct tm const *t);

time_t timelocal(struct tm const *t);

#endif

#endif // IC_TIMEUTILS_H
