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
 * testTaskExecutor.c
 *
 * unit test for the taskExecutor
 *
 * Author: jelderton -  11/14/18.
 *-----------------------------------------------*/


// cmocka & it's dependencies
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <icConcurrent/taskExecutor.h>
#include <icConcurrent/timedWait.h>
#include <icLog/logging.h>

#define LOG_CAT "taskExecTEST"

static uint16_t ranSimple = 0;
static pthread_mutex_t simpleMTX = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t simpleCOND;

typedef struct _simple
{
    char *name;
    uint16_t val;
} simple;

/*
 * runner functions
 */
static void runSimpleFunc(void *taskObj, void *taskArg)
{
    if (taskObj != NULL)
    {
        // just echo the name & val
        simple *s = (simple *) taskObj;
        icLogDebug(LOG_CAT, "got SIMPLE name=%s value=%" PRIu16, s->name, s->val);

        // update the counter, then broadcast on the conditional
        pthread_mutex_lock(&simpleMTX);
        ranSimple++;
        pthread_cond_broadcast(&simpleCOND);
        pthread_mutex_unlock(&simpleMTX);
    }
}

/*
 * free functions
 */
static void freeSimpleFunc(void *taskObj, void *taskArg)
{
    if (taskObj != NULL)
    {
        simple *s = (simple *) taskObj;
        free(s->name);
        free(s);
    }
    free(taskArg); // should be NULL
}

static simple *createSimple(const char *name, uint16_t value)
{
    simple *retVal = (simple *) calloc(1, sizeof(simple));
    retVal->name = strdup(name);
    retVal->val = value;

    return retVal;
}

/*
 * test to make sure the task executor can run a single task
 */
static void test_singleTask(void **state)
{
    icLogDebug(LOG_CAT, "running taskExecutor test '%s'", __FUNCTION__);
    icTaskExecutor *exec = createTaskExecutor();

    // clear flag
    pthread_mutex_lock(&simpleMTX);
    ranSimple = 0;

    // add a 'simple' object to the executor
    simple *s = createSimple("single task test", 10);
    appendTaskToExecutor(exec, s, NULL, runSimpleFunc, freeSimpleFunc);

    // wait for something to run
    //
    incrementalCondTimedWait(&simpleCOND, &simpleMTX, 2);
    pthread_mutex_unlock(&simpleMTX);

    // cleanup (should wait for this to complete)
    destroyTaskExecutor(exec);

    // see that it worked
    icLogDebug(LOG_CAT, "ran %" PRIu16 " tests", ranSimple);
    assert_true(ranSimple > 0);
}

/*
 * test to make sure the task executor can run a handful of tasks
 */
static void test_multipleTasks(void **state)
{
    icLogDebug(LOG_CAT, "running taskExecutor test '%s'", __FUNCTION__);
    icTaskExecutor *exec = createTaskExecutor();

    // clear flag
    pthread_mutex_lock(&simpleMTX);
    ranSimple = 0;

    // add 10 tasks
    //
    int i;
    for (i = 0; i < 10; i++)
    {
        // add a 'simple' object to the executor
        simple *s = createSimple("multi task test", (uint16_t) i);
        appendTaskToExecutor(exec, s, NULL, runSimpleFunc, freeSimpleFunc);
    }

    // wait for something to run
    //
    incrementalCondTimedWait(&simpleCOND, &simpleMTX, 2);
    pthread_mutex_unlock(&simpleMTX);

    // cleanup (should wait for some of these to complete)
    destroyTaskExecutor(exec);

    // see that all of them ran
    icLogDebug(LOG_CAT, "ran %" PRIu16 " tests", ranSimple);
    assert_true(ranSimple >= 1);
}


/*
 * main
 */
int main(int argc, const char **argv)
{
    // init logging and set to ERROR so we don't output a ton of log lines
    // NOTE: feel free to make this INFO or DEBUG when debugging
    setIcLogPriorityFilter(IC_LOG_ERROR);

    // init our conditional
    initTimedWaitCond(&simpleCOND);

    // make our array of tests to run
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_singleTask),
        cmocka_unit_test(test_multipleTasks),
    };

    // fire off the suite of tests
    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    return retval;
}
