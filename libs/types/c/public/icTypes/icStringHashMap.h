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
 * icStringHashMap.h
 *
 * A more specialized version of icHashMap that is
 * a little bit easier to use when storing only
 * string types for keys and values.
 *
 * See icHashMap.h
 *
 * NOTE: this does not perform any mutex locking to
 *       allow for single-threaded usage without the
 *       overhead.  If locking is required, it should
 *       be performed by the caller
 *
 * Author: tlea - 8/12/15
 *-----------------------------------------------*/

#ifndef IC_STRINGHASHMAP_H
#define IC_STRINGHASHMAP_H

#include "icTypes/icHashMap.h"
#include "sbrm.h"
#include <cjson/cJSON.h>
#include <stdbool.h>
#include <stdint.h>

/*-------------------------------*
 *
 *  icStringHashMap
 *
 *-------------------------------*/

/*
 * the StringHashMap object representation.
 */
typedef struct _icStringHashMap icStringHashMap;

/*
 * create a new string hash-map.  will need to be released when complete
 *
 * @return a new StringHashMap object
 * @see stringHashMapDestroy()
 */
icStringHashMap *stringHashMapCreate();

/*
 * create a new string hash-map from an existing, copying all keys and values
 *
 * @return a new StringHashMap object with same items
 * @see stringHashMapDestroy()
 */
icStringHashMap *stringHashMapDeepClone(icStringHashMap *hashMap);

/*
 * destroy a string hash-map and cleanup memory.  Note that
 * each 'key' and 'value' will just be release via free()
 * unless a 'helper' is provided.
 *
 * @param map - the StringHashMap to delete
 * @param helper - optional, only needed if the items need a custom mechanism to release the memory
 */
inline void stringHashMapDestroy(icStringHashMap *map, hashMapFreeFunc helper)
{
    hashMapDestroy((icHashMap *) map, helper);
}

/**
 * Destroy a string hashmap, freeing all keys and values.
 * @param map
 */
inline void stringHashMapDestroyGeneric(icStringHashMap *map)
{
    stringHashMapDestroy(map, NULL);
}

DEFINE_DESTRUCTOR(icStringHashMap, stringHashMapDestroyGeneric);

/**
 * Convenience macro to declare a scope-bound string hash map that destroys its values
 * at the end of its lifetime.
 */
#define scoped_icStringHashMap AUTO_CLEAN(stringHashMapDestroyGeneric__auto) icStringHashMap

/**
 * Destroy a string hashmap without destroying keys nor values.
 * @param map
 */
inline void stringHashMapDestroyNoFree(icStringHashMap *map)
{
    stringHashMapDestroy(map, standardDoNotFreeHashMapFunc);
}

DEFINE_DESTRUCTOR(icStringHashMap, stringHashMapDestroyNoFree);

/**
 * Convenience macro to declare a scope-bound string hash map that does not destroy its keys and values
 * at the end of its lifetime.
 */
#define scoped_icStringHashMapNoFree AUTO_CLEAN(stringHashMapDestroyNoFree__auto) icStringHashMap

/**
 * Remove all items from the map
 * @param map
 */
void stringHashMapClear(icStringHashMap *map);

/*
 * assign a key/value pair within the string hash-map
 * if 'key' is already in the map, this will fail
 * as the 'value' should be released first.
 *
 * @param map - the StringHashMap to add key/value to
 * @param key - the unique key to assign 'value'
 * @param value
 * @return true if successfully saved this key/value pair
 */
bool stringHashMapPut(icStringHashMap *map, char *key, char *value);

/*
 * assign a key/value pair within the string hash-map
 * if 'key' is already in the map, this will fail
 * as the 'value' should be released first.  This will create
 * copies of key and value that the map will own
 *
 * @param map - the StringHashMap to add key/value to
 * @param key - the unique key to assign 'value'
 * @param value
 * @return true if successfully saved this key/value pair
 */
bool stringHashMapPutCopy(icStringHashMap *map, const char *key, const char *value);

/*
 * returns the 'value' for 'key'.  Note that the caller
 * should NOT free the value as this is simply a pointer
 * to the item that still exists within the structure
 *
 * @param map - the StringHashMap to get the value from
 * @param key - the value to search for
 */
char *stringHashMapGet(icStringHashMap *map, const char *key);

/*
 * Determine if a key exists in the map, without getting its value.
 * @param map - the stringHashMap to search
 * @param key - the key
 * @return true if key is in the map, false otherwise
 */
bool stringHashMapContains(icStringHashMap *map, const char *key);

/*
 * removes the 'value' for 'key' from the map.
 *
 * @param map - the HashMap to delete
 * @param key - the value to search for
 * @param keyLen - number of bytes the 'key' structure uses
 * @param helper - optional, only needed if the items need a custom mechanism to release the memory
 */
bool stringHashMapDelete(icStringHashMap *map, const char *key, hashMapFreeFunc helper);

/*
 * return the number of elements in the map
 *
 * @param map - the HashMap to count
 */
uint16_t stringHashMapCount(icStringHashMap *map);


/*-------------------------------*
 *
 *  icStringHashMapIterator
 *
 *-------------------------------*/

typedef struct _icStringHashMapIterator icStringHashMapIterator;

/*
 * mechanism for iterating the string hash-map items
 * should be called initially to create the StringHashMapIterator,
 * then followed with calls to stringHashMapIteratorHasNext and
 * stringHashMapIteratorGetNext.  Once the iteration is done, this
 * must be freed via stringHashMapDestroyIterator
 *
 * @param map - the StringHashMap to iterate
 * @see stringHashMapDestroyIterator()
 */
icStringHashMapIterator *stringHashMapIteratorCreate(icStringHashMap *map);

/*
 * free a StringHashMapIterator
 */
void stringHashMapIteratorDestroy(icStringHashMapIterator *iterator);

DEFINE_DESTRUCTOR(icStringHashMapIterator, stringHashMapIteratorDestroy);

/**
 * Convenience macro to create a scope bound stringHashMapIterator
 */
#define scoped_icStringHashMapIterator AUTO_CLEAN(stringHashMapIteratorDestroy__auto) icStringHashMapIterator

/*
 * return if there are more items in the iterator to examine
 */
bool stringHashMapIteratorHasNext(icStringHashMapIterator *iterator);

/*
 * retrieve the next key/value pairing from the iterator
 *
 * @param iterator - the iterator to fetch from
 * @param key - output the key at 'next item'
 * @param value - output the value at 'next item'
 */
bool stringHashMapIteratorGetNext(icStringHashMapIterator *iterator, char **key, char **value);

/*
 * deletes the current item (i.e. the item returned from the last call to
 * stringHashMapIteratorGetNext) from the underlying HashMap.
 * just like other 'delete' functions, will release the memory
 * via free() unless a helper is supplied.
 *
 * only valid after 'getNext' is called, and returns if the delete
 * was successful or not.
 */
bool stringHashMapIteratorDeleteCurrent(icStringHashMapIterator *iterator, hashMapFreeFunc helper);

/**
 * Convenience function to convert a map to a JSON string containing a key-value object
 * @param map
 * @param formatPretty if true, the serializer will format the output with spaces, newlines, etc.
 * @return a JSON string (caller must free when done)
 */
char *stringHashMapJSONSerialize(const icStringHashMap *map, bool formatPretty);

/**
 * Convenience function to convert a JSON key-value object string to a map
 * @param in a JSON string containing a key-value map object
 * @return a map (caller must free when done), or NULL is not a key-value set containing
 *         only string values
 */
icStringHashMap *stringHashMapJSONDeserialize(const char *in);

/**
 * Convert a map to a cJSON key-value object
 * @param map
 * @return A cJSON key-value map (caller must free when done)
 */
cJSON *stringHashMapToJSON(const icStringHashMap *map);

/**
 * Convert a cJSON object to a map
 * @param in A cJSON key-value object containing only string or null values
 * @return a map representing the cJSON object (caller must free).
 */
icStringHashMap *stringHashMapFromJSON(const cJSON *in);

#endif // IC_STRINGHASHMAP_H
