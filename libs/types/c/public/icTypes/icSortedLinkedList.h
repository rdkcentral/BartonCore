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

#include <stdbool.h>
#include "icTypes/icLinkedList.h"

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
