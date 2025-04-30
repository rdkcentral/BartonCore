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
