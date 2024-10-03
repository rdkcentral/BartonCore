//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2022 Comcast
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

/*-----------------------------------------------
 * icStateMachine.c
 *
 * General use finite state machine that supports notifications
 * of state changes as-well-as running custom functions to "transition"
 * from one state to another.  This providing a simple, yet robust,
 * level of control for a state machine with pre-determined sequences.
 *
 * State values within the machine are treated as integer
 * to allow for enumeration-based states.
 *
 * Each state optionally contains two optional functions when transitioning
 * from one state to another:
 * 1. transition action - allow caller to drive underlying operations when changing states
 * 2. notification - notify caller of a change in state
 *
 * This implementation does not impose a specified direction
 * of the state transitions (i.e states changes do not have to
 * move to adjacent states), with the exception of the
 * "travel to state" function.
 *
 * NOTE: this does not perform any mutex locking to
 *       allow for single-threaded usage without the
 *       overhead.  If locking is required, it should
 *       be performed by the caller
 *
 * Author: jelderton - 10/19/22
 *-----------------------------------------------*/

#include <stdbool.h>
#include <stdlib.h>

#include <icTypes/icLinkedList.h>
#include <icUtil/icStateMachine.h>

/*
 * each "state" has a numeric value (allows for enums) and transition actions.
 */
typedef struct fsmState
{
    int stateValue;                           // numeric value of the state
    int offset;                               // reflect position within list.  primarily used for "traveling"
    stateTransitionActionFunc transitionFunc; // optional
    stateChangeNotifyFunc notifyFunc;         // optional
} fsmState;

/*
 * the icStateMachine object is essentially a linked list of fsmState objects
 * with a pointer to the current state.
 */
struct icStateMachine
{
    fsmState *currState;     // pointer to an element in stateList
    icLinkedList *stateList; // list of fsmState objects in "added" order
};

// used by stateMachineTravelToState when "folding" the stateList
typedef struct travelBounds
{
    int startOffset;
    int endOffset;
} travelBounds;

// private functions
static fsmState *
createState(int value, stateTransitionActionFunc transitionActionFunc, stateChangeNotifyFunc notificationFunc);
static void destroyState(fsmState *state);
static void destroyStateFromList(void *item);
static fsmState *findStateFromList(icLinkedList *list, int stateValue);
static int findStateOffsetFromList(icLinkedList *list, int stateValue);
static void *filterStatesWithinRange(void *item, void *context);


/**
 * Creates a state machine object without any known state values
 *
 * @see stateMachineAppendState() to add states and actions to the machine
 **/
icStateMachine *stateMachineCreate(void)
{
    icStateMachine *retVal = (icStateMachine *) calloc(1, sizeof(icStateMachine));
    retVal->stateList = linkedListCreate();

    return retVal;
}

/**
 * Destroy a state machine object
 **/
void stateMachineDestroy(icStateMachine *machine)
{
    if (machine != NULL)
    {
        linkedListDestroy(machine->stateList, destroyStateFromList);
        machine->stateList = NULL;
        machine->currState = NULL;
        free(machine);
    }
}

/**
 * Adds a new state value to the machine and associates action functions
 * to invoke during enter/exit transitions of this state.
 *
 * If this is the first state added to the machine, the machine will assume
 * this as the current state value, but NOT invoke the transition or notify
 * functions.
 *
 * Returns false if errors occur appending the state (i.e. duplicate)
 *
 * @param machine - the state machine to append the state to
 * @param stateValue - the integer value of the state
 * @param notificationFunc - optional function to call that performs the transition action
 * @param transitionActionFunc - optional function to call after transition is complete
 *
 * @see stateMachineSetCurrentState()
 **/
bool stateMachineAppendState(icStateMachine *machine,
                             int stateValue,
                             stateTransitionActionFunc transitionActionFunc,
                             stateChangeNotifyFunc notificationFunc)
{
    bool retVal = false;
    if (machine == NULL)
    {
        return retVal;
    }

    // do the append; but first ensure we are not adding a duplicate state value
    fsmState *search = findStateFromList(machine->stateList, stateValue);
    if (search == NULL)
    {
        // this stateValue is not a dup, create one and assign the 'offset' before appending.
        // Note: because we always append, the offset value is predetermined
        search = createState(stateValue, transitionActionFunc, notificationFunc);
        search->offset = linkedListCount(machine->stateList);
        if ((retVal = linkedListAppend(machine->stateList, search)) == true)
        {
            // assign this as current state if we don't have one
            if (machine->currState == NULL)
            {
                machine->currState = search;
            }
        }
        else
        {
            // error appending to the list
            destroyState(search);
        }
        search = NULL;
    }

    return retVal;
    ;
}

/**
 * Return the current state of this state machine
 * Can return UNDEFINED_STATE_MACHINE_VALUE if the current state is not defined
 **/
int stateMachineGetCurrentState(icStateMachine *machine)
{
    int retVal = UNDEFINED_STATE_MACHINE_VALUE;
    if (machine != NULL)
    {
        if (machine->currState != NULL)
        {
            retVal = machine->currState->stateValue;
        }
    }

    return retVal;
}

/**
 * Sets the new state of this state machine following the pattern of:
 * - if defined, call the state's stateTransitionActionFunc.
 * - if transition ran and fails:
 *     - stay at current state
 *     - return 'false'
 * - if transition ran successfully (or was skipped):
 *     - apply the new state,
 *     - call the state's stateChangeNotifyFunc (if defined)
 *     - return 'true'
 *
 * @param machine - the state machine to set the state against
 * @param newStateValue - state value to transition to
 * @param userArg - optional object passed to the exit/entry action callbacks
 **/
fsmSetStateResult stateMachineSetCurrentState(icStateMachine *machine, int newStateValue, void *userArg)
{
    fsmSetStateResult retVal = FSM_SET_STATE_INVALID_STATE;
    if (machine == NULL)
    {
        return retVal;
    }

    // ensure newStateValue is something we have in our list (and not currState)
    fsmState *currState = machine->currState;
    int currStateValue = currState->stateValue;
    fsmState *newState = findStateFromList(machine->stateList, newStateValue);
    if (newState != NULL && newState != currState)
    {
        // if defined, call the transition function to dictate if we proceed
        bool goForward = true;
        if (currState->transitionFunc != NULL)
        {
            goForward = currState->transitionFunc(currStateValue, newStateValue, userArg);
        }

        if (goForward == true)
        {
            // potentially inform notification function
            if (currState->notifyFunc != NULL)
            {
                currState->notifyFunc(currStateValue, newStateValue, userArg);
            }

            // safe to apply the new state
            machine->currState = newState;
            retVal = FSM_SET_STATE_SUCCESS;
        }
        else
        {
            retVal = FSM_SET_STATE_TRANSITION_FAIL;
        }
    }

    return retVal;
}

/**
 * Performs a sequential series of "set current state" calls, traveling from
 * "current state" until it reaches the "target state".  Mainly used when state
 * changes require some sort of unwinding and need to visit each state along the way.

 * For example: given states of A, B, C, D; a "travel" from B to D would perform
 *              "set" on C then on D.  Additionally, traveling from D to B
 *              would behave the same but in the reverse direction.
 *
 * Similar to stateMachineSetCurrentState(), this will call the appropriate state
 * functions as it transitions to each state in the sequence.  If any of those transition
 * functions returns false, the sequence is aborted and the machine will remain in the
 * last known state (i.e prior to the transition).
 *
 * @param machine - the state machine to set the state against
 * @param targetStateValue - state value to travel toward (then stop)
 * @param userArg - optional object passed to the exit/entry action callbacks
 **/
fsmSetStateResult stateMachineTravelToState(icStateMachine *machine, int targetStateValue, void *userArg)
{
    if (machine == NULL)
    {
        return FSM_SET_STATE_INVALID_STATE;
    }

    // determine the start/stop locations within the list and the direction of travel.
    int startOffset = findStateOffsetFromList(machine->stateList, machine->currState->stateValue);
    int endOffset = findStateOffsetFromList(machine->stateList, targetStateValue);

    // sanity check
    if (startOffset == -1 || endOffset == -1 || startOffset == endOffset)
    {
        return FSM_SET_STATE_INVALID_STATE;
    }

    // based on the travel direction (left-to-right || right-to-left), extract the
    // subset of states we will traverse.  Ex below (c=curr, t=target):
    //    1  2  3  4  5
    //       c     t        left-to-right; start=2 end=4
    //       t     c        right-to-left; start=4 end=2
    icLinkedList *travelStates = NULL;
    if (startOffset < endOffset)
    {
        // left-to-right
        travelBounds range = {.startOffset = startOffset, .endOffset = endOffset};
        travelStates = linkedListFilter(machine->stateList, filterStatesWithinRange, &range);
    }
    else
    {
        // right-to-left
        travelBounds range = {.startOffset = endOffset, .endOffset = startOffset};
        icLinkedList *reverse = linkedListFilter(machine->stateList, filterStatesWithinRange, &range);
        travelStates = linkedListReverse(reverse);
        linkedListDestroy(reverse, standardDoNotFreeFunc);
    }

    // begin traversing our subset (should be in the correct order)
    fsmSetStateResult rc = FSM_SET_STATE_SUCCESS;
    bool didSkip = false;
    icLinkedListIterator *loop = linkedListIteratorCreate(travelStates);
    while (linkedListIteratorHasNext(loop) == true && rc == FSM_SET_STATE_SUCCESS)
    {
        fsmState *curr = (fsmState *) linkedListIteratorGetNext(loop);
        if (didSkip == false)
        {
            // no need to 'set state' to where we start
            didSkip = true;
            continue;
        }

        // attempt transition to the next state in the ordered list
        rc = stateMachineSetCurrentState(machine, curr->stateValue, userArg);
    }
    linkedListIteratorDestroy(loop);
    linkedListDestroy(travelStates, standardDoNotFreeFunc);

    return rc;
}

/**
 * allocate a single state object
 **/
static fsmState *
createState(int value, stateTransitionActionFunc transitionActionFunc, stateChangeNotifyFunc notificationFunc)
{
    fsmState *retVal = (fsmState *) calloc(1, sizeof(fsmState));
    retVal->stateValue = value;
    retVal->transitionFunc = transitionActionFunc;
    retVal->notifyFunc = notificationFunc;

    return retVal;
}

/**
 * destroy a single state
 **/
static void destroyState(fsmState *state)
{
    free(state);
}

/**
 * destroy a single state from the linked list
 * (adheres to linkedListItemFreeFunc signature)
 **/
static void destroyStateFromList(void *item)
{
    destroyState((fsmState *) item);
}

/**
 * linkedListCompareFunc used when locating fsmState based on stateValue
 **/
static bool compareStateByValue(void *searchVal, void *item)
{
    int *searchStateValue = (int *) searchVal;
    fsmState *stateObj = (fsmState *) item;

    if (stateObj->stateValue == *searchStateValue)
    {
        return true;
    }
    return false;
}

/**
 * locate the fsmState object within 'list' that matches 'stateValue'
 **/
static fsmState *findStateFromList(icLinkedList *list, int stateValue)
{
    return (fsmState *) linkedListFind(list, &stateValue, compareStateByValue);
}

/**
 * locate the fsmState object within 'list' that matches 'stateValue' and
 * return the offset (element num) within the linked list, or -1 if not located
 **/
static int findStateOffsetFromList(icLinkedList *list, int stateValue)
{
    // leverage our other "find" then return the state->offset
    fsmState *located = findStateFromList(list, stateValue);
    if (located != NULL)
    {
        return located->offset;
    }
    return -1;
}

/**
 * linkedListFilterFunc used by stateMachineTravelToState to extract a range of fsmState objects
 **/
static void *filterStatesWithinRange(void *item, void *context)
{
    fsmState *state = (fsmState *) item;
    travelBounds *range = (travelBounds *) context;

    if (state->offset >= range->startOffset && state->offset <= range->endOffset)
    {
        return state;
    }

    return NULL;
}
