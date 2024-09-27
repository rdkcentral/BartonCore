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
 * icHashMap.h
 *
 * Simplistic hash-map to provide the basic need for
 * dynamic unordered collections.  Probably temporary
 * as we may switch to utilize a 3rd-party implementation
 * at some point.
 *
 * Can be used as a hash-set by using NULL for the 'value'
 *
 * Supports operations such as:
 *   create a hash
 *   put a key/value into the hash
 *   get a value from the hash for a particular key
 *   delete a value from the hash for a particular key
 *   destroy the hash
 *
 * NOTE: this does not perform any mutex locking to
 *       allow for single-threaded usage without the
 *       overhead.  If locking is required, it should
 *       be performed by the caller
 *
 * Author: jelderton - 6/18/15
 *-----------------------------------------------*/

#ifndef IC_HASHMAP_H
#define IC_HASHMAP_H

#include <stdint.h>
#include <stdbool.h>
#include "icHashMapFuncs.h"

/*-------------------------------*
 *
 *  icHashMap
 *
 *-------------------------------*/

/*
 * the HashMap object representation.
 */
typedef struct _icHashMap icHashMap;

/*
 * create a new hash-map.  will need to be released when complete
 *
 * @return a new HashMap object
 * @see hashMapDestroy()
 */
icHashMap *hashMapCreate();

/*
 * create a shallow-clone of an existing hash map.
 * will need to be released when complete, HOWEVER,
 * the items in the cloned list are not owned by it
 * and therefore should NOT be released.
 *
 * @see hashMapDestroy()
 * @see hashMapIsClone()
 */
icHashMap *hashMapClone(icHashMap *src);

/*
 * create a deep-clone of an existing hash map.
 * will need to be released when complete.
 *
 * @see hashMapDestroy()
 */
icHashMap *hashMapDeepClone(icHashMap *src, hashMapCloneFunc cloneFunc, void *context);

/*
 * destroy a hash-map and cleanup memory.  Note that
 * each 'key' and 'value' will just be release via free()
 * unless a 'helper' is provided.
 *
 * @param map - the HashMap to delete
 * @param helper - optional, only needed if the items need a custom mechanism to release the memory
 */
void hashMapDestroy(icHashMap *map, hashMapFreeFunc helper);

/*
 * return true if this map was created via hashMapClone
 *
 * @see hashMapClone
 */
bool hashMapIsClone(icHashMap *map);

/*
 * assign a key/value pair within the hash-map
 * if 'key' is already in the map, this will fail
 * as the 'value' should be released first.
 *
 * @note Use caution when key and value point to the same heap region: any custom entry destructors must not try to free both.
 *
 * @param map - the HashMap to add key/value to
 * @param key - the unique key to assign 'value'
 * @param keyLen - number of bytes the 'key' structure uses
 * @param value - optional, can be NULL if treating as a hash-set
 * @return true if successfully saved this key/value pair
 */
bool hashMapPut(icHashMap *map, void *key, uint16_t keyLen, void *value);

/**
 * Put a value in the map, copying the key and value.
 * @note If you don't want to store a value, pass NULL and 0 for value and valueLen
 * @see hashMapPut
 */
bool hashMapPutCopy(icHashMap *map, const void *key, uint16_t keyLen, const void *value, uint16_t valueLen);

/*
 * returns the 'value' for 'key'.  Note that the caller
 * should NOT free the value as this is simply a pointer
 * to the item that still exists within the structure
 *
 * @param map - the HashMap to get the value from
 * @param key - the value to search for
 * @param keyLen - number of bytes the 'key' structure uses
 */
void *hashMapGet(icHashMap *map, void *key, uint16_t keyLen);

/**
 * Determine if a key exists in the map, without getting its value.
 * @param map - the HashMap to search
 * @param key - the key
 * @param keyLen - sizeof(*key)
 * @return
 */
bool hashMapContains(icHashMap *map, void *key, uint16_t keyLen);

/*
 * removes the 'value' for 'key' from the map.
 *
 * @param map - the HashMap to delete
 * @param key - the value to search for
 * @param keyLen - number of bytes the 'key' structure uses
 * @param helper - optional, only needed if the items need a custom mechanism to release the memory
 */
bool hashMapDelete(icHashMap *map, void *key, uint16_t keyLen, hashMapFreeFunc helper);

/*
 * return the number of elements in the map
 *
 * @param map - the HashMap to count
 */
uint16_t hashMapCount(icHashMap *map);

/*
 * clear a hash-map and destroy the items.  Note that
 * each 'key' and 'value' will just be release via free()
 * unless a 'helper' is provided.
 *
 * @param map - the HashMap to clear
 * @param helper - optional, only needed if the items need a custom mechanism to release the memory
 */
void hashMapClear(icHashMap *map, hashMapFreeFunc helper);

/*-------------------------------*
 *
 *  icHashMapIterator
 *
 *-------------------------------*/

/*
 * the HashMap Iterator object
 */
typedef struct _icHashMapIterator icHashMapIterator;

/**
 * Convenience macro to create a scope bound hashMapIterator. When it goes out of scope,
 * hashMapIteratorDestroy will be called automatically.
 * @deprecated
 * @sa scoped_icHashMapIterator
 */
#define sbIcHashMapIterator AUTO_CLEAN(hashMapIteratorDestroy__auto) icHashMapIterator

/**
 * Convenience macro to create a scope bound hashMapIterator. When it goes out of scope,
 * hashMapIteratorDestroy will be called automatically.
 */
#define scoped_icHashMapIterator sbIcHashMapIterator

/*
 * mechanism for iterating the hash-map items
 * should be called initially to create the HashMapIterator,
 * then followed with calls to hashMapIteratorHasNext and
 * hashMapIteratorGetNext.  once the iteration is done, this
 * must be freed via hashMapDestroyIterator
 *
 * @param map - the HashMap to iterate
 * @see hashMapDestroyIterator()
 */
icHashMapIterator *hashMapIteratorCreate(icHashMap *map);

/*
 * free a HashMapIterator
 */
void hashMapIteratorDestroy(icHashMapIterator *iterator);

/**
 * HashMapIterator cleanup helper for use with AUTO_CLEAN
 * @param iter A pointer to an iterator pointer (provided by compiler)
 */
inline void hashMapIteratorDestroy__auto(icHashMapIterator **iter)
{
    hashMapIteratorDestroy(*iter);
}

/*
 * return if there are more items in the iterator to examine
 */
bool hashMapIteratorHasNext(icHashMapIterator *iterator);

/*
 * retrieve the next key/value pairing from the iterator
 *
 * @param iterator - the iterator to fetch from
 * @param key - output the key at 'next item'
 * @param keyLen - output the keyLen at 'next item'
 * @param value - output the value at 'next item'
 * @return false if the 'get' failed
 */
bool hashMapIteratorGetNext(icHashMapIterator *iterator, void **key, uint16_t *keyLen, void **value);

/*
 * deletes the current item (i.e. the item returned from the last call to
 * hashMapIteratorGetNext) from the underlying HashMap.
 * just like other 'delete' functions, will release the memory
 * via free() unless a helper is supplied.
 *
 * only valid after 'getNext' is called, and returns if the delete
 * was successful or not.
 */
bool hashMapIteratorDeleteCurrent(icHashMapIterator *iterator, hashMapFreeFunc helper);

#endif // IC_HASHMAP_H
