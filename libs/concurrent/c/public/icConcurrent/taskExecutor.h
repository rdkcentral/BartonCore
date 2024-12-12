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
 * executorTask.h
 *
 * fifo queue of tasks to execute serially.  can be
 * thought of as a "thread pool of one" for queueing
 * tasks to be executed in the order they are inserted.
 *
 * Author: jelderton -  11/13/18.
 *-----------------------------------------------*/

#ifndef ZILKER_TASKEXECUTOR_H
#define ZILKER_TASKEXECUTOR_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// opaque definition of our construct
typedef struct _icTaskExecutor icTaskExecutor;

// function to "execute" the task (see appendTaskToExecutor)
typedef void (*taskExecRunFunc)(void *taskObj, void *taskArg);

// function to "free" the task
typedef void (*taskExecFreeFunc)(void *taskObj, void *taskArg);

/*
 * creates a new task executor
 */
icTaskExecutor *createTaskExecutor(void);

/*
 * creates a new task executor that can only keep maxQueueSize jobs in its backlog
 */
icTaskExecutor *createBoundedTaskExecutor(uint16_t maxQueueSize);

/*
 * clears and destroys the task executor
 */
void destroyTaskExecutor(icTaskExecutor *executor);

/*
 * waits for all queued tasks to complete, then destroys the task executor
 */
void drainAndDestroyTaskExecutor(icTaskExecutor *executor);

/*
 * clears and destroys the queued tasks
 */
void clearTaskExecutor(icTaskExecutor *executor);

/*
 * adds a new task to the execution queue.  requires
 * the task object and a function to perform the processing.
 * once complete, the 'freeFunc' function is called to perform cleanup
 * on the taskObj and optional taskArg
 *
 * @param executor - the icTaskExecutor to queue/run this
 * @param taskObj - the task object (data to use for execution)
 * @param taskArg - optional argument passed to the exec/free function
 * @param runFunc - function to perform the task execution
 * @param freeFunc - function to call after runFunc to perform cleanup
 */
bool appendTaskToExecutor(icTaskExecutor *executor,
                          void *taskObj,
                          void *taskArg,
                          taskExecRunFunc runFunc,
                          taskExecFreeFunc freeFunc);

/*
 * returns the number of items in the backlog to execute
 */
uint16_t getTaskExecutorQueueCount(icTaskExecutor *executor);

#endif // ZILKER_TASKEXECUTOR_H
