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
//
// Created by mkoch201 on 7/26/19.
//

#ifndef ZILKER_ICTHREADSAFEWRAPPER_H
#define ZILKER_ICTHREADSAFEWRAPPER_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * This is a threadsafe wrapper for objects.  This wrapper is optimized for situations where modifications are
 * infrequent operations that must be eventually performed, but can be delayed for a short time.  Reads can be frequent,
 * but not constant, otherwise modifications may never apply.
 */

/**
 * An internal structure
 */
typedef struct icThreadSafeWrapperItemRef_ icThreadSafeWrapperItemRef;

/**
 * *****************************
 * Function callback definitions
 * *****************************
 */

/**
 * Get/create an instance of the item to be wrapped
 * @return the instance
 */
typedef void *(*ThreadSafeWrapperGetItemFunc)(void);

/**
 * Check whether to automatically release the wrapped item
 * @param item the item to check
 * @return true to release, false otherwise
 */
typedef bool (*ThreadSafeWrapperReleaseCheckFunc)(void *item);

/**
 * Destroy the wrapped item
 * @param item the item
 */
typedef void (*ThreadSafeWrapperDestroyItemFunc)(void *item);

/**
 * Read the wrapped item
 * @param item the item
 * @param context the context passed in
 * @see ThreadSafeWrapperReadItem
 */
typedef void (*ThreadSafeWrapperReadFunc)(const void *item, const void *context);

/**
 * Modify the wrapped item
 * @param item the item
 * @param context the context passed in
 * @see ThreadSafeWrapperEnqueueModification
 */
typedef void (*ThreadSafeWrapperModificationFunc)(void *item, void *context);

/**
 * Destroy function for the context in enqueued modifications
 * @param context the context to destroy
 * @see ThreadSafeWrapperEnqueueModification
 */
typedef void (*ThreadSafeWrapperDestroyContextFunc)(void *context);

/**
 * Structure representing an instance of this objects.  Contents should be considered opaque to consumers
 */
typedef struct
{
    // These contents are considered private and should not be accessed directly
    struct
    {
        icThreadSafeWrapperItemRef *ref;
        pthread_mutex_t mutex;
        ThreadSafeWrapperGetItemFunc autoAssignFunc;
        ThreadSafeWrapperReleaseCheckFunc autoReleaseCheckFunc;
        ThreadSafeWrapperDestroyItemFunc destroyItemFunc;
    } privateData;
} icThreadSafeWrapper;

/**
 * Static/stack initializer for icThreadSafeWrapper
 * @param autoAssignFuncArg function to create a wrapped item when the wrapper is released and a modification call is
 * made. If NULL is passed no auto assign is done
 * @param autoReleaseCheckFunc function to check whether to release the wrapped item ref after modifications are
 * applied.  If NULL is passed no auto release is ever performed
 * @param destroyItemFunc function to destroy the wrapped item.  If NULL is passed, then free is used
 */
#define THREAD_SAFE_WRAPPER_INIT(autoAssignFuncArg, autoReleaseCheckFuncArg, destroyItemFuncArg)                       \
    {                                                                                                                  \
        {                                                                                                              \
            .ref = NULL, .mutex = PTHREAD_MUTEX_INITIALIZER, .autoAssignFunc = autoAssignFuncArg,                      \
            .autoReleaseCheckFunc = autoReleaseCheckFuncArg, .destroyItemFunc = destroyItemFuncArg                     \
        }                                                                                                              \
    }

/**
 * Structure representing a future for enqueued writes.  Contents should be considered opaque to consumers
 */
typedef struct
{
    struct
    {
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        bool condInitialized;
        bool complete;
        bool applied;
    } privateData;
} icThreadSafeWrapperFuture;

/**
 * Thread safe wrapper future static initializer
 */
#define THREAD_SAFE_WRAPPER_FUTURE_INIT                                                                                \
    {                                                                                                                  \
        {                                                                                                              \
            .mutex = PTHREAD_MUTEX_INITIALIZER, .condInitialized = false, .complete = false, .applied = false          \
        }                                                                                                              \
    }

/**
 * Manually assign an item to the wrapper.
 * @param wrapper the wrapper instance
 * @param itemToWrap the item to wrap
 * @return true if successfully assigned, false if not assigned(because there is already an item assigned)
 */
bool threadSafeWrapperAssignItem(icThreadSafeWrapper *wrapper, void *itemToWrap);

/**
 * Manually assign only if already released in an atomic manner
 * @param wrapper the wrapper instance
 * @param getItemFunc the function used to get/create the item. If NULL then the autoAssignFunc set during
 * initialization is used
 * @return true if assignment was performed
 */
bool threadSafeWrapperAssignItemIfReleased(icThreadSafeWrapper *wrapper, ThreadSafeWrapperGetItemFunc getItemFunc);

/**
 * Manually release an item if one is assigned.  This can be called while there are concurrent reads happening.
 * @param wrapper the wrapper instance
 */
void threadSafeWrapperReleaseItem(icThreadSafeWrapper *wrapper);

/**
 * Conditionally release an item in an atomic manner.  This can be called while there are concurrent reads happening.
 * @param wrapper the wrapper instance
 * @param releaseCheckFunc the function to use check whether to release.  If NULL then the autoReleaseCheckFunc set
 * during initialization is used
 * @return true if the release was performed
 */
bool threadSafeWrapperConditionalReleaseItem(icThreadSafeWrapper *wrapper,
                                             ThreadSafeWrapperReleaseCheckFunc releaseCheckFunc);

/**
 * Perform a read on the item
 * @param wrapper the wrapper instance
 * @param readFunc the callback function to perform the read
 * @param context the context argument to be passed to the read callback function
 * @return true if the item was read
 */
bool threadSafeWrapperReadItem(icThreadSafeWrapper *wrapper, ThreadSafeWrapperReadFunc readFunc, void *context);

/**
 * Enqueue a modification request to be performed on the wrapped item.  The modification will be performed once there
 * are no active readers.  Note that this means if readers are highly active modifications could be delayed indefinitely
 * @param wrapper the wrapper instance
 * @param modificationFunc the callback function to perform the modification
 * @param context a context argument to be passed to the modification callback function
 * @param destroyContextFunc a function to destroy the context object once it is no longer need.  If NULL is passed free
 * @param future an optional future with which to wait on the modification.  Can be NULL.
 * will be used.
 * @return true if the modification was successfully enqueued(or applied)
 */
bool threadSafeWrapperEnqueueModification(icThreadSafeWrapper *wrapper,
                                          ThreadSafeWrapperModificationFunc modificationFunc,
                                          void *context,
                                          ThreadSafeWrapperDestroyContextFunc destroyContextFunc,
                                          icThreadSafeWrapperFuture *future);

/**
 * Block and perform a modification once no readers are present.  This could block indefinitely
 * @param wrapper the wrapper instance
 * @param modificationFunc the callback function to perform the modification
 * @param context a context argument to be passed to the modification callback function
 * @return true if the modification was successfully performed
 */
bool threadSafeWrapperModifyItem(icThreadSafeWrapper *wrapper,
                                 ThreadSafeWrapperModificationFunc modificationFunc,
                                 void *context);

/**
 * Helper for context items which do not require any cleanup
 * @param context the context
 */
void threadSafeWrapperDoNotFreeContextFunc(void *context);

/**
 * Initializer if future is not statically initialized
 * @param future the future
 */
void threadSafeWrapperFutureInit(icThreadSafeWrapperFuture *future);

/**
 * Cleanup if future is not statically initialized.  This should not be called on a future which is not complete yet.
 * Note caller still owns the memory for the future and must free it.
 * @param future the future
 */
void threadSafeWrapperFutureDestroy(icThreadSafeWrapperFuture *future);

/**
 * Wait for a future to complete
 * @param future the future to wait on
 * @param timeoutSecs the timeout in seconds
 * @return true if the future is complete, false otherwise
 */
bool threadSafeWrapperFutureAwait(icThreadSafeWrapperFuture *future, uint32_t timeoutSecs);

/**
 * Check if a future is complete
 * @param future the future
 * @return true if complete, false otherwise
 */
bool threadSafeWrapperFutureIsComplete(icThreadSafeWrapperFuture *future);

/**
 * Check if a future was applied
 * @param future the future
 * @return true if applied, false otherwise
 */
bool threadSafeWrapperFutureIsApplied(icThreadSafeWrapperFuture *future);

#endif // ZILKER_ICTHREADSAFEWRAPPER_H
