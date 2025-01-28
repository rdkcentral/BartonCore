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
// Created by mkoch201 on 11/2/18.
//

#pragma once

#include "icHashMap.h"

/*
 * function prototype for comparing items within the LinkedList
 * generally used when searching or deleting items
 *
 * @param searchVal - the 'searchVal' supplied to the "find" or "delete" function call
 * @param item - the current item in the list being examined
 * @return TRUE if 'item' matches 'searchVal', terminating the loop
 */
typedef bool (*linkedListCompareFunc)(void *searchVal, void *item);

/*
 * function prototype for iterating items within the LinkedList
 *
 * @param item - the current item in the list being examined
 * @param arg - the optional argument supplied from iterateLinkedList()
 * @return TRUE if the iteration should continue, otherwise stops the loop
 */
typedef bool (*linkedListIterateFunc)(void *item, void *arg);

/*
 * function prototype for freeing items within the LinkedList
 * used during deleteFromLinkedList() and destroyLinkedList()
 *
 * @param item - the current item in the list that needs to be freed
 */
typedef void (*linkedListItemFreeFunc)(void *item);

/*
 * function prototype for cloning items within the LinkedList
 * used during linkedListDeepClone()
 */
typedef void *(*linkedListCloneItemFunc)(void *item, void *context);

/*
 * function prototype for filtering a list of things to create a new list of things
 */
typedef void *(*linkedListFilterFunc)(void *item, void *context);

typedef struct MapFuncKeyContext
{
    void *key;
    uint16_t keyLen;
    void *value;
} MapFuncKeyContext;

/*
 * function prototype for generating map entries based on list elements.
 */
typedef MapFuncKeyContext *(*linkedListMapFunc)(void *item, void *context);

/*
 * function prototype for folding a list
 */
typedef void *(*linkedListFoldFunc)(void *runningValue, void *item, void *context);

/*
 * simple linked list compare function that can be used to compare strings in a list.
 *
 * @see linkedListFind
 * @see linkedListDelete
 */
bool linkedListStringCompareFunc(void *searchVal, void *item);

/*
 * simple linked list clone item function that can be used to deep clone lists of strings.
 *
 * @see linkedListDeepClone
 */
void *linkedListCloneStringItemFunc(void *item, void *context);

/*
 * common implementation of the linkedListItemFreeFunc function that can be
 * used in situations where the 'item' is not freed at all.
 * generally used when the list contains pointers to functions
 */
void standardDoNotFreeFunc(void *item);
