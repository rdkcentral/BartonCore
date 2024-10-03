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
 * icStringHashMap.c
 *
 * Simplistic string hash-map to provide the basic need for
 * dynamic unordered collections.
 *
 * This is just a wrapper around icHashMap that makes it a bit
 * easier to use when working with string->string maps.
 *
 * See icHashMap.c
 *
 * NOTE: this does not perform any mutex locking to
 *       allow for single-threaded usage without the
 *       overhead.  If locking is required, it should
 *       be performed by the caller
 *
 * Author: tlea - 8/12/15
 *-----------------------------------------------*/

#include <cjson/cJSON.h>
#include <icTypes/icLinkedList.h>
#include <stdlib.h>
#include <string.h>

#include "icTypes/icHashMap.h"
#include "icTypes/icStringHashMap.h"

/*
 * create a new hash-map.  will need to be released when complete
 *
 * @return a new StringHashMap object
 * @see stringHashMapDestroy()
 */
icStringHashMap *stringHashMapCreate()
{
    return (icStringHashMap *) hashMapCreate();
}

static void keyValueStringClone(void *key, void *value, void **clonedKey, void **clonedValue, void *context)
{
    (void) context; // Unused
    if (key != NULL)
    {
        *clonedKey = strdup((char *) key);
    }
    if (value != NULL)
    {
        *clonedValue = strdup((char *) value);
    }
}

/*
 * create a new string hash-map from an existing, copying all keys and values
 *
 * @return a new StringHashMap object with same items
 * @see stringHashMapDestroy()
 */
icStringHashMap *stringHashMapDeepClone(icStringHashMap *hashMap)
{
    return (icStringHashMap *) hashMapDeepClone((icHashMap *) hashMap, keyValueStringClone, NULL);
}

/*
 * destroy a string hash-map and cleanup memory.  Note that
 * each 'key' and 'value' will just be release via free()
 * unless a 'helper' is provided.
 *
 * @param map - the StringHashMap to delete
 * @param helper - optional, only needed if the items need a custom mechanism to release the memory
 */
extern inline void stringHashMapDestroy(icStringHashMap *map, hashMapFreeFunc helper);

extern inline void stringHashMapDestroyGeneric(icStringHashMap *map);

DECLARE_DESTRUCTOR(icStringHashMap, stringHashMapDestroyGeneric);

extern inline void stringHashMapDestroyNoFree(icStringHashMap *map);

DECLARE_DESTRUCTOR(icStringHashMap, stringHashMapDestroyNoFree);

void stringHashMapClear(icStringHashMap *map)
{
    hashMapClear((icHashMap *) map, NULL);
}

/*
 * assign a key/value pair within the hash-map
 * if 'key' is already in the map, this will fail
 * as the 'value' should be released first.
 *
 * @param map - the StringHashMap to add key/value to
 * @param key - the unique key to assign 'value'
 * @param value
 * @return true if successfully saved this key/value pair
 */
bool stringHashMapPut(icStringHashMap *map, char *key, char *value)
{
    return hashMapPut((icHashMap *) map, key, (uint16_t) (strlen(key) + 1), value);
}

/*
 * assign a key/value pair within the string hash-map
 * if 'key' is already in the map, this will fail
 * as the 'value' should be released first.  This will create
 * copies of key and value that the map will own
 *
 * @param map - the StringHashMap to add key/value to
 * @param key - the unique key to assign 'value'
 * @param value a string or NULL
 * @return true if successfully saved this key/value pair
 */
bool stringHashMapPutCopy(icStringHashMap *map, const char *key, const char *value)
{
    if (key == NULL)
    {
        return false;
    }

    uint16_t valLen = 0;
    if (value != NULL)
    {
        valLen = strlen(value) + 1;
    }

    return hashMapPutCopy((icHashMap *) map, key, strlen(key) + 1, value, valLen);
}

/*
 * returns the 'value' for 'key'.  Note that the caller
 * should NOT free the value as this is simply a pointer
 * to the item that still exists within the structure
 *
 * @param map - the StringHashMap to get the value from
 * @param key - the value to search for
 */
char *stringHashMapGet(icStringHashMap *map, const char *key)
{
    return (char *) hashMapGet((icHashMap *) map, (void *) key, (uint16_t) (strlen(key) + 1));
}

/*
 * Determine if a key exists in the map, without getting its value.
 * @param map - the stringHashMap to search
 * @param key - the key
 * @return true if key is in the map, false otherwise
 */
bool stringHashMapContains(icStringHashMap *map, const char *key)
{
    return hashMapContains((icHashMap *) map, (void *) key, (uint16_t) (strlen(key) + 1));
}

/*
 * removes the 'value' for 'key' from the map.
 *
 * @param map - the StringHashMap to delete
 * @param key - the value to search for
 * @param helper - optional, only needed if the items need a custom mechanism to release the memory
 */
bool stringHashMapDelete(icStringHashMap *map, const char *key, hashMapFreeFunc helper)
{
    return hashMapDelete((icHashMap *) map, (void *) key, (uint16_t) (strlen(key) + 1), helper);
}

/*
 * return the number of elements in the map
 *
 * @param map - the HashMap to count
 */
uint16_t stringHashMapCount(icStringHashMap *map)
{
    return hashMapCount((icHashMap *) map);
}

/*
 * mechanism for iterating the string hash-map items
 * should be called initially to create the StringHashMapIterator,
 * then followed with calls to stringHashMapIteratorHasNext and
 * stringHashMapIteratorGetNext.  once the iteration is done, this
 * must be freed via stingHashMapIteratorDestroy
 *
 * @param map - the StringHashMap to iterate
 * @see stringHashMapIteratorDestroy()
 */
icStringHashMapIterator *stringHashMapIteratorCreate(icStringHashMap *map)
{
    return (icStringHashMapIterator *) hashMapIteratorCreate((icHashMap *) map);
}

/*
 * free a StringHashMapIterator
 */
void stringHashMapIteratorDestroy(icStringHashMapIterator *iterator)
{
    hashMapIteratorDestroy((icHashMapIterator *) iterator);
}

DECLARE_DESTRUCTOR(icStringHashMapIterator, stringHashMapIteratorDestroy);

/*
 * return if there are more items in the iterator to examine
 */
bool stringHashMapIteratorHasNext(icStringHashMapIterator *iterator)
{
    return hashMapIteratorHasNext((icHashMapIterator *) iterator);
}

/*
 * retrieve the next key/value pairing from the iterator
 *
 * @param iterator - the iterator to fetch from
 * @param key - output the key at 'next item'
 * @param keyLen - output the keyLen at 'next item'
 * @param value - output the value at 'next item'
 */
bool stringHashMapIteratorGetNext(icStringHashMapIterator *iterator, char **key, char **value)
{
    uint16_t keyLen; // unused
    return hashMapIteratorGetNext((icHashMapIterator *) iterator, (void **) key, &keyLen, (void **) value);
}

/*
 * deletes the current item (i.e. the item returned from the last call to
 * stringHashMapIteratorGetNext) from the underlying HashMap.
 * just like other 'delete' functions, will release the memory
 * via free() unless a helper is supplied.
 *
 * only valid after 'getNext' is called, and returns if the delete
 * was successful or not.
 */
bool stringHashMapIteratorDeleteCurrent(icStringHashMapIterator *iterator, hashMapFreeFunc helper)
{
    return hashMapIteratorDeleteCurrent((icHashMapIterator *) iterator, helper);
}

/**
 * Convenience function to convert a map to a JSON string containing a key-value object
 * @param map
 * @param formatPretty if true, the serializer will format the output with spaces, newlines, etc.
 * @return a JSON string (caller must free when done)
 */
char *stringHashMapJSONSerialize(const icStringHashMap *map, bool format)
{
    cJSON *tmp = stringHashMapToJSON(map);
    char *out = NULL;

    if (tmp != NULL)
    {
        if (format)
        {
            out = cJSON_Print(tmp);
        }
        else
        {
            out = cJSON_PrintUnformatted(tmp);
        }

        cJSON_Delete(tmp);
    }

    return out;
}

/**
 * Convenience function to convert a JSON key-value object string to a map
 * @param in a JSON string containing a key-value map object
 * @return a map (caller must free when done), or NULL is not a key-value set containing
 *         only string values
 */
icStringHashMap *stringHashMapJSONDeserialize(const char *in)
{
    cJSON *tmp = cJSON_Parse(in);
    icStringHashMap *out = NULL;

    if (tmp != NULL)
    {
        out = stringHashMapFromJSON(tmp);
        cJSON_Delete(tmp);
    }

    return out;
}

/**
 * Convert a map to a cJSON key-value object
 * @param map
 * @return A cJSON key-value map (caller must free when done)
 */
cJSON *stringHashMapToJSON(const icStringHashMap *map)
{
    if (map == NULL)
    {
        return NULL;
    }

    cJSON *out = cJSON_CreateObject();

    icStringHashMapIterator *it = stringHashMapIteratorCreate((icStringHashMap *) map);

    while (stringHashMapIteratorHasNext(it))
    {
        char *key = NULL;
        char *value = NULL;
        stringHashMapIteratorGetNext(it, &key, &value);

        cJSON *jsonValue = NULL;
        if (value != NULL)
        {
            jsonValue = cJSON_CreateString(value);
        }
        else
        {
            jsonValue = cJSON_CreateNull();
        }

        cJSON_AddItemToObject(out, key, jsonValue);
    }

    stringHashMapIteratorDestroy(it);

    return out;
}

/**
 * Convert a cJSON object to a map
 * @param in A cJSON key-value object containing only string or null values
 * @return a map representing the cJSON object (caller must free).
 */
icStringHashMap *stringHashMapFromJSON(const cJSON *in)
{
    if (in == NULL)
    {
        return NULL;
    }

    if (!cJSON_IsObject(in))
    {
        return NULL;
    }

    icStringHashMap *map = stringHashMapCreate();
    cJSON *item = NULL;
    bool valid = true;

    cJSON_ArrayForEach(item, in)
    {
        if (cJSON_IsString(item) || cJSON_IsNull(item))
        {
            if (!stringHashMapPutCopy(map, item->string, item->valuestring))
            {
                valid = false;
                break;
            }
        }
        else
        {
            valid = false;
            break;
        }
    }

    if (!valid)
    {
        stringHashMapDestroy(map, NULL);
        map = NULL;
    }

    return map;
}
