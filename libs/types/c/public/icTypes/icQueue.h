//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2015 iControl Networks, Inc.
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
 * icQueue.h
 *
 * Simplistic First-In-Frst-Out queue.
 *
 * NOTE: this does not perform any mutex locking to
 *       allow for single-threaded usage without the
 *       overhead.  If locking is required, it should
 *       be performed by the caller
 *
 * Author: jelderton - 8/25/15
 *-----------------------------------------------*/
#ifndef FLEXCORE_ICQUEUE_H
#define FLEXCORE_ICQUEUE_H

#include <stdint.h>
#include <stdbool.h>

/*
 * the Queue object representation.
 */
typedef struct _icQueue icQueue;

/*
 * function prototype for freeing items within
 * the Queue.  used during queueDestroy()
 *
 * @param item - the current item in the queue that needs to be freed
 */
typedef void (*queueItemFreeFunc)(void *item);

/*
 * function prototype for comparing items within the queue
 * (specifically used when deleting items)
 *
 * @param searchVal - the 'searchVal' supplied to the "find" or "delete" function call
 * @param item - the current item in the queue being examined
 * @return TRUE if 'item' matches 'searchVal', terminating the loop
 */
typedef bool (*queueCompareFunc)(void *searchVal, void *item);

/*
 * function prototype for iterating items within the queue.
 * handy for dumping the queue contents.
 *
 * @param item - the current item in the queue being examined
 * @param arg - the optional argument supplied from iterateQueue()
 * @return TRUE if the iteration should continue, otherwise stops the loop
 */
typedef bool (*queueIterateFunc)(void *item, void *arg);

/*
 * create a new queue.  will need to be released when complete
 *
 * @return a new Queue object
 * @see queueDestroy()
 */
icQueue *queueCreate();

/*
 * destroy a queue and cleanup memory.  Note that
 * each 'item' will just be release via free() unless
 * a 'helper' is provided.
 *
 * @param queue - the Queue to delete
 * @param helper - optional, only needed if the items need a custom mechanism to release the memory
 */
void queueDestroy(icQueue *queue, queueItemFreeFunc helper);

/*
 * return the number of elements in the queue
 *
 * @param queue - the Queue to count
 */
uint16_t queueCount(icQueue *queue);

/*
 * append 'item' to the queue (i.e. add to the end)
 *
 * @param queue - the Queue to alter
 * @param item - the item to add
 * @return TRUE on success
 */
bool queuePush(icQueue *queue, void *item);

/*
 * removes and returns the next item in the queue.
 * note that the caller will be responsible for freeing
 * the returned item
 *
 * @param queue - the Queue to pull from
 * @return the item from the queue, or NULL if the queue is empty
 */
void *queuePop(icQueue *queue);

/*
 * iterate through the queue to find a particular item, and
 * if located delete the item and the node.
 *
 * @param queue - the Queue to search through
 * @param searchVal - the value looking for
 * @param searchFunc - the compare function to call for each item
 * @param freeFunc - optional, only needed if the item needs a custom mechanism to release the memory
 * @return TRUE on success
 */
bool queueDelete(icQueue *queue, void *searchVal, queueCompareFunc searchFunc, queueItemFreeFunc freeFunc);

/*
 * iterate through the queue to find a particular item
 *
 * @param queue - the Queue to search through
 * @param searchVal - the value looking for
 * @param searchFunc - the compare function to call for each item
 * @return TRUE on success
 */
void *queueFind(icQueue *queue, void *searchVal, queueCompareFunc searchFunc);

/*
 * iterate through the queue, calling 'queueIterateFunc' for each
 * item in the list.  helpful for dumping the contents of the queue.
 *
 * @param list - the queue to search through
 * @param callback - the function to call for each item
 * @param arg - optional parameter supplied to the 'callback' function
 */
void queueIterate(icQueue *queue, queueIterateFunc callback, void *arg);

/*
 * clear and destroy the contents of the queue.
 * Note that each 'item' will just be release via free() unless
 * a 'helper' is provided.
 *
 * @param queue - the Queue to clear
 * @param helper - optional, only needed if the items need a custom mechanism to release the memory
 */
void queueClear(icQueue *queue, queueItemFreeFunc helper);

#endif //FLEXCORE_ICQUEUE_H
