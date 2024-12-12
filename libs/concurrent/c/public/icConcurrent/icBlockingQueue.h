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
// Created by wboyd747 on 5/30/18.
//

#ifndef ZILKER_ICBLOCKINGQUEUE_H
#define ZILKER_ICBLOCKINGQUEUE_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#define BLOCKINGQUEUE_TIMEOUT_INFINITE (INT_MAX)
#define BLOCKINGQUEUE_MAX_CAPACITY     ((uint16_t) UINT16_MAX)

/*
 * function prototype for freeing items within
 * the Queue.  used during queueDestroy()
 *
 * @param item - the current item in the queue that needs to be freed
 */
typedef void (*blockingQueueItemFreeFunc)(void *item);

/*
 * function prototype for comparing items within the queue
 * (specifically used when deleting items)
 *
 * @param searchVal - the 'searchVal' supplied to the "find" or "delete" function call
 * @param item - the current item in the queue being examined
 * @return TRUE if 'item' matches 'searchVal', terminating the loop
 */
typedef bool (*blockingQueueCompareFunc)(void *searchVal, void *item);

/*
 * function prototype for iterating items within the queue.
 * handy for dumping the queue contents.
 *
 * @param item - the current item in the queue being examined
 * @param arg - the optional argument supplied from iterateQueue()
 * @return TRUE if the iteration should continue, otherwise stops the loop
 */
typedef bool (*blockingQueueIterateFunc)(void *item, void *arg);

/*
 * the Queue object representation.
 */
typedef struct _icBlockingQueue icBlockingQueue;

/**
 * Create a new blocking queue with a set max capacity.
 *
 * Note: Will need to be released when complete
 *
 * @return a new BlockingQueue object
 * @see blockingQueueDestroy()
 */
icBlockingQueue *blockingQueueCreate(uint16_t maxCapacity);

/**
 * Destroy a queue and cleanup memory.  Note that
 * each 'item' will just be release via free() unless
 * a 'helper' is provided.
 *
 * @param queue - the Queue to delete
 * @param helper - optional, only needed if the items need a custom mechanism to release the memory
 */
void blockingQueueDestroy(icBlockingQueue *queue, blockingQueueItemFreeFunc helper);

/*
 * return the number of elements in the queue
 *
 * @param queue - the Queue to count
 */
uint16_t blockingQueueCount(icBlockingQueue *queue);

/**
 * Append 'item' to the queue (i.e. add to the end)
 *
 * This is equivalent to:
 * blockingQueuePushTimeout(queue, item, BLOCKINGQUEUE_TIMEOUT_INFINITE);
 *
 * @param queue - the Queue to alter
 * @param item - the item to add
 * @return TRUE on success
 */
bool blockingQueuePush(icBlockingQueue *queue, void *item);

/**
 * Append 'item' to the queue (i.e. add to the end)
 * If the Queue is already at max capacity then the
 * push will block until for "timeout" milliseconds, or
 * an item is popped from the Queue.
 *
 * If Timeout is greater than zero then the call will
 * block until the timeout condition occurs. If the timeout
 * is zero then the call return immediately regardless of Queue
 * status. If the timeout is BLOCKINGQUEUE_TIMEOUT_INFINITE then the
 * routine will block until an item is able to be placed into the Queue, or
 * the Queue is destroyed.
 *
 * @param queue - the Queue to alter
 * @param item - the item to add
 * @param timeout The number of <em>seconds<em> to block for
 * until an item is placed into the Queue. If the timeout is
 * QUEUE_TIMEOUT_INFINITE the the routine will block forever until
 * an item is placed into the Queue. If the timeout is zero then the
 * routine will return immediately regardless of Queue statuus.
 * @return TRUE on success
 */
bool blockingQueuePushTimeout(icBlockingQueue *queue, void *item, int timeout);

/**
 * Same as blockingQueuePushTimeout but uses a timespec for granular (nanosecond precision) timeouts
 * @see blockingQueuePushTimeout
 * @param queue
 * @param item
 * @param timeout
 * @return
 */
bool blockingQueuePushTimeoutGranular(icBlockingQueue *queue, void *item, struct timespec *timeout);

/**
 * Removes and returns the next item in the queue.
 *
 * This is equivalent to:
 * blockingQueuePopTimeout(queue, item, BLOCKINGQUEUE_TIMEOUT_INFINITE);
 *
 * Note: that the caller will be responsible for freeing
 * the returned item
 *
 * @param queue - the Queue to pull from
 * @return the item from the queue, or NULL if the queue is empty
 */
void *blockingQueuePop(icBlockingQueue *queue);

/**
 * Removes and returns the next item in the queue.
 *
 * If Timeout is greater than zero then the call will
 * block until the timeout condition occurs. If the timeout
 * is zero then the call return immediately regardless of Queue
 * status. If the timeout is QUEUE_TIMEOUT_INFINITE then the
 * routine will block until an item is placed into the Queue, or
 * the Queue is destroyed.
 *
 * Note: that the caller will be responsible for freeing
 * the returned item
 *
 * @param queue the Queue to pull from
 * @param timeout The number of <em>seconds<em> to block for
 * until an item is placed into the Queue. If the timeout is
 * QUEUE_TIMEOUT_INFINITE the the routine will block forever until
 * an item is placed into the Queue. If the timeout is zero then the
 * routine will return immediately regardless of Queue statuus.
 * @return the item from the queue, or NULL. If the return value is
 * NULL <em>errno</em> must be checked for ETIMEDOUT to determine if
 * a timeout condition occurred.
 */
void *blockingQueuePopTimeout(icBlockingQueue *queue, int timeout);

/**
 * Same as blockingQueuePopTimeout but uses timepsec for granular (nanosecond precision) timeouts
 * @see blockingQueuePopTimeout
 * @param queue
 * @param timeout
 * @return
 */
void *blockingQueuePopTimeoutGranular(icBlockingQueue *queue, struct timespec *timeout);

/**
 * Iterate through the queue to find a particular item, and
 * if located delete the item and the node.
 *
 * @param queue - the Queue to search through
 * @param searchVal - the value looking for
 * @param searchFunc - the compare function to call for each item
 * @param freeFunc - optional, only needed if the item needs a custom mechanism to release the memory
 * @return TRUE on success
 */
bool blockingQueueDelete(icBlockingQueue *queue,
                         void *searchVal,
                         blockingQueueCompareFunc searchFunc,
                         blockingQueueItemFreeFunc freeFunc);

/**
 * Iterate through the queue, calling 'queueIterateFunc' for each
 * item in the list.  helpful for dumping the contents of the queue.
 *
 * @param list - the queue to search through
 * @param callback - the function to call for each item
 * @param arg - optional parameter supplied to the 'callback' function
 */
void blockingQueueIterate(icBlockingQueue *queue, blockingQueueIterateFunc callback, void *arg);

/**
 * Clear and destroy the contents of the queue.
 * Note that each 'item' will just be release via free() unless
 * a 'helper' is provided.
 *
 * @param queue - the Queue to clear
 * @param helper - optional, only needed if the items need a custom mechanism to release the memory
 */
void blockingQueueClear(icBlockingQueue *queue, blockingQueueItemFreeFunc helper);

/**
 * Disable this queue from accepting any new items and any existing blocking calls are
 * unblocked.
 *
 * @param queue - the Queue to unblock
 */
void blockingQueueDisable(icBlockingQueue *queue);

/*
 * returns if the queue is disabled or not
 */
bool blockingQueueIsDisabled(icBlockingQueue *queue);

#endif // ZILKER_ICBLOCKINGQUEUE_H
