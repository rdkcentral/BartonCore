//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2020 Comcast Corporation
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

//
// Created by Christian Leithner on 12/9/20
//

#ifndef ZILKER_ICDEBOUNCE_H
#define ZILKER_ICDEBOUNCE_H

#include <stdint.h>
#include <stdbool.h>
#include <icTypes/sbrm.h>

/**
 * icDebounce type. Provides a reusable timer to run and reset on debounce. Allows a
 * callback to be invoked when the timer expires.
 */
typedef struct _icDebounce icDebounce;

/**
 * Callback function to invoke when the timer expires.
 */
typedef void (*debounceExpiredFunc)(void *context);

/**
 * Create an icDebounce type. This type is a reusable timer that invokes a callback on timer expiration.
 * The timer can be reset on debounce.
 *
 * @param callback - The expire callback to invoke once the timer expires
 * @param context - An optional context object to pass to the expire callback
 * @param timeoutMillis - The amount of time, in milliseconds, the icDebounce timer should run for
 *
 * @return the new icDebounce
 */
icDebounce *debounceCreate(debounceExpiredFunc callback, void *context, uint64_t timeoutMillis);

/**
 * Destroys an icDebounce type.
 *
 * @param debounce - The icDebounce type to destroy
 */
void debounceDestroy(icDebounce *debounce);

/**
 * Determines is an icDebounce timer is running.
 *
 * @param debounce - The icDebounce type
 * @return true if the icDebounce timer is running, false otherwise
 */
bool debounceIsRunning(icDebounce *debounce);

/**
 * Notify an icDebounce that it should reschedule its timer if it is running, or start it fresh if it is not.
 *
 * @param debounce - The icDebounce type that's timer should be started/reset
 * @return true if the icDebounce timer was started or rescheduled, false if an error was encountered
 */
bool debounce(icDebounce *debounce);

inline void debounceDestroy__auto(icDebounce **debounce)
{
    debounceDestroy(*debounce);
}

#define scoped_icDebounce AUTO_CLEAN(debounceDestroy__auto) icDebounce

#endif //ZILKER_ICDEBOUNCE_H
