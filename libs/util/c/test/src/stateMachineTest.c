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
 * stateMachineTest.c
 *
 * Unit test to validate the state machine
 *
 * Author: jelderton - 10/19/22
 *-----------------------------------------------*/


// cmocka & it's dependencies
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <icLog/logging.h>
#include <icTypes/icStringBuffer.h>
#include <icUtil/icStateMachine.h>

typedef enum
{
    STATE_A,
    STATE_B,
    STATE_C,
    STATE_D,
    STATE_E,
    STATE_F,
} myStates;

static const char *myStateLabels[] = {"A", "B", "C", "D", "E", "F", NULL};

/*
 * simple test to create a state machine, add some states, get/set state (no action callbacks)
 */
static void test_fsmCreate(void **state)
{
    icStateMachine *fsm = stateMachineCreate();
    stateMachineAppendState(fsm, STATE_A, NULL, NULL);
    stateMachineAppendState(fsm, STATE_B, NULL, NULL);
    stateMachineAppendState(fsm, STATE_C, NULL, NULL);

    // curr state should be 'A'
    myStates curr = (myStates) stateMachineGetCurrentState(fsm);
    assert_int_equal(curr, STATE_A);

    // change state to 'C', then fetch current again
    stateMachineSetCurrentState(fsm, STATE_C, NULL);
    curr = (myStates) stateMachineGetCurrentState(fsm);
    assert_int_equal(curr, STATE_C);

    // cleanup
    stateMachineDestroy(fsm);
}

/*
 * ensure we cannot add a duplicate state value
 */
static void test_fsmDupState(void **state)
{
    icStateMachine *fsm = stateMachineCreate();
    bool worked;
    worked = stateMachineAppendState(fsm, STATE_A, NULL, NULL);
    assert_true(worked);
    worked = stateMachineAppendState(fsm, STATE_B, NULL, NULL);
    assert_true(worked);

    // add the dup, should return false
    worked = stateMachineAppendState(fsm, STATE_A, NULL, NULL);
    assert_false(worked);

    // cleanup
    stateMachineDestroy(fsm);
}

// static bool ranExitA = false;
// static bool ranEnterB = false;

// stateChangeNotifyFunc callback
static void stateChangedNotif(int oldStateValue, int newStateValue, void *userArg)
{
    // see that we received what is expected (as per expect_value calls)
    check_expected(oldStateValue);
    check_expected(newStateValue);
}

/*
 * validate that the action callbacks are working
 */
static void test_fsmActions(void **state)
{
    // simple state machine and assign notify functions
    icStateMachine *fsm = stateMachineCreate();
    stateMachineAppendState(fsm, STATE_A, NULL, stateChangedNotif);
    stateMachineAppendState(fsm, STATE_B, NULL, stateChangedNotif);

    // setup expected values passed to stateChangedNotif
    expect_value(stateChangedNotif, oldStateValue, STATE_A);
    expect_value(stateChangedNotif, newStateValue, STATE_B);

    // trigger the callbacks by changing state to B
    stateMachineSetCurrentState(fsm, STATE_B, NULL);

    // cleanup
    stateMachineDestroy(fsm);
}

// stateMachineSetCurrentState for travel tests
static void travelStateNotif(int oldStateValue, int newStateValue, void *userArg)
{
    // userArg should be a string buffer
    // append a "-" then 'old' state char/ to provide a trail of each state we traveled "from"
    // append a "+" then 'new' state char/ to provide a trail of each state we traveled "to"
    icStringBuffer *buff = (icStringBuffer *) userArg;
    stringBufferAppend(buff, " -");
    stringBufferAppend(buff, myStateLabels[oldStateValue]);
    stringBufferAppend(buff, " +");
    stringBufferAppend(buff, myStateLabels[newStateValue]);
}

static icStateMachine *createTravelStateMachine(void)
{
    // make one that has every known myState, each leveraging the same enter/exit actions
    icStateMachine *fsm = stateMachineCreate();
    int i = STATE_A;
    while (i <= STATE_F)
    {
        stateMachineAppendState(fsm, i, NULL, travelStateNotif);
        i++;
    }
    return fsm;
}

/*
 * validate that the action callbacks are working
 */
static void test_fsmTravelForward(void **state)
{
    icStateMachine *fsm = createTravelStateMachine();

    // travel from A to C
    icStringBuffer *buff = stringBufferCreate(5);
    stateMachineTravelToState(fsm, STATE_C, buff);
    char *result = stringBufferToString(buff);
    assert_string_equal(result, " -A +B -B +C");

    free(result);
    stringBufferDestroy(buff);
    stateMachineDestroy(fsm);
}

/*
 * validate that the action callbacks are working
 */
static void test_fsmTravelBackward(void **state)
{
    icStateMachine *fsm = createTravelStateMachine();

    // travel from F to C
    icStringBuffer *buff = stringBufferCreate(5);
    stateMachineSetCurrentState(fsm, STATE_F, NULL);
    stateMachineTravelToState(fsm, STATE_C, buff);
    char *result = stringBufferToString(buff);
    assert_string_equal(result, " -F +E -E +D -D +C");

    free(result);
    stringBufferDestroy(buff);
    stateMachineDestroy(fsm);
}

/**
 * main
 **/
int main(int argc, const char **argv)
{
    // init logging to DEBUG
    setIcLogPriorityFilter(IC_LOG_DEBUG);

    // make our array of tests to run
    const struct CMUnitTest tests[] = {cmocka_unit_test(test_fsmCreate),
                                       cmocka_unit_test(test_fsmDupState),
                                       cmocka_unit_test(test_fsmActions),
                                       cmocka_unit_test(test_fsmTravelForward),
                                       cmocka_unit_test(test_fsmTravelBackward)};

    // fire off the suite of tests
    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    return retval;
}
