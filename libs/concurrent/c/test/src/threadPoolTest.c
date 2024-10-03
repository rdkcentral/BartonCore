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
 * threadPoolTest.c
 *
 * Unit test the icThreadPool object
 *
 * Author: jelderton - 2/12/16
 *-----------------------------------------------*/


// cmocka & it's dependencies
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <icConcurrent/icBlockingQueue.h>
#include <icConcurrent/threadPool.h>
#include <icConcurrent/timedWait.h>
#include <icLog/logging.h>
#include <inttypes.h>
#include <pthread.h>

#define LOG_CAT "poolTEST"

/*
 * test to make sure the pool creation doesn't allow dump values
 */
static void test_doesPreventStupidity(void **state)
{
    icLogDebug(LOG_CAT, "running thread pool test '%s'", __FUNCTION__);
    icThreadPool *pool = NULL;

    // create a pool with crazy values
    // and make sure we fail
    //
    pool = threadPoolCreate("test", 100, 99, 250);
    assert_null(pool);

    // cleanup
    threadPoolDestroy(pool);
}

typedef struct
{
    const char *str;
    icBlockingQueue *resultQueue;
} TaskArg;

TaskArg *createTaskArg(const char *str, icBlockingQueue *resultQueue)
{
    TaskArg *taskArg = (TaskArg *) calloc(1, sizeof(TaskArg));
    taskArg->str = str;
    taskArg->resultQueue = resultQueue;

    return taskArg;
}

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static bool awoken = false;
static int counter = 0;

static const char *taskStrs[] = {"a", "b", "c", "d", "e", "f"};

/*
 * simple task to push an item into a queue
 */
static void simpleTask(void *arg)
{
    TaskArg *taskArg = (TaskArg *) arg;
    pthread_mutex_lock(&mutex);
    ++counter;
    icLogDebug(LOG_CAT,
               "simple thread pool task '%s', incremented counter to %d",
               (taskArg != NULL) ? taskArg->str : "null",
               counter);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    icLogDebug(LOG_CAT, "simple thread pool task '%s', pushing", (taskArg != NULL) ? taskArg->str : "null");
    if (blockingQueuePushTimeout(taskArg->resultQueue, (void *) taskArg->str, 10))
    {
        icLogDebug(LOG_CAT, "simple thread pool task '%s', pushing done", (taskArg != NULL) ? taskArg->str : "null");
    }
    else
    {
        icLogError(LOG_CAT, "simple thread pool task '%s', pushing failed!", (taskArg != NULL) ? taskArg->str : "null");
    }
}

static void doNotDestroyQueueItemFunc(void *item) {}

/*
 * simple test to create a pool, add some tasks, and tear it down
 */
static void test_canRunJobs(void **state)
{
    icBlockingQueue *blockingQueue = blockingQueueCreate(2);
    // make a small pool (3) and add 2 jobs
    // to make sure they run.
    //
    icLogDebug(LOG_CAT, "running thread pool test '%s'", __FUNCTION__);
    icThreadPool *pool = threadPoolCreate("test", 3, 5, 10);

    // add 2 jobs (no arguments)
    TaskArg *taskArgA = createTaskArg("a", blockingQueue);
    TaskArg *taskArgB = createTaskArg("b", blockingQueue);
    threadPoolAddTask(pool, simpleTask, taskArgA, NULL);
    threadPoolAddTask(pool, simpleTask, taskArgB, NULL);

    // wait for the jobs to put their data in
    icLogDebug(LOG_CAT, "...waiting to allow threads to execute");
    blockingQueuePopTimeout(blockingQueue, 10);
    blockingQueuePopTimeout(blockingQueue, 10);

    // check that the pool size is 0
    //
    uint32_t size = threadPoolGetBacklogCount(pool);
    icLogDebug(LOG_CAT, "pool size is %d", size);
    assert_int_equal(size, 0);

    // destroy the pool
    threadPoolDestroy(pool);

    // destory the queue
    blockingQueueDestroy(blockingQueue, doNotDestroyQueueItemFunc);
}

/*
 * test to create a pool, add overload with tasks to ensure
 * the pool properly queue's the tasks
 */
static void test_canQueueJobs(void **state)
{
    icBlockingQueue *blockingQueue = blockingQueueCreate(1);

    // make a small pool (3) and add 2 jobs
    // to make sure they run
    //
    icLogDebug(LOG_CAT, "running thread pool test '%s'", __FUNCTION__);
    icThreadPool *pool = threadPoolCreate("test", 3, 3, 10);

    // add 5 jobs
    TaskArg *taskArgA = createTaskArg("a", blockingQueue);
    TaskArg *taskArgB = createTaskArg("b", blockingQueue);
    TaskArg *taskArgC = createTaskArg("c", blockingQueue);
    TaskArg *taskArgD = createTaskArg("d", blockingQueue);
    TaskArg *taskArgE = createTaskArg("e", blockingQueue);
    threadPoolAddTask(pool, simpleTask, taskArgA, NULL);
    threadPoolAddTask(pool, simpleTask, taskArgB, NULL);
    threadPoolAddTask(pool, simpleTask, taskArgC, NULL); // at min
    threadPoolAddTask(pool, simpleTask, taskArgD, NULL);
    threadPoolAddTask(pool, simpleTask, taskArgE, NULL); // at max

    // since the queue can only hold 3 some will be waiting still
    //
    uint32_t size = threadPoolGetBacklogCount(pool);
    icLogDebug(LOG_CAT, "pool size is %d", size);
    assert_in_range(size, 1, UINT32_MAX);

    // let the tasks go
    for (int i = 0; i < 5; ++i)
    {
        assert_non_null(blockingQueuePopTimeout(blockingQueue, 10));
    }

    // destroy the pool
    threadPoolDestroy(pool);

    blockingQueueDestroy(blockingQueue, doNotDestroyQueueItemFunc);
}

/*
 * make sure the pool can grow up to the max
 */
static void test_canGrowJobs(void **state)
{
    icBlockingQueue *blockingQueue = blockingQueueCreate(1);

    // make a pool with room to grow
    //
    icLogDebug(LOG_CAT, "running thread pool test '%s'", __FUNCTION__);
    icThreadPool *pool = threadPoolCreate("test", 1, 5, 10);

    // intialize counter
    counter = 0;

    // add 6 jobs to see that the pool grows
    TaskArg *taskArgs[6];
    for (int i = 0; i < 6; ++i)
    {
        taskArgs[i] = createTaskArg(taskStrs[i], blockingQueue);
        threadPoolAddTask(pool, simpleTask, taskArgs[i], NULL);
    }

    // Wait for counter to get to 6, means 1 completed, 5 active, 0 backlog
    pthread_mutex_lock(&mutex);
    while (counter < 6)
    {
        if (incrementalCondTimedWait(&cond, &mutex, 10) == ETIMEDOUT)
        {
            break;
        }
    }
    int counterVal = counter;
    pthread_mutex_unlock(&mutex);
    assert_int_equal(counterVal, 6);
    uint32_t back = threadPoolGetBacklogCount(pool);
    uint32_t active = threadPoolGetActiveCount(pool);
    icLogDebug(LOG_CAT, "pool active=%d backlog=%d", active, back);
    assert_int_equal(active, 5);
    assert_int_equal(back, 0);

    // let the tasks go
    for (int i = 0; i < 6; ++i)
    {
        assert_non_null(blockingQueuePopTimeout(blockingQueue, 10));
    }

    // destroy the pool
    threadPoolDestroy(pool);

    blockingQueueDestroy(blockingQueue, doNotDestroyQueueItemFunc);
}

/*
 * make sure the pool can start with 0 threads
 */
static void test_canHaveZeroMinThreads(void **state)
{
    icBlockingQueue *blockingQueue = blockingQueueCreate(1);

    // make a pool with 0 min threads
    //
    icLogDebug(LOG_CAT, "running thread pool test '%s'", __FUNCTION__);
    icThreadPool *pool = threadPoolCreate("test", 0, 1, 10);

    // intialize counter
    counter = 0;

    // add 1 job to see that the pool grows
    TaskArg *taskArgA = createTaskArg("a", blockingQueue);
    TaskArg *taskArgB = createTaskArg("b", blockingQueue);
    threadPoolAddTask(pool, simpleTask, taskArgA, NULL);
    threadPoolAddTask(pool, simpleTask, taskArgB, NULL);

    // This waits for the thread to start
    // Wait for counter to get to 2, means 1 completed, 1 active, 0 backlog
    pthread_mutex_lock(&mutex);
    while (counter < 2)
    {
        if (incrementalCondTimedWait(&cond, &mutex, 10) == ETIMEDOUT)
        {
            break;
        }
    }
    int counterVal = counter;
    pthread_mutex_unlock(&mutex);
    assert_int_equal(counterVal, 2);
    uint32_t back = threadPoolGetBacklogCount(pool);
    uint32_t active = threadPoolGetActiveCount(pool);
    icLogDebug(LOG_CAT, "pool active=%d backlog=%d", active, back);

    // see that thread started as expected
    assert_int_equal(active, 1);
    assert_int_equal(back, 0);

    // let the tasks go
    assert_non_null(blockingQueuePopTimeout(blockingQueue, 10));

    // destroy the pool, which will wait for everything to complete
    threadPoolDestroy(pool);

    blockingQueueDestroy(blockingQueue, doNotDestroyQueueItemFunc);
}

/*
 * Task designed to block until a condition is signaled. Allows control over number of threads in the pool for
 * counting purposes.
 */
static void blockingTask(void *arg)
{
    (void) arg;

    pthread_mutex_lock(&mutex);
    while (awoken == false)
    {
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);
}

/*
 * Wait until all the threads are "active" in the pool. This is a silly sleep, but I don't want to add anymore
 * threads and we should hit this active count fairly quickly.
 */
static bool waitForThreadCount(icThreadPool *pool, int amount, uint16_t maxTimeSeconds)
{
    uint32_t threadCount = threadPoolGetThreadCount(pool);
    uint8_t sleepIncr = 1; // 1 second
    uint16_t totalTime = 0;
    while (threadCount != amount && totalTime < maxTimeSeconds)
    {
        sleep(sleepIncr);
        totalTime += sleepIncr;
        threadCount = threadPoolGetThreadCount(pool);
    }

    return threadCount == amount;
}

/*
 * Verifies that threads used in a threadpool are properly joined and cleaned up after they finish running.
 */
static void test_threadsAreCleanedUp(void **state)
{
    icLogDebug(LOG_CAT, "Starting test %s", __FUNCTION__);

    // Initialize variables
    uint16_t maxThreads = 10;
    uint16_t minThreads = 0;
    uint16_t numThreadsToAdd = 1;
    icThreadPool *pool = threadPoolCreate("test", minThreads, maxThreads, 15);
    counter = 0;
    awoken = false;

    icLogDebug(LOG_CAT, "Starting number of threads in pool = %" PRIu16, threadPoolGetThreadCount(pool));

    // Populate our pool with some number of blocking threads
    for (counter = 0; counter < numThreadsToAdd; counter++)
    {
        threadPoolAddTask(pool, blockingTask, NULL, threadPoolTaskArgDoNotFreeFunc);
    }

    // Wait until all threads are running
    bool success = waitForThreadCount(pool, counter + minThreads, 3);

    // If we didn't hit the active count, then this test is a failure.
    assert_true(success);

    icLogDebug(LOG_CAT, "Running number of threads in pool = %" PRIu16, threadPoolGetThreadCount(pool));

    // Now lets signal our threads so they finish.
    pthread_mutex_lock(&mutex);
    awoken = true;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);

    // Give the thread pool threads time to realize there's nothing to do and end (may take up to numThreads * 10
    // seconds) Plus one additional second to ensure the worker has woken up and realized it needs to end
    success = waitForThreadCount(pool, minThreads, (counter + minThreads) * 10 + 1);

    // If we didn't hit the thread count, then this test is a failure.
    assert_true(success);

    icLogDebug(LOG_CAT, "Final number of threads in pool = %" PRIu16, threadPoolGetThreadCount(pool));

    // Cleanup
    threadPoolDestroy(pool);
    icLogDebug(LOG_CAT, "Ending test %s", __FUNCTION__);
    (void) state;
}

static void doNotFreeArgFunc(void *arg)
{
    // Nothing to do
    (void) arg;
}

static void selfDestroyTask(void *arg)
{
    icThreadPool *pool = (icThreadPool *) arg;
    threadPoolDestroy(pool);
    pthread_mutex_lock(&mutex);
    awoken = true;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);
}

static void test_taskCanDestroyPool(void **state)
{
    // Reset just in case
    awoken = false;
    icThreadPool *pool = threadPoolCreate("test", 1, 1, 15);

    threadPoolAddTask(pool, selfDestroyTask, pool, doNotFreeArgFunc);
    // Need to make sure the task actually started and did the destroy
    pthread_mutex_lock(&mutex);
    while (awoken == false)
    {
        if (incrementalCondTimedWait(&cond, &mutex, 1) == ETIMEDOUT)
        {
            pthread_mutex_unlock(&mutex);
            fail_msg("self destroy task should have started");
            return;
        }
    }
    pthread_mutex_unlock(&mutex);

    assert_true(awoken);
}

/*
 * main
 */
int main(int argc, const char **argv)
{
    // init logging and set to ERROR so we don't output a ton of log lines
    // NOTE: feel free to make this INFO or DEBUG when debugging
    // setIcLogPriorityFilter(IC_LOG_ERROR);
    initTimedWaitCond(&cond);

    // make our array of tests to run
    const struct CMUnitTest tests[] = {cmocka_unit_test(test_doesPreventStupidity),
                                       cmocka_unit_test(test_canRunJobs),
                                       cmocka_unit_test(test_canQueueJobs),
                                       cmocka_unit_test(test_canGrowJobs),
                                       cmocka_unit_test(test_canHaveZeroMinThreads),
                                       cmocka_unit_test(test_threadsAreCleanedUp),
                                       cmocka_unit_test(test_taskCanDestroyPool)};

    // fire off the suite of tests
    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    return retval;
}
