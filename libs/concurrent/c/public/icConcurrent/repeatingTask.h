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
 * repeatingTask.h
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

#ifndef IC_REPEATTASK_H
#define IC_REPEATTASK_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// utilize common function prototypes and enumerations
#include "icConcurrent/delayedTask.h"

//-----------------------------------------------
//
// Traditional Repeating Tasks
//
//-----------------------------------------------

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
uint32_t createRepeatingTask(uint64_t delayAmount, delayUnits units, taskCallbackFunc func, void *userArg);

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
uint32_t createRepeatingTaskWithStartDelay(uint64_t startDelay, uint64_t delayAmount, delayUnits units,
                                           taskCallbackFunc func, void *userArg);

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
uint32_t createFixedRateRepeatingTask(uint64_t delayAmount, delayUnits units, taskCallbackFunc func, void *userArg);

/*
 * function prototype for being executed every iteration
 * of a back off repeating task
 *
 * @param userArg - optional input variable
 * @return - true to end task, false to keep going
 */
typedef bool (*backOffTaskRunCallbackFunc)(void *userArg);

/*
 * function prototype for when backOffTaskRunCallbackFunc
 * gets a return value of true (which is success)
 *
 * this prototype can be used as a cleanup function
 *
 * @param userArg - original userArg from backOffTaskRunCallbackFunc
 */
typedef void (*backOffTaskSuccessCallbackFunc)(void *userArg);

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
uint32_t createBackOffRepeatingTask(uint64_t initDelayAmount, uint64_t maxDelayAmount, uint64_t incrementDelayAmount,
                                    delayUnits units, backOffTaskRunCallbackFunc runFunc,
                                    backOffTaskSuccessCallbackFunc cleanFunc, void *userArg);

/*
 * change the delayAmount for a traditional repeating or fixed task.  if the 'changeNow' flag
 * is true, then it will reset the current pause time and start again.  otherwise
 * this will not apply until the next pause.
 */
void changeRepeatingTask(uint32_t taskHandle, uint32_t delayAmount, delayUnits units, bool changeNow);

/*
 * cancel the traditional repeating task. returns the original 'arg' supplied
 * during the creation - allowing cleanup to safely occur.
 * WARNING:
 * this should NOT be called while holding a lock that the task
 * function can also hold or a deadlock might result
 */
void *cancelRepeatingTask(uint32_t taskHandle);


//-----------------------------------------------
//
// 'Policy Based' Repeating Tasks
//
//-----------------------------------------------

// pre-define the Repeating Task Policy object
typedef struct _repeatingTaskPolicy RepeatingTaskPolicy;

/*
 * function to run after the delay time has expired.
 *
 * @param userArg - optional input variable
 * @return - true to end the repeating task, false to keep going
 */
typedef bool (*repeatingTaskRunFunc)(void *userArg);

/*
 * cleanup resources associated with the repeating task.
 * called when the task completes or is canceled.
 *
 * @param taskHandle - the handle of the task that is exiting
 * @param userArg - optional input variable that needs to be destroyed
 * @param policy - the task policy to destroy (see destroyRepeatingPolicy)
 */
typedef void (*destroyRepeatingTaskObjectsFunc)(uint32_t taskHandle, void *userArg, RepeatingTaskPolicy *policy);

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
                                   void *userArg);

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
bool resetPolicyRepeatingTask(uint32_t taskHandle, RepeatingTaskPolicy *newPolicy, bool changeNow);

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
bool cancelPolicyRepeatingTask(uint32_t taskHandle);


//-----------------------------------------------
//
// Functions for both Traditional and Policy-based
//
//-----------------------------------------------

/*
 * cancels the delay of the repeating task with this taskHandle,
 * forcing the task to execute immediately
 *
 * @param taskHandle - handle of the repeating task to short-circuit
 */
void shortCircuitRepeatingTask(uint32_t taskHandle);


//-----------------------------------------------
//
// Policy definition and common implementations
//
//-----------------------------------------------


// called to determine the amount of time to delay before executing
// the next iteration of a repeating task.  the implementation must
// populate the timespec with the amount of time to delay (not factoring
// in the current time).
// @param delayContext - ctx of the poliicy
// @param whenToRun - timespec to populate
typedef void (*getDelayForNextRunFunc)(void *delayContext, struct timespec *whenToRun);

// if defined, called immediately BEFORE the task executes.
// potentially allows policy objects to make delay adjustments
// based on when the task last executed - or to allow tracking
// of execution duration
// @param delayContext - ctx of the poliicy
typedef void (*taskPreRunNotifyFunc)(void *delayContext);

// if defined, called immediately AFTER the task executes.
// potentially allows policy objects to make delay adjustments
// based on when the task last executed - or to allow tracking
// of execution duration
// @param delayContext - ctx of the poliicy
typedef void (*taskPostRunNotifyFunc)(void *delayContext);

// destroy the delayContext associated with this policy
// @param delayContext - ctx of the poliicy
typedef void (*destroyDelayContextFunc)(void *delayContext);

/*
 * The "repeating task policy" object, which is required when creating Policy-based
 * repeating tasks.  This structure is defined to allow for custom policy objects.
 */
struct _repeatingTaskPolicy
{
    void    *policyContext;                      // optional data used by the policy implementation
    getDelayForNextRunFunc   getDelayFunc;       // REQUIRED
    taskPreRunNotifyFunc     preRunNotifyFunc;   // optional
    taskPostRunNotifyFunc    postRunNotifyFunc;  // optional
    destroyDelayContextFunc  destroyContextFunc; // optional - if not defined, will use "free(policyContext)"
};

/*
 * Construct an empty RepeatingTaskPolicy
 */
RepeatingTaskPolicy *createRepeatingPolicy(void);

/*
 * Destroy a RepeatingTaskPolicy object and the policyContext via
 * policy->destroyContextFunc() or free()
 */
void destroyRepeatingPolicy(RepeatingTaskPolicy *policy);

/*
 * Create a repeating task policy to pause 'delayAmount' after
 * each execution of the task.
 *
 * @param delayAmount - number of 'units' to delay
 * @param units - amount of time 'delayAmount' represents
 */
RepeatingTaskPolicy *createStandardRepeatingTaskPolicy(uint64_t delayAmount, delayUnits units);

/*
 * Create a repeating task policy to pause 'startDelayAmount', then execute.  From that
 * point forward, the delay will switch to 'delayAmount' between executions of the task.
 *
 * @param startDelayAmount - the number of `units` to delay for the initial run
 * @param delayAmount - number of 'units' to delay
 * @param units - amount of time 'delayAmount' represents
 */
RepeatingTaskPolicy *createRepeatingTaskWithStartDelayPolicy(uint64_t startDelayAmount,
                                                             uint64_t delayAmount,
                                                             delayUnits units);

/*
 * Create a repeating task policy to execute the task on a fixed interval
 * (regardless of the amount of time it takes to execute the task).  This
 * is commonly used for situations where each execution must start exactly
 * X "units" apart (i.e. every minute)
 *
 * @param delayAmount - number of 'units' to delay
 * @param units - amount of time 'delayAmount' represents
 */
RepeatingTaskPolicy *createFixedRepeatingTaskPolicy(uint64_t delayAmount, delayUnits units);

/*
 * Create a repeating task policy to execute the task using random intervals,
 * which is bound between the 'minDelayAmount' and 'maxDelayAmount'.  Each delay
 * will regenerate the random number.
 *
 * @param minDelayAmount - the min delay amount for the randomization
 * @param maxDelayAmount - the max delay amount for the randomization
 * @param units - amount of time each 'amount' represents
 */
RepeatingTaskPolicy *createRandomRepeatingTaskPolicy(uint32_t minDelayAmount, uint32_t maxDelayAmount, delayUnits units);

/*
 * Create a repeating task policy to execute the task using an incremental backoff interval.
 * The initial delay will be 'initDelayAmount', and the next delay will add 'incrementDelayAmount'.
 * This pattern will continue until the delay reaches 'maxDelayAmount'
 *
 * @param initDelayAmount - the delay amount after the 1st pass
 * @param maxDelayAmount - the max delay amount to be reached
 * @param incrementDelayAmount - the delay amount to be increased by each iteration
 * @param units - amount of time each 'amount' represents
 *
 */
RepeatingTaskPolicy *createIncrementalRepeatingTaskPolicy(uint64_t initDelayAmount, uint64_t maxDelayAmount,
                                                          uint64_t incrementDelayAmount, delayUnits units);

/*
 * Create a repeating task policy to execute the task using an exponential backoff interval.
 * The first run of the task will occur immediately, and if the task returned false and needs
 * to be repeated, then every subsequent run of the task will have a calculated delay.
 * These delays are determined by defining the 'delayBase', then exponentiating the 'delayBase'.
 * The exponent is incremented upon each execution of the task, so the delay will be
 * exponentially larger upon each execution. This pattern will continue until the delay
 * reaches 'maxDelayAmount'.
 *
 * Example: If the delayBase is set to 3 and the max delay is set to 100, the delays would be:
 *          3, 9, 27, 81, 100, 100, ...
 *
 * @param delayBase - the base of the exponentiated delay
 * @param maxDelayAmount - the max delay amount to be reached
 * @param units - amount of time each 'amount' represents
 *
 */
RepeatingTaskPolicy *createExponentialRepeatingTaskPolicy(uint8_t delayBase, uint64_t maxDelayAmount, delayUnits units);


#endif // IC_REPEATTASK_H
