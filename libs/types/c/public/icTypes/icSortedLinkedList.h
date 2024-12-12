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
 * icSortedLinkedList.h
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

#ifndef IC_SORTED_LINKEDLIST_H
#define IC_SORTED_LINKEDLIST_H

#include "icTypes/icLinkedList.h"
#include <stdbool.h>

/*
 * extend the icLinkedList object
 */
typedef icLinkedList icSortedLinkedList;

/*
 * function prototype for order comparison when inserting items
 * into the linked list.  like normal 'sort compare' functions,
 * it should return -1 if the 'element' is less then 'newItem'
 *           return  0 if the 'element' is equal to 'newItem'
 *           return  1 if the 'element' is greater then 'newItem'
 *
 * @param item - the new item being inserted into the list
 * @param element - a node in the linked list being compared against to determine the order
 * @return -1, 0, 1 depending on how 'element' compares to 'newItem'
 */
typedef int8_t (*linkedListSortCompareFunc)(void *newItem, void *element);

/*
 * create a new sorted linked list.  will need to be released when complete
 *
 * @param compareFunc - function to use when inserting items into this sorted list
 * @return a new icSortedLinkedList object
 * @see linkedListDestroy()
 */
icSortedLinkedList *sortedLinkedListCreate();

/*
 * adds 'item' to the correct position within the icSortedLinkedList.
 * uses the linkedListSortCompareFunc provided when the list was created
 * to find the correct position for the insert.
 *
 * @param list - the LinkedList to alter
 * @param item - the item to add
 * @return TRUE on success
 */
bool sortedLinkedListAdd(icSortedLinkedList *list, void *item, linkedListSortCompareFunc compareFunc);


#endif // IC_SORTED_LINKEDLIST_H
