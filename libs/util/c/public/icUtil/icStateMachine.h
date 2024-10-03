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
 * icStateMachine.h
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

#ifndef ZILKER_STATE_MACHINE_H
#define ZILKER_STATE_MACHINE_H

#include <stdbool.h>

// for stateMachineGetCurrentState if current state is undefined
#define UNDEFINED_STATE_MACHINE_VALUE -99

/*
 * Opaque definition of the state machine.
 */
typedef struct icStateMachine icStateMachine;

/**
 * Function signature of a state machine state change notification.
 * Called after a "state transition" is complete to report that
 * the machine "exited" the old state and "entered" the new state.
 *
 * When called, the state machine will have a read-lock which will deadlock
 * if this function attempts stateMachineAppendState()
 *
 * @param oldStateValue - the state transitioning from
 * @param newStateValue - the state transitioning to
 * @param userArg - optional argument supplied during stateMachineSetCurrentState()
 **/
typedef void (*stateChangeNotifyFunc)(int exitedStateValue, int enteredStateValue, void *userArg);

/**
 * Function signature of a state machine transition action.
 * Called to facilitate a transition from one state to another.
 *
 * If the function returns true, the transition was successful and the
 * stateChangeNotifyFunc will be called (if defined).  If the function returns
 * false, no state changes will occur.
 *
 * When called, the state machine will have a read-lock which will deadlock
 * if this function attempts stateMachineAppendState()
 *
 * @param fromStateValue - the state transitioning from
 * @param toStateValue - the state transitioning to
 * @param userArg - optional argument supplied during stateMachineSetCurrentState()
 **/
typedef bool (*stateTransitionActionFunc)(int fromStateValue, int toStateValue, void *userArg);

/**
 * Creates a state machine object without any known state values
 *
 * @see stateMachineAppendState() to add states and actions to the machine
 **/
icStateMachine *stateMachineCreate(void);

/**
 * Destroy a state machine object
 **/
void stateMachineDestroy(icStateMachine *machine);

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
                             stateChangeNotifyFunc notificationFunc);

/**
 * Return the current state of this state machine.
 * Can return UNDEFINED_STATE_MACHINE_VALUE if the current state is not defined
 **/
int stateMachineGetCurrentState(icStateMachine *machine);

/*
 * return value for stateMachineSetCurrentState()
 */
typedef enum
{
    FSM_SET_STATE_SUCCESS,        // return code when transition to newStateValue was successful
    FSM_SET_STATE_INVALID_STATE,  // return code when newStateValue is unknown/invalid
    FSM_SET_STATE_TRANSITION_FAIL // return code when stateTransitionActionFunc returned 'false'
} fsmSetStateResult;

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
fsmSetStateResult stateMachineSetCurrentState(icStateMachine *machine, int newStateValue, void *userArg);

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
fsmSetStateResult stateMachineTravelToState(icStateMachine *machine, int targetStateValue, void *userArg);

#endif // ZILKER_STATE_MACHINE_H
