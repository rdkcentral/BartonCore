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
 * list.h
 *
 * Define the internals of the linked list structure
 * so it can be shared between other icType implementations
 *
 * Author: jelderton - 8/24/15
 *-----------------------------------------------*/

#include <icLog/logging.h>
#include <stdlib.h>
#include <string.h>

#include "icTypes/icLinkedList.h"

// Define our list 'head' so that it doesn't change - allowing the
// caller to create the data-structure and not have to worry about
// the list being new, full, etc.
//
//  listHead       listNode
//  +---------+    +---------+
//  |         |    |         |
//  |  item --+--->|  item --+---> NULL
//  |         |    |         |
//  +---------+    +---------+
//

// a single node in the LinkedList
//
typedef struct _listNode listNode;
struct _listNode
{
    void *item;
    listNode *next;
};

// the head of the list
//
struct _icLinkedList
{
    uint16_t size;
    listNode *first;
    bool cloned;
};

/*
 * internal helper to allocate mem (and clear)
 */
listNode *createListNode();

/*
 * internal function to remove the 'prev' pointer.  called by hashMap
 * when deleting from within the iterator directly.  prevents having
 * pointers to bad memory during iteration.
 */
void linkedListIteratorClearPrev(icLinkedListIterator *iterator);
