//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2011. iControl Networks
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


//
// Created by Christian Leithner on 11/5/2018.
//

#include <stdio.h>
#include <icTypes/icStringHashMap.h>
#include <icLog/logging.h>
#include <memory.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>
#include <cjson/cJSON.h>
#include <icTypes/sbrm.h>
#include <jsonHelper/jsonHelper.h>

#define LOG_CAT     "logTEST"

#define KEY_PREFIX_STR  "test %d"
#define VAL_PREFIX_STR  "test %d val"

static void test_canCreateDestroyStringMap(void **state)
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    icStringHashMap *map = stringHashMapCreate();

    assert_int_equal(stringHashMapCount(map), 0);

    stringHashMapDestroy(map, NULL);

    (void) state;
}

static void test_canPutGetStringMap(void **state)
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    icStringHashMap *map = stringHashMapCreate();

    char *key = (char *)malloc(sizeof(char) * 24);
    char *val = (char *)malloc(sizeof(char) * 24);

    sprintf(key, KEY_PREFIX_STR, 1);
    sprintf(val, VAL_PREFIX_STR, 1);

    stringHashMapPut(map, key, val);

    assert_int_equal(stringHashMapCount(map), 1);

    char *valGot = stringHashMapGet(map, key);

    assert_ptr_equal(valGot, val);

    stringHashMapDestroy(map, NULL);

    (void) state;
}

static void test_canDeleteStringMap(void **state)
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    icStringHashMap *map = stringHashMapCreate();

    char *key = (char *)malloc(sizeof(char) * 24);
    char *val = (char *)malloc(sizeof(char) * 24);

    sprintf(key, KEY_PREFIX_STR, 1);
    sprintf(val, VAL_PREFIX_STR, 1);

    stringHashMapPut(map, key, val);

    assert_int_equal(stringHashMapCount(map), 1);
    assert_true(stringHashMapDelete(map, key, NULL));

    stringHashMapDestroy(map, NULL);

    (void) state;
}

static void test_canIterateStringMap(void **state)
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    icStringHashMap *map = stringHashMapCreate();

    for(int i = 0; i<15; i++)
    {
        char *key = (char *)malloc(sizeof(char) * 24);
        char *val = (char *)malloc(sizeof(char) * 24);

        sprintf(key, KEY_PREFIX_STR, i);
        sprintf(val, VAL_PREFIX_STR, i);

        stringHashMapPut(map, key, val);
    }

    int count = stringHashMapCount(map);
    int x = 0;
    icStringHashMapIterator *loop = stringHashMapIteratorCreate(map);
    while(stringHashMapIteratorHasNext(loop) == true)
    {
        char *mapKey;
        char *mapValue;

        // get the next message from the iterator
        //
        stringHashMapIteratorGetNext(loop, &mapKey, &mapValue);
        printf("map: x=%d k=%s v=%s\n", x, mapKey, mapValue);

        if(x < 5)
        {
            // delete the first 5 items
            //
            stringHashMapIteratorDeleteCurrent(loop, NULL);
        }
        x++;
    }
    stringHashMapIteratorDestroy(loop);

    // fail if we didn't print each items
    //
    assert_int_equal(x, count);

    // see how many items are left
    //
    x = stringHashMapCount(map);
    printf("after loop & delete, %d items left in map (started with %d)\n", x, count);

    stringHashMapDestroy(map, NULL);

    (void) state;
}

static void test_canDeepCloneStringMap(void **state)
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    icStringHashMap *map = stringHashMapCreate();

    for(int i = 0; i<15; i++)
    {
        char *key = (char *)malloc(sizeof(char) * 24);
        char *val = (char *)malloc(sizeof(char) * 24);

        sprintf(key, KEY_PREFIX_STR, i);
        sprintf(val, VAL_PREFIX_STR, i);

        stringHashMapPut(map, key, val);
    }

    icStringHashMap *mapClone = stringHashMapDeepClone(map);
    assert_ptr_not_equal(mapClone, NULL);
    assert_int_equal(stringHashMapCount(map), stringHashMapCount(mapClone));

    icStringHashMapIterator *iter = stringHashMapIteratorCreate(map);
    icStringHashMapIterator *clonedIter = stringHashMapIteratorCreate(mapClone);

    while(stringHashMapIteratorHasNext(iter) && stringHashMapIteratorHasNext(clonedIter))
    {
        char *mapKey;
        char *mapValue;

        char *cloneKey;
        char *cloneVal;

        stringHashMapIteratorGetNext(iter, &mapKey, &mapValue);
        stringHashMapIteratorGetNext(clonedIter, &cloneKey, &cloneVal);

        //Can't be equal pointers
        assert_ptr_not_equal(mapKey, cloneKey);
        assert_ptr_not_equal(mapValue, cloneVal);

        assert_int_equal(strcmp(mapKey, cloneKey), 0);
        assert_int_equal(strcmp(mapValue, cloneVal), 0);

    }

    stringHashMapIteratorDestroy(iter);
    stringHashMapIteratorDestroy(clonedIter);

    stringHashMapDestroy(mapClone, NULL);
    stringHashMapDestroy(map, NULL);

    (void) state;
}

static void test_canPutCopyStringMap(void **state)
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    icStringHashMap *map = stringHashMapCreate();

    const char *key = "PUT_COPY_KEY";
    const char *value = "PUT_COPY_VALUE";
    char *valueCopy = (char *)malloc(strlen(value)+1);
    strcpy(valueCopy, value);

    stringHashMapPutCopy(map, key, valueCopy);

    assert_int_equal(stringHashMapCount(map), 1);

    // Change a character in the string, since we did a put copy this shouldn't affect what's in the map
    valueCopy[0] = 'O';

    char *valGot = stringHashMapGet(map, key);

    assert_string_equal(valGot, value);

    stringHashMapDestroy(map, NULL);

    free(valueCopy);

    (void) state;
}

static void testCanSerializeDeserializeMap(void **state)
{
    icLogDebug(LOG_CAT, "Testing %s", __FUNCTION__);

    icStringHashMap *map = stringHashMapCreate();
    stringHashMapPutCopy(map, "3", "fish");
    stringHashMapPutCopy(map, "yellow", "banana");
    stringHashMapPutCopy(map, "blue", "ocean");
    stringHashMapPutCopy(map, "green", "apple");
    stringHashMapPut(map, strdup("justAKey"), NULL);

    char *mapString = stringHashMapJSONSerialize(map, true);
    assert_non_null(mapString);

    icLogDebug(LOG_CAT, "%s: map->json: %s", __FUNCTION__, mapString);

    AUTO_CLEAN(cJSON_Delete__auto) cJSON *parsed = cJSON_Parse(mapString);
    free(mapString);
    assert_non_null(parsed);

    icStringHashMap *checkMap = stringHashMapFromJSON(parsed);
    assert_non_null(checkMap);

    icLogDebug(LOG_CAT, "%s: checking re-serialized map equals parsed original serialized map", __FUNCTION__);
    AUTO_CLEAN(cJSON_Delete__auto) cJSON *check = stringHashMapToJSON(checkMap);
    assert_true(cJSON_Compare(check, parsed, true));

    stringHashMapDestroy(map, NULL);
    stringHashMapDestroy(checkMap, NULL);
}

static void testCanNotSerializeDeserializeBadInput(void **state)
{
    AUTO_CLEAN(free_generic__auto) char *badSerialized = stringHashMapJSONSerialize(NULL, true);
    assert_null(badSerialized);

    const char *badJSON = "{\"3\":5,\"yellow\":\"banana\",\"blue\":\"ocean\",\"green\":\"apple\",\"justAKey\":null}";
    cJSON *missingQuote = cJSON_Parse(badJSON);
    assert_non_null(missingQuote);

    icStringHashMap *missingQuoteCheck = stringHashMapFromJSON(missingQuote);
    cJSON_Delete(missingQuote);
    assert_null(missingQuoteCheck);

    const char *nestedJSON = "{\"map\":{\"3\":\"fish\",\"blue\":\"ocean\"},\"other\":\"2\"}";
    cJSON *nested = cJSON_Parse(nestedJSON);
    assert_non_null(nested);

    icStringHashMap *nestedCheck = stringHashMapFromJSON(nested);
    cJSON_Delete(nested);
    assert_null(nestedCheck);

    const char *arrayJSON = "[\"a\", \"b\"]";
    cJSON *array = cJSON_Parse(arrayJSON);
    assert_non_null(array);

    icStringHashMap *arrayCheck = stringHashMapFromJSON(array);
    cJSON_Delete(array);
    assert_null(arrayCheck);
}

int main(int argc, char **argv)
{
    const struct CMUnitTest tests[] =
            {
                    cmocka_unit_test(test_canCreateDestroyStringMap),
                    cmocka_unit_test(test_canPutGetStringMap),
                    cmocka_unit_test(test_canDeleteStringMap),
                    cmocka_unit_test(test_canIterateStringMap),
                    cmocka_unit_test(test_canDeepCloneStringMap),
                    cmocka_unit_test(test_canPutCopyStringMap),
                    cmocka_unit_test(testCanSerializeDeserializeMap),
                    cmocka_unit_test(testCanNotSerializeDeserializeBadInput)
            };

    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    return retval;
}
