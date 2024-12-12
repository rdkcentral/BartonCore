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
