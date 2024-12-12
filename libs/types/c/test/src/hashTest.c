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
// Created by John Elderton on 6/18/15.
//


#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashTest.h"
#include "icTypes/icHashMap.h"
#include <icTypes/sbrm.h>
#include <sys/time.h>

#define NUM_BUCKETS  31
#define USEC_PER_SEC 1000000

/*
 * calculate a hash number based on 'key' and a 'seed'
 * NOTE: this is a copy of the 'defaultHash' function
 *       within icHashMap.c so we can validate the algorithm
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
static unsigned int assignedBucket(unsigned int hash)
{
    // probably a better way to do this, but simply
    // divide by the max slots (actually modulus)
    //
    return (hash % NUM_BUCKETS);
}

static unsigned long long getTime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long long) (tv.tv_sec * USEC_PER_SEC + tv.tv_usec);
}

/*
 * temporary, just validating the algorithm
 */
bool hashAlgoTest()
{
    const char *samples[] = {
        "abc",
        "123456",
        "",
        "abc",
        "null",
        "ABC",
        "thisis a test",
        "this is a test",
        "this isa test",
        "this isatest",
        NULL,
    };

    //    unsigned int seed = random();
    uint32_t seed = (uint32_t) getTime();

    int i = 0;
    uint32_t lastHash = 0;
    for (i = 0; samples[i] != NULL; i++)
    {
        uint32_t hash = defaultHash(samples[i], (uint16_t) (strlen(samples[i]) + 1), seed);
        unsigned int b = assignedBucket(hash);
        printf("k=%s h=%d b=%d\n", samples[i], hash, b);

        if (lastHash != 0)
        {
            // compare 'hash' to the last one to make sure it changed
            //
            if (lastHash == hash)
            {
                // problem
                printf("error runningg test, calculated hash was the same as the last one, %d\n", hash);
                return false;
            }
        }
        lastHash = hash;
    }
    return true;
}

void printMap(icHashMap *map)
{
    printf("Map size = %d\n", hashMapCount(map));
    icHashMapIterator *loop = hashMapIteratorCreate(map);
    while (hashMapIteratorHasNext(loop) == true)
    {
        void *k;
        void *v;
        uint16_t len;

        hashMapIteratorGetNext(loop, &k, &len, &v);
        printf("map: k=%s len=%d v=%s\n", (char *) k, len, (char *) v);
    }
    hashMapIteratorDestroy(loop);
}

#define KEY_PREFIX_STR "test %d"
#define VAL_PREFIX_STR "test %d val"

bool canSetInMap(icHashMap *map, bool withValue)
{
    printf("\nRunning test %s with value: %s\n", __FUNCTION__, withValue ? "true" : "false");

    int i = 0;
    for (i = 0; i < 15; i++)
    {
        char *key = malloc(24);
        char *val = NULL;
        sprintf(key, KEY_PREFIX_STR, (i + 1));

        if (withValue)
        {
            val = malloc(24);
            sprintf(val, VAL_PREFIX_STR, (i + 1));
        }

        if (hashMapPut(map, key, (uint16_t) (strlen(key) + 1), val) == false)
        {
            return false;
        }
    }

    // print the contents
    //
    printMap(map);

    return true;
}

bool canSetCopyInMap(icHashMap *map)
{
    printf("\nRunning test %s\n", __FUNCTION__);

    int i = 0;
    for (i = 15; i < 30; i++)
    {
        char key[24];
        char val[24];
        sprintf(key, KEY_PREFIX_STR, (i + 1));
        sprintf(val, VAL_PREFIX_STR, (i + 1));

        if (hashMapPutCopy(map, key, (uint16_t) (strlen(key) + 1), val, (uint16_t) (strlen(val) + 1)) == false)
        {
            return false;
        }
    }

    // print the contents
    //
    printMap(map);

    return true;
}

bool canNotSetDupInMap(icHashMap *map)
{
    printf("\nRunning test %s\n", __FUNCTION__);

    // create another node that is the same as the 'set' above
    // and hope it fails
    //
    char *key = (char *) malloc(sizeof(char) * 24);
    char *val = (char *) malloc(sizeof(char) * 24);
    sprintf(key, KEY_PREFIX_STR, 10);
    sprintf(val, VAL_PREFIX_STR, 10);

    if (hashMapPut(map, key, (uint16_t) (strlen(key) + 1), val) == false)
    {
        // successfully prevented a duplicate!
        //
        free(key);
        free(val);
        return true;
    }

    return false;
}

bool canFindInMap(icHashMap *map)
{
    printf("\nRunning test %s\n", __FUNCTION__);

    // use key that should be there from 'set' above
    //
    char key[24];
    sprintf(key, KEY_PREFIX_STR, 7);

    char *value = hashMapGet(map, key, (uint16_t) (strlen(key) + 1));
    if (value != NULL)
    {
        printf("got v=%s when searching for k=%s\n", key, value);
        return true;
    }

    // not found
    //
    return false;
}

bool canFindKeyInMap(icHashMap *map)
{
    printf("\nRunning test %s\n", __FUNCTION__);

    // use key that should be there from 'set' above
    //
    char key[24];
    sprintf(key, KEY_PREFIX_STR, 7);

    return hashMapContains(map, key, (uint16_t) (strlen(key) + 1));
}

bool doesNotFindBadKeyInMap(icHashMap *map)
{
    printf("\nRunning test %s\n", __FUNCTION__);

    // use key that should NOT be there from 'set' above
    //
    char key[24];
    sprintf(key, KEY_PREFIX_STR, 70);

    if (hashMapContains(map, key, (uint16_t) (strlen(key) + 1)))
    {
        printf("Map contains k=%s but should not\n", key);
        return false;
    }

    // not found, so good to go
    //
    return true;
}

bool canNotFindInMap(icHashMap *map)
{
    printf("\nRunning test %s\n", __FUNCTION__);

    // use key that should NOT be there from 'set' above
    //
    char key[24];
    sprintf(key, KEY_PREFIX_STR, 70);

    char *value = hashMapGet(map, key, (uint16_t) (strlen(key) + 1));
    if (value != NULL)
    {
        printf("got v=%s when searching for k=%s\n", key, value);
        return false;
    }

    // not found, so good to go
    //
    return true;
}

bool canDeleteFromMap(icHashMap *map)
{
    printf("\nRunning test %s\n", __FUNCTION__);

    // delete the 8 key
    //
    char key[24];
    sprintf(key, KEY_PREFIX_STR, 8);
    if (hashMapDelete(map, key, (uint16_t) (strlen(key) + 1), NULL) == false)
    {
        printf("unable to delete key=%s\n", key);
        return false;
    }

    printMap(map);

    return true;
}


bool canIterateMap(icHashMap *map)
{
    printf("\nRunning test %s\n", __FUNCTION__);

    // see if we can loop through the items in map, printing each out as we go along
    //
    int count = hashMapCount(map);
    int x = 0;
    sbIcHashMapIterator *loop = hashMapIteratorCreate(map);
    while (hashMapIteratorHasNext(loop) == true)
    {
        void *mapKey;
        void *mapValue;
        uint16_t mapKeyLen = 0;

        // get the next message from the iterator
        //
        hashMapIteratorGetNext(loop, &mapKey, &mapKeyLen, &mapValue);
        printf("map: x=%d k=%s len=%d v=%s\n", x, (char *) mapKey, mapKeyLen, (char *) mapValue);

        if (x < 5)
        {
            // delete the first 5 items
            //
            hashMapIteratorDeleteCurrent(loop, NULL);
        }
        x++;
    }

    // fail if we didn't print each items
    //
    if (x != count)
    {
        printf("FAILED to iterate entire map\n");
        return false;
    }

    // see how many items are left
    //
    x = hashMapCount(map);
    printf("after loop & delete, %d items left in map (started with %d)\n", x, count);
    printMap(map);

    return true;
}

bool canShallowCloneMap(icHashMap *map)
{
    printf("\nRunning test %s\n", __FUNCTION__);

    icHashMap *mapClone = hashMapClone(map);
    if (hashMapIsClone(mapClone) == false)
    {
        hashMapDestroy(mapClone, NULL);
        return false;
    }

    if (hashMapCount(map) != hashMapCount(mapClone))
    {
        hashMapDestroy(mapClone, NULL);
        return false;
    }

    icHashMapIterator *iter = hashMapIteratorCreate(map);
    icHashMapIterator *clonedIter = hashMapIteratorCreate(mapClone);
    bool retVal = true;

    while (hashMapIteratorHasNext(iter) && hashMapIteratorHasNext(clonedIter))
    {
        void *mapKey;
        uint16_t keyLen = 0;
        void *mapValue;

        void *cloneKey;
        uint16_t cloneKeyLen = 0;
        void *cloneVal;

        hashMapIteratorGetNext(iter, &mapKey, &keyLen, &mapValue);
        hashMapIteratorGetNext(clonedIter, &cloneKey, &cloneKeyLen, &cloneVal);

        if (mapKey != cloneKey || keyLen != cloneKeyLen || mapValue != cloneVal)
        {
            retVal = false;
        }
    }

    hashMapIteratorDestroy(iter);
    hashMapIteratorDestroy(clonedIter);

    hashMapDestroy(mapClone, NULL);

    return retVal;
}

// Helper for deep clone (stolen from icStringHashMap since we use strings as keys/vals in this unit test)
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

bool canDeepCloneMap(icHashMap *map)
{
    printf("\nRunning test %s\n", __FUNCTION__);

    icHashMap *mapClone = hashMapDeepClone(map, keyValueStringClone, NULL);
    if (mapClone == NULL)
    {
        return false;
    }

    if (hashMapCount(map) != hashMapCount(mapClone))
    {
        hashMapDestroy(mapClone, NULL);
        return false;
    }

    icHashMapIterator *iter = hashMapIteratorCreate(map);
    icHashMapIterator *clonedIter = hashMapIteratorCreate(mapClone);
    bool retVal = true;

    while (hashMapIteratorHasNext(iter) && hashMapIteratorHasNext(clonedIter))
    {
        void *mapKey;
        uint16_t keyLen = 0;
        void *mapValue;

        void *cloneKey;
        uint16_t cloneKeyLen = 0;
        void *cloneVal;

        hashMapIteratorGetNext(iter, &mapKey, &keyLen, &mapValue);
        hashMapIteratorGetNext(clonedIter, &cloneKey, &cloneKeyLen, &cloneVal);

        // Can't be equal pointers
        if (mapKey == cloneKey || keyLen != cloneKeyLen || mapValue == cloneVal)
        {
            retVal = false;
        }

        if (strcmp(mapKey, cloneKey) != 0 || strcmp(mapValue, cloneVal) != 0)
        {
            retVal = false;
        }
    }

    hashMapIteratorDestroy(iter);
    hashMapIteratorDestroy(clonedIter);

    hashMapDestroy(mapClone, NULL);

    return retVal;
}

/*
 * main
 */
bool runHashTests()
{
    bool worked = false;
    if (hashAlgoTest() == false)
    {
        return false;
    }

    // create a map
    //
    icHashMap *map = hashMapCreate();
    icHashMap *set = hashMapCreate();

    if (canSetInMap(map, true) == false)
    {
        goto exit;
    }

    if (canSetInMap(set, false) == false)
    {
        goto exit;
    }

    if (!canSetCopyInMap(map))
    {
        goto exit;
    }

    if (canNotSetDupInMap(map) == false)
    {
        goto exit;
    }

    if (canFindInMap(map) == false)
    {
        goto exit;
    }

    if (!canFindKeyInMap(set) || !canFindKeyInMap(map))
    {
        goto exit;
    }

    if (canNotFindInMap(map) == false)
    {
        goto exit;
    }

    if (!doesNotFindBadKeyInMap(set) || !doesNotFindBadKeyInMap(map))
    {
        goto exit;
    }

    if (canDeleteFromMap(map) == false)
    {
        goto exit;
    }

    if (canIterateMap(map) == false)
    {
        goto exit;
    }

    if (canShallowCloneMap(map) == false)
    {
        goto exit;
    }

    if (canDeepCloneMap(map) == false)
    {
        goto exit;
    }

    worked = true;

exit:
    // mem cleanup
    //
    hashMapDestroy(map, NULL);
    hashMapDestroy(set, NULL);

    return worked;
}
