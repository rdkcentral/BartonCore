//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2018 Comcast
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
// Comcast retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------
//
// Created by mkoch201 on 2/8/19.
//

// cmock support
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>

#include <icConcurrent/repeatingTask.h>
#include <icConcurrent/threadUtils.h>
#include <icConcurrent/timedWait.h>
#include <icLog/logging.h>
#include <icTime/timeUtils.h>
#include <icTypes/icLinkedList.h>

#define LOG_TAG                   "repeatingTaskTest"
#define MAX_TASK_RUNS             3
#define MAX_EXPONENTIAL_TASK_RUNS 5

static pthread_mutex_t TEST_MTX = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
static pthread_cond_t TEST_COND = PTHREAD_COND_INITIALIZER;

extern void shutdownRepeatingTasksForTest(void);

/*------------------------
 * test supporting functions
 *------------------------*/

static bool taskFinshed = false;

// object used as "userArg" for all of the tests
typedef struct
{
    uint32_t counter;
    const char *label; // for logging
} testUserArg;

static testUserArg *createUserArg(const char *label)
{
    testUserArg *retVal = (testUserArg *) calloc(1, sizeof(testUserArg));
    retVal->label = label;
    return retVal;
}

/*
 * called before starting task execution
 */
static void initCompleteIndicator(void)
{
    mutexLock(&TEST_MTX);
    taskFinshed = false;
    mutexUnlock(&TEST_MTX);
}

/*
 * called by task cleanup
 */
static void setCompletedIndicator(void)
{
    mutexLock(&TEST_MTX);
    taskFinshed = true;
    pthread_cond_broadcast(&TEST_COND);
    mutexUnlock(&TEST_MTX);
}

/*
 * called waiting for tasks to complete
 * returns the value of taskFinished
 */
static bool waitCompleteIndication(uint64_t timeoutMillis)
{
    mutexLock(&TEST_MTX);
    if (taskFinshed == false)
    {
        incrementalCondTimedWaitMillis(&TEST_COND, &TEST_MTX, timeoutMillis);
    }
    bool retVal = taskFinshed;
    mutexUnlock(&TEST_MTX);

    return retVal;
}

// repeatingTaskRunFunc for most of the policy tests
static bool basicRunFunc(void *userArg)
{
    // arg should contain a counter and a label
    testUserArg *arg = (testUserArg *) userArg;
    icLogDebug(LOG_TAG, "  '%s' run number %d", arg->label, arg->counter);

    // run the task MAX_TASK_RUNS times, then bail
    if (arg->counter >= MAX_TASK_RUNS)
    {
        return true;
    }
    arg->counter += 1;
    return false;
}

// destroyRepeatingTaskObjectsFunc for most of the tests
static void basicCleanupFunc(uint32_t taskHandle, void *userArg, RepeatingTaskPolicy *policy)
{
    destroyRepeatingPolicy(policy);
    free(userArg); // should be testUserArg object

    // send broadcast that we finished this task
    setCompletedIndicator();
}

/*------------------------
 * Traditional Tests
 *------------------------*/

/*
 * taskCallbackFunc for traditional tests
 */
static void traditionalRunFunc(void *arg)
{
    // just inrement the number of times this function runs
    testUserArg *userArg = (testUserArg *) arg;
    userArg->counter += 1;
    icLogDebug(LOG_TAG, "  '%s' run number %d", userArg->label, userArg->counter);
}

/*
 * called via cmocka
 */
static void test_traditionalMillisTasks(void **state)
{
    // reset "completed" indicator
    initCompleteIndicator();

    // run our task for 301 millis and then ensure we ran it 3 times
    testUserArg *arg = createUserArg("traditional millis");
    uint32_t handle = createRepeatingTask(100, DELAY_MILLIS, traditionalRunFunc, arg);

    // leverage the waitCompleteIndication() function as our timer.
    // since these are not policy-based, the task won't exit on its own
    waitCompleteIndication(301);

    // cancel the task and make sure it returns 'arg'
    void *duplicate = cancelRepeatingTask(handle);
    assert_true(duplicate == arg);

    // our counter should be 3 (maybe 4 or 5 on a slow machine)
    assert_in_range(arg->counter, 3, 5);

    free(arg);
}

/*------------------------
 * Standard Policy Tests
 *------------------------*/

/*
 * called via cmocka
 */
static void test_standardPolicyMillisTasks(void **state)
{
    // reset "completed" indicator
    initCompleteIndicator();

    // run the task MAX_TASK_RUNS times, pausing for 100 millis between
    testUserArg *arg = createUserArg("standard millis");
    RepeatingTaskPolicy *policy = createStandardRepeatingTaskPolicy(100, DELAY_MILLIS);
    uint32_t handle = createPolicyRepeatingTask(basicRunFunc, basicCleanupFunc, policy, arg);
    assert_int_not_equal(handle, 0);

    // wait up-to 1 second for the notification that this completed
    bool done = waitCompleteIndication(1000);
    assert_true(done);
}

/*
 * called via cmocka
 */
static void test_standardPolicySecsTasks(void **state)
{
    // reset "completed" indicator
    initCompleteIndicator();

    // run the task MAX_TASK_RUNS times, pausing for 1 second between
    testUserArg *arg = createUserArg("standard secs");
    RepeatingTaskPolicy *policy = createStandardRepeatingTaskPolicy(1, DELAY_SECS);
    uint32_t handle = createPolicyRepeatingTask(basicRunFunc, basicCleanupFunc, policy, arg);
    assert_int_not_equal(handle, 0);

    // wait for the notification that this completed (proving enough time)
    bool done = waitCompleteIndication((MAX_TASK_RUNS + 1) * 1000);
    assert_true(done);
}


/*------------------------
 * Fixed Policy Tests
 *------------------------*/

// repeatingTaskRunFunc for fixed test, to allow for more than the MAX_TASK_RUNS
static bool fixedRunFunc(void *userArg)
{
    // arg should contain a counter and a label
    testUserArg *arg = (testUserArg *) userArg;
    icLogDebug(LOG_TAG, "  '%s' run number %d", arg->label, arg->counter);

    // run the task 10 times, then bail
    if (arg->counter >= 9)
    {
        return true;
    }
    arg->counter += 1;
    return false;
}

/*
 * called via cmocka
 */
static void test_fixedPolicyTasks(void **state)
{
    // reset "completed" indicator
    initCompleteIndicator();

    // run the task MAX_TASK_RUNS times, pausing for 1 second between
    testUserArg *arg = createUserArg("fixed secs");
    RepeatingTaskPolicy *policy = createFixedRepeatingTaskPolicy(1, DELAY_SECS);
    uint32_t handle = createPolicyRepeatingTask(fixedRunFunc, basicCleanupFunc, policy, arg);
    assert_int_not_equal(handle, 0);

    // wait for the notification that this completed (proving enough time)
    bool done = waitCompleteIndication(10 * 1000);
    assert_true(done);
}


/*------------------------
 * Random Policy Tests
 *------------------------*/

/*
 * called via cmocka
 */
static void test_randomPolicyTasks(void **state)
{
    // reset "completed" indicator
    initCompleteIndicator();

    // run the task MAX_TASK_RUNS times
    testUserArg *arg = createUserArg("random millis");
    RepeatingTaskPolicy *policy = createRandomRepeatingTaskPolicy(100, 500, DELAY_MILLIS);
    uint32_t handle = createPolicyRepeatingTask(basicRunFunc, basicCleanupFunc, policy, arg);
    assert_int_not_equal(handle, 0);

    // wait for the notification that this completed (proving enough time)
    bool done = waitCompleteIndication((MAX_TASK_RUNS + 1) * 500);
    assert_true(done);
}


/*------------------------
 * Incremental Policy Tests
 *------------------------*/

/*
 * called via cmocka
 */
static void test_incrementalPolicyTasks(void **state)
{
    // reset "completed" indicator
    initCompleteIndicator();

    // run the task MAX_TASK_RUNS times
    testUserArg *arg = createUserArg("exponential millis");
    RepeatingTaskPolicy *policy = createIncrementalRepeatingTaskPolicy(100, 500, 200, DELAY_MILLIS);
    uint32_t handle = createPolicyRepeatingTask(basicRunFunc, basicCleanupFunc, policy, arg);
    assert_int_not_equal(handle, 0);

    // wait for the notification that this completed (proving enough time)
    bool done = waitCompleteIndication((MAX_TASK_RUNS + 1) * 500);
    assert_true(done);
}

// object used as "userArg" for all of the tests
typedef struct
{
    uint32_t counter;
    const char *label;       // for logging
    icLinkedList *execTimes; // list of uint64_t objects, one for each execution
} trackingTestUserArg;

// repeatingTaskRunFunc specific for createIncrementalRepeatingTaskPolicy
static bool incrementalTrackRunFunc(void *userArg)
{
    // arg should contain the items we need for the test.
    // first, the counter and logging.
    trackingTestUserArg *arg = (trackingTestUserArg *) userArg;
    icLogDebug(LOG_TAG, "  '%s' run number %d", arg->label, arg->counter);

    // save current time into our linked list
    uint64_t *now = (uint64_t *) malloc(sizeof(uint64_t));
    *now = getMonotonicMillis();
    linkedListAppend(arg->execTimes, now);

    // run the task MAX_TASK_RUNS times, then bail
    if (arg->counter >= MAX_TASK_RUNS)
    {
        return true;
    }
    arg->counter += 1;
    return false;
}

// destroyRepeatingTaskObjectsFunc specific for createIncrementalRepeatingTaskPolicy
static void incrementakTrackCleanupFunc(uint32_t taskHandle, void *userArg, RepeatingTaskPolicy *policy)
{
    destroyRepeatingPolicy(policy);

    // NOTE: purposefully not destroying the linked list of times so the unit test
    //       can examine them after finished running all of the tasks
    trackingTestUserArg *arg = (trackingTestUserArg *) userArg;
    arg->execTimes = NULL;
    free(arg);

    // send broadcast that we finished this task
    setCompletedIndicator();
}

/*
 * called via cmocka
 */
static void test_incremental2PolicyTasks(void **state)
{
    // reset "completed" indicator
    initCompleteIndicator();

    // run and record each time we run the function.  we are verifying
    // that the increments are spaced out property
    trackingTestUserArg *myArg = (trackingTestUserArg *) calloc(1, sizeof(trackingTestUserArg));
    icLinkedList *times = linkedListCreate();
    myArg->label = "incremental tracked millis";
    myArg->execTimes = times;
    RepeatingTaskPolicy *policy = createIncrementalRepeatingTaskPolicy(100, 500, 100, DELAY_MILLIS);
    uint32_t handle = createPolicyRepeatingTask(incrementalTrackRunFunc, incrementakTrackCleanupFunc, policy, myArg);
    assert_int_not_equal(handle, 0);

    // wait for the notification that this completed (proving enough time)
    bool done = waitCompleteIndication(5000);
    assert_true(done);

    // this should have ran 4 times, so grab each timestamp
    uint64_t *runA = (uint64_t *) linkedListGetElementAt(times, 0);
    uint64_t *runB = (uint64_t *) linkedListGetElementAt(times, 1);
    uint64_t *runC = (uint64_t *) linkedListGetElementAt(times, 2);
    uint64_t *runD = (uint64_t *) linkedListGetElementAt(times, 3);

    uint64_t diff = 0;
    diff = (*runB - *runA);
    icLogDebug(LOG_TAG, "  'incremental tracked' run #1 time diff=%" PRIu64, diff);
    assert_in_range(diff, 80, 120);

    diff = (*runC - *runB);
    icLogDebug(LOG_TAG, "  'incremental tracked' run #2 time diff=%" PRIu64, diff);
    assert_in_range(diff, 180, 220);

    diff = (*runD - *runC);
    icLogDebug(LOG_TAG, "  'incremental tracked' run #3 time diff=%" PRIu64, diff);
    assert_in_range(diff, 280, 320);

    linkedListDestroy(times, NULL);
    times = NULL;
}

/*------------------------
 * Exponential Policy Tests
 *------------------------*/

/*
 * repeatingTaskRunFunc specific for createExponentialRepeatingTaskPolicy
 */
static bool exponentialTrackRunFunc(void *userArg)
{
    // arg should contain the items we need for the test.
    // first, the counter and logging.
    trackingTestUserArg *arg = (trackingTestUserArg *) userArg;
    icLogDebug(LOG_TAG, "  '%s' run number %d", arg->label, arg->counter);

    // save current time into our linked list
    uint64_t *now = (uint64_t *) malloc(sizeof(uint64_t));
    *now = getMonotonicMillis();
    linkedListAppend(arg->execTimes, now);

    // run the task 5 times, then bail
    if (arg->counter >= MAX_EXPONENTIAL_TASK_RUNS)
    {
        return true;
    }
    arg->counter += 1;
    return false;
}

/*
 * destroyRepeatingTaskObjectsFunc specific for createExponentialRepeatingTaskPolicy
 */
static void exponentialTrackCleanupFunc(uint32_t taskHandle, void *userArg, RepeatingTaskPolicy *policy)
{
    destroyRepeatingPolicy(policy);

    // NOTE: purposefully not destroying the linked list of times so the unit test
    //       can examine them after finished running all of the tasks
    trackingTestUserArg *arg = (trackingTestUserArg *) userArg;
    arg->execTimes = NULL;
    free(arg);

    // send broadcast that we finished this task
    setCompletedIndicator();
}

/*
 * called via cmocka
 */
static void test_exponentialPolicyTasks(void **state)
{
    // reset "completed" indicator
    initCompleteIndicator();

    // run and record each time we run the function.  we are verifying
    // that the increments are spaced out properly
    trackingTestUserArg *myArg = (trackingTestUserArg *) calloc(1, sizeof(trackingTestUserArg));
    icLinkedList *times = linkedListCreate();
    myArg->label = "exponential tracked millis";
    myArg->execTimes = times;
    uint8_t base = 5;
    uint64_t maxDelay = pow(base, MAX_EXPONENTIAL_TASK_RUNS - 1);
    RepeatingTaskPolicy *policy = createExponentialRepeatingTaskPolicy(base, maxDelay, DELAY_MILLIS);
    uint32_t handle = createPolicyRepeatingTask(exponentialTrackRunFunc, exponentialTrackCleanupFunc, policy, myArg);
    assert_int_not_equal(handle, 0);

    // wait for the notification that this completed (proving enough time)
    bool done = waitCompleteIndication(5000);
    assert_true(done);

    uint64_t diff = 0;
    uint64_t expectedDelay = 0;

    for (int i = 0; i < MAX_EXPONENTIAL_TASK_RUNS; i++)
    {
        uint64_t *runA = (uint64_t *) linkedListGetElementAt(times, i);
        uint64_t *runB = (uint64_t *) linkedListGetElementAt(times, i + 1);
        diff = (*runB - *runA);

        if (i < MAX_EXPONENTIAL_TASK_RUNS - 1)
        {
            // On these runs, the expected delay should be the calculated delay for the proceeding run
            expectedDelay = pow(base, i + 1);
        }
        else
        {
            // On this run, the exponentiated delay should have surpassed the maxDelay, so the actual delay should be
            // equal to the maxDelay
            expectedDelay = maxDelay;
        }

        icLogDebug(LOG_TAG,
                   "  'exponential tracked' delay between run %d and run %d = %" PRIu64 "; expected delay: %" PRIu64,
                   i,
                   i + 1,
                   diff,
                   expectedDelay);
        assert_in_range(diff, expectedDelay - base, expectedDelay + base);
    }

    linkedListDestroy(times, NULL);
    times = NULL;
}


/*------------------------
 * Policy Agnostic Functions & Tests
 *------------------------*/

// repeatingTaskRunFunc for reset test, to allow for more than the MAX_TASK_RUNS
static bool resetRunFunc(void *userArg)
{
    // arg should contain a counter and a label
    testUserArg *arg = (testUserArg *) userArg;
    icLogDebug(LOG_TAG, "  '%s' run number %d", arg->label, arg->counter);

    // run the task 7 times, then bail
    if (arg->counter >= 6)
    {
        return true;
    }
    arg->counter += 1;
    return false;
}

/*
 * called via cmocka
 */
static void test_resetPolicy(void **state)
{
    // reset "completed" indicator
    initCompleteIndicator();

    // create a standard policy task, giving it a 1-second delay to give us time
    testUserArg *arg = createUserArg("reset 1 second");
    RepeatingTaskPolicy *policy = createStandardRepeatingTaskPolicy(1, DELAY_SECS);
    uint32_t handle = createPolicyRepeatingTask(resetRunFunc, basicCleanupFunc, policy, arg);
    assert_int_not_equal(handle, 0);

    // pause a second, then swap out with a faster policy
    usleep(1500 * 1000);
    RepeatingTaskPolicy *newPolicy = createStandardRepeatingTaskPolicy(250, DELAY_MILLIS);
    icLogDebug(LOG_TAG, "  resetting policy to 250 millis (later)");
    bool worked = resetPolicyRepeatingTask(handle, newPolicy, false);
    assert_true(worked);

    // pause 1 second to get a few executions, then swap again
    sleep(1);
    RepeatingTaskPolicy *finalPolicy = createStandardRepeatingTaskPolicy(1, DELAY_SECS);
    icLogDebug(LOG_TAG, "  resetting policy back to 1 second (now)");
    worked = resetPolicyRepeatingTask(handle, finalPolicy, true);
    assert_true(worked);

    // should take about 3 seconds to finish the new policy
    bool done = waitCompleteIndication(4 * 1000);
    assert_true(done);
}

/*
 * called via cmocka
 */
static void test_shortCircuit(void **state)
{
    // reset "completed" indicator
    initCompleteIndicator();

    // create a standard policy task, giving it a 3-second delay to give us time to short-circuit
    testUserArg *arg = createUserArg("short-circuit");
    RepeatingTaskPolicy *policy = createStandardRepeatingTaskPolicy(2, DELAY_SECS);
    uint32_t handle = createPolicyRepeatingTask(basicRunFunc, basicCleanupFunc, policy, arg);
    assert_int_not_equal(handle, 0);

    // pause a few millis, then short-circuit
    usleep(1000 * 250);
    shortCircuitRepeatingTask(handle);

    // should take less than 8 seconds to finish since iteration #2 was done early
    bool done = waitCompleteIndication((MAX_TASK_RUNS * 2 * 1000) - 200);
    assert_true(done);
}


/*
 * called via cmocka
 */
static void test_cancelTask(void **state)
{
    // reset "completed" indicator
    initCompleteIndicator();

    // create a standard policy task, giving it a somewhat long delay to give us time to cancel it
    testUserArg *arg = createUserArg("cancel");
    RepeatingTaskPolicy *policy = createStandardRepeatingTaskPolicy(1, DELAY_MINS);
    uint32_t handle = createPolicyRepeatingTask(basicRunFunc, basicCleanupFunc, policy, arg);
    assert_int_not_equal(handle, 0);

    // pause a few millis, then cancel
    usleep(1000 * 250);
    bool canceled = cancelPolicyRepeatingTask(handle);
    assert_true(canceled);

    // wait up-to 1 second for the notification that the cancel went through
    bool done = waitCompleteIndication(1000);
    assert_true(done);
}

// repeatingTaskRunFunc for shutdown test
static bool shutdownRunFunc(void *userArg)
{
    icLogDebug(LOG_TAG, "  'shutdown' task running");
    return false;
}

static int shutdownCounter = 0;

// destroyRepeatingTaskObjectsFunc for shutdown test
static void shutdownCleanupFunc(uint32_t taskHandle, void *userArg, RepeatingTaskPolicy *policy)
{
    destroyRepeatingPolicy(policy);

    // increment counter
    mutexLock(&TEST_MTX);
    shutdownCounter++;
    mutexUnlock(&TEST_MTX);
}

/*
 * main
 */
int main(int argc, const char **argv)
{
    setIcLogPriorityFilter(IC_LOG_TRACE);
    icLogDebug(LOG_TAG, "Starting repeating task tests...");

    // setup our conditional
    mutexLock(&TEST_MTX);
    initTimedWaitCond(&TEST_COND);
    mutexUnlock(&TEST_MTX);

    const struct CMUnitTest tests[] = {
        // traditional task tests
        cmocka_unit_test(test_traditionalMillisTasks),

        // policy-based task tests
        cmocka_unit_test(test_standardPolicyMillisTasks),
        cmocka_unit_test(test_standardPolicySecsTasks),
        cmocka_unit_test(test_fixedPolicyTasks),
        cmocka_unit_test(test_randomPolicyTasks),
        cmocka_unit_test(test_incrementalPolicyTasks),
        cmocka_unit_test(test_incremental2PolicyTasks),
        cmocka_unit_test(test_exponentialPolicyTasks),

        cmocka_unit_test(test_resetPolicy),
        cmocka_unit_test(test_shortCircuit),
        cmocka_unit_test(test_cancelTask),
    };

    int retval = cmocka_run_group_tests(tests, NULL, NULL);
    return retval;
}
