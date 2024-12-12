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
 * icHashMap.c
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

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "icTypes/icHashMap.h"
#include "icTypes/icLinkedList.h"
#include "list.h"

// Define our 'map' so that it's location doesn't change - allowing the
// caller to create the data-structure and not have to worry about
// the map being new, full, etc.
//
// Like typical Hash implementations, keep an array of 'buckets'
// that have the hashed value of 'key', and a LinkedList
// containing the 'values'.  This allows collision in the hashing
// while providing decent performance of searching
//
//  mapHead        LinkedList
//  +---------+    +---------+     +---------+
//  |  +---+  |    |         |     |         |
//  |  | B |--+--->|  item --+---> |  item --+--> NULL
//  |  +---+  |    |         |     |         |
//  |         |    +---------+     +---------+
//  |         |
//  |         |    +---------+
//  |  +---+  |    |         |
//  |  | B |--+--->|  item --+---> NULL
//  |  +---+  |    |         |
//  |         |    +---------+
//  +---------+
//

// define number of buckets to create.  should be small and
// 'prime' if possible since it dictates how much memory
// we allocated in the hash, but also used to determine
// the "hash - to - bucket" distribution
//
#define NUM_BUCKETS  31
#define USEC_PER_SEC 1000000

extern inline void hashMapIteratorDestroy__auto(icHashMapIterator **iter);

/*
 * container of the hashed number and the LinkedList
 * of mapItem elements that have the same hashed number
 */
typedef struct _mapBucket
{
    //    unsigned int hashedKey;
    icLinkedList *values; // lazy load; NULL until something appears in the bucket
} mapBucket;

/*
 * data element saved within the LinkedList
 */
typedef struct _mapItem
{
    void *key;       // original key
    uint16_t keyLen; // length (in bytes) of the key
    void *value;     // original value
} mapItem;

/*
 * the head of our hash-map
 */
struct _icHashMap
{
    uint16_t count;                 // total number of mapItem instances in the hash
    uint32_t seed;                  // random seed for this map
    mapBucket buckets[NUM_BUCKETS]; // array of buckets
    bool cloned;
};

/*
 * model the iterator
 */
struct _icHashMapIterator
{
    icHashMap *head;                    // top of the hash map
    int currBucket;                     // bucket currently being iterated
    icLinkedListIterator *currIterator; // linkedList iterator helper
                                        //    icLinkedList *prevList;               // used for 'delete'
    mapItem *prevItem;                  // used for 'delete'
};

/*
 * container used during 'find' calls to linked list
 */
typedef struct _searchContainer
{
    void *key;
    unsigned int keyLen;
} searchContainer;

/*
 * Helper structure to use when deep cloning the list of mapItems
 */
typedef struct
{
    hashMapCloneFunc hashMapCloneFunc;
    void *hashMapCloneContext;
} deepCloneMapItemContext;

/*
 * private functions
 */
static mapItem *searchHashMap(icHashMap *map, void *key, uint16_t keyLen);
static mapItem *searchBucket(icHashMap *map, int bucket, void *key, unsigned int keyLen);
static bool searchLinkedList(void *searchVal, void *item);
static uint32_t createSeed();
static uint32_t defaultHash(const char *key, uint16_t keyLen, uint32_t seed);
static uint16_t assignedBucket(uint32_t hash);
static mapItem *createItem();
static void destroyItem(mapItem *item, hashMapFreeFunc helper);
static void *hashMapDeepCloneHelper(void *item, void *context);

/*-------------------------------*
 *
 *  icHashMap
 *
 *-------------------------------*/

/*
 * create a new hash-map.  will need to be released when complete
 *
 * @return a new HashMap object
 * @see hashMapDestroy()
 */
icHashMap *hashMapCreate()
{
    // allocate mem, and clear it all out
    //
    icHashMap *retVal = (icHashMap *) calloc(1, sizeof(icHashMap));
    if (retVal == NULL)
    {
        return NULL;
    }

    // create a seed for this hash instance
    //
    retVal->seed = createSeed();
    retVal->cloned = false;

    return retVal;
}

/*
 * create a shallow-clone of an existing hash map.
 * will need to be released when complete, HOWEVER,
 * the items in the cloned list are not owned by it
 * and therefore should NOT be released.
 *
 * @see hashMapDestroy()
 * @see hashMapIsClone()
 */
icHashMap *hashMapClone(icHashMap *src)
{
    // allocate the space
    //
    icHashMap *retVal = (icHashMap *) malloc(sizeof(icHashMap));
    if (retVal == NULL)
    {
        return NULL;
    }
    memset(retVal, 0, sizeof(icHashMap));
    retVal->seed = src->seed;
    retVal->cloned = true;

    // iterate through the containers within 'src'
    // and copy each, but not the item within
    //
    int i;
    for (i = 0; i < NUM_BUCKETS; i++)
    {
        if (src->buckets[i].values != NULL)
        {
            retVal->buckets[i].values = linkedListClone(src->buckets[i].values);
        }
    }

    retVal->count = src->count;

    return retVal;
}

/*
 * create a deep-clone of an existing hash map.
 * will need to be released when complete.
 *
 * @see hashMapDestroy()
 */
icHashMap *hashMapDeepClone(icHashMap *src, hashMapCloneFunc cloneFunc, void *context)
{
    // allocate the space
    //
    icHashMap *retVal = (icHashMap *) malloc(sizeof(icHashMap));
    if (retVal == NULL)
    {
        return NULL;
    }
    memset(retVal, 0, sizeof(icHashMap));
    retVal->seed = src->seed;

    // Setup the context object for when we deep clone the mapItems
    deepCloneMapItemContext ctx = {.hashMapCloneFunc = cloneFunc, .hashMapCloneContext = context};

    // iterate through the containers within 'src'
    // and copy each
    //
    int i;
    for (i = 0; i < NUM_BUCKETS; i++)
    {
        if (src->buckets[i].values != NULL)
        {
            retVal->buckets[i].values = linkedListDeepClone(src->buckets[i].values, hashMapDeepCloneHelper, &ctx);
        }
    }
    retVal->count = src->count;

    return retVal;
}

/*
 * common implementation of the hashMapFreeFunc function that can be
 * used in situations where the 'key' and 'value' are not freed at all.
 */
void standardDoNotFreeHashMapFunc(void *key, void *value)
{
    // do nothing
}

/*
 * destroy a hash-map and cleanup memory.  Note that
 * each 'key' and 'value' will just be release via free()
 * unless a 'helper' is provided.
 *
 * @param map - the HashMap to delete
 * @param helper - optional, only needed if the items need a custom mechanism to release the memory
 */
void hashMapDestroy(icHashMap *map, hashMapFreeFunc helper)
{
    if (map == NULL)
    {
        return;
    }

    // use 'clear' to remove and cleanup all of the items in the hashMap
    //
    hashMapClear(map, helper);

    // now the map structure itself
    //
    free(map);
}

/*
 * return true if this map was created via hashMapClone
 *
 * @see hashMapClone
 */
bool hashMapIsClone(icHashMap *map)
{
    if (map == NULL)
    {
        return false;
    }
    return map->cloned;
}

/*
 * assign a key/value pair within the hash-map
 * if 'key' is already in the map, this will fail
 * as the 'value' should be released first.
 *
 * @param map - the HashMap to add key/value to
 * @param key - the unique key to assign 'value'
 * @param keyLen - number of bytes the 'key' structure uses
 * @param value - optional, can be NULL if treating as a hash-set
 * @return true if successfully saved this key/value pair
 */
bool hashMapPut(icHashMap *map, void *key, uint16_t keyLen, void *value)
{
    if (map == NULL || key == NULL)
    {
        return false;
    }

    // find the bucket to place this in
    //
    unsigned int hash = defaultHash(key, keyLen, map->seed);
    unsigned int b = assignedBucket(hash);

    // go to said 'bucket' and iterate the list to make sure we don't have
    // another item with the same 'key'
    //
    bool doAdd = true;
    if (map->buckets[b].values == NULL)
    {
        // no list for this bucket yet
        //
        map->buckets[b].values = linkedListCreate();
    }
    else
    {
        // search, but use a container so we can pass in the key + len
        //
        if (searchBucket(map, b, key, keyLen) != NULL)
        {
            // found an item in the list with the same key
            //
            doAdd = false;
        }
    }

    if (doAdd == true)
    {
        // not in the list, so add it
        //
        mapItem *node = createItem();
        node->key = key;
        node->keyLen = keyLen;
        node->value = value;
        linkedListAppend(map->buckets[b].values, node);

        map->count++;
    }

    return doAdd;
}

bool hashMapPutCopy(icHashMap *map, const void *key, uint16_t keyLen, const void *value, uint16_t valueLen)
{
    void *kp = malloc(keyLen);
    void *vp = NULL;

    if (valueLen > 0)
    {
        vp = malloc(valueLen);
        memcpy(vp, value, valueLen);
    }

    bool ok = hashMapPut(map, memcpy(kp, key, keyLen), keyLen, vp);
    if (!ok)
    {
        free(kp);
        free(vp);
    }
    return ok;
}

/*
 * returns the 'value' for 'key'.  Note that the caller
 * should NOT free the value as this is simply a pointer
 * to the item that still exists within the structure
 *
 * @param map - the HashMap to add key/value to
 * @param key - the value to search for
 * @param keyLen - number of bytes the 'key' structure uses
 */
void *hashMapGet(icHashMap *map, void *key, uint16_t keyLen)
{
    void *value = NULL;
    mapItem *item = searchHashMap(map, key, keyLen);

    if (item)
    {
        value = item->value;
    }

    return value;
}

bool hashMapContains(icHashMap *map, void *key, uint16_t keyLen)
{
    return searchHashMap(map, key, keyLen) != NULL;
}

/*
 * removes the 'value' for 'key' from the map.
 *
 * @param map - the HashMap to delete
 * @param key - the value to search for
 * @param keyLen - number of bytes the 'key' structure uses
 * @param helper - optional, only needed if the items need a custom mechanism to release the memory
 */
bool hashMapDelete(icHashMap *map, void *key, uint16_t keyLen, hashMapFreeFunc helper)
{
    if (map == NULL || key == NULL)
    {
        return false;
    }

    // find the bucket this key would be in
    //
    uint32_t hash = defaultHash(key, keyLen, map->seed);
    uint16_t b = assignedBucket(hash);

    // make sure the buck is initialized and there
    //
    if (map->buckets[b].values != NULL)
    {
        // search through this bucket for the element with the same key
        //
        mapItem *found = searchBucket(map, b, key, keyLen);
        if (found != NULL)
        {
            // tell the list to delete it, but NOT TO FREE the item
            // unfortunately the linkedList only allows us to pass
            // 1 value to use for comparison, therefore create a container
            // to supply the key + len
            // Yes, this is duplicated from 'searchBucket', but need the same search for the 'delete'
            //
            searchContainer search;
            memset(&search, 0, sizeof(searchContainer));
            search.key = key;
            search.keyLen = keyLen;
            linkedListDelete(map->buckets[b].values, &search, searchLinkedList, standardDoNotFreeFunc);

            // NOTE: if the list is empty, leave it in place.  in a perfect world,
            // we would also destroy the list, however if we're doing this during
            // an iterator...it can be catastrophic

            if (map->cloned == false)
            {
                // now free the memory here where we can control who does it
                //
                destroyItem(found, helper);
            }

            // update counter
            //
            if (map->count > 0)
            {
                map->count--;
            }
            return true;
        }
    }

    return false;
}

/*
 * return the number of elements in the map
 *
 * @param map - the HashMap to count
 */
uint16_t hashMapCount(icHashMap *map)
{
    if (map == NULL)
    {
        return 0;
    }
    return map->count;
}

/*
 * clear a hash-map and destroy the items.  Note that
 * each 'key' and 'value' will just be release via free()
 * unless a 'helper' is provided.
 *
 * @param map - the HashMap to clear
 * @param helper - optional, only needed if the items need a custom mechanism to release the memory
 */
void hashMapClear(icHashMap *map, hashMapFreeFunc helper)
{
    if (map == NULL)
    {
        return;
    }

    // loop through all of our buckets, freeing the linked lists, keys, and values
    //
    int i;
    for (i = 0; i < NUM_BUCKETS; i++)
    {
        if (map->buckets[i].values != NULL)
        {
            // have a LinkedList for this bucket.
            // the trick is to call our 'helper' for each so that
            // both the key and the value can be released before
            // we destroy the list
            //
            icLinkedListIterator *loop = linkedListIteratorCreate(map->buckets[i].values);
            while (linkedListIteratorHasNext(loop) == true)
            {
                // ask the linked list iterator to delete the node,
                // but not to free the memory inside the element.
                // this way we can call the correct function to free the memory
                //
                mapItem *next = linkedListIteratorGetNext(loop);
                linkedListIteratorDeleteCurrent(loop, standardDoNotFreeFunc);

                if (map->cloned == false)
                {
                    // now that the list good, destroy the mapItem
                    //
                    destroyItem(next, helper);
                }
            }

            // destroy the iterator & the list (note that we tell the
            // list to not free the nodes - since we already did that)
            //
            linkedListIteratorDestroy(loop);
            linkedListDestroy(map->buckets[i].values, standardDoNotFreeFunc);
            map->buckets[i].values = NULL;
        }
    }

    // reset internals
    //
    map->count = 0;
    map->cloned = false;
}

static mapItem *searchHashMap(icHashMap *map, void *key, uint16_t keyLen)
{
    if (map == NULL || key == NULL)
    {
        return NULL;
    }

    // find the bucket this key would be in
    //
    unsigned int hash = defaultHash(key, keyLen, map->seed);
    unsigned int b = assignedBucket(hash);

    // make sure the buck is initialized and there
    //
    if (map->buckets[b].values != NULL)
    {
        // search through this bucket for the element with the same key
        //
        mapItem *found = searchBucket(map, b, key, keyLen);
        if (found != NULL)
        {
            // found an item in the list with the same key
            //
            return found;
        }
    }

    return NULL;
}

/*
 *
 */
static mapItem *searchBucket(icHashMap *map, int bucket, void *key, unsigned int keyLen)
{
    // perform the search in the linked list at 'bucket'
    // unfortunately the linkedList only allows us to pass
    // 1 value to use for comparison, therefore create a container
    // to supply the key + len
    //
    searchContainer search;
    memset(&search, 0, sizeof(searchContainer));
    search.key = key;
    search.keyLen = keyLen;

    // let linked list do the rest
    //
    return linkedListFind(map->buckets[bucket].values, &search, searchLinkedList);
}

/*
 * supplied to linkedListFind() when looking for an
 * element that has a matching 'key'
 */
static bool searchLinkedList(void *searchVal, void *item)
{
    // item should be a 'mapItem'
    // searchVal should be a 'searchContainer'
    //
    mapItem *node = (mapItem *) item;
    searchContainer *wrap = (searchContainer *) searchVal;

    // first compare lengths
    //
    if (node->keyLen != wrap->keyLen)
    {
        return false;
    }

    // lengths are the same, so compare the memory
    //
    if (memcmp(wrap->key, node->key, node->keyLen) == 0)
    {
        // found match
        //
        return true;
    }

    return false;
}

/*
 * use current time as a 'seed' for the hashing
 */
static unsigned int createSeed()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned int) (tv.tv_sec * USEC_PER_SEC + tv.tv_usec);
}

/*
 * calculate a hash number based on 'key' and a 'seed'
 */
static uint32_t defaultHash(const char *key, uint16_t keyLen, uint32_t seed)
{
    // similar to the 'times 33' hash algo used by Perl and Berkley DB
    //
    unsigned char *ptr = (unsigned char *) key;
    uint16_t i = 0;
    uint32_t hash = seed;

    while (i < keyLen)
    {
        // multiply current hash by 33, then add the value
        // of the current offset character
        //
        hash = (hash * 33) + *ptr;

        // next char
        //
        i++;
        ptr++;
    }

    return hash;
}

/*
 * given a hash (assume result of defaultHash() call), determine
 * which bucket to assign the hash value
 */
static uint16_t assignedBucket(unsigned int hash)
{
    // probably a better way to do this, but simply
    // divide by the max slots (actually modulus)
    //
    return (uint16_t) (hash % NUM_BUCKETS);
}

/*
 * internal helper to allocate mem (and clear)
 */
static mapItem *createItem()
{
    mapItem *node = (mapItem *) malloc(sizeof(mapItem));
    memset(node, 0, sizeof(mapItem));
    return node;
}

/*
 * internal helper to totally free a hashItem
 */
static void destroyItem(mapItem *item, hashMapFreeFunc helper)
{
    // of we have a 'helper', let it do the free
    //
    if (helper != NULL)
    {
        helper(item->key, item->value);
    }
    else
    {
        /* Key and value may be the same, so don't free the same region twice */
        if (item->key != item->value)
        {
            free(item->value);
        }
        free(item->key);

        item->key = NULL;
        item->value = NULL;
    }

    free(item);
}


/*-------------------------------*
 *
 *  icHashMapIterator
 *
 *-------------------------------*/

/*
 * mechanism for iterating the hash-map items
 * should be called initially to create the HashMapIterator,
 * then followed with calls to hashMapIteratorHasNext and
 * hashMapIteratorGetNext.  once the iteration is done, this
 * must be freed via hashMapIteratorDestroy
 *
 * @param map - the HashMap to iterate
 * @see destroyHashMapIterator()
 */
icHashMapIterator *hashMapIteratorCreate(icHashMap *map)
{
    if (map == NULL)
    {
        return NULL;
    }

    // create the bare struct
    //
    icHashMapIterator *retVal = (icHashMapIterator *) malloc(sizeof(icHashMapIterator));
    memset(retVal, 0, sizeof(icHashMapIterator));

    // fill in minimal needed
    //
    retVal->head = map;
    retVal->currBucket = -1;

    return retVal;
}

/*
 * free a HashMapIterator
 */
void hashMapIteratorDestroy(icHashMapIterator *iterator)
{
    if (iterator != NULL)
    {
        // kill linked list iterator helpers
        //
        if (iterator->currIterator != NULL)
        {
            linkedListIteratorDestroy(iterator->currIterator);
            iterator->currIterator = NULL;
        }
        iterator->prevItem = NULL;
        //    iterator->prevList = NULL;
        iterator->head = NULL;
        free(iterator);
    }
}

/*
 * find next populated bucket within the map
 */
static int findNextOccupiedSlot(icHashMapIterator *node)
{
    // advance to the next one (assume current is empty or exhausted)
    //
    int bucket = node->currBucket + 1;

    // loop until we find a bucket that has items
    // or hit the logical end
    //
    while (bucket < NUM_BUCKETS)
    {
        if (node->head != NULL && node->head->buckets[bucket].values != NULL &&
            linkedListCount(node->head->buckets[bucket].values) > 0)
        {
            // this bucket has data, return this bucket number
            //
            return bucket;
        }

        // nothing there, go to the next one
        //
        bucket++;
    }

    // if we got here, no more populated buckets
    //
    return -1;
}

/*
 * move iterator to next populated bucket within the map
 */
static void useSlot(icHashMapIterator *node, int bucket)
{
    // save current iterator as the 'prev'.  only going to
    // keep it around in case the caller wants to delete
    // what was returned from the last 'getNext'.  handles the
    // scenario where getNext moved us forward to the next
    // bucket, and need to keep a pointer backward for 'delete'
    //
    if (node->currIterator != NULL)
    {
        linkedListIteratorDestroy(node->currIterator);
    }

    // assign current bucket
    //
    node->currBucket = bucket;

    // create new iterator
    //
    node->currIterator = linkedListIteratorCreate(node->head->buckets[bucket].values);
}

/*
 * return if there are more items in the iterator to examine
 */
bool hashMapIteratorHasNext(icHashMapIterator *iterator)
{
    if (iterator == NULL)
    {
        return false;
    }

    // see if this is the first time through
    //
    if (iterator->currBucket < 0)
    {
        // get the first bucket that has values
        //
        int x = findNextOccupiedSlot(iterator);
        if (x != -1)
        {
            // got a good slot - so set it up and return
            //
            useSlot(iterator, x);
            return true;
        }

        // totally empty?
        //
        return false;
    }

    // see if we went past the boundry
    //
    else if (iterator->currBucket >= NUM_BUCKETS)
    {
        return false;
    }

    // see if we have an established linked list iterator
    //
    if (iterator->currIterator != NULL)
    {
        // if it has 'next', then we're good to go
        //
        if (linkedListIteratorHasNext(iterator->currIterator) == true)
        {
            return true;
        }

        // this bucket has been exhausted, so free the iterator and
        // fall through to check out the next bucket
        //
        linkedListIteratorDestroy(iterator->currIterator);
        iterator->currIterator = NULL;
    }

    // between 0 and MAX, so search for the next bucket
    // that has data in it
    //
    int x = findNextOccupiedSlot(iterator);
    if (x != -1)
    {
        // good slot, use it and return
        //
        useSlot(iterator, x);
        return true;
    }

    // must have iterated through them all
    //
    return false;
}

/*
 * retrieve the next key/value pairing from the iterator
 *
 * @param iterator - the iterator to fetch from
 * @param key - output the key at 'next item'
 * @param keyLen - output the keyLen at 'next item'
 * @param value - output the value at 'next item'
 * @return false if the 'get' failed
 */
bool hashMapIteratorGetNext(icHashMapIterator *iterator, void **key, uint16_t *keyLen, void **value)
{
    if (iterator == NULL)
    {
        return false;
    }

    // assume that the 'has next' was called to setup our helper
    // and put us in the correct place
    //
    if (iterator->currIterator != NULL)
    {
        mapItem *element = (mapItem *) linkedListIteratorGetNext(iterator->currIterator);
        if (element != NULL)
        {
            // apply values from 'mapItem' to the in/out parms
            //
            *key = element->key;
            *keyLen = element->keyLen;
            *value = element->value;

            // before returning (or moving forward), save this list & item
            // to support the 'delete' function (if it is called).  note that
            // we are choosing NOT to use the delete with in the linked list iterator
            // due to the complexity of multiple lists/iterators to synchronize on
            //
            //            iterator->prevList = iterator->head->buckets[iterator->currBucket].values;
            iterator->prevItem = element;

            return true;
        }
    }

    // problem retrieving the next element
    //
    //    iterator->prevList = NULL;
    iterator->prevItem = NULL;
    return false;
}

/*
 * deletes the current item (i.e. the item returned from the last call to
 * hashMapIteratorGetNext) from the underlying HashMap.
 * just like other 'delete' functions, will release the memory
 * via free() unless a helper is supplied.
 *
 * only valid after 'getNext' is called, and returns if the delete
 * was successful or not.
 */
bool hashMapIteratorDeleteCurrent(icHashMapIterator *iterator, hashMapFreeFunc helper)
{
    if (iterator == NULL)
    {
        return false;
    }

    // only valid if 'getNext' was successfully called
    //
    //    if (iterator->prevList != NULL && iterator->prevItem != NULL)
    if (iterator->prevItem != NULL)
    {
        // inform the linked list iterator that we're deleting what it
        // would call 'prev'
        //
        linkedListIteratorClearPrev(iterator->currIterator);

        // remove the 'prevItem' container from the hash.  this should be safe
        // to perform since the iterator has already moved passed this element
        // AND we're not using the 'delete' functionality of the linkedList iterator
        //
        bool retVal = hashMapDelete(iterator->head, iterator->prevItem->key, iterator->prevItem->keyLen, helper);

        // prevent double delete
        //
        iterator->prevItem = NULL;
        return retVal;
    }

    return false;
}

/*
 * Helper function for deep cloning the list of mapItems
 */
static void *hashMapDeepCloneHelper(void *item, void *context)
{
    deepCloneMapItemContext *ctx = (deepCloneMapItemContext *) context;
    mapItem *node = (mapItem *) item;
    mapItem *newNode = createItem();
    ctx->hashMapCloneFunc(node->key, node->value, &(newNode->key), &(newNode->value), ctx->hashMapCloneContext);
    newNode->keyLen = node->keyLen;

    return newNode;
}
