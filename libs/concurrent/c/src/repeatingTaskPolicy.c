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
// iControl Networks retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*-----------------------------------------------
 * retryTaskPolicy.c
 *
 * function implementations for commonly used RepeatingTaskPolicy
 *
 * Author: jelderton - 5/6/22
 *-----------------------------------------------*/

#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#include <icLog/logging.h>
#include <icConcurrent/repeatingTask.h>
#include <icTime/timeUtils.h>
#include <icUtil/numberUtils.h>

/*
 * policy object definitions
 */

// basic policy context
typedef struct {
    delayUnits  units;                  // The unit of time to use (min, secs, etc.)
    uint64_t    currentDelay;           // the delay amount, might change depending on the task
} standardPolicyContext;

// start-delay policy context
typedef struct {
    delayUnits  units;                  // The unit of time to use (min, secs, etc.)
    uint64_t    startDelay;             // the delay amount, might change depending on the task
    uint64_t    currentDelay;           // the delay amount, might change depending on the task
    bool        ranOnce;                // indicator to switch from 'start' to 'current' delay
} startDelayPolicyContext;

// fixed policy context
typedef struct {
    delayUnits  units;                  // The unit of time to use (min, secs, etc.)
    uint64_t    currentDelay;           // the delay amount, might change depending on the task
    bool        ranOnce;
    struct timespec  lastTimeRan;       // last time this was executed
} fixedPolicyContext;

// random policy context
typedef struct {
    delayUnits  units;                  // The unit of time to use (min, secs, etc.)
    uint32_t    minRange;
    uint32_t    maxRange;
    uint64_t    lastValueUsed;          // large enough to hold the rand number
} randomPolicyContext;

// incremental backoff policy context
typedef struct {
    delayUnits  units;                  // The unit of time to use (min, secs, etc.)
    uint64_t    currentDelay;           // the delay amount, might change depending on the task
    uint64_t    startDelay;             // the the delay amount after 1st run
    uint64_t    maxDelay;               // the max delay amount to hit
    uint64_t    incrementDelay;         // the amount delay amount will increase after each iteration
} incrementalPolicyContext;

// exponential backoff policy context
typedef struct {
    delayUnits  units;                  // The unit of time to use (min, secs, etc.)
    uint64_t    attempts;               // the number of attempts made
    uint64_t    currentDelay;           // the delay amount, might change depending on the task
    uint8_t     base;                   // the base of the exponentiated delay
    uint64_t    maxDelay;               // the max delay amount to hit
} exponentialPolicyContext;


//-----------------------------------------------
//
// base constructor/destructor for policy objets
//
//-----------------------------------------------

/*
 * Construct an empty RepeatingTaskPolicy
 */
RepeatingTaskPolicy *createRepeatingPolicy(void)
{
    return (RepeatingTaskPolicy *)calloc(1, sizeof(RepeatingTaskPolicy));
}

/*
 * Destroy a RepeatingTaskPolicy object and it's policyContext via
 * policy->destroyContextFunc() or free()
 */
void destroyRepeatingPolicy(RepeatingTaskPolicy *policy)
{
    // first destroy the policy's context
    if (policy != NULL)
    {
        if (policy->destroyContextFunc != NULL)
        {
            // use policy destroy function to free the context
            policy->destroyContextFunc(policy->policyContext);
        }
        else
        {
            // standard free
            free(policy->policyContext);
        }
        policy->policyContext = NULL;
    }

    // now the policy object
    free(policy);
}

/*
 * Given a delay and units convert it to a timespec representing the delay
 */
static void populateTimespecDelay(uint64_t delay, delayUnits units, struct timespec *delaySpec)
{
    if (units == DELAY_HOURS)
    {
        delaySpec->tv_sec = 60 * 60 * delay;
        delaySpec->tv_nsec = 0;
    }
    else if (units == DELAY_MINS)
    {
        delaySpec->tv_sec = 60 * delay;
        delaySpec->tv_nsec = 0;
    }
    else if (units == DELAY_SECS)
    {
        delaySpec->tv_sec = delay;
        delaySpec->tv_nsec = 0;
    }
    else
    {
        // Convert from millis
        delaySpec->tv_sec = delay / 1000;
        delaySpec->tv_nsec = (delay % 1000) * 1000000;
    }
}

//-----------------------------------------------
//
// standard policy functions
//
//-----------------------------------------------

/*
 * getDelayForNextRunFunc impl for 'standard repeating task policy'
 */
static void standardGetDelayForNextRunFunc(void *delayContext, struct timespec *whenToRun)
{
    standardPolicyContext *ctx = (standardPolicyContext *)delayContext;
    populateTimespecDelay(ctx->currentDelay, ctx->units, whenToRun);
}

/*
 * destroyDelayContextFunc impl for 'standard repeating task policy'
 */
static void standardDestroyContextFunc(void *delayContext)
{
    // nothing fancy here, the standardPolicyContext is simple
    free(delayContext);
}

/*
 * Create a repeating task policy to pause 'delayAmount' after
 * each execution of the task.
 *
 * @param delayAmount - number of 'units' to delay
 * @param units - amount of time 'delayAmount' represents
 */
RepeatingTaskPolicy *createStandardRepeatingTaskPolicy(uint64_t delayAmount, delayUnits units)
{
    // first the policy and supporting functions
    RepeatingTaskPolicy *retVal = createRepeatingPolicy();
    retVal->getDelayFunc = standardGetDelayForNextRunFunc;
    retVal->destroyContextFunc = standardDestroyContextFunc;

    // now add the context to store our input variables and state information
    standardPolicyContext *ctx = (standardPolicyContext *)calloc(1, sizeof(standardPolicyContext));
    ctx->currentDelay = delayAmount;
    ctx->units = units;
    retVal->policyContext = ctx;

    return retVal;
}

//-----------------------------------------------
//
// start-delay policy functions
//
//-----------------------------------------------

/*
 * getDelayForNextRunFunc impl for 'start-delay repeating task policy'
 */
static void startDelayCalcNextRunFunc(void *delayContext, struct timespec *whenToRun)
{
    startDelayPolicyContext *ctx = (startDelayPolicyContext *)delayContext;

    // if this is the first run, use 'startDelay' otherwise 'currentDelay'
    if (ctx->ranOnce == false)
    {
        ctx->ranOnce = true;
        populateTimespecDelay(ctx->startDelay, ctx->units, whenToRun);
    }
    else
    {
        populateTimespecDelay(ctx->currentDelay, ctx->units, whenToRun);
    }
}

/*
 * destroyDelayContextFunc impl for 'start-delay repeating task policy'
 */
static void startDelayDestroyContextFunc(void *delayContext)
{
    // nothing fancy here, the context is simple
    free(delayContext);
}

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
                                                             delayUnits units)
{
    // first the policy and supporting functions
    RepeatingTaskPolicy *retVal = createRepeatingPolicy();
    retVal->getDelayFunc = startDelayCalcNextRunFunc;
    retVal->destroyContextFunc = startDelayDestroyContextFunc;

    // now add the context to store our input variables and state information
    startDelayPolicyContext *ctx = (startDelayPolicyContext *)calloc(1, sizeof(startDelayPolicyContext));
    ctx->startDelay = startDelayAmount;
    ctx->currentDelay = delayAmount;
    ctx->units = units;
    ctx->ranOnce = false;
    retVal->policyContext = ctx;

    return retVal;
}

//-----------------------------------------------
//
// fixed policy functions
//
//-----------------------------------------------

/*
 * getDelayForNextRunFunc impl for 'fixed repeating task policy'
 */
static void fixedGetDelayForNextRunFunc(void *delayContext, struct timespec *whenToRun)
{
    fixedPolicyContext *ctx = (fixedPolicyContext *)delayContext;

    // if this is the first run, just return the millis for the delay
    if (ctx->ranOnce == false)
    {
        populateTimespecDelay(ctx->currentDelay, ctx->units, whenToRun);
        return;
    }

    // add 'delay' to the last time we ran
    struct timespec when;
    struct timespec delay;
    struct timespec now;
    populateTimespecDelay(ctx->currentDelay, ctx->units, &delay);
    timespecAdd(&(ctx->lastTimeRan), &delay, &when);

    // return difference between now and whenToRun (in millis)
    getCurrentTime(&now, true);
    timespecDiff(&when, &now, whenToRun);
}

/*
 * taskPreRunNotifyFunc impl for 'fixed repeating task policy'
 */
static void fixedPreRunNotifyFunc(void *delayContext)
{
    // save current time so we can calculate when to run again
    fixedPolicyContext *ctx = (fixedPolicyContext *)delayContext;
    getCurrentTime(&ctx->lastTimeRan, true);
}

/*
 * taskPostRunNotifyFunc impl for 'fixed repeating task policy'
 */
static void fixedPostRunNotifyFunc(void *delayContext)
{
    // update ranOnce flag
    fixedPolicyContext *ctx = (fixedPolicyContext *)delayContext;
    ctx->ranOnce = true;
}

/*
 * destroyDelayContextFunc impl for 'fixed repeating task policy'
 */
static void fixedDestroyContextFunc(void *delayContext)
{
    // nothing fancy here, the fixedPolicyContext is simple
    free(delayContext);
}

/*
 * Create a repeating task policy to execute the task on a fixed interval
 * (regardless of the amount of time it takes to execute the task).  This
 * is commonly used for situations where each execution must start exactly
 * X "units" apart (i.e. every minute)
 *
 * @param delayAmount - number of 'units' to delay
 * @param units - amount of time 'delayAmount' represents
 */
RepeatingTaskPolicy *createFixedRepeatingTaskPolicy(uint64_t delayAmount, delayUnits units)
{
    // first the policy and supporting functions
    RepeatingTaskPolicy *retVal = createRepeatingPolicy();
    retVal->getDelayFunc = fixedGetDelayForNextRunFunc;
    retVal->preRunNotifyFunc = fixedPreRunNotifyFunc;
    retVal->postRunNotifyFunc = fixedPostRunNotifyFunc;
    retVal->destroyContextFunc = fixedDestroyContextFunc;

    // now add the context to store our input variables and state information
    fixedPolicyContext *ctx = (fixedPolicyContext *)calloc(1, sizeof(fixedPolicyContext));
    ctx->currentDelay = delayAmount;
    ctx->units = units;
    ctx->ranOnce = false;
    retVal->policyContext = ctx;

    return retVal;
}

//-----------------------------------------------
//
// random policy functions
//
//-----------------------------------------------

/*
 * getDelayForNextRunFunc impl for 'random repeating task policy'
 */
static void randomGetDelayForNextRunFunc(void *delayContext, struct timespec *whenToRun)
{
    randomPolicyContext *ctx = (randomPolicyContext *)delayContext;
    uint64_t randNum = 0;
    if (generateRandomNumberInRange(ctx->minRange, ctx->maxRange, &randNum) == true)
    {
        // save as our 'lastUsed'
        ctx->lastValueUsed = randNum;
    }
    else
    {
        // log warning? use the same value we had last time
        icLogWarn("randPolicy", "%s unable to obtain random number.  using %"PRIu64, __FUNCTION__, ctx->lastValueUsed);
    }

    // basic math of millisecond representation of the amount in units
    populateTimespecDelay(randNum, ctx->units, whenToRun);
}

/*
 * destroyDelayContextFunc impl for 'standard repeating task policy'
 */
static void randomDestroyContextFunc(void *delayContext)
{
    // nothing fancy here, the randomPolicyContext is simple
    free(delayContext);
}

/*
 * Create a repeating task policy to execute the task using random intervals,
 * which is bound between the 'minDelayAmount' and 'maxDelayAmount'.  Each delay
 * will regenerate the random number.
 *
 * @param minDelayAmount - the min delay amount for the randomization
 * @param maxDelayAmount - the max delay amount for the randomization
 * @param units - amount of time each 'amount' represents
 */
RepeatingTaskPolicy *createRandomRepeatingTaskPolicy(uint32_t minDelayAmount, uint32_t maxDelayAmount, delayUnits units)
{
    // first the policy and supporting functions
    RepeatingTaskPolicy *retVal = createRepeatingPolicy();
    retVal->getDelayFunc = randomGetDelayForNextRunFunc;
    retVal->destroyContextFunc = randomDestroyContextFunc;

    // now add the context to store our input variables and state information
    randomPolicyContext *ctx = (randomPolicyContext *)calloc(1, sizeof(randomPolicyContext));
    ctx->units = units;
    ctx->minRange = minDelayAmount;
    ctx->maxRange = maxDelayAmount;
    ctx->lastValueUsed = minDelayAmount;    // safeguard if random num generation fails
    retVal->policyContext = ctx;

    return retVal;
}

//-----------------------------------------------
//
// incremental policy functions
//
//-----------------------------------------------

/*
 * getDelayForNextRunFunc impl for 'incremental repeating task policy'
 */
static void incrementalGetDelayForNextRunFunc(void *delayContext, struct timespec *whenToRun)
{
    incrementalPolicyContext *ctx = (incrementalPolicyContext *)delayContext;
    populateTimespecDelay(ctx->currentDelay, ctx->units, whenToRun);
}

/*
 * taskPostRunNotifyFunc impl for 'incremental repeating task policy'
 */
static void incrementalPostRunNotifyFunc(void *delayContext)
{
    incrementalPolicyContext *ctx = (incrementalPolicyContext *)delayContext;

    if (ctx->currentDelay == 0)
    {
        // special case of first delay (incorporate 'inital delay')
        ctx->currentDelay = ctx->startDelay;
    }
    else if ((ctx->currentDelay + ctx->incrementDelay) <= ctx->maxDelay)
    {
        // now that the task ran, increment for the next iteration
        ctx->currentDelay += ctx->incrementDelay;
    }
}

/*
 * destroyDelayContextFunc impl for 'incremental repeating task policy'
 */
static void incrementalDestroyContextFunc(void *delayContext)
{
    // nothing fancy here, the incrementalPolicyContext is simple
    free(delayContext);
}

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
                                                          uint64_t incrementDelayAmount, delayUnits units)
{
    // first the policy and supporting functions
    RepeatingTaskPolicy *retVal = createRepeatingPolicy();
    retVal->getDelayFunc = incrementalGetDelayForNextRunFunc;
    retVal->postRunNotifyFunc = incrementalPostRunNotifyFunc;
    retVal->destroyContextFunc = incrementalDestroyContextFunc;

    // now add the context to store our input variables and state information
    incrementalPolicyContext *ctx = (incrementalPolicyContext *)calloc(1, sizeof(incrementalPolicyContext));
    ctx->units = units;
    ctx->currentDelay = 0;
    ctx->startDelay = initDelayAmount;
    ctx->maxDelay = maxDelayAmount;
    ctx->incrementDelay = incrementDelayAmount;
    retVal->policyContext = ctx;

    return retVal;
}

/*
 * Given a task policy created via createIncrementalRepeatingTaskPolicy,
 * this returns a deep clone, but updates the current delay amount.
 * Solely used by changeRepeatingTask
 */
RepeatingTaskPolicy *cloneAndChangeIncrementalRepeatingTaskPolicy(RepeatingTaskPolicy *orig,
                                                                  uint32_t delayAmount, delayUnits units)
{
    RepeatingTaskPolicy *retVal = NULL;

    // need to create a new incrementalPolicyContext and transfer over as
    // much as we can from the context within 'orig'
    incrementalPolicyContext *origCtx = (incrementalPolicyContext *)orig->policyContext;
    incrementalPolicyContext *ctx = (incrementalPolicyContext *)calloc(1, sizeof(incrementalPolicyContext));
    ctx->units = units;
    ctx->currentDelay = (uint64_t) delayAmount;
    ctx->startDelay = origCtx->startDelay;
    ctx->maxDelay = origCtx->maxDelay;
    ctx->incrementDelay = origCtx->incrementDelay;

    // now create the policy
    retVal = createRepeatingPolicy();
    retVal->getDelayFunc = incrementalGetDelayForNextRunFunc;
    retVal->postRunNotifyFunc = incrementalPostRunNotifyFunc;
    retVal->destroyContextFunc = incrementalDestroyContextFunc;
    retVal->policyContext = ctx;

    return retVal;
}

//-----------------------------------------------
//
// exponential policy functions
//
//-----------------------------------------------

/*
 * getDelayForNextRunFunc impl for 'exponential repeating task policy'
 */
static void exponentialGetDelayForNextRunFunc(void *delayContext, struct timespec *whenToRun)
{
    exponentialPolicyContext *ctx = (exponentialPolicyContext *)delayContext;
    populateTimespecDelay(ctx->currentDelay, ctx->units, whenToRun);
}

/*
 * taskPostRunNotifyFunc impl for 'exponential repeating task policy'
 */
static void exponentialPostRunNotifyFunc(void *delayContext)
{
    exponentialPolicyContext *ctx = (exponentialPolicyContext *)delayContext;
    ctx->attempts++;

    if (ctx->currentDelay != ctx->maxDelay)
    {
        uint64_t nextDelay = ctx->currentDelay;

        if (nextDelay < ctx->maxDelay)
        {
            nextDelay = pow(ctx->base, ctx->attempts);
        }

        if (nextDelay > ctx->maxDelay)
        {
            nextDelay = ctx->maxDelay;
        }

        ctx->currentDelay = nextDelay;
    }
}

/*
 * destroyDelayContextFunc impl for 'exponential repeating task policy'
 */
static void exponentialDestroyContextFunc(void *delayContext)
{
    // nothing fancy here, the exponentialPolicyContext is simple
    free(delayContext);
}

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
RepeatingTaskPolicy *createExponentialRepeatingTaskPolicy(uint8_t delayBase, uint64_t maxDelayAmount, delayUnits units)
{
    // first the policy and supporting functions
    RepeatingTaskPolicy *retVal = createRepeatingPolicy();
    retVal->getDelayFunc = exponentialGetDelayForNextRunFunc;
    retVal->postRunNotifyFunc = exponentialPostRunNotifyFunc;
    retVal->destroyContextFunc = exponentialDestroyContextFunc;

    // now add the context to store our input variables and state information
    exponentialPolicyContext *ctx = (exponentialPolicyContext *)calloc(1, sizeof(exponentialPolicyContext));
    ctx->units = units;
    ctx->currentDelay = 0;
    ctx->attempts = 0;
    ctx->base = delayBase;
    ctx->maxDelay = maxDelayAmount;
    retVal->policyContext = ctx;

    return retVal;
}
