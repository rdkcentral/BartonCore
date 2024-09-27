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
 * icSortedLinkedList.c
 *
 * Extension of icLinkedList to store elements in sorted order.
 * Note that adding items to this collection should always be
 * done via the sortedLinkedListAdd.  All other icLinkedList
 * functions are supported with this collection.  For example:
 *   icLinkedList *sorted = sortedLinkedListCreate();
 *   sortedLinkedListAdd(sorted, item1, compareFunc);
 *   sortedLinkedListAdd(sorted, item2, compareFunc);
 *   sortedLinkedListAdd(sorted, item3, compareFunc);
 *   linkedListDelete(sorted, item2, searchFunc, freeFunc);
 *   linkedListDestroy(sorted, freeFunc);
 *
 * NOTE: this does not perform any mutex locking to
 *       allow for single-threaded usage without the
 *       overhead.  If locking is required, it should
 *       be performed by the caller
 *
 * Author: jelderton - 8/21/15
 *-----------------------------------------------*/

#include "icTypes/icSortedLinkedList.h"
#include "list.h"

/*
 * create a new sorted linked list.  will need to be released when complete
 *
 * @param compareFunc - function to use when inserting items into this sorted list
 * @return a new icSortedLinkedList object
 * @see linkedListDestroy()
 */
icSortedLinkedList *sortedLinkedListCreate()
{
   return (icSortedLinkedList *)linkedListCreate();
}

/*
 * adds 'item' to the correct position within the icSortedLinkedList.
 * uses the linkedListSortCompareFunc provided when the list was created
 * to find the correct position for the insert.
 *
 * @param list - the LinkedList to alter
 * @param item - the item to add
 * @return TRUE on success
 */
bool sortedLinkedListAdd(icSortedLinkedList *list, void *item, linkedListSortCompareFunc compareFunc)
{
    if (list == NULL)
    {
        return false;
    }

    listNode *ptr = list->first;
    listNode *prev = NULL;

    // handle easy case of empty list
    //
    if (ptr == NULL)
    {
        return linkedListAppend(list, item);
    }

    // loop through the list until we find a node
    // that is greater then 'item'.  for example,
    // adding a new node 'C' should be inserted
    // after 'B' and before 'D'
    //
    while (ptr != NULL)
    {
        // compare 'item' to the current node
        // to see if this should be prepended or not
        //
        int rc = compareFunc(item, ptr->item);
        if (rc <= 0)
        {
            // create the container for 'item'
            //
            listNode *node = createListNode();
            node->item = item;

            // 'item' should be in front of 'ptr'
            //
            node->next = ptr;
            if (prev == NULL)
            {
                list->first = node;
            }
            else
            {
                prev->next = node;
            }

            // update counter
            //
            list->size++;
            return true;
        }

        // move to next node
        //
        prev = ptr;
        ptr = ptr->next;
    }

    // if we got this far, hit the end of the list
    // so append this new item
    //
    return linkedListAppend(list, item);
}


