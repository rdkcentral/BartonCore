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
 * icQueue.c
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

#include <stdlib.h>
#include <string.h>

#include "icTypes/icQueue.h"
#include "list.h"

/*
 * the Queue object representation.
 */
struct _icQueue
{
    // borrow the structures used within linked list
    //
    uint16_t  size;
    listNode  *first;
    listNode  *last;
};

/*
 * create a new queue.  will need to be released when complete
 *
 * @return a new Queue object
 * @see queueDestroy()
 */
icQueue *queueCreate()
{
    // create the queue structure, then clear it
    //
    icQueue *retVal = (icQueue *)malloc(sizeof(icQueue));
    if (retVal != NULL)
    {
        memset(retVal, 0, sizeof(icQueue));
    }

    return retVal;
}

/*
 * destroy a queue and cleanup memory.  Note that
 * each 'item' will just be release via free() unless
 * a 'helper' is provided.
 *
 * @param queue - the Queue to delete
 * @param helper - optional, only needed if the items need a custom mechanism to release the memory
 */
void queueDestroy(icQueue *queue, queueItemFreeFunc helper)
{
    if (queue == NULL)
    {
        return;
    }

    // keep 'popping' from the queue until it is empty
    // this way we can free each item, then the queue itself
    //
    void *next = NULL;
    while ((next = queuePop(queue)) != NULL)
    {
        if (helper != NULL)
        {
            // release via helper
            //
            helper(next);
        }
        else
        {
            free(next);
        }
    }

    // now the queue
    //
    free(queue);
}

/*
 * return the number of elements in the queue
 *
 * @param queue - the Queue to count
 */
uint16_t queueCount(icQueue *queue)
{
    if (queue == NULL)
    {
        return 0;
    }
    return queue->size;
}

/*
 * append 'item' to the queue (i.e. add to the end)
 *
 * @param queue - the Queue to alter
 * @param item - the item to add
 * @return TRUE on success
 */
bool queuePush(icQueue *queue, void *item)
{
    if (queue == NULL)
    {
        return false;
    }

    // make the listNode to contain 'item'
    //
    listNode *node = createListNode();
    if (node == NULL)
    {
        return false;
    }
    node->item = item;

    // now append to the tail of the queue
    //
    if (queue->last != NULL)
    {
        // append to last
        //
        queue->last->next = node;
        queue->last = node;
    }
    else
    {
        // must be the first
        //
        queue->first = node;
        queue->last = node;
    }

    // update length
    //
    queue->size++;
    return true;
}

/*
 * removes and returns the next item in the queue.
 * note that the caller will be responsible for freeing
 * the returned item
 *
 * @param queue - the Queue to pull from
 * @return the item from the queue, or NULL if the queue is empty
 */
void *queuePop(icQueue *queue)
{
    void *retVal = NULL;

    if (queue != NULL && queue->first != NULL)
    {
        // get the node
        //
        listNode *ptr = queue->first;

        // adjust 'first'
        //
        queue->first = ptr->next;
        if (queue->last == ptr)
        {
            // just removed 'last'
            //
            queue->last = NULL;
        }

        // save retVal, then release the listNode wrapper
        //
        retVal = ptr->item;
        free(ptr);

        // update size
        //
        if (queue->size > 0)
        {
            queue->size--;
        }
    }

    return retVal;
}

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
bool queueDelete(icQueue *queue, void *searchVal, queueCompareFunc searchFunc, queueItemFreeFunc freeFunc)
{
    if (queue == NULL || queue->first == NULL)
    {
        return false;
    }

    // loop through all nodes until we find a match
    //
    listNode *ptr = queue->first;
    listNode *prev = NULL;

    while (ptr != NULL)
    {
        if (searchFunc(searchVal, ptr->item) == true)
        {
            // match so need to delete this item.
            // first, point prev to next
            //
            if (prev != NULL)
            {
                prev->next = ptr->next;
            }
            else
            {
                // removing first node
                //
                queue->first = ptr->next;
            }

            // see if removing last (or just did by killing the first)
            //
            if (ptr == queue->last)
            {
                queue->last = prev;
            }

            // free item within the node
            //
            if (freeFunc != NULL)
            {
                // release via helper
                //
                freeFunc(ptr->item);
            }
            else
            {
                free(ptr->item);
            }

            // finally release the node
            //
            free(ptr);

            // update size
            //
            if (queue->size > 0)
            {
                queue->size--;
            }

            return true;
        }

        // go to next node in queue
        //
        prev = ptr;
        ptr = ptr->next;
    }

    // not found
    //
    return false;
}

/*
 * iterate through the queue to find a particular item
 *
 * @param queue - the Queue to search through
 * @param searchVal - the value looking for
 * @param searchFunc - the compare function to call for each item
 * @return TRUE on success
 */
void *queueFind(icQueue *queue, void *searchVal, queueCompareFunc searchFunc)
{
    void *retVal = NULL;

    if (queue == NULL || queue->first == NULL)
    {
        return false;
    }

    // loop through all nodes until we find a match or hit
    // the end of the queue
    //
    listNode *ptr = queue->first;
    while (ptr != NULL)
    {
        if (searchFunc(searchVal, ptr->item) == true)
        {
            retVal = ptr->item;
            break;
        }

        // go to next node in queue
        //
        ptr = ptr->next;
    }

    return retVal;
}

/*
 * iterate through the queue, calling 'queueIterateFunc' for each
 * item in the list.  helpful for dumping the contents of the queue.
 *
 * @param list - the queue to search through
 * @param callback - the function to call for each item
 * @param arg - optional parameter supplied to the 'callback' function
 */
void queueIterate(icQueue *queue, queueIterateFunc callback, void *arg)
{
    if (queue == NULL || queue->size == 0)
    {
        // nothing to loop through
        //
        return;
    }

    // iterate through the list, calling helper for each
    //
    listNode *ptr = queue->first;
    while (ptr != NULL)
    {
        // notify callback function
        //
        if (callback(ptr->item, arg) == false)
        {
            // callback told us to stop looping
            //
            break;
        }

        // go to the next node
        //
        ptr = ptr->next;
    }
}

/*
 * clear and destroy the contents of the queue.
 * Note that each 'item' will just be release via free() unless
 * a 'helper' is provided.
 *
 * @param queue - the Queue to clear
 * @param helper - optional, only needed if the items need a custom mechanism to release the memory
 */
void queueClear(icQueue *queue, queueItemFreeFunc helper)
{
    if (queue == NULL || queue->size == 0)
    {
        return;
    }

    // keep 'popping' from the queue until it is empty
    // this way we can free each item, then the queue itself
    //
    void *next = NULL;
    while ((next = queuePop(queue)) != NULL)
    {
        if (helper != NULL)
        {
            // release via helper
            //
            helper(next);
        }
        else
        {
            free(next);
        }
    }

    // reset internals
    //
    queue->size = 0;
    queue->first = NULL;
    queue->last = NULL;
}




