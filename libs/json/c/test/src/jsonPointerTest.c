//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2020 Comcast
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
// Created by mkoch201 on 2/25/20.
//

// cmocka & it's dependencies
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <icLog/logging.h>
#include <jsonPointer/jsonPointer.h>

static void test_fetchByJSONPointer(void **state)
{
    // Example from JSON Pointer RFC, except have to double escape backslashes to make them escaped in the JSON...
    cJSON *json = cJSON_Parse("{\n"
                              "      \"foo\": [\"bar\", \"baz\"],\n"
                              "      \"\": 0,\n"
                              "      \"a/b\": 1,\n"
                              "      \"c%d\": 2,\n"
                              "      \"e^f\": 3,\n"
                              "      \"g|h\": 4,\n"
                              "      \"i\\\\\\\\j\": 5,\n"
                              "      \"k\\\\\\\"l\": 6,\n"
                              "      \" \": 7,\n"
                              "      \"m~n\": 8,\n"
                              "      \"bool\": true\n"
                              "   }");

    assert_non_null(json);

    // ""           // the whole document
    cJSON *result = jsonPointerStringResolve(json, "");
    assert_ptr_equal(json, result);

    //  "/foo"       ["bar", "baz"]
    result = jsonPointerStringResolve(json, "/foo");
    assert_non_null(result);
    assert_true(cJSON_IsArray(result));
    assert_int_equal(2, cJSON_GetArraySize(result));
    cJSON *elem = cJSON_GetArrayItem(result, 0);
    assert_string_equal("bar", elem->valuestring);
    elem = cJSON_GetArrayItem(result, 1);
    assert_string_equal("baz", elem->valuestring);

    //    "/foo/0"     "bar"
    result = jsonPointerStringResolve(json, "/foo/0");
    assert_non_null(result);
    // Test resolveString
    char *str = NULL;
    assert_string_equal("bar", result->valuestring);
    assert_true(jsonPointerStringResolveString(json, "/foo/0", &str));
    assert_string_equal("bar", str);
    // Test with compiled pointer
    JSONPointer *pointer = jsonPointerCompile("/foo/0");
    str = NULL;
    assert_true(jsonPointerResolveString(json, pointer, &str));
    assert_string_equal("bar", str);
    // Test ToString
    assert_string_equal("/foo/0", jsonPointerToString(pointer));
    jsonPointerDestroy(pointer);

    //    "/"          0
    result = jsonPointerStringResolve(json, "/");
    assert_non_null(result);
    assert_int_equal(0, result->valueint);
    // Test resolveInt
    int intVal = -1;
    assert_true(jsonPointerStringResolveInt(json, "/", &intVal));
    assert_int_equal(0, intVal);
    // Test with compiled pointer
    intVal = -1;
    pointer = jsonPointerCompile("/");
    assert_true(jsonPointerResolveInt(json, pointer, &intVal));
    assert_int_equal(0, intVal);
    jsonPointerDestroy(pointer);

    // Test resolveDouble
    double doubleVal = -1;
    assert_true(jsonPointerStringResolveDouble(json, "/", &doubleVal));
    assert_float_equal(doubleVal, 0.0, 0.001);
    // Test with compiled pointer
    doubleVal = -1;
    pointer = jsonPointerCompile("/");
    assert_true(jsonPointerResolveDouble(json, pointer, &doubleVal));
    assert_float_equal(doubleVal, 0.0, 0.001);
    jsonPointerDestroy(pointer);

    //    "/a~1b"      1
    result = jsonPointerStringResolve(json, "/a~1b");
    assert_non_null(result);
    assert_int_equal(1, result->valueint);

    //    "/c%d"       2
    result = jsonPointerStringResolve(json, "/c%d");
    assert_non_null(result);
    assert_int_equal(2, result->valueint);

    //    "/e^f"       3
    result = jsonPointerStringResolve(json, "/e^f");
    assert_non_null(result);
    assert_int_equal(3, result->valueint);

    //    "/g|h"       4
    result = jsonPointerStringResolve(json, "/g|h");
    assert_non_null(result);
    assert_int_equal(4, result->valueint);

    //    "/i\\j"      5
    result = jsonPointerStringResolve(json, "/i\\\\j");
    assert_non_null(result);
    assert_int_equal(5, result->valueint);

    //    "/k\"l"      6
    result = jsonPointerStringResolve(json, "/k\\\"l");
    assert_non_null(result);
    assert_int_equal(6, result->valueint);

    //    "/ "         7
    result = jsonPointerStringResolve(json, "/ ");
    assert_non_null(result);
    assert_int_equal(7, result->valueint);

    //    "/m~0n"      8
    result = jsonPointerStringResolve(json, "/m~0n");
    assert_non_null(result);
    assert_int_equal(8, result->valueint);

    //    "/bool"      true
    result = jsonPointerStringResolve(json, "/bool");
    assert_non_null(result);
    assert_true(cJSON_IsTrue(result));
    // Test resolveBool
    bool boolVal = false;
    assert_true(jsonPointerStringResolveBool(json, "/bool", &boolVal));
    assert_true(boolVal);
    // Test with compiled pointer
    boolVal = false;
    pointer = jsonPointerCompile("/bool");
    assert_true(jsonPointerResolveBool(json, pointer, &boolVal));
    assert_true(boolVal);
    jsonPointerDestroy(pointer);


    cJSON_Delete(json);
}

#define A_NODE    "A"
#define B_NODE    "B"
#define C_NODE    "C"
#define D_NODE    "D"
#define A_POINTER "/" A_NODE
#define B_POINTER A_POINTER "/" B_NODE
#define C_POINTER B_POINTER "/" C_NODE
#define D_POINTER C_POINTER "/" D_NODE

static JSONPointer *aPointer = NULL;
static JSONPointer *bPointer = NULL;
static JSONPointer *cPointer = NULL;
static JSONPointer *dPointer = NULL;

static int setup_JSONPointerCreate(void **state)
{
    aPointer = jsonPointerCompile(A_POINTER);
    bPointer = jsonPointerCompile(B_POINTER);
    cPointer = jsonPointerCompile(C_POINTER);
    dPointer = jsonPointerCompile(D_POINTER);

    return 0;
}

static void test_JSONPointerCreate(void **state)
{
    const char *foo = "foo";
    char *testValue = NULL;

    // Start: "/"
    // Goal: "/A"
    cJSON *root = cJSON_CreateObject();
    cJSON *fooJson = cJSON_CreateString(foo);
    root = jsonPointerCreateObject(root, aPointer, fooJson);
    assert_non_null(root);
    assert_true(jsonPointerResolveString(root, aPointer, &testValue));
    assert_string_equal(testValue, foo);
    testValue = NULL;
    cJSON_Delete(root);
    fooJson = NULL;
    root = NULL;

    // Start: NULL
    // Goal: "/A"
    root = NULL;
    fooJson = cJSON_CreateString(foo);
    root = jsonPointerCreateObject(root, aPointer, fooJson);
    assert_non_null(root);
    assert_true(jsonPointerResolveString(root, aPointer, &testValue));
    assert_string_equal(testValue, foo);
    testValue = NULL;
    cJSON_Delete(root);
    fooJson = NULL;
    root = NULL;

    // Start: NULL
    // Goal: "/A/B"
    root = NULL;
    fooJson = cJSON_CreateString(foo);
    root = jsonPointerCreateObject(root, bPointer, fooJson);
    assert_non_null(root);
    assert_true(jsonPointerResolveString(root, bPointer, &testValue));
    assert_string_equal(testValue, foo);
    testValue = NULL;
    cJSON_Delete(root);
    fooJson = NULL;
    root = NULL;

    // Start: NULL
    // Goal: "/A/B/C"
    root = NULL;
    fooJson = cJSON_CreateString(foo);
    root = jsonPointerCreateObject(root, cPointer, fooJson);
    assert_non_null(root);
    assert_true(jsonPointerResolveString(root, cPointer, &testValue));
    assert_string_equal(testValue, foo);
    testValue = NULL;
    cJSON_Delete(root);
    fooJson = NULL;
    root = NULL;

    // Start: NULL
    // Goal: "/A/B/C/D"
    root = NULL;
    fooJson = cJSON_CreateString(foo);
    root = jsonPointerCreateObject(root, dPointer, fooJson);
    assert_non_null(root);
    assert_true(jsonPointerResolveString(root, dPointer, &testValue));
    assert_string_equal(testValue, foo);
    testValue = NULL;
    cJSON_Delete(root);
    fooJson = NULL;
    root = NULL;

    // Start: "/A"
    // Goal: "/A/B"
    root = cJSON_CreateObject();
    cJSON_AddObjectToObject(root, A_NODE);
    fooJson = cJSON_CreateString(foo);
    root = jsonPointerCreateObject(root, bPointer, fooJson);
    assert_non_null(root);
    assert_true(jsonPointerResolveString(root, bPointer, &testValue));
    assert_string_equal(testValue, foo);
    testValue = NULL;
    cJSON_Delete(root);
    fooJson = NULL;
    root = NULL;

    // Start: "/A"
    // Goal: "/A/B/C"
    root = cJSON_CreateObject();
    cJSON_AddObjectToObject(root, A_NODE);
    fooJson = cJSON_CreateString(foo);
    root = jsonPointerCreateObject(root, cPointer, fooJson);
    assert_non_null(root);
    assert_true(jsonPointerResolveString(root, cPointer, &testValue));
    assert_string_equal(testValue, foo);
    testValue = NULL;
    cJSON_Delete(root);
    fooJson = NULL;
    root = NULL;

    // Start: "/A"
    // Goal: "/A" (replace value)
    root = cJSON_CreateObject();
    cJSON_AddObjectToObject(root, A_NODE);
    fooJson = cJSON_CreateString(foo);
    root = jsonPointerCreateObject(root, aPointer, fooJson);
    assert_non_null(root);
    assert_true(jsonPointerResolveString(root, aPointer, &testValue));
    assert_string_equal(testValue, foo);
    testValue = NULL;
    cJSON_Delete(root);
    fooJson = NULL;
    root = NULL;

    // Start: "/A/B"
    // Goal: "/A/B" (replace value)
    root = cJSON_CreateObject();
    cJSON *aNode = cJSON_AddObjectToObject(root, A_NODE);
    cJSON_AddObjectToObject(aNode, B_NODE);
    fooJson = cJSON_CreateString(foo);
    root = jsonPointerCreateObject(root, bPointer, fooJson);
    assert_non_null(root);
    assert_true(jsonPointerResolveString(root, bPointer, &testValue));
    assert_string_equal(testValue, foo);
    testValue = NULL;
    cJSON_Delete(root);
    fooJson = NULL;
    root = NULL;

    // NULL JsonPointer
    root = jsonPointerCreateObject(NULL, NULL, fooJson);
    assert_null(root);

    // NULL value
    root = jsonPointerCreateObject(NULL, aPointer, NULL);
    assert_null(root);

    // NULL JsonPointer for String
    root = jsonPointerCreateString(NULL, NULL, foo);
    assert_null(root);

    // NULL JsonPointer for Number
    double testDouble = 0.0;
    root = jsonPointerCreateNumber(NULL, NULL, &testDouble);
    assert_null(root);

    // NULL JsonPointer for Boolean
    bool testBool = false;
    root = jsonPointerCreateBoolean(NULL, NULL, &testBool);
    assert_null(root);
}

/*
 * main
 */
int main(int argc, char *argv[])
{
    setIcLogPriorityFilter(IC_LOG_TRACE);

    const struct CMUnitTest tests[] = {cmocka_unit_test(test_fetchByJSONPointer),
                                       cmocka_unit_test_setup(test_JSONPointerCreate, setup_JSONPointerCreate)};

    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    return retval;
}
