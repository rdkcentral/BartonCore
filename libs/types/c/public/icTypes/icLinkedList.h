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
 * icLinkedList.h
 *
 * Simplistic linked-list to provide the basic need for
 * dynamic Lists.  Probably temporary as we may switch
 * to utilize a 3rd-party implementation at some point.
 *
 * Supports operations such as:
 *   create a list
 *   append an item to the end of a list
 *   insert an item to the front of a list
 *   delete an item from the list
 *   destroy the list
 *
 * NOTE: this does not perform any mutex locking to
 *       allow for single-threaded usage without the
 *       overhead.  If locking is required, it should
 *       be performed by the caller
 *
 * Author: jelderton - 6/18/15
 *-----------------------------------------------*/

#ifndef IC_LINKEDLIST_H
#define IC_LINKEDLIST_H

#include "icHashMap.h"
#include "icLinkedListFuncs.h"
#include "sbrm.h"
#include <stdbool.h>
#include <stdint.h>

/*-------------------------------*
 *
 *  icLinkedList
 *
 *-------------------------------*/

/*
 * the LinkedList object representation.
 * allows the caller to switch out head/tail and have
 * multiple instances without concern of memory reallocation
 */
typedef struct _icLinkedList icLinkedList;

/*
 * create a new linked list.  will need to be released when complete
 *
 * @return a new LinkedList object
 * @see linkedListDestroy()
 */
icLinkedList *linkedListCreate();

/*
 * create a shallow-clone of an existing linked list.
 * will need to be released when complete, HOWEVER,
 * the items in the cloned list are not owned by it
 * and therefore should NOT be released.
 *
 * @see linkedListDestroy()
 * @see linkedListIsClone()
 */
icLinkedList *linkedListClone(icLinkedList *src);

/*
 * create a deep-clone of an existing linked list.
 * will need to be released when complete.
 *
 * @see linkedListDestroy()
 */
icLinkedList *linkedListDeepClone(icLinkedList *src, linkedListCloneItemFunc cloneItemFunc, void *context);

/*
 * destroy a linked list and cleanup memory.  note that
 * each 'item' will just be release via free() unless
 * a 'helper' is provided.
 * NOTE: items in the list are NOT RELEASED if this list is a 'clone' (regardless of 'helper' parameter)
 *
 * @param list - the LinkedList to delete
 * @param helper - optional, only needed if the items need a custom mechanism to release the memory
 *
 * @see linkedListClone()
 */
void linkedListDestroy(icLinkedList *list, linkedListItemFreeFunc helper);

inline void linkedListDestroy_nofree__auto(icLinkedList **list)
{
    linkedListDestroy(*list, standardDoNotFreeFunc);
}

/**
 * Convenience macro to declare a scope-bound linked list that will not own its contents.
 * When the list goes out of scope, the collection is destroyed, leaving the contents
 * alone.
 */
#define scoped_icLinkedListNofree AUTO_CLEAN(linkedListDestroy_nofree__auto) icLinkedList

/**
 * Auto-free a linked list whose keys/values do not need a linkedListItemFreeFunc
 * @param list
 */
inline void linkedListDestroy_generic__auto(icLinkedList **list)
{
    linkedListDestroy(*list, NULL);
}

/**
 * Convenience macro to declare a scope-bound linked list that will destroy its contents
 * with the standard free() libc function.
 */
#define scoped_icLinkedListGeneric AUTO_CLEAN(linkedListDestroy_generic__auto) icLinkedList

/*
 * return true if this list was created via linkedListClone
 *
 * @see linkedListClone
 */
bool linkedListIsClone(icLinkedList *list);

/*
 * return the number of elements in the list
 *
 * @param list - the LinkedList to count
 */
uint16_t linkedListCount(icLinkedList *list);

/*
 * append 'item' to the end of the LinkedList
 * (ex: add to the end)
 *
 * @param list - the LinkedList to alter
 * @param item - the item to add
 * @return TRUE on success
 */
bool linkedListAppend(icLinkedList *list, void *item);

/*
 * prepend 'item' to the beginning of the LinkedList
 * (ex: add to the front)
 *
 * @param list - the LinkedList to alter
 * @param item - the item to add
 * @return TRUE on success
 */
bool linkedListPrepend(icLinkedList *list, void *item);

/*
 * iterate through the LinkedList to find a particular item
 * for each node in the list, will call the 'searchFunc'
 * with the current item and the 'searchVal' to perform
 * the comparison.
 *
 * @param list - the LinkedList to search through
 * @param searchVal - the value looking for
 * @param searchFunc - the compare function to call for each item
 * @return the 'item' found, or NULL if not located
 */
void *linkedListFind(icLinkedList *list, void *searchVal, linkedListCompareFunc searchFunc);

/*
 * remove the element at 'offset' (similar to an array).
 *
 * @param list - the LinkedList to search through
 * @param offset - element number, 0 through (getCount - 1)
 * @return the removed element or NULL if invalid list/offset given
 */
void *linkedListRemove(icLinkedList *list, uint32_t offset);

/*
 * iterate through the LinkedList to find a particular item, and
 * if located delete the item and the node.  Operates similar to
 * findInLinkedList.
 *
 * @param list - the LinkedList to search through
 * @param searchVal - the value looking for
 * @param searchFunc - the compare function to call for each item
 * @param freeFunc - optional, only needed if the item needs a custom mechanism to release the memory
 * @return TRUE on success
 */
bool linkedListDelete(icLinkedList *list,
                      void *searchVal,
                      linkedListCompareFunc searchFunc,
                      linkedListItemFreeFunc freeFunc);

/*
 * Reverses the order of elements in a list, returning a shallow clone of the list in reverse order.
 *
 * @param list
 * @return
 */
icLinkedList *linkedListReverse(icLinkedList *list);

/*
 * Iterate through a LinkedList and apply the provided filter function on each item, passing the item and the provided
 * context. If the filter function provides a non-NULL return value, that returned value will be added to a new list.
 * Finally, the new list of items will be returned.
 *
 * @param list - the LinkedList to filter items from.
 * @param filterFunc - a filter function that returns some new item based on the provided item (from list) and context
 * @param context - an optional context object to forward to the filter function.
 * @return a new list of filtered items, or NULL if bad arguments are provided.
 */
icLinkedList *linkedListFilter(icLinkedList *list, linkedListFilterFunc filterFunc, void *context);

/*
 * Iterate through the LinkedList and apply the provided map function on each item, passing the item and the provided
 * context. If the map function provides a non-NULL return value, then the returned value will be used as the key/value
 * pair in a map entry. Finally, the new map will be returned.
 *
 * @param list - the LinkedList to create map entries from.
 * @param mapFunc - a map function that returns a key/value pair to be used in the new map
 * @param context  - an optional context object to forward to the map function
 * @param mapFreeFunc - a free function to use on the new map if an error is encountered and it needs to be destroyed.
 *                      If NULL is provided, NULL will be passed to hashMapDestroy and free will be used for offending
 *                      MapFuncKeyContext memory.
 * @return a map of keys and values generated from the provided mapFunc, or NULL if bad arguments are provided.
 */
icHashMap *
linkedListMapFromList(icLinkedList *list, linkedListMapFunc mapFunc, void *context, hashMapFreeFunc mapFreeFunc);

/*
 * Mimics a classic foldl function. Initially invokes the provided fold function, passing the
 * initial argument and the first item in the list. Then, the result of that function will be passed
 * to the fold function along with the second item in the list, and so on. The final result of the fold
 * functions will be returned.
 *
 * Please note items returned by the fold function, and then subsequently supplied to the fold function, should
 * have their memory cleaned up by the fold function if necessary.
 *
 * @param list - the LinkedList to fold
 * @param foldFunc - a fold function that returns a new running value based on the previous running value and the next
 *                   element in the list (from left to right)
 * @param initial - the initial running value to pass to the fold function
 * @param context - an optional context object to forward to the fold function
 * @return the final value returned by the fold function, or the initial argument if the list is empty or bad arguments
 *          are provided.
 */
void *linkedListFoldL(icLinkedList *list, linkedListFoldFunc foldFunc, void *initial, void *context);

/*
 * Mimics a classic foldr function. Initially invokes the provided fold function, passing the
 * initial argument and the last item in the list. Then, the result of that function will be passed
 * to the fold function along with the penultimate item in the list, and so on. The final result of the fold
 * functions will be returned.
 *
 * Please note items returned by the fold function, and then subsequently supplied to the fold function, should
 * have their memory cleaned up by the fold function if necessary.
 *
 * @param list - the LinkedList to fold
 * @param foldFunc - a fold function that returns a new running value based on the previous running value and the next
 *                   element in the list (from right to left)
 * @param initial - the initial running value to pass to the fold function
 * @param context - an optional context object to forward to the fold function
 * @return the final value returned by the fold function, or the initial argument if the list is empty or bad arguments
 *          are provided.
 */
void *linkedListFoldR(icLinkedList *list, linkedListFoldFunc foldFunc, void *initial, void *context);

/*
 * iterate through the LinkedList, calling 'linkedListIterateFunc' for each
 * item in the list.
 *
 * @param list - the LinkedList to search through
 * @param callback - the function to call for each item
 * @param arg - optional parameter supplied to the 'callback' function
 */
void linkedListIterate(icLinkedList *list, linkedListIterateFunc callback, void *arg);

/*
 * return the element at 'offset' (similar to an array).
 * caller should not remove the return object as it is
 * still part of the linkedList.
 *
 * @param list - the LinkedList to search through
 * @param offset - element number, 0 through (getCount - 1)
 */
void *linkedListGetElementAt(icLinkedList *list, uint32_t offset);

/*
 * clear and destroy the items in the list.  note that
 * each 'item' will just be release via free() unless
 * a 'helper' is provided.
 * NOTE: items in the list are NOT RELEASED if this list is a 'clone' (regardless of 'helper' parameter)
 *
 * @param list - the LinkedList to clear
 * @param helper - optional, only needed if the items need a custom mechanism to release the memory
 *
 * @see linkedListClone()
 */
void linkedListClear(icLinkedList *list, linkedListItemFreeFunc helper);

/*
 * Simple search function to use with linkedListFind to locate a simple string (case sensitive)
 */
bool linkedListStringCompareSearchFunc(void *searchVal, void *item);

/*-------------------------------*
 *
 *  icLinkedListIterator
 *
 *-------------------------------*/

typedef struct _icLinkedListIterator icLinkedListIterator;

/**
 * Convenience macro to create a scope bound linkedListIterator. When it goes out of scope,
 * linkedListIteratorDestroy will be called automatically.
 */
#define sbIcLinkedListIterator      AUTO_CLEAN(linkedListIteratorDestroy__auto) icLinkedListIterator
#define scoped_icLinkedListIterator sbIcLinkedListIterator

/*
 * alternative mechanism to iterating the linked list items
 * should be called initially to create the LinkedListIterator,
 * then followed with calls to linkedListIteratorHasNext and
 * linkedListIteratorGetNext.  once the iteration is done, this
 * MUST BE FREED via linkedListIteratorDestroy
 *
 * @param list - the LinkedList to iterate
 * @see linkedListIteratorDestroy()
 */
icLinkedListIterator *linkedListIteratorCreate(icLinkedList *list);

/*
 * free a LinkedListIterator
 */
void linkedListIteratorDestroy(icLinkedListIterator *iterator);

inline void linkedListIteratorDestroy__auto(icLinkedListIterator **iter)
{
    linkedListIteratorDestroy(*iter);
}

/*
 * return if there are more items in the iterator to examine
 */
bool linkedListIteratorHasNext(icLinkedListIterator *iterator);

/*
 * return the next item available in the LinkedList via the iterator
 */
void *linkedListIteratorGetNext(icLinkedListIterator *iterator);

/*
 * deletes the current item (i.e. the item returned from the last call to
 * linkedListIteratorGetNext) from the underlying LinkedList.
 * just like other 'delete' functions, will release the memory
 * via free() unless a helper is supplied.
 *
 * only valid after 'getNext' is called, and returns if the delete
 * was successful or not.
 */
bool linkedListIteratorDeleteCurrent(icLinkedListIterator *iterator, linkedListItemFreeFunc helper);


#endif // IC_LINKEDLIST_H
