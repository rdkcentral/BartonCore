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
 * delayedTask.h
 *
 * Helper utility to perform a task in a background thread,
 * after waiting for an initial period of time.
 *
 * NOTE: uses the concept of 'handles' vs 'objects' due to
 * the nature of tasks executing and releasing in the background.
 * this prevents memory issues with the caller having a pointer
 * to an object that may have been released in the background.
 *
 * Author: jelderton - 7/6/15
 *-----------------------------------------------*/

#ifndef IC_DELAYTASK_H
#define IC_DELAYTASK_H

#include <stdbool.h>
#include <stdint.h>

/*
 * function to run after the delay time has expired.
 * the implementation should free 'arg' if necessary.
 *
 * @param arg - the optional argument supplied when the task was started
 */
typedef void (*taskCallbackFunc)(void *arg);

/*
 * define the delay units of measure
 */
typedef enum
{
    DELAY_HOURS,
    DELAY_MINS,
    DELAY_SECS,
    DELAY_MILLIS
} delayUnits;

/*
 * create a task to be executed one-time after
 * imposing a time delay.  NOTE: once the 'taskCallbackFunc'
 * is called, the delayedTask handle will no long be
 * valid and should be set to 0
 *
 * @param delayAmount - number of 'units' to delay
 * @param units - amount of time each 'amount' represents
 * @param func - function to execute after the delay
 * @param arg - optional argument passed to the taskCallbackFunc
 *
 * @return a delayedTask handle (positive number) that can be queried or canceled
 */
uint32_t scheduleDelayTask(uint64_t delayAmount, delayUnits units, taskCallbackFunc func, void *arg);

/*
 * create a task to be executed at a particular time of day.
 * NOTE: once the 'taskCallbackFunc' is called, the delayedTask
 * handle will no long be valid and should be set to 0
 *
 * @param hour - hour of day to wait until      (0-23)
 * @param min - minute of 'hour' to wait until  (0-59)
 * @param func - function to execute after the delay
 * @param arg - optional argument passed to the taskCallbackFunc
 *
 * @return a delayedTask handle (positive number) that can be queried or canceled
 */
uint32_t scheduleTimeOfDayTask(uint8_t hour, uint8_t min, taskCallbackFunc func, void *arg);

/**
 * Reschedule an existing task so that it expires at a new time.
 *
 * @param task The task handle provided at creation.
 * @param delayAmount The amount of time in provided units to delay.
 * @param units The unit type.
 * @return True if the task was successfully rescheduled.
 */
bool rescheduleDelayTask(uint32_t task, uint64_t delayAmount, delayUnits units);

/*
 * check to see if the task is still waiting for the delay to expire
 */
bool isDelayTaskWaiting(uint32_t task);

/*
 * cancel the task.  only has an effect if the task is waiting
 * for the delay to expire.  returns the original 'arg' supplied
 * during the creation - allowing cleanup to occur.
 */
void *cancelDelayTask(uint32_t task);

/*
 * force the execution of the task to occur now; invoking the
 * taskCallbackFunc.  only has an effect if the task is still
 * actively waiting for the delay to expire.
 *
 * @return true if the task was executed
 */
bool exceuteDelayTask(uint32_t task);

#if defined(CONFIG_DEBUG_SINGLE_PROCESS) || defined(CONFIG_PRODUCT_ANGELSENVY)
/*
 * Force and wait for all delayed tasks to complete
 */
void finalizeAllDelayTasks();
#endif // CONFIG_DEBUG_SINGLE_PROCESS || CONFIG_PRODUCT_ANGELSENVY


#endif // IC_DELAYTASK_H
