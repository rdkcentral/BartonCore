//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2016 iControl Networks, Inc.
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
 * threadPool.c
 *
 * Simple implementation of a "dynamic thread pool" with a
 * "bounded queue" of tasks to run.  Uses POSIX threads
 * (pthread) to process incoming tasks with minimal
 * re-creation of the threads when needed.
 *
 * Threads that are created past the 'min' will eventually
 * expire when there is nothing to process for several seconds.
 *
 * Author: jelderton - 2/12/16
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>

#include <icLog/logging.h>
#include <icConcurrent/threadUtils.h>
#include <icTypes/icLinkedList.h>
#include <icUtil/stringUtils.h>
#include "icConcurrent/icBlockingQueue.h"
#include "icConcurrent/threadPool.h"

#define LOG_CAT "threadPool"

#define ADD_TASK_TIMEOUT_MS 10

/*
 * define our internal thread pool object
 */
struct _icThreadPool
{
    char            *name;            // name of the pool.  used for debug and stats
    icBlockingQueue *queue;           // queue of 'icTask' objects (things to run)

    pthread_t       *threads;         // array of pthread_t
    uint16_t        threadCount;      // number of threads in array
    uint16_t        minThreads;       // min number of threads to maintain
    uint16_t        maxThreads;       // max number of threads allowed

    pthread_mutex_t poolMutex;        // mutex for this pool instance

    bool            running;          // indicator of when to shutdown
    uint16_t        active;           // number of threads executing a task
    pthread_key_t   *threadLocalKey;  // The thread local key for this pool

    threadPoolStats stats;            // statistics gathered during execution
};

/*
 * objects to shove into the queue
 */
typedef struct _icTask
{
    threadPoolTask  run;
    void            *argument;
    threadPoolTaskArgFreeFunc argumentFreeFunc;
    bool            selfDestructed;   // true if the task destroyed the pool it belongs to
    pthread_key_t   *threadLocalKey;  // Copy of thread local key for the pool, in case the task needs to destroy it
} icTask;

/*
 * passed to worker threads
 */
typedef struct _workerArgs
{
    icThreadPool    *pool;      // pool this worker belogs to
    int             offset;     // index of pool->threads this instance is.  needed during cleanup.
} workerArgs;

/*
 * private functions
 */
static bool addWorkerThreads(icThreadPool *pool, uint16_t numToAdd);
static void *runWorker(void *arg);
static void taskFreeFunc(void *item);

static bool isRunning(icThreadPool* pool)
{
    bool ret;

    pthread_mutex_lock(&pool->poolMutex);
    ret = pool->running;
    pthread_mutex_unlock(&pool->poolMutex);

    return ret;
}
/*
 * create a new thread pool.  will pre-create 'minThreads' workers
 * and a fixed queue capable of holding 'maxQueueSize' tasks.
 * will return NULL if:
 *   there is a problem creating the threads,
 *   minThreads > maxThreads ,
 *   maxThreads > MAX_NUM_THREADS, or
 *   maxQueueSize > MAX_QUEUE_SIZE
 *
 * @param name - label for this pool.  only used for debugging and statistics collection
 * @param minThreads - lower bounds of the pool
 * @param maxThreads - upper bounds of the pool (max size it can grow to)
 * @param maxQueueSize - max number of 'tasks' to queue while waiting for worker thread availability
 * @see threadPoolDestroy()
 */
icThreadPool *threadPoolCreate(const char *name, uint16_t minThreads, uint16_t maxThreads, uint32_t maxQueueSize)
{
    // check inputs
    //
    if (minThreads > maxThreads || maxThreads > MAX_NUM_THREADS || maxQueueSize > MAX_QUEUE_SIZE)
    {
        // bad input values
        return NULL;
    }

    // allocate the object container
    //
    icThreadPool *retVal = (icThreadPool *)malloc(sizeof(icThreadPool));
    if (retVal == NULL)
    {
        // error getting memory
        return NULL;
    }

    if (maxQueueSize == 0) {
        maxQueueSize = MAX_QUEUE_SIZE;
    }

    // setup internal variables
    //
    memset(retVal, 0, sizeof(icThreadPool));
    retVal->queue = blockingQueueCreate((uint16_t) maxQueueSize);
    pthread_mutex_init(&retVal->poolMutex, NULL);

    // Initialize the thread local key, we have one key per thread pool
    retVal->threadLocalKey = (pthread_key_t *)malloc(sizeof(pthread_key_t));
    pthread_key_create(retVal->threadLocalKey, NULL);

    // apply a name
    //
    if (name != NULL && strlen(name) > 0)
    {
        retVal->name = strdup(name);
    }
    else
    {
        retVal->name = strdup("tpool");
    }

    // create the array of thread pointers, making one for each
    // (i.e. max) even though they won't all get used
    //
    retVal->threads = (pthread_t *)malloc(sizeof(pthread_t) * maxThreads);
    retVal->threadCount = 0;
    retVal->minThreads = minThreads;
    retVal->maxThreads = maxThreads;
    icLogDebug(retVal->name, "creating threadpool; workers=%d", minThreads);

    // set running state, then fire off each of our threads
    //
    retVal->running = true;
    addWorkerThreads(retVal, minThreads);

    return retVal;
}

/*
 * destroys a thread pool.  will wait for all threads to complete before returning.
 */
void threadPoolDestroy(icThreadPool *pool)
{
    if (pool == NULL || pool->name == NULL)
    {
        // not valid
        return;
    }
    icLogDebug(pool->name, "destroying threadpool; workers=%d", pool->threadCount);

    // set the running flag to FALSE, preventing new activity
    //
    pthread_mutex_lock(&pool->poolMutex);
    if (pool->running == true)
    {
        pool->running = false;
        // This should unblock any threads that are waiting on work
        blockingQueueDisable(pool->queue);

        // Get a snapshot so we can iterate over threads without it changing
        int threadCount = pool->threadCount;

        // MUST release the mutex or workers won't be able to
        // perform their cleanup.  sucks, but not sure what else to do here
        //
        pthread_mutex_unlock(&pool->poolMutex);

        // wait for all of them to die
        //
        pthread_t currThread = pthread_self();
        int i = 0;
        for (i = 0; i < threadCount; i++)
        {
            // Check for a task in the thread pool destroying itself
            if (pthread_equal(currThread,pool->threads[i]) == false)
            {
                int retval = pthread_join(pool->threads[i], NULL);
                if (retval != 0)
                {
                    icLogWarn(pool->name, "Join on thread %d failed with error %d", i, retval);
                }
            }
            else
            {
                // Get the thread local variable for the current task that is running
                icTask *task = NULL;
                if (pool->threadLocalKey != NULL)
                {
                    task = (icTask *)pthread_getspecific(*pool->threadLocalKey);
                }
                if (task != NULL)
                {
                    task->selfDestructed = true;
                    // The task will be responsible for freeing the key
                    pool->threadLocalKey = NULL;
                }
                else
                {
                    icLogWarn(pool->name, "Failed to find thread local task for self destruct");
                }
                pthread_detach(pool->threads[i]);
            }

        }
        icLogDebug(pool->name, "done waiting on worker threads");
    }
    else
    {
        pthread_mutex_unlock(&pool->poolMutex);
    }

    blockingQueueDestroy(pool->queue, taskFreeFunc);

    if (pool->threadLocalKey != NULL)
    {
        pthread_key_delete(*pool->threadLocalKey);
        free(pool->threadLocalKey);
    }

    // cleanup.  queue should be empty, but just in case
    // we'll let it use normal release mechanism to remove
    // the icTask containers
    //
    free(pool->threads);
    pthread_mutex_destroy(&pool->poolMutex);
    if (pool->name != NULL)
    {
        free(pool->name);
    }
    free(pool);
}

/*
 * adds a task to the thread pool queue.  once resources are
 * available, it will execute 'task' and supply it with the
 * optional 'arg'.  the 'task' should release 'arg' at the end
 * of the execution.
 *
 * returns false if the queue is full
 *
 * @param pool - the thread pool to use for the 'task'
 * @param task - the task to execute
 * @param arg - optional argument to pass to 'task' when it executes
 * @param argFreeFunc - function to cleanup the task argument
 */
bool threadPoolAddTask(icThreadPool *pool, threadPoolTask task, void *arg, threadPoolTaskArgFreeFunc argFreeFunc)
{
    bool ret = false;

    if (pool == NULL || pool->running == false)
    {
        if (argFreeFunc != NULL)
        {
            argFreeFunc(arg);
        }
        else
        {
            free(arg);
        }
        return false;
    }

    // check the queue size
    //
    pthread_mutex_lock(&pool->poolMutex);
    // create the task object
    //
    icTask *job = (icTask *)malloc(sizeof(icTask));
    job->run = task;
    job->argument = arg;
    job->argumentFreeFunc = argFreeFunc;
    job->threadLocalKey = pool->threadLocalKey;
    job->selfDestructed = false;

    /* add to the queue & return (let the thread that
     * executes perform the cleanup on 'job
     *
     * This will be non-blocking so that we don't hold up
     * anything that was expecting us to return immediately.
     */

    struct timespec retryWait = { .tv_sec = 0, .tv_nsec = ADD_TASK_TIMEOUT_MS * 1000 * 1000 };
    if (blockingQueuePushTimeoutGranular(pool->queue, job, &retryWait)) {
        uint16_t currSize = blockingQueueCount(pool->queue);

        // if we have available room, create another worker
        // so this task won't have to wait around
        //
        uint32_t empty = (pool->maxThreads - pool->threadCount);
        if (empty > 0 && pool->threadCount < pool->maxThreads)
        {
            icLogTrace(pool->name, "adding extra worker; empty=%d min=%d max=%d curr=%d active=%d queue=%d",
                   empty, pool->minThreads, pool->maxThreads, pool->threadCount, pool->active, currSize);
            // have room to add 1 thread
            //
            addWorkerThreads(pool, 1);
        }
        else
        {
            icLogTrace(pool->name, "place task in queue; empty=%d min=%d max=%d curr=%d active=%d queue=%d",
                   empty, pool->minThreads, pool->maxThreads, pool->threadCount, pool->active, currSize);
        }

        // update stats
        //
        pool->stats.totalTasksQueued++;
        if (currSize > pool->stats.maxTasksQueued)
        {
            pool->stats.maxTasksQueued = currSize;
        }

        ret = true;
    } else {
        icLogWarn(LOG_CAT, "%s: unable to add task: queue full", pool->name);
        taskFreeFunc(job);
    }
    pthread_mutex_unlock(&pool->poolMutex);

    return ret;
}

/*
 * returns the number of tasks currently running
 */
uint32_t threadPoolGetActiveCount(icThreadPool *pool)
{
    uint32_t retVal = 0;
    if (pool == NULL || pool->running == false)
    {
        return retVal;
    }

    pthread_mutex_lock(&pool->poolMutex);
    retVal = pool->active;
    pthread_mutex_unlock(&pool->poolMutex);

    return retVal;
}

/*
 * returns the number of threads currently running in the pool
 */
uint16_t threadPoolGetThreadCount(icThreadPool *pool)
{
    uint16_t retVal = 0;
    if (pool == NULL || pool->running == false)
    {
        return retVal;
    }

    pthread_mutex_lock(&pool->poolMutex);
    retVal = pool->threadCount;
    pthread_mutex_unlock(&pool->poolMutex);

    return retVal;
}

/*
 * returns the number of tasks waiting in the queue
 */
uint32_t threadPoolGetBacklogCount(icThreadPool *pool)
{
    uint32_t retVal = 0;

    if (pool) {
        pthread_mutex_lock(&pool->poolMutex);
        if (pool->running) {
            retVal = blockingQueueCount(pool->queue);
        }
        pthread_mutex_unlock(&pool->poolMutex);
    }

    return retVal;
}

/*
 * uses the icQueue's 'queueIterate' function to iterate through the
 * backlog so we can debug the items waiting for execution.  helpful
 * for debugging.
 *
 * @see queueIterate()
 */
void threadPoolShowBacklog(icThreadPool *pool, queueIterateFunc printFunc, void *printArg)
{
    if (pool == NULL || pool->running == false)
    {
        return;
    }

    // forward on to the pool's queue so it can print the backlog items
    //
    pthread_mutex_lock(&pool->poolMutex);
    blockingQueueIterate(pool->queue, (blockingQueueIterateFunc) printFunc, printArg);
    pthread_mutex_unlock(&pool->poolMutex);
}

/*
 * return a copy of the current statistics collected.  caller must free the returned object.
 * if 'thenClear' is true, stats will be cleared after the copy.
 */
threadPoolStats *threadPoolGetStatistics(icThreadPool *pool, bool thenClear)
{
    // create the return object
    //
    threadPoolStats *retVal = (threadPoolStats *)malloc(sizeof(threadPoolStats));

    // copy stats from 'pool'
    //
    pthread_mutex_lock(&pool->poolMutex);
    memcpy(retVal, &pool->stats, sizeof(threadPoolStats));
    if (thenClear == true)
    {
        // clear the stats now that we finished copying them
        //
        memset(&pool->stats, 0, sizeof(threadPoolStats));
    }
    pthread_mutex_unlock(&pool->poolMutex);

    return retVal;
}

/*
 * clear current statistics collected thus far
 */
void threadPoolClearStatistics(icThreadPool *pool)
{
    // zero-out the stats from this pool
    //
    pthread_mutex_lock(&pool->poolMutex);
    memset(&pool->stats, 0, sizeof(threadPoolStats));
    pthread_mutex_unlock(&pool->poolMutex);
}

/*
 * threadPoolTaskArgFreeFunc that does not free the argument
 */
void threadPoolTaskArgDoNotFreeFunc(void *arg)
{
    (void)arg;
    // Nothing to do
}

/*
 * create more workers for a pool.  note that we always append
 * using the pool->threadCount as the indicator as to how many
 * exist (and are active).  not as efficient, but no fragmentation
 * of the array and easier to manage.
 */
static bool addWorkerThreads(icThreadPool *pool, uint16_t numToAdd)
{
    // prevent making more then there's room for
    //
    uint16_t emptyAvail = pool->maxThreads - pool->threadCount;
    if (emptyAvail == 0)
    {
        // no room
        return false;
    }
    else if (numToAdd > emptyAvail)
    {
        // asking for more workers then empty slots are available
        numToAdd = emptyAvail;
    }
    if (numToAdd == 0)
    {
        return false;
    }

    icLogTrace(pool->name, "adding %d workers to threadpool; avail=%d", numToAdd, emptyAvail);

    int i = 0;
    for (i = 0 ; i < numToAdd ; i++)
    {
        // creat a workerArgs to supply to the worker thread
        //
        workerArgs *parm = (workerArgs *)malloc(sizeof(workerArgs));
        parm->pool = pool;
        parm->offset = pool->threadCount;

        // start the worker, at array offset 'threadCount'
        //
        char *workerName = stringBuilder("%s-w%d", pool->name, parm->offset);

        createThread(&(pool->threads[pool->threadCount]), runWorker, parm, workerName);
        free(workerName);

        pool->threadCount++;
    }

    icLogTrace(pool->name, "done adding %d workers to threadpool; current=%d", numToAdd, pool->threadCount);

    return true;
}

/*
 * worker thread
 */
static void *runWorker(void *arg)
{
    workerArgs *parm = (workerArgs *)arg;
    if (parm == NULL)
    {
        // exit cleanly for the 'join' to work
        //
        return NULL;
    }

    icThreadPool *pool = parm->pool;
    // Self destructive tasks can destroy the pool. So to be able to use pool name as log tag even after pool
    // is destroyed, we are keeping a clone of pool name
    //
    char *poolName = strdup(pool->name);

    // loop until told to shutdown
    //
    bool keepGoing = true;
    while (keepGoing == true)
    {
        icTask* task = NULL;

        if (isRunning(pool))
        {
            task = blockingQueuePopTimeout(pool->queue, 10);
        }

        if (!isRunning(pool))
        {
            keepGoing = false;
            taskFreeFunc(task);
            continue;
        }


        // grab next thing to work on and release for other
        // threads to have a chance at the queue
        //
        if (task == NULL)
        {
            pthread_mutex_lock(&pool->poolMutex);
            // nothing to do.  check if this thread is a candidate for removal.
            //
            if (parm->offset >= pool->minThreads && parm->offset == (pool->threadCount - 1))
            {
                // we are the 'last' thread in the set (beyond the min boundry) - with
                // nothing to do.  go ahead and clear this thread from the pool.
                //
                if (isIcLogPriorityTrace() == true)
                {
                    icLogTrace(poolName, "removing woker # %d; min=%d active=%d", pool->threadCount, pool->minThreads, pool->active);
                }

                //Do one last check if the pool is running. If it's not, we are in shutdown so don't detach because we're
                //going to get joined anyway.
                if (pool->running == true)
                {
                    pthread_detach(pthread_self());
                }
                pool->threadCount--;
                keepGoing = false;
            }
            pthread_mutex_unlock(&pool->poolMutex);
        }
        else
        {
            pthread_mutex_lock(&pool->poolMutex);
            if (isIcLogPriorityTrace() == true)
            {
                icLogTrace(poolName, "worker #%d processing task", parm->offset);
            }

            // got something to do, increment the 'active' counter
            //
            pool->active++;

            // update stats
            //
            pool->stats.totalTasksRan++;
            if (pool->active > pool->stats.maxConcurrentTasks)
            {
                pool->stats.maxConcurrentTasks = pool->active;
            }
            pthread_mutex_unlock(&pool->poolMutex);

            // Set our task into the thread local so if the task destroys the pool, it can set the flag on the task
            pthread_setspecific(*task->threadLocalKey, task);

            // execute the task
            //
            task->run(task->argument);
            if (isIcLogPriorityTrace() == true)
            {
                icLogTrace(poolName, "worker #%d DONE with task", parm->offset);
            }

            // Check if the thread pool task destroyed the thread pool
            if (task->selfDestructed == true)
            {
                // Have to clean this up if we self destructed
                pthread_key_delete(*task->threadLocalKey);
                free(task->threadLocalKey);
                // Stop
                keepGoing = false;
            }

            // release the task object (not the contents)
            //
            taskFreeFunc(task);

            // Only update the pool if its still around
            if (keepGoing == true)
            {
                // decrement 'active' counter
                //
                pthread_mutex_lock(&pool->poolMutex);
                if (pool->active > 0)
                {
                    pool->active--;
                }
                pthread_mutex_unlock(&pool->poolMutex);
            }
        }
    }

    // cleanup the workerArgs container, then
    // exit cleanly for the 'join' to work
    //
    if (isIcLogPriorityTrace() == true)
    {
        icLogTrace(poolName, "worker #%d exiting thread", parm->offset);
    }
    free(poolName);
    free(arg);

    return NULL;
}

static void taskFreeFunc(void *item)
{
    icTask *job = (icTask *)item;
    if (job != NULL)
    {
        if (job->argumentFreeFunc != NULL)
        {
            job->argumentFreeFunc(job->argument);
        }
        else
        {
            free(job->argument);
        }
    }

    free(job);
}
