/*
 * timedWait.h
 *
 *  Created on: Oct 13, 2010
 *      Author: gfaulkner
 */

#ifndef TIMEDWAIT_H_
#define TIMEDWAIT_H_
#include <pthread.h>
#include <stdint.h>

/**
 * initialize a conditional for use in a timed wait
 */
extern int initTimedWaitCond(pthread_cond_t *cond);

/**
 * does a timed wait based on the monotonic clock (if supported).
 * The cond parm should have been initialized using initTimedWaitCond().
 * The mutex should already be held by the caller
 *
 * @see initTimedWaitCond()
 */
extern int incrementalCondTimedWait(pthread_cond_t *cond, pthread_mutex_t *mtx, int timeoutSecs);

/**
 * does a timed wait based on the monotonic clock (if supported).
 * The cond parm should have been initialized using initTimedWaitCond().
 * The mutex should already be held by the caller
 *
 * @see initTimedWaitCond()
 */
extern int incrementalCondTimedWaitMillis(pthread_cond_t *cond, pthread_mutex_t *mtx, uint64_t timeoutMillis);


/**
 * does a timed wait based on the monotonic clock.  The cond parm should have been initialized using
 * initTimedWaitCond().  The mutex should already be held by the caller.  So, these waits should be considered
 * relative.  If you want an absolute wait, you need to calculate the timeoutSecs accordingly.
 * This one allows for more granular control (includes nanoseconds)
 */
extern int granularIncrementalCondTimedWait(pthread_cond_t *cond, pthread_mutex_t *mtx, struct timespec *waitTime);

#endif /* TIMEDWAIT_H_ */
