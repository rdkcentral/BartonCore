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

#ifndef ZILKER_ICDEBOUNCE_H
#define ZILKER_ICDEBOUNCE_H

#include <icTypes/sbrm.h>
#include <stdbool.h>
#include <stdint.h>

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

#endif // ZILKER_ICDEBOUNCE_H
