//------------------------------ tabstop = 4 ----------------------------------
// 
// Copyright (C) 2019 Comcast
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
// Created by mkoch201 on 7/26/19.
//

#include "icConcurrent/icThreadSafeWrapper.h"

#include <stdint.h>
#include <icTypes/icLinkedList.h>
#include <icConcurrent/icThreadSafeWrapper.h>
#include <icConcurrent/timedWait.h>

/**
 * The main structure of this is a icThreadSafeWrapper object, which contains a icThreadSafeWrapperItemRef object that
 * can be detached to support full thread safety. The mutex in the wrapper protects all data(readerCount,
 * pendingModifications, and the ref itself), even when the ref is released(detached) from the main wrapped object.
 */

// Supports a couple of different modification strategies:
// 1) Sync Modification - modifications wait on reads, reads can be concurrent.  Essentially a r/w lock around the data
// 2) Async Modification - modifications are asynchronous.  An optional future can be used to await on completion and
// detect whether the modification was applied or discarded(because the item was released)
// TODO: Could add a 3rd modification strategy
// 3) Copy on Modification - modifications cause a copy to be created(releasing the current ref, cloning it, and then
//                           attaching the cloned data as the new ref).  This way modifications happen immediately, but
//                           at a cost of the clone.

/**
 * Is the structure that contains the wrapped item.  This is separate from the main wrapper so it can be detached
 * from the main object, and then be destroyed once the readerCount is back to 0.
 */
struct icThreadSafeWrapperItemRef_
{
    void *wrappedItem;
    uint16_t readerCount;
    // current strategy has the modifications stored with the ref, and as such if the item is released the modifications
    // are discarded.  Might need to revisit this an just apply the modifications on whatever ref(if any) is present at
    // the time we can write.  This might trigger an unexpected auto-assignment.  Not sure which behavior would be
    // more desirable
    icLinkedList *pendingModifications;
};

/**
 * Stores info about pending modifications
 */
typedef struct
{
    ThreadSafeWrapperModificationFunc modificationFunc;
    void *context;
    ThreadSafeWrapperDestroyContextFunc destroyContextFunc;
    icThreadSafeWrapperFuture *future;
} PendingModificationInfo;

static icThreadSafeWrapperItemRef *createRef(void *itemToWrap);

static void destroyRef(icThreadSafeWrapperItemRef *ref, ThreadSafeWrapperDestroyItemFunc destroyFunc);

static icThreadSafeWrapperItemRef *getRef(icThreadSafeWrapper *wrapper);

static void applyModifications(icThreadSafeWrapperItemRef *ref);

static void destroyPendingModificationFunc(void *item);

static bool alwaysReleaseFunc(void *item);

static void freeModificationContext(void *context, ThreadSafeWrapperDestroyContextFunc destroyContextFunc);

static void postModificationCheckRelease(icThreadSafeWrapper *wrapper, icThreadSafeWrapperItemRef *ref);

static void setFutureComplete(PendingModificationInfo *pendingModificationInfo, bool applied);

/**
 * Manually assign an item to the wrapper.
 * @param wrapper the wrapper instance
 * @param itemToWrap the item to wrap
 * @return true if successfully assigned, false if not assigned(because there is already an item assigned)
 */
bool threadSafeWrapperAssignItem(icThreadSafeWrapper *wrapper, void *itemToWrap)
{
    bool assigned = false;
    if (wrapper != NULL && itemToWrap != NULL)
    {
        pthread_mutex_lock(&wrapper->privateData.mutex);
        if (wrapper->privateData.ref == NULL)
        {
            wrapper->privateData.ref = createRef(itemToWrap);
            assigned = true;
        }
        pthread_mutex_unlock(&wrapper->privateData.mutex);
    }

    return assigned;
}

/**
 * Manually assign only if already released in an atomic manner
 * @param wrapper the wrapper instance
 * @param getItemFunc the function used to get/create the item. If NULL then the autoAssignFunc set during
 * initialization is used
 * @return true if assignment was performed
 */
bool threadSafeWrapperAssignItemIfReleased(icThreadSafeWrapper *wrapper, ThreadSafeWrapperGetItemFunc getItemFunc)
{
    bool assigned = false;
    if (wrapper != NULL && (getItemFunc != NULL || wrapper->privateData.autoAssignFunc != NULL))
    {
        pthread_mutex_lock(&wrapper->privateData.mutex);
        // Check for a current ref
        icThreadSafeWrapperItemRef *ref = getRef(wrapper);
        if (ref == NULL)
        {
            if (getItemFunc == NULL)
            {
                getItemFunc = wrapper->privateData.autoAssignFunc;
            }
            wrapper->privateData.ref = createRef(getItemFunc());
            assigned = true;
        }
        pthread_mutex_unlock(&wrapper->privateData.mutex);
    }
    return assigned;
}

/**
 * Manually release an item if one is assigned.  This can be called while there are concurrent reads happening.
 * @param wrapper the wrapper instance
 */
void threadSafeWrapperReleaseItem(icThreadSafeWrapper *wrapper)
{
    // Just re-use logic from conditional version
    threadSafeWrapperConditionalReleaseItem(wrapper, alwaysReleaseFunc);
}

/**
 * Conditionally release an item in an atomic manner.  This can be called while there are concurrent reads happening.
 * @param wrapper the wrapper instance
 * @param releaseCheckFunc the function to use check whether to release
 * @return true if the release was performed
 */
bool threadSafeWrapperConditionalReleaseItem(icThreadSafeWrapper *wrapper, ThreadSafeWrapperReleaseCheckFunc releaseCheckFunc)
{
    bool released = false;
    if (wrapper != NULL && (releaseCheckFunc != NULL || wrapper->privateData.autoReleaseCheckFunc != NULL))
    {
        pthread_mutex_lock(&wrapper->privateData.mutex);
        // Get the current reference
        icThreadSafeWrapperItemRef *ref = getRef(wrapper);
        if (ref != NULL)
        {
            if (releaseCheckFunc == NULL)
            {
                releaseCheckFunc = wrapper->privateData.autoReleaseCheckFunc;
            }
            // Make the callback to determine if we should release
            if (releaseCheckFunc(ref->wrappedItem) == true)
            {
                // If no readers we can go ahead and destroy
                if (ref->readerCount == 0)
                {
                    destroyRef(ref, wrapper->privateData.destroyItemFunc);
                }
                // release our reference
                wrapper->privateData.ref = NULL;
                released = true;
            }
        }

        pthread_mutex_unlock(&wrapper->privateData.mutex);
    }

    return released;
}

/**
 * Perform a read on the item
 * @param wrapper the wrapper instance
 * @param readFunc the callback function to perform the read
 * @param context the context argument to be passed to the read callback function
 * @return true if the item was read
 */
bool threadSafeWrapperReadItem(icThreadSafeWrapper *wrapper, ThreadSafeWrapperReadFunc readFunc, void *context)
{
    bool didRead = false;
    if (wrapper != NULL && readFunc != NULL)
    {
        pthread_mutex_lock(&wrapper->privateData.mutex);
        // Get our current reference, if one exists
        icThreadSafeWrapperItemRef *ref = getRef(wrapper);
        if (ref != NULL)
        {
            // Simply increase our ref count
            ++ref->readerCount;
            // Release the lock while we read
            pthread_mutex_unlock(&wrapper->privateData.mutex);

            // Perform the read
            readFunc(ref->wrappedItem, context);
            didRead = true;

            // Take the lock again
            pthread_mutex_lock(&wrapper->privateData.mutex);
            // Decrement and check for modifications if no more refs
            if (--ref->readerCount == 0)
            {
                // Check to see if it got released while we were reading
                if (ref != wrapper->privateData.ref)
                {
                    // Its already been released, destroy it.
                    // coverity[use] ref is only stale when privateData.ref is NULLed by auto-release
                    destroyRef(ref, wrapper->privateData.destroyItemFunc);
                }
                else if (ref->pendingModifications != NULL)
                {
                    // coverity[use] ref is only stale when privateData.ref is NULLed by auto-release
                    applyModifications(ref);
                    postModificationCheckRelease(wrapper, ref);
                }
            }
        }
        pthread_mutex_unlock(&wrapper->privateData.mutex);
    }

    return didRead;
}

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
bool
threadSafeWrapperEnqueueModification(icThreadSafeWrapper *wrapper, ThreadSafeWrapperModificationFunc modificationFunc,
                                  void *context,
                                  ThreadSafeWrapperDestroyContextFunc destroyContextFunc,
                                  icThreadSafeWrapperFuture *future)
{
    bool didEnqueue = false;
    if (wrapper != NULL && modificationFunc != NULL)
    {
        pthread_mutex_lock(&wrapper->privateData.mutex);
        icThreadSafeWrapperItemRef *ref = getRef(wrapper);
        // No existing ref, create if needed
        if (ref == NULL && wrapper->privateData.autoAssignFunc != NULL)
        {
            wrapper->privateData.ref = createRef(wrapper->privateData.autoAssignFunc());
            ref = wrapper->privateData.ref;
        }
        if (ref != NULL)
        {
            // Check if we can just apply now
            if (ref->readerCount == 0)
            {
                // Just apply
                modificationFunc(ref->wrappedItem, context);
                // Check for auto release
                postModificationCheckRelease(wrapper, ref);
                // Cleanup
                freeModificationContext(context, destroyContextFunc);
                // Handle updating future
                if (future != NULL)
                {
                    future->privateData.complete = true;
                    future->privateData.applied = true;
                }
                didEnqueue = true;
            }
            else
            {
                // Enqueue it for later
                if (wrapper->privateData.ref->pendingModifications == NULL)
                {
                    wrapper->privateData.ref->pendingModifications = linkedListCreate();
                }
                PendingModificationInfo *pendingModificationInfo = (PendingModificationInfo *) calloc(1,
                                                                                                      sizeof(PendingModificationInfo));
                pendingModificationInfo->modificationFunc = modificationFunc;
                pendingModificationInfo->context = context;
                pendingModificationInfo->destroyContextFunc = destroyContextFunc;
                pendingModificationInfo->future = future;
                didEnqueue = linkedListAppend(wrapper->privateData.ref->pendingModifications, pendingModificationInfo);
            }
        }
        pthread_mutex_unlock(&wrapper->privateData.mutex);
    }

    if (didEnqueue == false)
    {
        freeModificationContext(context, destroyContextFunc);
    }

    return didEnqueue;
}

/**
 * Block and perform a modification once no readers are present.  This could block indefinitely
 * @param wrapper the wrapper instance
 * @param modificationFunc the callback function to perform the modification
 * @param context a context argument to be passed to the modification callback function
 * @return true if the modification was successfully performed
 */
bool
threadSafeWrapperModifyItem(icThreadSafeWrapper *wrapper, ThreadSafeWrapperModificationFunc modificationFunc,
                            void *context)
{
    bool didModify = false;

    if (wrapper != NULL && modificationFunc != NULL)
    {
        icThreadSafeWrapperFuture future = THREAD_SAFE_WRAPPER_FUTURE_INIT;
        if (threadSafeWrapperEnqueueModification(wrapper, modificationFunc, context,
                                                 threadSafeWrapperDoNotFreeContextFunc,
                                                 &future) == true)
        {
            // Wait for it to complete
            while (threadSafeWrapperFutureAwait(&future, 10) == false) {}
            // Return whether it was actually applied
            didModify = threadSafeWrapperFutureIsApplied(&future);
        }
    }

    return didModify;
}

/**
 * Deref a ref to access the wrapped item
 * @param ref the ref
 * @return the wrapped item
 * TODO: are we keeping this API?
 */
void *threadSafeWrapperItemDeref(icThreadSafeWrapperItemRef *ref)
{
    void *item = NULL;
    if (ref != NULL)
    {
        item = ref->wrappedItem;
    }
    return item;
}

/**
 * Helper for context items which do not require any cleanup
 * @param context the context
 */
void threadSafeWrapperDoNotFreeContextFunc(void *context)
{
}

/**
 * Initializer if future is not statically initialized
 * @param future the future
 */
void threadSafeWrapperFutureInit(icThreadSafeWrapperFuture *future)
{
    if (future != NULL)
    {
        pthread_mutex_init(&future->privateData.mutex, NULL);
        initTimedWaitCond(&future->privateData.cond);
        future->privateData.condInitialized = true;
        future->privateData.complete = false;
        future->privateData.applied = false;
    }
}

/**
 * Cleanup if future is not statically initialized.  This should not be called on a future which is not complete yet.
 * Note caller still owns the memory for the future and must free it.
 * @param future the future
 */
void threadSafeWrapperFutureDestroy(icThreadSafeWrapperFuture *future)
{
    if (future != NULL)
    {
        pthread_mutex_destroy(&future->privateData.mutex);
        pthread_cond_destroy(&future->privateData.cond);
    }
}

/**
 * Wait for a future to complete
 * @param future the future to wait on
 * @param timeoutSecs the timeout in seconds
 * @return true if the future is complete, false otherwise
 */
bool threadSafeWrapperFutureAwait(icThreadSafeWrapperFuture *future, uint32_t timeoutSecs)
{
    bool complete = false;
    if (future != NULL)
    {
        pthread_mutex_lock(&future->privateData.mutex);
        if (future->privateData.complete == true)
        {
            complete = true;
        }
        else
        {
            if (future->privateData.condInitialized == false)
            {
                initTimedWaitCond(&future->privateData.cond);
                future->privateData.condInitialized = true;
            }

            // Wait for the condition
            incrementalCondTimedWait(&future->privateData.cond, &future->privateData.mutex, timeoutSecs);

            // Return whether its complete or not
            complete = future->privateData.complete;
        }
        pthread_mutex_unlock(&future->privateData.mutex);
    }

    return complete;

}

/**
 * Check if a future is complete
 * @param future the future
 * @return true if complete, false otherwise
 */
bool threadSafeWrapperFutureIsComplete(icThreadSafeWrapperFuture *future)
{
    bool complete = false;
    if (future != NULL)
    {
        pthread_mutex_lock(&future->privateData.mutex);
        complete = future->privateData.complete;
        pthread_mutex_unlock(&future->privateData.mutex);
    }
    return complete;
}

/**
 * Check if a future was applied
 * @param future the future
 * @return true if applied, false otherwise
 */
bool threadSafeWrapperFutureIsApplied(icThreadSafeWrapperFuture *future)
{
    bool applied = false;
    if (future != NULL)
    {
        pthread_mutex_lock(&future->privateData.mutex);
        applied = future->privateData.applied;
        pthread_mutex_unlock(&future->privateData.mutex);
    }
    return applied;
}

/**
 * Helper to create a new ref instance
 * @param itemToWrap the wrapped item to put into the ref
 * @return the new ref instance
 */
static icThreadSafeWrapperItemRef *createRef(void *itemToWrap)
{
    icThreadSafeWrapperItemRef *ref = (icThreadSafeWrapperItemRef *) calloc(1, sizeof(icThreadSafeWrapperItemRef));
    ref->wrappedItem = itemToWrap;
    return ref;
}

/**
 * Helper to destroy a ref and all its memory
 * @param ref the ref to destroy
 * @param destroyFunc the destroy function to use on the wrapped item
 */
static void destroyRef(icThreadSafeWrapperItemRef *ref, ThreadSafeWrapperDestroyItemFunc destroyFunc)
{
    if (ref != NULL)
    {
        if (destroyFunc != NULL)
        {
            destroyFunc(ref->wrappedItem);
        }
        else
        {
            free(ref->wrappedItem);
        }
        if (ref->pendingModifications != NULL)
        {
            linkedListDestroy(ref->pendingModifications, destroyPendingModificationFunc);
        }
        free(ref);
    }
}

/**
 * Helper to obtain the current ref.  Takes care of applying any pending modifications if possible before returning the
 * ref. Will also destroy the ref after applying modifications if there is an autoReleaseCheckFunc.  If the ref is
 * destroyed this will return NULL.  Assumes caller holds the mutex.
 * @param wrapper the wrapper instance
 * @return the current ref or NULL if no ref is currently assigned(possibly because the ref got released)
 */
static icThreadSafeWrapperItemRef *getRef(icThreadSafeWrapper *wrapper)
{
    icThreadSafeWrapperItemRef *retval = NULL;
    if (wrapper->privateData.ref != NULL)
    {
        // See whether we can apply modifications
        if (wrapper->privateData.ref->readerCount == 0)
        {
            applyModifications(wrapper->privateData.ref);
            postModificationCheckRelease(wrapper, wrapper->privateData.ref);
        }
        // This will either point to the current ref, or NULL if it got auto released
        // coverity[use_after_free] This can only be a valid ptr or it was set to NULL in postModificationCheckRelease
        retval = wrapper->privateData.ref;
    }

    return retval;
}

/**
 * Apply pending modifications to the wrapped item
 * @param ref the ref containing the wrapped item
 */
static void applyModifications(icThreadSafeWrapperItemRef *ref)
{
    if (ref != NULL && ref->pendingModifications != NULL)
    {
        icLinkedListIterator *iter = linkedListIteratorCreate(ref->pendingModifications);
        while (linkedListIteratorHasNext(iter))
        {
            PendingModificationInfo *pendingModificationInfo = (PendingModificationInfo *) linkedListIteratorGetNext(iter);
            pendingModificationInfo->modificationFunc(ref->wrappedItem, pendingModificationInfo->context);
            setFutureComplete(pendingModificationInfo, true);

        }
        linkedListIteratorDestroy(iter);

        linkedListDestroy(ref->pendingModifications, destroyPendingModificationFunc);
        ref->pendingModifications = NULL;
    }
}

/**
 * Destroy a PendingModificationInfo instance
 * @param item the PendingModificationInfo to destroy
 */
static void destroyPendingModificationFunc(void *item)
{
    PendingModificationInfo *pendingModificationInfo = (PendingModificationInfo *) item;
    setFutureComplete(pendingModificationInfo, false);
    freeModificationContext(pendingModificationInfo->context, pendingModificationInfo->destroyContextFunc);
    free(pendingModificationInfo);
}

/**
 * Helper autoReleaseCheckFunc that always returns true to release
 * @param item the wrapped item
 * @return true
 */
static bool alwaysReleaseFunc(void *item)
{
    return true;
}

/**
 * Helper to clean up the context for a pending modification
 * @param context the context to cleanup
 * @param destroyContextFunc the destroy function
 */
static void freeModificationContext(void *context, ThreadSafeWrapperDestroyContextFunc destroyContextFunc)
{
    if (destroyContextFunc != NULL)
    {
        destroyContextFunc(context);
    }
    else
    {
        free(context);
    }
}

/**
 * Should be called after modifications to check whether to auto release or not
 * @param wrapper the wrapper
 */
static void postModificationCheckRelease(icThreadSafeWrapper *wrapper, icThreadSafeWrapperItemRef *ref)
{
    if (wrapper->privateData.autoReleaseCheckFunc != NULL &&
        wrapper->privateData.autoReleaseCheckFunc(ref->wrappedItem) == true)
    {
        destroyRef(ref, wrapper->privateData.destroyItemFunc);
        if (ref == wrapper->privateData.ref)
        {
            wrapper->privateData.ref = NULL;
        }
    }
}

/**
 * Mark a future as complete
 * @param pendingModificationInfo the modification with the future
 * @param applied whether the modification was applied or not
 */
static void setFutureComplete(PendingModificationInfo *pendingModificationInfo, bool applied)
{
    if (pendingModificationInfo->future != NULL)
    {
        // Get the pointer and NULL it out, we don't want anyone referencing the future
        // after we broadcast as it might be destroyed by the owner
        icThreadSafeWrapperFuture *future = pendingModificationInfo->future;
        if (future != NULL)
        {
            pendingModificationInfo->future = NULL;
            pthread_mutex_lock(&future->privateData.mutex);
            future->privateData.complete = true;
            future->privateData.applied = applied;
            if (future->privateData.condInitialized == false)
            {
                initTimedWaitCond(&future->privateData.cond);
                future->privateData.condInitialized = true;
            }
            pthread_cond_broadcast(&future->privateData.cond);
            pthread_mutex_unlock(&future->privateData.mutex);
        }
    }
}