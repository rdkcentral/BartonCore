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
    return (icSortedLinkedList *) linkedListCreate();
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
