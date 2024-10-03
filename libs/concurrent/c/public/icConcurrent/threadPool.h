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
 * threadPool.h
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

#ifndef ZILKER_THREADPOOL_H
#define ZILKER_THREADPOOL_H

#include <icTypes/icQueue.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_NUM_THREADS 64
#define MAX_QUEUE_SIZE  128

/*
 * object to contain the statistics
 */
typedef struct _threadPoolStats
{
    uint32_t totalTasksQueued;   // total number of tasks added to the queue
    uint32_t totalTasksRan;      // total number of tasks executed
    uint32_t maxTasksQueued;     // largest size the task queue got to (backlog of tasks)
    uint32_t maxConcurrentTasks; // largest number of tasks running at the same time
    //    uint32_t avgConcurrentTasks;    // average number of tasks running at the same time
} threadPoolStats;

/*
 * the thread-pool object representation.
 */
typedef struct _icThreadPool icThreadPool;

/*
 * function prototype for the 'task' to execute via the pool.
 * this task is responsible for releasing the optional 'arg'
 * when the operation is complete.
 *
 * @param arg - the 'arg' supplied to the "threadPoolAddTask" function call
 */
typedef void (*threadPoolTask)(void *arg);

/*
 * function prototype for cleanup function for arguments to a task
 * @param arg the argument to the task function
 */
typedef void (*threadPoolTaskArgFreeFunc)(void *arg);

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
icThreadPool *threadPoolCreate(const char *name, uint16_t minThreads, uint16_t maxThreads, uint32_t maxQueueSize);

/*
 * destroys a thread pool.  will wait for all threads to complete before returning.
 */
void threadPoolDestroy(icThreadPool *pool);

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
bool threadPoolAddTask(icThreadPool *pool, threadPoolTask task, void *arg, threadPoolTaskArgFreeFunc argFreeFunc);

/*
 * returns the number of tasks currently running
 */
uint32_t threadPoolGetActiveCount(icThreadPool *pool);

/*
 * returns the number of threads currently running in the pool
 */
uint16_t threadPoolGetThreadCount(icThreadPool *pool);

/*
 * returns the number of tasks waiting in the queue
 */
uint32_t threadPoolGetBacklogCount(icThreadPool *pool);

/*
 * uses the icQueue's 'queueIterate' function to iterate through the
 * backlog so we can debug the items waiting for execution.  helpful
 * for debugging.
 *
 * @see queueIterate()
 */
void threadPoolShowBacklog(icThreadPool *pool, queueIterateFunc printFunc, void *printArg);

/*
 * return a copy of the current statistics collected.  caller must free the returned object.
 * if 'thenClear' is true, stats will be cleared after the copy.
 */
threadPoolStats *threadPoolGetStatistics(icThreadPool *pool, bool thenClear);

/*
 * clear current statistics collected thus far
 */
void threadPoolClearStatistics(icThreadPool *pool);

/*
 * threadPoolTaskArgFreeFunc that does not free the argument
 */
void threadPoolTaskArgDoNotFreeFunc(void *arg);

#endif // ZILKER_THREADPOOL_H
