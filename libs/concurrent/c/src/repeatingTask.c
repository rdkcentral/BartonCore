//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2018 Comcast Cable
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
// Comcast Cable retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*-----------------------------------------------
 * repeatingTask.c
 *
 * Creates a repeating task that will loop until told to cancel.
 * Each iteration of the loop can pause for a specified amount of
 * time before executing again.  Helpful for things such as "monitor threads"
 * that need to execute the same operation over and over again, with an
 * optional delay between executions.
 *
 * NOTE: uses the concept of 'handles' vs 'objects' due to
 * the nature of tasks executing and releasing in the background.
 * this prevents memory issues with the caller having a pointer
 * to an object that may have been released in the background.
 *
 * Author: jelderton - 10/8/18
 *-----------------------------------------------*/

#include <glib.h>
#include <inttypes.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repeatingTaskPolicyPrivate.h"
#include <icConcurrent/repeatingTask.h>
#include <icConcurrent/threadUtils.h>
#include <icConcurrent/timedWait.h>
#include <icLog/logging.h>
#include <icTime/timeTracker.h>
#include <icTime/timeUtils.h>
#include <icTypes/icLinkedList.h>
#include <icUtil/stringUtils.h>

#define LOG_TAG                 "repeatTask"

//-----------------------------------------------
// private data structures
//-----------------------------------------------

#define TASK_NO_HINT            0
#define TASK_SHORT_CIRCUIT_HINT 1
#define TASK_RESET_HINT         2

/*
 * The state of a task
 */
typedef enum
{
    REPEAT_TASK_IDLE,     // prior to 'wait'
    REPEAT_TASK_WAITING,  // waiting for time to expire
    REPEAT_TASK_RUNNING,  // executing callback function
    REPEAT_TASK_CANCELED, // signal a 'stop waiting', returns to IDLE
} repeatTaskState;

/*
 * The different types of tasks
 */
typedef enum
{
    NORMAL_REPEATING_TASK_TYPE,     // deprecated basic repeating task
    INIT_DELAY_REPEATING_TASK_TYPE, // deprecated basic repeating task with initial delay
    FIXED_RATE_REPEATING_TASK_TYPE, // deprecated fixed-rate repeating task
    BACK_OFF_REPEATING_TASK_TYPE,   // deprecated backoff repeating task
    POLICY_REPEATING_TASK_TYPE      // anything created via createPolicyRepeatingTask()
} repeatingTaskType;

/*
 * represents a repeatTask object
 */
typedef struct
{
    pthread_mutex_t mutex; // mutex for the object
    pthread_cond_t cond;   // condition used for canceling
    pthread_t tid;         // thread id of the tasks' thread
    bool joinable;

    uint32_t handle;            // identifier returned to callers
    repeatTaskState state;      // current state
    uint8_t actionFlag;         // operational action without changing state
    repeatingTaskType taskType; // needed for traditional task support

    RepeatingTaskPolicy *policy;                 // object to calculate the next execution time
    RepeatingTaskPolicy *resetPolicy;            // only used during resetPolicyRepeatingTask
    repeatingTaskRunFunc runFunc;                // function to run at execution time
    destroyRepeatingTaskObjectsFunc cleanupFunc; // cleanup policy and context
    void *userArg;                               // optional arg supplied by caller

} RepeatTask;

/*
 * private variables
 */
static icLinkedList *tasks = NULL; // list of repeatTask objects
static pthread_mutex_t TASK_MTX = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
static uint64_t handleCounter = 0;
static pthread_once_t initOnce = PTHREAD_ONCE_INIT;

/*
 * private internal functions
 */
static uint32_t createPolicyRepeatingTaskInternal(repeatingTaskRunFunc runFunc,
                                                  destroyRepeatingTaskObjectsFunc cleanupFunc,
                                                  RepeatingTaskPolicy *policy,
                                                  void *userArg,
                                                  repeatingTaskType type);
static void defaultDestroyRepeatingTaskObjectsFunc(uint32_t taskHandle, void *userArg, RepeatingTaskPolicy *policy);
static RepeatTask *incrementRepeatTaskRef(RepeatTask *task);
static void decrementRepeatTaskRef(RepeatTask *task);
static RepeatTask *findTaskInListLocked(uint32_t taskHandle);
static RepeatTask *removeTaskFromList(uint32_t taskHandle);
static bool traditionalTaskRunWrapper(void *arg);
static void traditionalTaskClean(uint32_t taskHandle, void *userArg, RepeatingTaskPolicy *policy);
static bool traditionalBackoffTaskRunWrapper(void *arg);
static void traditionalBackoffTaskClean(uint32_t taskHandle, void *userArg, RepeatingTaskPolicy *policy);
static uint32_t assignHandleLocked(void);
static bool cancelRepeatingTaskObject(RepeatTask *obj);
static void shutdownRepeatingTasks(void);
static void *runRepeatTaskThread(void *arg);

/*
 * one-time init for cleanup
 */
static void oneTimeInit(void)
{
    // register to cleanup tasks on exit
    icLogDebug(LOG_TAG, "registering shutdown hook");
    atexit(shutdownRepeatingTasks);
}


//-----------------------------------------------
//
// Traditional Repeating Tasks
//
//-----------------------------------------------

//
// object used internally for traditional "create repeating task" implementations
//
typedef struct
{
    taskCallbackFunc callbackFunc;
    void *userArg;
} TraditionalRepeatTaskCtx;

/*
 * create a traditional repeating task that will invoke 'taskCallbackFunc'
 * (passing 'arg'); then pause for 'delayAmount' before executing
 * again.  this pattern will continue until the task is canceled.
 *
 * NOTE: This is an older function definition that can be deprecated.
 *       Consider using "policy based" repeating tasks below.
 *
 * @param delayAmount - number of 'units' to delay
 * @param units - amount of time each 'amount' represents
 * @param func - function to execute at each iteration
 * @param userArg - optional argument passed to the taskCallbackFunc
 *
 * @return a repeatingTask handle that can be queried or canceled
 */
uint32_t createRepeatingTask(uint64_t delayAmount, delayUnits units, taskCallbackFunc func, void *userArg)
{
    // we kept this function around to reduce the amount of change necessary
    // when introducing the "policy" approach of repeating tasks.

    // taskCallbackFunc has a different signature than repeatingTaskRunFunc, so need
    // a proxy/wrapper of sorts.  place this info into a container, then use the newer
    // policy-based approach for the actual task.
    TraditionalRepeatTaskCtx *ctx = (TraditionalRepeatTaskCtx *) calloc(1, sizeof(TraditionalRepeatTaskCtx));
    ctx->callbackFunc = func;
    ctx->userArg = userArg;

    // plugin into the newer policy approach
    RepeatingTaskPolicy *policy = createStandardRepeatingTaskPolicy(delayAmount, units);
    return createPolicyRepeatingTaskInternal(
        traditionalTaskRunWrapper, traditionalTaskClean, policy, ctx, NORMAL_REPEATING_TASK_TYPE);
}


/*
 * create a repeating task that will invoke 'taskCallbackFunc'
 * (passing 'arg'); after startDelay then pause for 'delayAmount' before executing
 * again.  this pattern will continue until the task is canceled.
 *
 * NOTE: This is an older function definition that can be deprecated.
 *       Consider using "policy based" repeating tasks below.
 *
 * @param startDelay - the number of `units` to delay for the initial run
 * @param delayAmount - number of 'units' to delay
 * @param units - amount of time each 'amount' represents
 * @param func - function to execute at each iteration
 * @param userArg - optional argument passed to the taskCallbackFunc
 *
 * @return a repeatingTask handle that can be queried or canceled
 */
uint32_t createRepeatingTaskWithStartDelay(uint64_t startDelay,
                                           uint64_t delayAmount,
                                           delayUnits units,
                                           taskCallbackFunc func,
                                           void *userArg)
{
    // we kept this function around to reduce the amount of change necessary
    // when introducing the "policy" approach of repeating tasks.

    // taskCallbackFunc has a different signature than repeatingTaskRunFunc, so need
    // a proxy/wrapper of sorts.  place this info into a container, then use the newer
    // policy-based approach for the actual task.
    TraditionalRepeatTaskCtx *ctx = (TraditionalRepeatTaskCtx *) calloc(1, sizeof(TraditionalRepeatTaskCtx));
    ctx->callbackFunc = func;
    ctx->userArg = userArg;

    // plugin into the newer policy approach
    RepeatingTaskPolicy *policy = createRepeatingTaskWithStartDelayPolicy(startDelay, delayAmount, units);
    return createPolicyRepeatingTaskInternal(
        traditionalTaskRunWrapper, traditionalTaskClean, policy, ctx, INIT_DELAY_REPEATING_TASK_TYPE);
}

/*
 * create a repeating task that will invoke 'taskCallbackFunc'
 * (passing 'arg'); then run it again after the given delay relative
 * to when the task initially started.  Then again after 2 * delay, etc.
 *
 * NOTE: This is an older function definition that can be deprecated.
 *       Consider using "policy based" repeating tasks below.
 *
 * @param delayAmount - number of 'units' to delay
 * @param units - amount of time each 'amount' represents
 * @param func - function to execute at each iteration
 * @param userArg - optional argument passed to the taskCallbackFunc
 *
 * @return a repeatingTask handle that can be queried or canceled
 */
uint32_t createFixedRateRepeatingTask(uint64_t delayAmount, delayUnits units, taskCallbackFunc func, void *userArg)
{
    // we kept this function around to reduce the amount of change necessary
    // when introducing the "policy" approach of repeating tasks.

    // taskCallbackFunc has a different signature than repeatingTaskRunFunc, so need
    // a proxy/wrapper of sorts.  place this info into a container, then use the newer
    // policy-based approach for the actual task.
    TraditionalRepeatTaskCtx *ctx = (TraditionalRepeatTaskCtx *) calloc(1, sizeof(TraditionalRepeatTaskCtx));
    ctx->callbackFunc = func;
    ctx->userArg = userArg;

    // plugin into the newer policy approach
    RepeatingTaskPolicy *policy = createFixedRepeatingTaskPolicy(delayAmount, units);
    return createPolicyRepeatingTaskInternal(
        traditionalTaskRunWrapper, traditionalTaskClean, policy, ctx, FIXED_RATE_REPEATING_TASK_TYPE);
}

//
// object used internally for traditional "create repeating task" implementations
//
typedef struct
{
    backOffTaskRunCallbackFunc callbackFunc;
    backOffTaskSuccessCallbackFunc cleanFunc;
    void *userArg;
} TraditionalBackoffTaskCtx;

/*
 * create a repeating task that will invoke 'backOffTaskRunCallbackFunc'
 * (passing 'arg'); will wait 'initDelayAmount', then run again and again.
 * Every iteration will then increase the delay amount by 'delayInterval'
 * each time until 'maxDelayAmount' is reached, then stay at that delay amount.
 *
 * If the callback 'backOffTaskRunCallbackFunc' returns true then the backOffTask
 * will finish and invoke 'backOffTaskSuccessCallbackFunc' to notify of success.
 *
 * If the back off task is cancelled at any time it will be handled just like any other
 * repeating tasks.
 *
 * NOTE: This is an older function definition that can be deprecated.
 *       Consider using "policy based" repeating tasks below.
 *
 * @param initDelayAmount - the delay amount after the 1st pass
 * @param maxDelayAmount - the max delay amount to be reached
 * @param incrementDelayAmount - the delay amount to be increased by each iteration
 * @param units - amount of time each 'amount' represents
 * @param runFunc - function to execute at each iteration
 * @param successFunc - function to execute when backOffTaskRunCallbackFunc gets a 'true' response
 * @param userArg - optional argument passed to the backOffTaskRunCallbackFunc
 *
 * @return a repeatingTask handle that can be queried or canceled
 */
uint32_t createBackOffRepeatingTask(uint64_t initDelayAmount,
                                    uint64_t maxDelayAmount,
                                    uint64_t incrementDelayAmount,
                                    delayUnits units,
                                    backOffTaskRunCallbackFunc runFunc,
                                    backOffTaskSuccessCallbackFunc cleanFunc,
                                    void *userArg)
{
    // we kept this function around to reduce the amount of change necessary
    // when introducing the "policy" approach of repeating tasks.

    // the two callback functions have a slightly different signature than the policy-based obes, so need
    // a proxy/wrapper.  place these functions and userArg into a container, then use the newer
    // policy-based approach for the actual task.
    TraditionalBackoffTaskCtx *ctx = (TraditionalBackoffTaskCtx *) calloc(1, sizeof(TraditionalBackoffTaskCtx));
    ctx->callbackFunc = runFunc;
    ctx->cleanFunc = cleanFunc;
    ctx->userArg = userArg;

    // plugin into the newer policy approach
    RepeatingTaskPolicy *policy =
        createIncrementalRepeatingTaskPolicy(initDelayAmount, maxDelayAmount, incrementDelayAmount, units);
    return createPolicyRepeatingTaskInternal(
        traditionalBackoffTaskRunWrapper, traditionalBackoffTaskClean, policy, ctx, BACK_OFF_REPEATING_TASK_TYPE);
}

/*
 * change the delayAmount for a traditional repeating or fixed task.  if the 'changeNow' flag
 * is true, then it will reset the current pause time and start again.  otherwise
 * this will not apply until the next pause.
 */
void changeRepeatingTask(uint32_t taskHandle, uint32_t delayAmount, delayUnits units, bool changeNow)
{
    // we kept this function around to reduce the amount of change necessary
    // when introducing the "policy" approach of repeating tasks.

    // we first need to locate the task with this handle, then examine the 'type'
    // so we know which kind of policy to recreate (standard or fixed)
    RepeatingTaskPolicy *policy = NULL;
    mutexLock(&TASK_MTX);
    RepeatTask *obj = findTaskInListLocked(taskHandle); // increments ref count
    if (obj != NULL)
    {
        switch (obj->taskType)
        {
            case NORMAL_REPEATING_TASK_TYPE:
            case INIT_DELAY_REPEATING_TASK_TYPE: // can't alter initial delay
                policy = createStandardRepeatingTaskPolicy(delayAmount, units);
                break;

            case FIXED_RATE_REPEATING_TASK_TYPE:
                policy = createFixedRepeatingTaskPolicy(delayAmount, units);
                break;

            case BACK_OFF_REPEATING_TASK_TYPE:
                policy = cloneAndChangeIncrementalRepeatingTaskPolicy(obj->policy, delayAmount, units);
                break;

            case POLICY_REPEATING_TASK_TYPE:
            default:
                // not supported via this function
                icLogError(LOG_TAG, "developer error!  %s not supported for policy-based tasks", __func__);
                break;
        }
    }
    decrementRepeatTaskRef(obj); // dec reference count due to findTaskInListLocked
    mutexUnlock(&TASK_MTX);

    // finally, apply the new policy
    if (policy != NULL)
    {
        resetPolicyRepeatingTask(taskHandle, policy, changeNow);
    }
}

/*
 * cancel the traditional repeating task. returns the original 'arg' supplied
 * during the creation - allowing cleanup to safely occur.
 * WARNING:
 * this should NOT be called while holding a lock that the task
 * function can also hold or a deadlock might result
 */
void *cancelRepeatingTask(uint32_t taskHandle)
{
    void *retVal = NULL;

    // find the task for this handle, but only extract the userArg
    // if this was created as a traditional task.  otherwise we could
    // be altering memory incorrectly.
    RepeatTask *obj = removeTaskFromList(taskHandle);
    if (obj != NULL)
    {
        // only grab 'userArg' if this is traditional
        if (obj->taskType != POLICY_REPEATING_TASK_TYPE)
        {
            if (obj->taskType == BACK_OFF_REPEATING_TASK_TYPE)
            {
                // backoff had different context
                TraditionalBackoffTaskCtx *ctx = (TraditionalBackoffTaskCtx *) obj->userArg;
                if (ctx != NULL)
                {
                    retVal = ctx->userArg;
                }
            }
            else
            {
                // one of the 'traditional' tasks, so extract original userArg
                TraditionalRepeatTaskCtx *ctx = (TraditionalRepeatTaskCtx *) obj->userArg;
                if (ctx != NULL)
                {
                    retVal = ctx->userArg;
                }
            }
        }

        // At this point, we were first to claim the list ref. Attempt to cancel and then decrement.
        cancelRepeatingTaskObject(obj);
        decrementRepeatTaskRef(obj);
    }

    // return original userArg for cleanup
    return retVal;
}


//-----------------------------------------------
//
// 'Policy Based' Repeating Tasks
//
//-----------------------------------------------

/*
 * create new RepeatTask object
 */
static RepeatTask *createRepeatTaskObj(void)
{
    // want to leverage glib ArcBox (atomically reference counted box)
    return (RepeatTask *) g_atomic_rc_box_new0(RepeatTask);
}

/*
 * called by g_atomic_rc_box_release_full
 * DO NOT USE DIRECTLY
 */
static void _clearFullRepeatTaskObj(RepeatTask *task)
{
    icLogTrace(LOG_TAG, "destroying task; handle=%" PRIu32, task->handle);

    // destroy mutex and the conditional.
    pthread_mutex_destroy(&task->mutex);
    pthread_cond_destroy(&task->cond);

    // call user supplied cleanup function to destroy userArg & policy
    task->cleanupFunc(task->handle, task->userArg, task->policy);
    task->policy = NULL;
    task->userArg = NULL;

    // NOTE: do not free 'task'.  glib does that for us
    icLogTrace(LOG_TAG, "destroyed task handle %" PRIu32, task->handle);
}

/*
 * decrement the reference count for a RepeatTask object.
 if this puts the count at 0, it will destroy the object.
 */
static void decrementRepeatTaskRef(RepeatTask *task)
{
    // decrement ref, and free if count == 0
    if (task != NULL)
    {
        icLogTrace(LOG_TAG, "task handle %" PRIu32 " decrementing curr ref-count", task->handle);
        g_atomic_rc_box_release_full(task, (GDestroyNotify) _clearFullRepeatTaskObj);
    }
}

/*
 * increment the reference count for a RepeatTask object
 */
static RepeatTask *incrementRepeatTaskRef(RepeatTask *task)
{
    // increment ref
    if (task != NULL)
    {
        icLogTrace(LOG_TAG, "task handle %" PRIu32 " incrementing curr ref-count", task->handle);
        return g_atomic_rc_box_acquire(task);
    }
    return NULL;
}


/*
 * Internal version of "create a policy-based repeating task".
 */
static uint32_t createPolicyRepeatingTaskInternal(repeatingTaskRunFunc runFunc,
                                                  destroyRepeatingTaskObjectsFunc cleanupFunc,
                                                  RepeatingTaskPolicy *policy,
                                                  void *userArg,
                                                  repeatingTaskType type)
{
    pthread_once(&initOnce, oneTimeInit);

    // sanity check
    if (cleanupFunc == NULL)
    {
        // use default cleanup as outlined in function description of createPolicyRepeatingTask()
        cleanupFunc = defaultDestroyRepeatingTaskObjectsFunc;
    }
    if (runFunc == NULL || policy == NULL)
    {
        // invoke cleanup just like we would on a "failed to create thread"
        cleanupFunc(0, userArg, policy);
        icLogWarn(LOG_TAG, "unable to create repeating task; missing 'runFunc' or 'policy'");
        return 0;
    }

    // create and populate a task definition with the supplied objects
    RepeatTask *def = createRepeatTaskObj();
    def->state = REPEAT_TASK_IDLE;
    def->runFunc = runFunc;
    def->cleanupFunc = cleanupFunc;
    def->userArg = userArg;
    def->policy = policy;
    def->taskType = type;

    // setup concurrency items
    mutexInitWithType(&def->mutex, PTHREAD_MUTEX_ERRORCHECK);
    initTimedWaitCond(&def->cond);

    // assign a handle, then add to our list
    mutexLock(&TASK_MTX);
    if (tasks == NULL)
    {
        tasks = linkedListCreate();
    }
    uint32_t retVal = assignHandleLocked();
    def->handle = retVal;

    // increase ref-count so object remains alive between now and thread execution
    incrementRepeatTaskRef(def);

    // start the thread
    scoped_generic char *threadName = stringBuilder("rptTask:%" PRIu32, retVal);
    if (createThread(&def->tid, runRepeatTaskThread, def, threadName) == true)
    {
        def->joinable = true;
        linkedListAppend(tasks, def);
    }
    else
    {
        // no luck creating the thread
        icLogError(LOG_TAG, "error creating thread, unable to create repeating task!");
        retVal = 0;
        decrementRepeatTaskRef(def); // increment above
        decrementRepeatTaskRef(def); // allocation of the obj
        def = NULL;
    }
    mutexUnlock(&TASK_MTX);

    return retVal;
}

/*
 * Create a Policy-based repeating task.  In general, this will execute
 * 'runFunc' on a periodic basis, until 'runFunc' returns 'true' or the
 * task is canceled.
 *
 * The interval that the task executes is dictated by the 'policy' object.
 * After the task has completed (or is canceled), the 'cleanupFunc'
 * will be called to cleanup resources used for this repeating task.
 *
 * @param runFunc - function to execute at each iteration
 * @param cleanupFunc - function to destroy resources associated with this task.
 *                      if this is NOT provided:
 *                        - policy will be destroyed via destroyRepeatingPolicy()
 *                        - userArg will be destroyed via free()
 * @param policy - object of functions to drive the repeating interval
 * @param userArg - optional argument passed to runFunc
 *
 * @return a taskHandle to allow for other operations against, or 0 if there was
 *         an issue creating the thread and aborted the repeating task
 */
uint32_t createPolicyRepeatingTask(repeatingTaskRunFunc runFunc,
                                   destroyRepeatingTaskObjectsFunc cleanupFunc,
                                   RepeatingTaskPolicy *policy,
                                   void *userArg)
{
    return createPolicyRepeatingTaskInternal(runFunc, cleanupFunc, policy, userArg, POLICY_REPEATING_TASK_TYPE);
}

/*
 * Replaces the current RepeatingTaskPolicy with the newly supplied object.
 * If the 'changeNow' flag is true, the thread will stop the current pause,
 * and utilize the new policy to restart the pause timer.
 * If 'changeNow' is false, the new policy will take effect on the next pause.
 *
 * @param taskHandle - handle of the repeating task to reset
 * @param newPolicy - object of functions to drive the repeating interval
 * @param changeNow - apply reset now or not
 *
 * @return if the policy change was successfully applied
 */
bool resetPolicyRepeatingTask(uint32_t taskHandle, RepeatingTaskPolicy *newPolicy, bool changeNow)
{
    // sanity check.  we don't allow null policy
    if (newPolicy == NULL)
    {
        icLogWarn(LOG_TAG, "unable to reset task; missing policy for handle %" PRIu32, taskHandle);
        return false;
    }

    // find the task with this handle
    mutexLock(&TASK_MTX);
    RepeatTask *obj = findTaskInListLocked(taskHandle);
    mutexUnlock(&TASK_MTX);

    // sanity checks before moving forward
    if (obj == NULL)
    {
        icLogWarn(LOG_TAG, "unable to reset task; missing task for handle=%" PRIu32, taskHandle);
        destroyRepeatingPolicy(newPolicy);
        return false;
    }

    mutexLock(&obj->mutex);
    if (obj->policy == newPolicy)
    {
        // nothing to do here
        decrementRepeatTaskRef(obj); // dec reference count due to findTaskInListLocked
        mutexUnlock(&obj->mutex);
        return true;
    }
    else if (obj->resetPolicy != NULL)
    {
        // we have the lock, so whatever policy was staged can just be removed
        destroyRepeatingPolicy(obj->resetPolicy);
        obj->resetPolicy = NULL;
    }

    // last prereq check - make sure task isn't being canceled
    bool retVal = false;
    if (obj->state != REPEAT_TASK_CANCELED)
    {
        // save the new policy as 'resetPolicy' and let the thread take care
        // of this when it's safe to do so (i.e. not RUNNING)
        obj->resetPolicy = newPolicy;
        if (changeNow == true)
        {
            // update state to RESET and broadcast to stop the 'pause'
            obj->actionFlag = TASK_RESET_HINT;
            pthread_cond_broadcast(&obj->cond);
        }
        retVal = true;
    }
    else
    {
        icLogWarn(LOG_TAG, "unable to reset task; state is CANCELING - handle=%" PRIu32, taskHandle);
    }
    mutexUnlock(&obj->mutex);
    decrementRepeatTaskRef(obj); // dec reference count due to findTaskInListLocked

    if (retVal == false)
    {
        // destroy the newPolicy since we were unable to update the task
        destroyRepeatingPolicy(newPolicy);
        newPolicy = NULL;
    }
    return retVal;
}

/*
 * Cancel the Policy-based repeating task, then calls the cleanupFunc
 * supplied during the creation of the task.
 *
 * WARNING: this should NOT be called while holding a lock that the task
 *          function can also hold or a deadlock might result
 *
 * @param taskHandle - handle of the repeating task to cancel
 *
 * @return if the cancel was successful or not
 */
bool cancelPolicyRepeatingTask(uint32_t taskHandle)
{
    // find the task for this handle and remove from our list
    RepeatTask *obj = removeTaskFromList(taskHandle);
    if (obj == NULL)
    {
        // task is not in the list anymore.  possible that
        // it's currently executing or already canceled.
        //
        icLogWarn(LOG_TAG, "unable to cancel task; missing object for handle %" PRIu32, taskHandle);
        return false;
    }

    // At this point, we were first to claim the list ref. Attempt to cancel and then decrement.
    bool retVal = cancelRepeatingTaskObject(obj);
    decrementRepeatTaskRef(obj);
    return retVal;
}


//-----------------------------------------------
//
// Functions for both Traditional and Policy-based
//
//-----------------------------------------------

/*
 * cancels the delay of this repeating task, forcing the task to execute immediately
 *
 * @param taskHandle - handle of the repeating task to short-circuit
 */
void shortCircuitRepeatingTask(uint32_t taskHandle)
{
    // find the task with this handle
    mutexLock(&TASK_MTX);
    RepeatTask *obj = findTaskInListLocked(taskHandle);
    mutexUnlock(&TASK_MTX);
    if (obj != NULL)
    {
        // lock this task, then broadcast to stop the 'pause'
        mutexLock(&obj->mutex);
        if (obj->state != REPEAT_TASK_CANCELED)
        {
            obj->actionFlag = TASK_SHORT_CIRCUIT_HINT;
            pthread_cond_broadcast(&obj->cond);
        }
        mutexUnlock(&obj->mutex);
    }
    decrementRepeatTaskRef(obj); // dec reference count due to findTaskInListLocked
}

/*
 * cancel using the RepeatTask object
 */
static bool cancelRepeatingTaskObject(RepeatTask *obj)
{
    bool retVal = false;

    // only need to cancel if state is not CANCELED
    mutexLock(&obj->mutex);
    icLogTrace(LOG_TAG, "canceling task handle %" PRIu32, obj->handle);
    if (obj->state != REPEAT_TASK_CANCELED)
    {
        // reset flag so the thread does not try to detach
        bool joinable = obj->joinable;
        obj->joinable = false;

        // set state to cancel, then pop the condition
        // (to address when in the 'wait' state)
        // NOTE: the thread should release the task resources
        obj->state = REPEAT_TASK_CANCELED;
        pthread_cond_broadcast(&obj->cond);

        pthread_t threadId = obj->tid;
        uint32_t handle = obj->handle;
        mutexUnlock(&obj->mutex);
        if (joinable == true)
        {
            // wait for the thread to die, so we know it's safe to return
            icLogTrace(LOG_TAG, "waiting for task handle %" PRIu32 " thread to complete...", handle);
            pthread_join(threadId, NULL);
        }
        retVal = true;
    }
    else
    {
        // already 'canceling'
        icLogTrace(LOG_TAG, "unable to cancel task; already canceling handle %" PRIu32, obj->handle);
        mutexUnlock(&obj->mutex);
    }
    return retVal;
}

/*
 * shutdown all repeating tasks
 */
static void shutdownRepeatingTasks(void)
{
    icLogTrace(LOG_TAG, "shutting down repeating tasks...");
    icLogInfo(LOG_TAG, "shutting down repeating tasks...");

    // create a shallow-clone of 'tasks' to ensure we are not
    // holding the mutex during the actual cancel
    mutexLock(&TASK_MTX);
    icLinkedList *copy = linkedListClone(tasks);
    linkedListDestroy(tasks, standardDoNotFreeFunc);
    tasks = NULL;
    mutexUnlock(&TASK_MTX);

    // cancel each repeating task
    icLinkedListIterator *loop = linkedListIteratorCreate(copy);
    while (linkedListIteratorHasNext(loop) == true)
    {
        RepeatTask *curr = (RepeatTask *) linkedListIteratorGetNext(loop);
        cancelRepeatingTaskObject(curr);
        decrementRepeatTaskRef(curr);
    }
    linkedListIteratorDestroy(loop);
    linkedListDestroy(copy, standardDoNotFreeFunc);
    copy = NULL;
}

//-----------------------------------------------
//
// private functions
//
//-----------------------------------------------

/*
 * repeatingTaskRunFunc implementation to support a
 * traditional repeating task, which calls a taskCallbackFunc
 */
static bool traditionalTaskRunWrapper(void *arg)
{
    // the traditional function and userArg are within our container
    TraditionalRepeatTaskCtx *ctx = (TraditionalRepeatTaskCtx *) arg;
    ctx->callbackFunc(ctx->userArg);

    // keep this going until told to cancel
    return false;
}

/*
 * destroyRepeatingTaskObjectsFunc implementation to support
 * a traditional repeating task
 */
static void traditionalTaskClean(uint32_t taskHandle, void *userArg, RepeatingTaskPolicy *policy)
{
    // NOTE: This is a traditional repeating task, so the destruction of the
    //       original userArg is handled by our owner via cancelRepeatingTask;
    //       therefore, no cleanup of that here.
    TraditionalRepeatTaskCtx *ctx = (TraditionalRepeatTaskCtx *) userArg;
    ctx->userArg = NULL;
    free(ctx);

    // now the policy and it's context
    destroyRepeatingPolicy(policy);
}

/*
 * repeatingTaskRunFunc implementation to support a traditional
 * backoff task, which calls a backOffTaskRunCallbackFunc
 */
static bool traditionalBackoffTaskRunWrapper(void *arg)
{
    // extract our container for this traditional backoff task
    TraditionalBackoffTaskCtx *ctx = (TraditionalBackoffTaskCtx *) arg;

    // exec the original 'runFunc' supplied when this backoff task was created
    return ctx->callbackFunc(ctx->userArg);
}

/*
 * destroyRepeatingTaskObjectsFunc implementation to support a traditional
 * backoff repeating task, which calls a backOffTaskSuccessCallbackFunc
 */
static void traditionalBackoffTaskClean(uint32_t taskHandle, void *userArg, RepeatingTaskPolicy *policy)
{
    // extract our container for this traditional backoff task
    TraditionalBackoffTaskCtx *ctx = (TraditionalBackoffTaskCtx *) userArg;

    // exec the original 'cleanFunc' supplied when this backoff task was created
    ctx->cleanFunc(ctx->userArg);
    ctx->userArg = NULL;
    free(ctx);

    // now the policy and it's context
    destroyRepeatingPolicy(policy);
}

/*
 * Asks 'policy' for the millis to pause.  That is then saved into 'timeToPause', then added to "now"
 * and saved into 'whenToRun'
 */
static void
getPauseTimeFromPolicy(RepeatingTaskPolicy *policy, struct timespec *timeToPause, struct timespec *whenToRun)
{
    // ask policy how long to pause before the next execution
    policy->getDelayFunc(policy->policyContext, timeToPause);

    // add timeToPause to "now" so we have a target time of when to stop pausing
    struct timespec now;
    getCurrentTime(&now, true);
    timespecAdd(&now, timeToPause, whenToRun);
}

/*
 * calculate the amount of time to wait by subtracting pauseTime from current time
 */
static void calcRemainingTimeToPause(struct timespec *executeTime, struct timespec *pauseTime)
{
    struct timespec now;
    getCurrentTime(&now, true);
    // remaining = targetRunTime - now
    timespecDiff(executeTime, &now, pauseTime);
}

/*
 * called by runRepeatTaskThread to switch out the policy while locked
 */
static void updatePolicyLocked(RepeatTask *task)
{
    icLogTrace(LOG_TAG, "updating policy for task handle %" PRIu32, task->handle);
    RepeatingTaskPolicy *oldPolicy = task->policy;
    task->policy = task->resetPolicy;
    task->resetPolicy = NULL;
    destroyRepeatingPolicy(oldPolicy);
}

/*
 * thread to execute in
 */
static void *runRepeatTaskThread(void *arg)
{
    // get the RepeatTask object supplied when creating the thread.
    // NOTE: the createThread should have incremented the ref-count
    //
    RepeatTask *def = (RepeatTask *) arg;
    bool toldToCancel = false;

    // loop until told to stop
    //
    while (toldToCancel == false)
    {
        // check state of the task to see if told to cancel
        mutexLock(&def->mutex);
        if (def->state == REPEAT_TASK_CANCELED)
        {
            // break from loop and this thread
            toldToCancel = true;
            mutexUnlock(&def->mutex);
            continue;
        }

        // not canceled yet, so perform the RUNNING step
        repeatingTaskRunFunc runFunc = def->runFunc;
        taskPreRunNotifyFunc preFunc = def->policy->preRunNotifyFunc;
        taskPostRunNotifyFunc postFunc = def->policy->postRunNotifyFunc;
        void *policyCtx = def->policy->policyContext;
        def->state = REPEAT_TASK_RUNNING;
        icLogTrace(LOG_TAG, "running task handle %" PRIu32, def->handle);
        mutexUnlock(&def->mutex);

        // if the policy has pre/post defined, do them along with the "exec"
        if (preFunc != NULL)
        {
            preFunc(policyCtx);
        }
        toldToCancel = runFunc(def->userArg);
        if (postFunc != NULL)
        {
            // do this even if canceling the task
            postFunc(policyCtx);
        }

        // only keep going if 'runFunc' didn't tell us to stop
        if (toldToCancel == false)
        {
            mutexLock(&def->mutex);

            // see if we reset our policy
            if (def->resetPolicy != NULL)
            {
                updatePolicyLocked(def);
            }

            // process directives (cancel/short-circuit/reset) that came in during the RUNNING step
            if (def->state == REPEAT_TASK_CANCELED)
            {
                // break from loop and this thread
                toldToCancel = true;
                mutexUnlock(&def->mutex);
                continue;
            }
            else if (def->actionFlag == TASK_SHORT_CIRCUIT_HINT)
            {
                // don't enter "wait" loop and run immediately
                def->actionFlag = TASK_NO_HINT;
                mutexUnlock(&def->mutex);
                continue;
            }
            else if (def->actionFlag == TASK_RESET_HINT)
            {
                // policy changed with "now" flag, but since we haven't calculated the
                // pause timing yet, there is nothing more to do except reset the flag
                def->actionFlag = TASK_NO_HINT;
            }

            // ask policy how long to pause before the next execution, which we then
            // can create the "when to run" timestamp
            struct timespec timeToPause;
            struct timespec whenToRun;
            getPauseTimeFromPolicy(def->policy, &timeToPause, &whenToRun);

            // update state to 'waiting', then pause
            def->state = REPEAT_TASK_WAITING;

            // Handle spurious wake-ups by doing this in a loop
            while (timeToPause.tv_sec > 0 || (timeToPause.tv_sec == 0 && timeToPause.tv_nsec >= 0))
            {
                // now pause
                granularIncrementalCondTimedWait(&def->cond, &def->mutex, &timeToPause);

                // process directives (cancel/short-circuit/reset) that came in during the WAITING step
                if (def->state == REPEAT_TASK_CANCELED)
                {
                    // got cancel request while paused
                    toldToCancel = true;
                    break;
                }

                // it's possible we broke early from the timed-wait (short-circuit or reset-policy).
                // handle the updated policy before looking for those special-case scenarios
                if (def->resetPolicy != NULL)
                {
                    // update policy
                    updatePolicyLocked(def);
                }
                if (def->actionFlag == TASK_SHORT_CIRCUIT_HINT)
                {
                    // break from "wait" loop to run immediately
                    def->actionFlag = TASK_NO_HINT;
                    break;
                }
                else if (def->actionFlag == TASK_RESET_HINT)
                {
                    // policy changed with "now" flag, so need to recalculate
                    // our pause timing then loop around and pause again
                    getPauseTimeFromPolicy(def->policy, &timeToPause, &whenToRun);
                    def->actionFlag = TASK_NO_HINT;
                    continue;
                }

                // see if we need to pause for more time
                calcRemainingTimeToPause(&whenToRun, &timeToPause);

                // reset hint (reset or short-circuit)
                def->actionFlag = TASK_NO_HINT;
            }

            // reset hint (reset or short-circuit)
            def->actionFlag = TASK_NO_HINT;
            mutexUnlock(&def->mutex);
        }
    }

    // outside main loop, so canceling or done running the tasks
    icLogTrace(LOG_TAG, "task handle %" PRIu32 " complete, destroying thread", def->handle);

    // if "canceled" then it will wait for the thread to die (via join)
    mutexLock(&def->mutex);
    if (def->joinable == true)
    {
        def->joinable = false;
        pthread_detach(pthread_self());
    }
    uint32_t handle = def->handle;
    mutexUnlock(&def->mutex);

    // destroy the task, it's policy, and userArg
    icLogTrace(LOG_TAG, "destroying task handle %" PRIu32, handle);
    RepeatTask *self = removeTaskFromList(handle);
    if (self != NULL && self != def)
    {
        // this could be NULL when canceling a traditional
        icLogError(LOG_TAG,
                   "something wrong, we've crossed the handle-streams!  "
                   "looking for %" PRIu32 " but got %" PRIu32,
                   handle,
                   self->handle);
    }
    else if (self == NULL)
    {
        icLogTrace(LOG_TAG, "handle %" PRIu32 " already removed from list, exiting thread", handle);
    }

    // We own the thread ref, and we may or may not own the list ref. Scheduling of task cancellation can
    // lead to another thread calling removeTaskFromList before us. decrementRepeatTaskRef will handle the
    // NULL case gracefully (another thread removed from list before us).
    decrementRepeatTaskRef(self);
    decrementRepeatTaskRef(def); // thread-creation
    def = NULL;

    return NULL;
}

/*
 * 'linkedListCompareFunc' to find a delayTask object from our 'tasks' list,
 * using the 'handle' as the search criteria.
 */
static bool findRepeatTaskByHandle(void *searchVal, void *item)
{
    uint32_t *id = (uint32_t *) searchVal;
    RepeatTask *task = (RepeatTask *) item;

    // compare the handle values
    //
    if (*id == task->handle)
    {
        return true;
    }
    return false;
}

/*
 * find the RepeatTask object in our list and return it.
 * will increment the ref count before returning.
 *
 * assumes caller has the TASK_MTX held
 */
static RepeatTask *findTaskInListLocked(uint32_t taskHandle)
{
    RepeatTask *obj = (RepeatTask *) linkedListFind(tasks, &taskHandle, findRepeatTaskByHandle);
    if (obj != NULL)
    {
        return incrementRepeatTaskRef(obj);
    }
    return NULL;
}

/*
 * remove the task with this handle from the list and returns the object.
 */
static RepeatTask *removeTaskFromList(uint32_t taskHandle)
{
    // find the task for this handle
    mutexLock(&TASK_MTX);
    RepeatTask *obj = findTaskInListLocked(taskHandle); // increment ref count
    if (obj == NULL)
    {
        // task is not in the list anymore.
        mutexUnlock(&TASK_MTX);
        return NULL;
    }

    // now remove the task from our list, but DO NOT FREE IT.
    linkedListDelete(tasks, &taskHandle, findRepeatTaskByHandle, standardDoNotFreeFunc);

    // if the list is empty, release it (could be we're shutting down)
    if (linkedListCount(tasks) == 0)
    {
        linkedListDestroy(tasks, standardDoNotFreeFunc);
        tasks = NULL;
    }
    mutexUnlock(&TASK_MTX);

    // decrement ref count due to findTaskInList. We still own a ref that originally was the list ref, and we're
    // forwarding it to the caller.
    decrementRepeatTaskRef(obj);

    return obj;
}

/*
 * default impl of destroyRepeatingTaskObjectsFunc for tasks that did not have one
 */
static void defaultDestroyRepeatingTaskObjectsFunc(uint32_t taskHandle, void *userArg, RepeatingTaskPolicy *policy)
{
    // keep with description in createPolicyRepeatingTask(), and use:
    //   destroyRepeatingPolicy for policy
    //   free for userArg
    destroyRepeatingPolicy(policy);
    free(userArg);
}

/*
 * internal call, assumes LIST_MTX is held
 */
static uint32_t assignHandleLocked(void)
{
    // Don't overflow, ensures we never use 0
    if (handleCounter == UINT32_MAX)
    {
        handleCounter = 0;
    }

    return ++handleCounter;
}
