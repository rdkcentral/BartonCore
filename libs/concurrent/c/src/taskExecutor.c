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
 * executorTask.c
 *
 * fifo queue of tasks to execute serially.  can be
 * thought of as a "thread pool of one" for queueing
 * tasks to be executed in the order they are inserted.
 *
 * Author: jelderton -  11/13/18.
 *-----------------------------------------------*/

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <icConcurrent/icBlockingQueue.h>
#include <icConcurrent/taskExecutor.h>
#include <icConcurrent/threadUtils.h>

#define MAX_QUEUE_SIZE 100

// possible states of the task executor
typedef enum
{
    TASK_EXEC_STATE_RUN,    // normal state
    TASK_EXEC_STATE_FINISH, // finalizing (wait for all to finish)
    TASK_EXEC_STATE_CANCEL, // canceling
} icTaskExecState;

// detailed definition of our construct
struct _icTaskExecutor
{
    icBlockingQueue *queue;    // queue to hold taskContainers
    icTaskExecState state;     // current state
    pthread_t execThread;      // processing thread
    pthread_mutex_t execMutex; // mutex for this instance
};

// object that is inserted into the queue
typedef struct _taskContainer
{
    taskExecRunFunc runFunc;
    taskExecFreeFunc freeFunc;
    void *taskObj;
    void *taskArg;
} taskContainer;

// private functions
static void internalDestroy(icTaskExecutor *executor);
static void freeQueueItemsFunc(void *item);
static void freeTaskContainer(taskContainer *container);
static void *execWorkerThread(void *arg);

/*
 * creates a new task executor
 */
icTaskExecutor *createTaskExecutor(void)
{
    return createBoundedTaskExecutor(MAX_QUEUE_SIZE);
}

/*
 * creates a new task executor that can only keep maxQueueSize jobs in its backlog
 */
icTaskExecutor *createBoundedTaskExecutor(uint16_t maxQueueSize)
{
    // make the container and give it a decent sized queue
    //
    icTaskExecutor *retVal = (icTaskExecutor *) calloc(1, sizeof(icTaskExecutor));
    retVal->state = TASK_EXEC_STATE_RUN;
    pthread_mutex_init(&retVal->execMutex, NULL);
    retVal->queue = blockingQueueCreate(maxQueueSize);

    // create the mutex and the thread
    //
    createThread(&retVal->execThread, execWorkerThread, retVal, "taskExecutor");

    return retVal;
}

/*
 * clears and destroys the task executor
 */
void destroyTaskExecutor(icTaskExecutor *executor)
{
    if (executor == NULL)
    {
        return;
    }

    pthread_mutex_lock(&executor->execMutex);
    if (executor->state != TASK_EXEC_STATE_CANCEL)
    {
        // set state to CANCEL
        //
        executor->state = TASK_EXEC_STATE_CANCEL;

        // disable to queue, which should unblock the "pop" and the
        // execWorkerThread picks up the cancel state change
        //
        blockingQueueDisable(executor->queue);
        pthread_mutex_unlock(&executor->execMutex);

        // wait for the thread to die
        //
        pthread_join(executor->execThread, NULL);

        // we can now safely delete the queue and the mutex
        //
        internalDestroy(executor);
    }
    else
    {
        // already in CANCEL state
        //
        pthread_mutex_unlock(&executor->execMutex);
    }
}

/*
 * waits for all queued tasks to complete, then destroys the task executor
 */
void drainAndDestroyTaskExecutor(icTaskExecutor *executor)
{
    if (executor == NULL)
    {
        return;
    }

    // only works if currently in RUN state
    //
    pthread_mutex_lock(&executor->execMutex);
    if (executor->state == TASK_EXEC_STATE_RUN)
    {
        // set state to FINISH (modified version of CANCEL).  this prevents us from
        // adding anymore tasks to the blocking queue.
        //
        executor->state = TASK_EXEC_STATE_FINISH;

        // if the queue is empty, just CANCEL and return
        //
        if (blockingQueueCount(executor->queue) == 0)
        {
            pthread_mutex_unlock(&executor->execMutex);
            destroyTaskExecutor(executor);
            return;
        }

        // the queue isn't empty, so assume the thread will figure out our state soon.
        // wait for it to exit, then perform the cleanup.
        //
        pthread_mutex_unlock(&executor->execMutex);
        pthread_join(executor->execThread, NULL);

        // we can now safely delete the queue and the mutex
        //
        internalDestroy(executor);
    }
    else
    {
        // already in CANCEL or FINISH state
        //
        pthread_mutex_unlock(&executor->execMutex);
    }
}

/*
 * called by destroyTaskExecutor and drainAndDestroyTaskExecutor
 */
static void internalDestroy(icTaskExecutor *executor)
{
    // delete the queue and the mutex
    //
    pthread_mutex_lock(&executor->execMutex);
    blockingQueueDestroy(executor->queue, freeQueueItemsFunc);
    executor->queue = NULL;
    pthread_mutex_unlock(&executor->execMutex);
    pthread_mutex_destroy(&executor->execMutex);

    // finally, the executor itself
    //
    free(executor);
}

/*
 * clears and destroys the queued tasks
 */
void clearTaskExecutor(icTaskExecutor *executor)
{
    // call clear on the queue, passing an internal function
    // that can typecast the element to taskContainer - from
    // there we can call the custom free function
    //
    if (executor != NULL)
    {
        pthread_mutex_lock(&executor->execMutex);
        if (executor->state == TASK_EXEC_STATE_RUN)
        {
            // perform the clear
            //
            blockingQueueClear(executor->queue, freeQueueItemsFunc);
        }
        pthread_mutex_unlock(&executor->execMutex);
    }
}

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
                          taskExecFreeFunc freeFunc)
{
    bool retVal = false;
    if (executor != NULL)
    {
        // prevent adding new tasks when in FINISH or CANCEL state
        //
        pthread_mutex_lock(&executor->execMutex);
        if (executor->state == TASK_EXEC_STATE_RUN)
        {
            // create the container and shove all of this into it
            //
            taskContainer *container = (taskContainer *) calloc(1, sizeof(taskContainer));
            container->runFunc = runFunc;
            container->freeFunc = freeFunc;
            container->taskObj = taskObj;
            container->taskArg = taskArg;

            // add to the queue
            //
            retVal = blockingQueuePushTimeout(executor->queue, container, 10);
            if (retVal == false)
            {
                // destroy the container and the args.  set freeFunc to NULL to prevent double free below
                //
                freeTaskContainer(container);
                freeFunc = NULL;
            }
        }
        pthread_mutex_unlock(&executor->execMutex);
    }

    // failed to add, so call supplied cleanup function
    //
    if (retVal == false && freeFunc != NULL)
    {
        freeFunc(taskObj, taskArg);
    }
    return retVal;
}

/*
 * returns the number of items in the backlog to execute
 */
uint16_t getTaskExecutorQueueCount(icTaskExecutor *executor)
{
    uint16_t retVal = 0;

    if (executor != NULL)
    {
        pthread_mutex_lock(&executor->execMutex);
        if (executor->state != TASK_EXEC_STATE_CANCEL)
        {
            retVal = blockingQueueCount(executor->queue);
        }
        pthread_mutex_unlock(&executor->execMutex);
    }

    return retVal;
}

/*
 * release the container using the function it has
 */
static void freeTaskContainer(taskContainer *container)
{
    // first the task obj/arg
    //
    if (container->freeFunc != NULL)
    {
        // free via function
        //
        container->freeFunc(container->taskObj, container->taskArg);
    }
    else
    {
        // just free it directly
        //
        free(container->taskObj);
        free(container->taskArg);
    }

    // now the container
    //
    container->taskObj = NULL;
    container->taskArg = NULL;
    container->runFunc = NULL;
    container->freeFunc = NULL;
    free(container);
}

/*
 * function to free the items in the blockingQueue.  adheres
 * to the blockingQueueItemFreeFunc signature.
 */
static void freeQueueItemsFunc(void *item)
{
    // typecast to the container, which gives us the
    // actual data to release as-well-as the function to
    // do that.
    //
    taskContainer *container = (taskContainer *) item;
    freeTaskContainer(container);
}

/*
 * thread for processing items for a single taskExecutor
 */
static void *execWorkerThread(void *arg)
{
    icTaskExecutor *exec = (icTaskExecutor *) arg;

    // loop until told to cancel
    //
    bool keepGoing = true;
    while (keepGoing == true)
    {
        // first, check our state to see if told to cancel
        //
        pthread_mutex_lock(&exec->execMutex);
        if (exec->state == TASK_EXEC_STATE_CANCEL)
        {
            // bail
            //
            pthread_mutex_unlock(&exec->execMutex);
            keepGoing = false;
            continue;
        }
        else if (exec->state == TASK_EXEC_STATE_FINISH)
        {
            // bail if nothing left in the queue
            //
            if (blockingQueueCount(exec->queue) == 0)
            {
                pthread_mutex_unlock(&exec->execMutex);
                keepGoing = false;
                continue;
            }
        }
        pthread_mutex_unlock(&exec->execMutex);

        // wait for something to show in our queue
        //
        taskContainer *task = (taskContainer *) blockingQueuePopTimeout(exec->queue, 10);
        if (task == NULL)
        {
            // timeout getting something to do
            //
            continue;
        }

        if (task->runFunc == NULL)
        {
            // could be an empty one (part of the 'destroy').  just free this and loop around
            //
            freeTaskContainer(task);
            continue;
        }

        // execute the task
        //
        task->runFunc(task->taskObj, task->taskArg);

        // now free the task (no longer in the queue)
        //
        freeTaskContainer(task);
    }

    // nothing to do here.  let whatever canceled us do the cleanup
    //
    return NULL;
}
