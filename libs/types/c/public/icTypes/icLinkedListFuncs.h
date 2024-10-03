//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2018 Comcast
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
// Comcast retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------
//
// Created by mkoch201 on 11/2/18.
//

#ifndef ZILKER_ICLISTFUNCS_H
#define ZILKER_ICLISTFUNCS_H

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

#endif // ZILKER_ICLISTFUNCS_H
