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


#include <inttypes.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <icConcurrent/delayedTask.h>
#include <icConcurrent/threadUtils.h>
#include <icConcurrent/timedWait.h>
#include <icTime/timeUtils.h>
#include <icTypes/icLinkedList.h>
#include <icUtil/stringUtils.h>

/*
 * private data structures
 */

typedef enum
{
    DELAY_TASK_IDLE,     // prior to 'wait'
    DELAY_TASK_WAITING,  // waiting for time to expire
    DELAY_TASK_RUNNING,  // executing callback function
    DELAY_TASK_CANCELED, // signal a 'stop waiting', returns to IDLE
} taskState;

/*
 * represents a DelayedTask object
 */
typedef struct _delayedTask
{
    uint32_t handle;           // identifier returned to callers
    taskCallbackFunc callback; // function to execute after the delay
    taskState state;           // current state
    uint64_t delay;
    delayUnits units;
    void *arg;                 // optional arg supplied by caller
    pthread_mutex_t mutex;     // mutex for the object
    pthread_cond_t cond;       // condition used for canceling
    pthread_t tid;             // thread id of the tasks' thread
    struct timespec startTime; // when timer started
    bool joinable;             // true when run thread should detach or otherwise safe to pthread_join
} delayedTask;

typedef enum
{
    TASKS_STATE_AVAILABLE,
    TASKS_STATE_FINALIZING
} tasksState;

static icLinkedList *tasks = NULL; // list of delayedTask objects
static pthread_mutex_t TASK_MTX = PTHREAD_MUTEX_INITIALIZER;

#if defined(CONFIG_DEBUG_SINGLE_PROCESS)
// Keep a state tracking if we are told to finalize tasks. This is so we can short-circuit
// scheduleDelayedTask calls when we are trying to finalize the existing ones. This prevents
// deadlock scenarios around the TASK_MTX;
static pthread_mutex_t STATE_MTX = PTHREAD_MUTEX_INITIALIZER;
static tasksState state = TASKS_STATE_AVAILABLE;
#endif // CONFIG_DEBUG_SINGLE_PROCESS

/*
 * private internal functions
 */
static void *runDelayTaskThread(void *arg);
static void listDeleteDelayTask(void *item);
static void releaseDelayTask(delayedTask *task);
static bool findDelayedTaskByHandle(void *searchVal, void *item);
static uint32_t assignHandle();
static bool finalizeTaskInternal(delayedTask *obj);

/**
 * Get the delayed task object. This call
 * is atomic.
 *
 * @param task The task handle to look up.
 * @return A pointer to the delayed task object, or NULL if
 *         no object could be found.
 */
static delayedTask *getDelayedTask(uint32_t task)
{
    delayedTask *ret;

    // find the task for this handle
    //
    pthread_mutex_lock(&TASK_MTX);
    ret = (delayedTask *) linkedListFind(tasks, &task, findDelayedTaskByHandle);
    pthread_mutex_unlock(&TASK_MTX);

    return ret;
}

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
uint32_t scheduleDelayTask(uint64_t delayAmount, delayUnits units, taskCallbackFunc func, void *arg)
{
    // create and populate a task definition
    //
    delayedTask *def = (delayedTask *) calloc(1, sizeof(delayedTask));
    if (def == NULL)
    {
        return 0;
    }

    pthread_mutex_init(&def->mutex, NULL);
    initTimedWaitCond(&def->cond);

    def->callback = func;
    def->state = DELAY_TASK_IDLE;
    def->delay = delayAmount;
    def->units = units;
    def->arg = arg;
    def->joinable = true;

#if defined(CONFIG_DEBUG_SINGLE_PROCESS)
    // Verify our state to make sure we can actually queue up the task.
    pthread_mutex_lock(&STATE_MTX);

    if (state == TASKS_STATE_FINALIZING)
    {
        releaseDelayTask(def);
        pthread_mutex_unlock(&STATE_MTX);
        return 0;
    }

    // Obtain TASK_MTX while holding STATE_MTX to ensure no races with finalize.
    pthread_mutex_lock(&TASK_MTX);
    pthread_mutex_unlock(&STATE_MTX);
#else
    pthread_mutex_lock(&TASK_MTX);
#endif // CONFIG_DEBUG_SINGLE_PROCESS

    // assign a handle and add to our list
    //
    if (tasks == NULL)
    {
        tasks = linkedListCreate();
    }
    uint32_t retVal = assignHandle();
    def->handle = retVal;
    linkedListAppend(tasks, def);
    pthread_mutex_unlock(&TASK_MTX);

    // TODO: Wbb: this seems dangerous as we allow for the arbitrary creation of threads in the system. We should
    // consider moving this to a single thread that sorts a queue by expiration time. create and start the thread
    //
    char *threadName = stringBuilder("delayedTask:%" PRIu32, retVal);

    getCurrentTime(&(def->startTime), true);
    createThread(&def->tid, runDelayTaskThread, def, threadName);
    free(threadName);

    return retVal;
}

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
uint32_t scheduleTimeOfDayTask(uint8_t hour, uint8_t min, taskCallbackFunc func, void *arg)
{
    // get current wall-clock time so we can see how long between
    // now and the next occurrence of hour:min
    //
    time_t now = time(NULL);

    // starting with 'now', apply the supplied hour/min
    //
    struct tm future;
    localtime_r(&now, &future);
    future.tm_hour = hour;
    future.tm_min = min;
    time_t maybe = mktime(&future);

    // see if we need to increase 'day'
    // (given a fudge factor of 1 minute)
    //
    double diffSecs = difftime(maybe, now);
    if (diffSecs <= 60)
    {
        // use tomorrow's hour:min, not todays
        //
        future.tm_mday++;
        maybe = mktime(&future);
        diffSecs = difftime(maybe, now);
    }

    // call 'func' after the delay
    //
    return scheduleDelayTask((uint32_t) diffSecs, DELAY_SECS, func, arg);
}

bool rescheduleDelayTask(uint32_t task, uint64_t delayAmount, delayUnits units)
{
    bool ret = false;

    delayedTask *obj = getDelayedTask(task);

    if (obj != NULL)
    {
        pthread_mutex_lock(&obj->mutex);
        if ((obj->state == DELAY_TASK_IDLE) || (obj->state == DELAY_TASK_WAITING))
        {
            obj->delay = delayAmount;
            obj->units = units;

            getCurrentTime(&(obj->startTime), true);

            /*
             * Wake up the thread so that it can compute the new time.
             */
            pthread_cond_broadcast(&obj->cond);

            ret = true;
        }
        pthread_mutex_unlock(&obj->mutex);
    }

    return ret;
}
/*
 * check to see if the task is still waiting for the delay to expire
 */
extern bool isDelayTaskWaiting(uint32_t task)
{
    // find the task for this handle
    //
    delayedTask *obj = getDelayedTask(task);
    if (obj == NULL)
    {
        // task is not in the list anymore.  possible that
        // it's currently executing or already canceled.
        //
        return false;
    }

    // look at the state to see if 'WAITING'
    //
    bool retVal = false;
    pthread_mutex_lock(&obj->mutex);
    // Count IDLE too, since it just means it hasn't quite gotten to WAITING yet
    if (obj->state == DELAY_TASK_WAITING || obj->state == DELAY_TASK_IDLE)
    {
        retVal = true;
    }
    pthread_mutex_unlock(&obj->mutex);

    return retVal;
}

/*
 * cancel the task.  only has an effect if the task is waiting
 * for the delay to expire.  returns the original 'arg' supplied
 * during the creation - allowing cleanup to occur.
 */
void *cancelDelayTask(uint32_t task)
{
    // find the task for this handle
    //
    delayedTask *obj = getDelayedTask(task);
    if (obj == NULL)
    {
        // task is not in the list anymore.  possible that
        // it's currently executing or already canceled.
        //
        return NULL;
    }

    // look at the state to see if 'WAITING'
    //
    void *retVal = NULL;
    pthread_mutex_lock(&obj->mutex);
    if (obj->state == DELAY_TASK_WAITING || obj->state == DELAY_TASK_IDLE)
    {
        // set state to cancel, then pop the condition
        // to stop the 'wait'
        //
        obj->state = DELAY_TASK_CANCELED;
        pthread_cond_broadcast(&obj->cond);

        // return the 'arg' from within this task
        //
        retVal = obj->arg;
    }
    pthread_mutex_unlock(&obj->mutex);

    return retVal;
}

/*
 * force the execution of the task to occur now; invoking the
 * taskCallbackFunc.  only has an effect if the task is still
 * actively waiting for the delay to expire.
 *
 * @return true if the task was executed
 */
bool exceuteDelayTask(uint32_t task)
{
    // find the task for this handle
    //
    delayedTask *obj = getDelayedTask(task);
    if (obj == NULL)
    {
        // task is not in the list anymore.  possible that
        // it's currently executing or already canceled.
        //
        return false;
    }

    bool retVal = finalizeTaskInternal(obj);

    return retVal;
}

#if defined(CONFIG_DEBUG_SINGLE_PROCESS)

// FIXME: This api is has concurrency problems due to delayed tasks rescheduling themselves (software trouble
//  generation). It is currently only practically used to cleanly shutdown single process for CI, so I've added
//  rudimentary safeguards under the assumption this function only gets called from one thread at a time. However,
//  this function and *tasks module callbacks need to be investigated at a future date.
/*
 * Force and wait for all delayed tasks to complete
 */
void finalizeAllDelayTasks()
{
    pthread_mutex_lock(&STATE_MTX);
    state = TASKS_STATE_FINALIZING;
    pthread_mutex_unlock(&STATE_MTX);

    pthread_mutex_lock(&TASK_MTX);
    icLinkedListIterator *iter = linkedListIteratorCreate(tasks);
    while (linkedListIteratorHasNext(iter))
    {
        delayedTask *item = (delayedTask *) linkedListIteratorGetNext(iter);
        bool joinable;
        pthread_t tid;

        pthread_mutex_lock(&item->mutex);

        joinable = item->joinable;
        item->joinable = false;
        tid = item->tid;

        pthread_mutex_unlock(&item->mutex);

        finalizeTaskInternal(item);

        if (joinable == true)
        {
            pthread_join(tid, NULL);
        }
    }
    linkedListIteratorDestroy(iter);

    linkedListDestroy(tasks, listDeleteDelayTask);
    tasks = NULL;
    pthread_mutex_unlock(&TASK_MTX);

    pthread_mutex_lock(&STATE_MTX);
    state = TASKS_STATE_AVAILABLE;
    pthread_mutex_unlock(&STATE_MTX);
}

#endif // CONFIG_DEBUG_SINGLE_PROCESS

/**
 * Force a task to start running if its not already running
 * @param obj the task to finalize
 * @return true if the task was executed
 */
static bool finalizeTaskInternal(delayedTask *obj)
{
    // look at the state to see if 'WAITING'
    //
    bool retVal = false;

    pthread_mutex_lock(&obj->mutex);
    if (obj->state == DELAY_TASK_WAITING || obj->state == DELAY_TASK_IDLE)
    {
        // set state to 'running' and message the conditional
        // to expedite the execution (as if the timer expired)
        // NOTE: the task will be deleted after execution
        //
        obj->state = DELAY_TASK_RUNNING;

        pthread_cond_broadcast(&obj->cond);
        retVal = true;
    }
    pthread_mutex_unlock(&obj->mutex);

    return retVal;
}

static void computeWaitTime(delayedTask *def, struct timespec *waitTime)
{
    if (def->units == DELAY_HOURS)
    {
        waitTime->tv_sec = 60 * 60 * def->delay;
        waitTime->tv_nsec = 0;
    }
    else if (def->units == DELAY_MINS)
    {
        waitTime->tv_sec = 60 * def->delay;
        waitTime->tv_nsec = 0;
    }
    else if (def->units == DELAY_SECS)
    {
        waitTime->tv_sec = def->delay;
        waitTime->tv_nsec = 0;
    }
    else
    {
        // Convert from millis
        waitTime->tv_sec = def->delay / 1000;
        waitTime->tv_nsec = (def->delay % 1000) * 1000000;
    }

    struct timespec now;
    getCurrentTime(&now, true);
    // See how much time has elapsed since we our start time
    struct timespec elapsed;
    // elapsed = now - targetRunTime
    timespecDiff(&now, &(def->startTime), &elapsed);
    // waitTime = waitTime - elapsed;
    timespecDiff(waitTime, &elapsed, waitTime);
}

/*
 *
 */
static void *runDelayTaskThread(void *arg)
{
    // get the taskDef parameters
    //
    delayedTask *def = (delayedTask *) arg;
    uint32_t id = def->handle;
    bool detachSelf = false;

    pthread_mutex_lock(&def->mutex);
    // Check to see if we got canceled before we started
    if (def->state == DELAY_TASK_CANCELED)
    {
        // already got canceled, just bail
        //
        detachSelf = def->joinable;
        if (detachSelf == true)
        {
            pthread_detach(def->tid);
            def->joinable = false;
        }
        pthread_mutex_unlock(&def->mutex);
        goto cleanup;
    }

    // set initial state
    //
    def->state = DELAY_TASK_WAITING;

    // figure out what our overall delay is
    //
    struct timespec waitTime;
    computeWaitTime(def, &waitTime);

    // Keep waiting until after delay time, this handles spurious wake ups
    while (waitTime.tv_sec > 0 || (waitTime.tv_sec == 0 && waitTime.tv_nsec > 0))
    {
        granularIncrementalCondTimedWait(&def->cond, &def->mutex, &waitTime);
        if (def->state != DELAY_TASK_WAITING)
        {
            break;
        }
        computeWaitTime(def, &waitTime);
    }

    // Only execute if we didn't get canceled
    if (def->state != DELAY_TASK_CANCELED)
    {
        // Update to say we are running
        def->state = DELAY_TASK_RUNNING;
        // Unlock while we run to avoid deadlocks in case they call back in to cancel this task while running
        pthread_mutex_unlock(&def->mutex);

        // forced to execute.  empty out the arg as the
        // callback should have released it.
        //
        def->callback(def->arg);
        def->arg = NULL;

        // Relock
        pthread_mutex_lock(&def->mutex);

        // done running
        //
        def->state = DELAY_TASK_CANCELED;
    }

    detachSelf = def->joinable;
    if (detachSelf == true)
    {
        pthread_detach(def->tid);
        def->joinable = false;
    }
    pthread_mutex_unlock(&def->mutex);

// now that we're done, delete this task from the list
// and free it.  we assume the callback already freed
// the 'arg' within the task (due to execute or cancel)
// if the list is empty, release it (could be we're shutting down)
//
cleanup:
    if (detachSelf == true)
    {
        pthread_mutex_lock(&TASK_MTX);
        linkedListDelete(tasks, &id, findDelayedTaskByHandle, listDeleteDelayTask);
        if (linkedListCount(tasks) == 0)
        {
            linkedListDestroy(tasks, standardDoNotFreeFunc);
            tasks = NULL;
        }
        pthread_mutex_unlock(&TASK_MTX);
    }

    pthread_exit(NULL);
}

/*
 * 'linkedListItemFreeFunc' implementation
 */
static void listDeleteDelayTask(void *item)
{
    if (item != NULL)
    {
        releaseDelayTask((delayedTask *) item);
    }
}

/*
 * release the task
 */
static void releaseDelayTask(delayedTask *task)
{
    if (task != NULL)
    {
        // destroy mutex and the container.
        // NOTE: not doing anything with the task's "arg"
        //
        pthread_mutex_destroy(&task->mutex);
        pthread_cond_destroy(&task->cond);
        free(task);
    }
}

/*
 * 'linkedListCompareFunc' to find a delayTask object from our 'tasks' list,
 * using the 'handle' as the search criteria.
 */
static bool findDelayedTaskByHandle(void *searchVal, void *item)
{
    uint32_t *id = (uint32_t *) searchVal;
    delayedTask *task = (delayedTask *) item;

    // comparet he handle values
    //
    if (*id == task->handle)
    {
        return true;
    }
    return false;
}

/*
 * internal call, assumes LIST_MTX is held.
 * this will return positive numbers (greater then 0)
 */
static uint32_t assignHandle()
{
    // if our list is empty, just assign '1'
    //
    int count = linkedListCount(tasks);
    if (count == 0)
    {
        return 1;
    }

    // look at the last item in the list, then
    // add 1 to it
    //
    delayedTask *last = (delayedTask *) linkedListGetElementAt(tasks, (uint32_t) (count - 1));
    return (last->handle + 1);
}
