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
// Created by mkoch201 on 10/18/18.
//

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>
#include <limits.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#include <icLog/logging.h>
#include <icTypes/sbrm.h>
#include <icUtil/array.h>
#include <icUtil/stringUtils.h>

#define LOG_TAG     "stringTest"

#define TEST_STRING "A towel is the most important tool for a hitchhiker."

// *************************
// Tests
// *************************

static void test_strReplace(void **state)
{
    char *newStr = NULL;
    char *original = NULL;
    char *replace = NULL;
    char *with = NULL;

    // Replace whole string
    original = "test";
    replace = "test";
    with = "pear";
    newStr = stringReplace(original, replace, with);
    assert_non_null(newStr);
    assert_string_equal(newStr, "pear");
    free(newStr);

    // No substring exists
    original = "test";
    replace = "grape";
    with = "pear";
    newStr = stringReplace(original, replace, with);
    assert_non_null(newStr);
    assert_string_equal(newStr, "test");
    free(newStr);

    // Replace prefix
    original = "test";
    replace = "te";
    with = "fi";
    newStr = stringReplace(original, replace, with);
    assert_non_null(newStr);
    assert_string_equal(newStr, "fist");
    free(newStr);

    // Replace suffix
    original = "test";
    replace = "st";
    with = "mp";
    newStr = stringReplace(original, replace, with);
    assert_non_null(newStr);
    assert_string_equal(newStr, "temp");
    free(newStr);

    // Replace the middle
    original = "test";
    replace = "es";
    with = "in";
    newStr = stringReplace(original, replace, with);
    assert_non_null(newStr);
    assert_string_equal(newStr, "tint");
    free(newStr);

    // Replace multiples
    original = "testingstring";
    replace = "st";
    with = "mp";
    newStr = stringReplace(original, replace, with);
    assert_non_null(newStr);
    assert_string_equal(newStr, "tempingmpring");
    free(newStr);

    // Replace with lowercase and capitals
    original = "TeStSTRinG";
    replace = "tSTRi";
    with = "pEArs";
    newStr = stringReplace(original, replace, with);
    assert_non_null(newStr);
    assert_string_equal(newStr, "TeSpEArsnG");
    free(newStr);

    // Replace with symbols
    original = "1337$p3#&";
    replace = "37$p3#&";
    with = "!@.,0%/";
    newStr = stringReplace(original, replace, with);
    assert_non_null(newStr);
    assert_string_equal(newStr, "13!@.,0%/");
    free(newStr);

    // Replace with whitespace
    original = "  test  \r\n";
    replace = "test  \r\n";
    with = "pear \n\r\t";
    newStr = stringReplace(original, replace, with);
    assert_non_null(newStr);
    assert_string_equal(newStr, "  pear \n\r\t");
    free(newStr);

    // Replace with longer string than substring
    original = "teststring";
    replace = "eststr";
    with = "supereststr";
    newStr = stringReplace(original, replace, with);
    assert_non_null(newStr);
    assert_string_equal(newStr, "tsupereststring");
    free(newStr);

    // Replace with shorter string than substring
    original = "teststring";
    replace = "eststr";
    with = "eep";
    newStr = stringReplace(original, replace, with);
    assert_non_null(newStr);
    assert_string_equal(newStr, "teeping");
    free(newStr);

    // Replace with empty original
    original = "";
    replace = "test";
    with = "pear";
    newStr = stringReplace(original, replace, with);
    assert_non_null(newStr);
    assert_string_equal(newStr, "");
    free(newStr);

    // Replace with empty replace
    original = "test";
    replace = "";
    with = "pear";
    newStr = stringReplace(original, replace, with);
    assert_non_null(newStr);
    assert_string_equal(newStr, original);
    free(newStr);

    // Replace with empty with
    original = "test";
    replace = "es";
    with = "";
    newStr = stringReplace(original, replace, with);
    assert_non_null(newStr);
    assert_string_equal(newStr, "tt");
    free(newStr);

    // Replace with null original
    original = NULL;
    replace = "test";
    with = "pear";
    newStr = stringReplace(original, replace, with);
    assert_null(newStr);

    // Replace with null replace
    original = "test";
    replace = NULL;
    with = "pear";
    newStr = stringReplace(original, replace, with);
    assert_non_null(newStr);
    assert_string_equal(newStr, original);
    free(newStr);

    // Replace with null with
    original = "test";
    replace = "es";
    with = NULL;
    newStr = stringReplace(original, replace, with);
    assert_non_null(newStr);
    assert_string_equal(newStr, "tt");
    free(newStr);


    (void) state;
}

static void test_generateRandomToken(void **state)
{
    char *newStr = NULL;
    uint16_t minLen = 0;
    uint16_t maxLen = 0;
    int seedAdd = 0;

    // Zero min/max
    minLen = 0;
    maxLen = 0;
    seedAdd = 0;
    newStr = generateRandomToken(minLen, maxLen, seedAdd);
    assert_non_null(newStr);
    assert_in_range(strlen(newStr), minLen, maxLen);
    free(newStr);

    // Zero min length
    minLen = 0;
    maxLen = 10;
    seedAdd = 0;
    newStr = generateRandomToken(minLen, maxLen, seedAdd);
    assert_non_null(newStr);
    assert_in_range(strlen(newStr), minLen, maxLen);
    free(newStr);

    // Max_val min length
    minLen = UINT16_MAX;
    maxLen = 0;
    seedAdd = 0;
    newStr = generateRandomToken(minLen, maxLen, seedAdd);
    assert_null(newStr);

    // Zero max length
    minLen = 10;
    maxLen = 0;
    seedAdd = 0;
    newStr = generateRandomToken(minLen, maxLen, seedAdd);
    assert_null(newStr);

    // Max_val max length
    minLen = 0;
    maxLen = UINT16_MAX;
    seedAdd = 0;
    newStr = generateRandomToken(minLen, maxLen, seedAdd);
    assert_non_null(newStr);
    assert_in_range(strlen(newStr), minLen, maxLen);
    free(newStr);

    // Positive seed
    minLen = 0;
    maxLen = 10;
    seedAdd = 672;
    newStr = generateRandomToken(minLen, maxLen, seedAdd);
    assert_non_null(newStr);
    assert_in_range(strlen(newStr), minLen, maxLen);
    free(newStr);

    // 1 seed
    minLen = 0;
    maxLen = 10;
    seedAdd = 1;
    newStr = generateRandomToken(minLen, maxLen, seedAdd);
    assert_non_null(newStr);
    assert_in_range(strlen(newStr), minLen, maxLen);
    free(newStr);

    // 0 seed
    minLen = 0;
    maxLen = 10;
    seedAdd = 0;
    newStr = generateRandomToken(minLen, maxLen, seedAdd);
    assert_non_null(newStr);
    assert_in_range(strlen(newStr), minLen, maxLen);
    free(newStr);

    // -1 seed
    minLen = 0;
    maxLen = 10;
    seedAdd = -1;
    newStr = generateRandomToken(minLen, maxLen, seedAdd);
    assert_non_null(newStr);
    assert_in_range(strlen(newStr), minLen, maxLen);
    free(newStr);

    // Negative seed
    minLen = 0;
    maxLen = 10;
    seedAdd = -672;
    newStr = generateRandomToken(minLen, maxLen, seedAdd);
    assert_non_null(newStr);
    assert_in_range(strlen(newStr), minLen, maxLen);
    free(newStr);

    // Max_value seed
    minLen = 0;
    maxLen = 10;
    seedAdd = INT_MAX;
    newStr = generateRandomToken(minLen, maxLen, seedAdd);
    assert_non_null(newStr);
    assert_in_range(strlen(newStr), minLen, maxLen);
    free(newStr);

    // Zero min length
    minLen = 0;
    maxLen = 10;
    seedAdd = INT_MIN;
    newStr = generateRandomToken(minLen, maxLen, seedAdd);
    assert_non_null(newStr);
    assert_in_range(strlen(newStr), minLen, maxLen);
    free(newStr);


    (void) state;
}

static void test_safeStringCopy(void **state)
{
    char newStr[30];
    uint16_t maxDestChars = 10;
    char *src = "Test String";
    bool resVal = false;

    // NULL dest
    maxDestChars = 10;
    resVal = safeStringCopy(NULL, maxDestChars, src);
    assert_false(resVal);

    // Zero maxDestChars
    resVal = safeStringCopy(newStr, 0, src);
    assert_false(resVal);

    // NULL src
    resVal = safeStringCopy(newStr, maxDestChars, NULL);
    assert_false(resVal);

    // Correct copy (whole source)
    maxDestChars = 20;
    resVal = safeStringCopy(newStr, maxDestChars, src);
    assert_true(resVal);
    assert_string_equal(newStr, src);
    memset(newStr, 0, 30);

    // Correct copy (part of source)
    maxDestChars = 7;
    resVal = safeStringCopy(newStr, maxDestChars, src);
    assert_true(resVal);
    assert_string_equal(newStr, "Test S");
    memset(newStr, 0, 30);


    (void) state;
}

static void test_safeStringAppend(void **state)
{
    char newStr[30] = "Prefix";
    uint16_t maxDestChars = 10;
    char *src = "Test String";
    bool resVal = false;

    // NULL dest
    maxDestChars = 10;
    resVal = safeStringAppend(NULL, maxDestChars, src);
    assert_false(resVal);

    // Zero maxDestChars
    resVal = safeStringAppend(newStr, 0, src);
    assert_false(resVal);

    // NULL src
    resVal = safeStringAppend(newStr, maxDestChars, NULL);
    assert_false(resVal);

    // Correct copy (whole source)
    maxDestChars = 20;
    resVal = safeStringAppend(newStr, maxDestChars, src);
    assert_true(resVal);
    assert_string_equal(newStr, "PrefixTest String");
    memset(newStr, 0, 30);

    // Correct copy (part of source)
    strcpy(newStr, "Prefix");
    maxDestChars = 7 + strlen(newStr);
    resVal = safeStringAppend(newStr, maxDestChars, src);
    assert_true(resVal);
    assert_string_equal(newStr, "PrefixTest S");
    memset(newStr, 0, 30);


    (void) state;
}

static void test_stringCompare(void **state)
{
    char *left = NULL;
    char *right = NULL;
    bool ignoreCase = false;
    int8_t retVal = 0;

    // NULL left
    right = "rightString";
    retVal = stringCompare(left, right, ignoreCase);
    assert_int_equal(retVal, 1);

    // NULL right
    left = "leftString";
    right = NULL;
    retVal = stringCompare(left, right, ignoreCase);
    assert_int_equal(retVal, -1);

    // NULL both
    left = NULL;
    retVal = stringCompare(left, right, ignoreCase);
    assert_int_equal(retVal, 0);

    // Success case (equal)
    left = "theStrinG";
    right = "theStrinG";
    retVal = stringCompare(left, right, ignoreCase);
    assert_int_equal(retVal, 0);

    // Failure case (not equal)
    right = "thestring";
    retVal = stringCompare(left, right, ignoreCase);
    assert_int_equal(retVal, -1);

    // Success case (ignore case)
    ignoreCase = true;
    retVal = stringCompare(left, right, ignoreCase);
    assert_int_equal(retVal, 0);

    // Failure case (ignore case)
    left = "ZZZZZZZ";
    retVal = stringCompare(left, right, ignoreCase);
    assert_int_equal(retVal, 1);


    (void) state;
}

static void test_stringStartsWith(void **state)
{
    char *string = NULL;
    char *prefix = NULL;
    bool ignoreCase = false;
    bool retVal = false;

    // NULL string
    prefix = "prefix";
    retVal = stringStartsWith(string, prefix, ignoreCase);
    assert_false(retVal);

    // NULL prefix
    string = "The String";
    prefix = NULL;
    retVal = stringStartsWith(string, prefix, ignoreCase);
    assert_false(retVal);

    // Equal strings
    prefix = "The String";
    retVal = stringStartsWith(string, prefix, ignoreCase);
    assert_true(retVal);

    // Correct prefix
    prefix = "The ";
    retVal = stringStartsWith(string, prefix, ignoreCase);
    assert_true(retVal);

    // Incorrect prefix
    prefix = "the ";
    retVal = stringStartsWith(string, prefix, ignoreCase);
    assert_false(retVal);

    // Equal strings (ignore case)
    prefix = "the string";
    ignoreCase = true;
    retVal = stringStartsWith(string, prefix, ignoreCase);
    assert_true(retVal);

    // Correct prefix (ignore case)
    prefix = "the ";
    retVal = stringStartsWith(string, prefix, ignoreCase);
    assert_true(retVal);

    // Incorrect prefix (ignore case)
    prefix = "prefix";
    retVal = stringStartsWith(string, prefix, ignoreCase);
    assert_false(retVal);

    // Longer prefix
    prefix = "the string but longer";
    retVal = stringStartsWith(string, prefix, ignoreCase);
    assert_false(retVal);

    // Empty string
    string = "";
    prefix = "prefix";
    retVal = stringStartsWith(string, prefix, ignoreCase);
    assert_false(retVal);

    // Empty prefix
    string = "The String";
    prefix = "";
    retVal = stringStartsWith(string, prefix, ignoreCase);
    assert_true(retVal);

    // Empty both
    string = "";
    retVal = stringStartsWith(string, prefix, ignoreCase);
    assert_true(retVal);


    (void) state;
}

static void test_stringEndsWith(void **state)
{
    char *string = NULL;
    char *suffix = NULL;
    bool ignoreCase = false;
    bool retVal = false;

    // NULL string
    suffix = "suffix";
    retVal = stringEndsWith(string, suffix, ignoreCase);
    assert_false(retVal);

    // NULL suffix
    string = "The String";
    suffix = NULL;
    retVal = stringEndsWith(string, suffix, ignoreCase);
    assert_false(retVal);

    // Equal strings
    suffix = "The String";
    retVal = stringEndsWith(string, suffix, ignoreCase);
    assert_true(retVal);

    // Correct suffix
    suffix = " String";
    retVal = stringEndsWith(string, suffix, ignoreCase);
    assert_true(retVal);

    // Incorrect suffix
    suffix = " string";
    retVal = stringEndsWith(string, suffix, ignoreCase);
    assert_false(retVal);

    // Equal strings (ignore case)
    suffix = "the string";
    ignoreCase = true;
    retVal = stringEndsWith(string, suffix, ignoreCase);
    assert_true(retVal);

    // Correct suffix (ignore case)
    suffix = " string";
    retVal = stringEndsWith(string, suffix, ignoreCase);
    assert_true(retVal);

    // Incorrect suffix (ignore case)
    suffix = "suffix";
    retVal = stringStartsWith(string, suffix, ignoreCase);
    assert_false(retVal);

    // Longer suffix
    suffix = "longer the string";
    retVal = stringEndsWith(string, suffix, ignoreCase);
    assert_false(retVal);

    // Empty string
    string = "";
    suffix = "suffix";
    retVal = stringEndsWith(string, suffix, ignoreCase);
    assert_false(retVal);

    // Empty suffix
    string = "The String";
    suffix = "";
    retVal = stringEndsWith(string, suffix, ignoreCase);
    assert_true(retVal);

    // Empty both
    string = "";
    retVal = stringEndsWith(string, suffix, ignoreCase);
    assert_true(retVal);


    (void) state;
}

static void test_trimString(void **state)
{
    char *newStr = NULL;

    // No trimming needed
    newStr = trimString("test");
    assert_non_null(newStr);
    assert_string_equal(newStr, "test");
    free(newStr);

    // Trim leading space
    newStr = trimString(" test");
    assert_non_null(newStr);
    assert_string_equal(newStr, "test");
    free(newStr);

    // Trim trailing space
    newStr = trimString("test ");
    assert_non_null(newStr);
    assert_string_equal(newStr, "test");
    free(newStr);

    // Trim leading and trailing space
    newStr = trimString(" test ");
    assert_non_null(newStr);
    assert_string_equal(newStr, "test");
    free(newStr);

    // Trim all space
    newStr = trimString("\t\ftest\n\r\v");
    assert_non_null(newStr);
    assert_string_equal(newStr, "test");
    free(newStr);

    (void) state;
}

static void test_stringSplitOnToken(void **state)
{
    char *inputStr = NULL;
    char token = '.';
    char leftOut[50];
    char rightOut[50];
    memset(leftOut, 0, 50);
    memset(rightOut, 0, 50);
    bool retVal = false;

    // NULL inputStr
    retVal = stringSplitOnToken(inputStr, token, leftOut, rightOut);
    assert_false(retVal);
    assert_string_equal(leftOut, "");
    assert_string_equal(rightOut, "");

    // Empty inputStr
    inputStr = "";
    retVal = stringSplitOnToken(inputStr, token, leftOut, rightOut);
    assert_false(retVal);
    assert_string_equal(leftOut, "");
    assert_string_equal(rightOut, "");

    // First char token
    inputStr = "The Lion King";
    token = 'T';
    retVal = stringSplitOnToken(inputStr, token, leftOut, rightOut);
    assert_true(retVal);
    assert_string_equal(leftOut, "");
    assert_string_equal(rightOut, "he Lion King");
    memset(leftOut, 0, 50);
    memset(rightOut, 0, 50);

    // Last char token
    token = 'g';
    retVal = stringSplitOnToken(inputStr, token, leftOut, rightOut);
    assert_true(retVal);
    assert_string_equal(leftOut, "The Lion Kin");
    assert_string_equal(rightOut, "");
    memset(leftOut, 0, 50);
    memset(rightOut, 0, 50);

    // Terminating token
    token = '\0';
    retVal = stringSplitOnToken(inputStr, token, leftOut, rightOut);
    assert_false(retVal);
    assert_string_equal(leftOut, "");
    assert_string_equal(rightOut, "");

    // Correct case
    token = 'o';
    retVal = stringSplitOnToken(inputStr, token, leftOut, rightOut);
    assert_true(retVal);
    assert_string_equal(leftOut, "The Li");
    assert_string_equal(rightOut, "n King");
    memset(leftOut, 0, 50);
    memset(rightOut, 0, 50);

    // Correct case with whitespace
    inputStr = "\t The Li\ro n King  \n";
    retVal = stringSplitOnToken(inputStr, token, leftOut, rightOut);
    assert_true(retVal);
    assert_string_equal(leftOut, "The Li");
    assert_string_equal(rightOut, "n King");
    memset(leftOut, 0, 50);
    memset(rightOut, 0, 50);

    // Incorrect case
    inputStr = "The Lion King";
    token = '3';
    retVal = stringSplitOnToken(inputStr, token, leftOut, rightOut);
    assert_false(retVal);
    assert_string_equal(leftOut, "");
    assert_string_equal(rightOut, "");
    memset(leftOut, 0, 50);
    memset(rightOut, 0, 50);

    (void) state;
}

static void test_stringToLowerCase(void **state)
{
    char *inputStr = NULL;

    // NULL string
    stringToLowerCase(inputStr);
    assert_null(inputStr);

    // Empty string
    inputStr = strdup("");
    stringToLowerCase(inputStr);
    assert_string_equal(inputStr, "");
    free(inputStr);

    // All lowercase
    inputStr = strdup("the string of course");
    stringToLowerCase(inputStr);
    assert_string_equal(inputStr, "the string of course");
    free(inputStr);

    // Mix lower/upper case
    inputStr = strdup("ThE StRiNG oF coUrSe");
    stringToLowerCase(inputStr);
    assert_string_equal(inputStr, "the string of course");
    free(inputStr);

    // All uppercase
    inputStr = strdup("THE STRING OF COURSE");
    stringToLowerCase(inputStr);
    assert_string_equal(inputStr, "the string of course");
    free(inputStr);

    // With whitespace
    inputStr = strdup("\tThE StRiNG\roF coUrSe\n");
    stringToLowerCase(inputStr);
    assert_string_equal(inputStr, "\tthe string\rof course\n");
    free(inputStr);

    // With symbols
    inputStr = strdup("7h3 5tRiNG#^ *)/oF coUrSe!!!.!+");
    stringToLowerCase(inputStr);
    assert_string_equal(inputStr, "7h3 5tring#^ *)/of course!!!.!+");
    free(inputStr);


    (void) state;
}

static void test_stringToUpperCase(void **state)
{
    char *inputStr = NULL;

    // NULL string
    stringToLowerCase(inputStr);
    assert_null(inputStr);

    // Empty string
    inputStr = strdup("");
    stringToLowerCase(inputStr);
    assert_string_equal(inputStr, "");
    free(inputStr);

    // All lowercase
    inputStr = strdup("the string of course");
    stringToUpperCase(inputStr);
    assert_string_equal(inputStr, "THE STRING OF COURSE");
    free(inputStr);

    // Mix lower/upper case
    inputStr = strdup("ThE StRiNG oF coUrSe");
    stringToUpperCase(inputStr);
    assert_string_equal(inputStr, "THE STRING OF COURSE");
    free(inputStr);

    // All uppercase
    inputStr = strdup("THE STRING OF COURSE");
    stringToUpperCase(inputStr);
    assert_string_equal(inputStr, "THE STRING OF COURSE");
    free(inputStr);

    // With whitespace
    inputStr = strdup("\tThE StRiNG\roF coUrSe\n");
    stringToUpperCase(inputStr);
    assert_string_equal(inputStr, "\tTHE STRING\rOF COURSE\n");
    free(inputStr);

    // With symbols
    inputStr = strdup("7h3 5tRiNG#^ *)/oF coUrSe!!!.!+");
    stringToUpperCase(inputStr);
    assert_string_equal(inputStr, "7H3 5TRING#^ *)/OF COURSE!!!.!+");
    free(inputStr);


    (void) state;
}

static void test_stringToCamelCase(void **state)
{
    char *inputStr = NULL;
    char *outputStr = NULL;

    // NULL string test
    outputStr = stringToCamelCase(inputStr);
    assert_null(outputStr);

    // Empty string test
    inputStr = strdup("");
    outputStr = stringToCamelCase(inputStr);
    assert_null(outputStr);
    free(inputStr);
    free(outputStr);

    // All lower case test
    inputStr = strdup("this is-a_string");
    outputStr = stringToCamelCase(inputStr);
    assert_string_equal(outputStr, "thisIsAString");
    free(inputStr);
    free(outputStr);

    // All upperCase test
    inputStr = strdup("THIS IS-A_STRING");
    outputStr = stringToCamelCase(inputStr);
    assert_string_equal(outputStr, "thisIsAString");
    free(inputStr);
    free(outputStr);

    // With white space
    inputStr = strdup("this\tis\ra\nstring");
    outputStr = stringToCamelCase(inputStr);
    assert_string_equal(outputStr, "this\tis\ra\nstring");
    free(inputStr);
    free(outputStr);

    // Special characters
    inputStr = strdup("7h#5 1s-@_s+r!ng p|u5_m0r3 .!^()");
    outputStr = stringToCamelCase(inputStr);
    assert_string_equal(outputStr, "7h#51s@S+r!ngP|u5M0r3.!^()");
    free(inputStr);
    free(outputStr);

    // Mix of lowercase and uppercase
    inputStr = strdup("thIS iS-a_sTrInG");
    outputStr = stringToCamelCase(inputStr);
    assert_string_equal(outputStr, "thisIsAString");
    free(inputStr);
    free(outputStr);
}

static void test_stringBuilder(void **state)
{
    char *tmp = NULL;

    // use stringBuilder to allocate some mem
    tmp = stringBuilder("%s + %d", "123", 4);
    assert_non_null(tmp);
    assert_int_equal(strlen(tmp), 7);
    free(tmp);
}

static void test_stringEditInvalidBounds(void **state)
{
    AUTO_CLEAN(free_generic__auto) char *editedString = NULL;

    editedString = stringEdit(strdup("Hello, World!"), 24, 15, "blah");
    assert_null(editedString);
    free(editedString);

    editedString = stringEdit(strdup("Hello, World!"), 10, 15, "blah");
    assert_null(editedString);
    free(editedString);

    editedString = stringEdit(strdup("Hello, World!"), 12, 3, ".");
    assert_null(editedString);
}

void test_stringEditDelete(void **state)
{
    AUTO_CLEAN(free_generic__auto) char *editedString = strdup(TEST_STRING);

    editedString = stringEdit(editedString, 2, 6, NULL);
    assert_string_equal("A is the most important tool for a hitchhiker.", editedString);
    free(editedString);

    editedString = stringEdit(strdup(TEST_STRING), 41, 5, NULL);
    assert_string_equal("A towel is the most important tool for a hiker.", editedString);
    free(editedString);

    AUTO_CLEAN(free_generic__auto) char *trimmed = strdup(TEST_STRING);
    trimmed[strlen(trimmed) - 1] = 0;

    editedString = stringEdit(strdup(TEST_STRING), 51, 1, NULL);
    assert_string_equal(trimmed, editedString);
}

void test_stringEditReplace(void **state)
{
    AUTO_CLEAN(free_generic__auto) char *editedString = strdup(TEST_STRING);

    /* Direct replacements */
    editedString = stringEdit(editedString, 51, 1, ",");
    assert_string_equal(editedString, "A towel is the most important tool for a hitchhiker,");
    free(editedString);

    editedString = stringEdit(strdup(TEST_STRING), 8, 2, "IS");
    assert_string_equal(editedString, "A towel IS the most important tool for a hitchhiker.");
    free(editedString);

    /* Longer replacements */

    editedString = stringEdit(strdup(TEST_STRING), 0, 1, "The");
    assert_string_equal(editedString, "The towel is the most important tool for a hitchhiker.");
    free(editedString);

    editedString = stringEdit(strdup(TEST_STRING), 51, 1, " TEST EXTRA");
    assert_string_equal(editedString, "A towel is the most important tool for a hitchhiker TEST EXTRA");
    free(editedString);

    editedString = stringEdit(strdup(TEST_STRING), 8, 2, "TEST");
    assert_string_equal(editedString, "A towel TEST the most important tool for a hitchhiker.");
    free(editedString);

    /* Shorter replacements */

    editedString = stringEdit(strdup(TEST_STRING), 41, 10, "Ford");
    assert_string_equal(editedString, "A towel is the most important tool for a Ford.");
    free(editedString);

    editedString = stringEdit(strdup(TEST_STRING), 20, 9, "impt.");
    assert_string_equal(editedString, "A towel is the most impt. tool for a hitchhiker.");
    free(editedString);

    editedString = stringEdit(strdup(TEST_STRING), 0, strlen(TEST_STRING), "Hello, World!");
    assert_string_equal(editedString, "Hello, World!");
}

void test_stringToNumberConversions(void **state)
{
    // Yeah, I'm lazy and didn't add tests for octal

    uint8_t uint8Val;
    assert_true(stringToUint8("0", &uint8Val));
    assert_int_equal(uint8Val, 0);
    assert_true(stringToUint8("0x0", &uint8Val));
    assert_int_equal(uint8Val, 0);
    assert_true(stringToUint8("255", &uint8Val));
    assert_int_equal(uint8Val, 255);
    assert_true(stringToUint8("0xff", &uint8Val));
    assert_int_equal(uint8Val, 255);
    assert_false(stringToUint8("-1", &uint8Val));
    // Should retain its prior value when conversion fails
    assert_int_equal(uint8Val, 255);
    assert_false(stringToUint8("256", &uint8Val));
    assert_false(stringToUint8("0x100", &uint8Val));
    assert_false(stringToUint8(NULL, &uint8Val));

    uint16_t uint16Val;
    assert_true(stringToUint16("0", &uint16Val));
    assert_int_equal(uint16Val, 0);
    assert_true(stringToUint16("0x0", &uint16Val));
    assert_int_equal(uint16Val, 0);
    assert_true(stringToUint16("65535", &uint16Val));
    assert_int_equal(uint16Val, 65535);
    assert_true(stringToUint16("0xffff", &uint16Val));
    assert_int_equal(uint16Val, 65535);
    assert_false(stringToUint16("-1", &uint16Val));
    // Should retain its prior value when conversion fails
    assert_int_equal(uint16Val, 65535);
    assert_false(stringToUint16("65536", &uint16Val));
    assert_false(stringToUint16("0x10000", &uint16Val));
    assert_false(stringToUint16(NULL, &uint16Val));

    uint32_t uint32Val;
    assert_true(stringToUint32("0", &uint32Val));
    assert_int_equal(uint32Val, 0);
    assert_true(stringToUint32("0x0", &uint32Val));
    assert_int_equal(uint32Val, 0);
    assert_true(stringToUint32("4294967295", &uint32Val));
    assert_int_equal(uint32Val, 4294967295U);
    assert_true(stringToUint32("0xffffffff", &uint32Val));
    assert_int_equal(uint32Val, 4294967295U);
    assert_false(stringToUint32("-1", &uint32Val));
    // Should retain its prior value when conversion fails
    assert_int_equal(uint32Val, 4294967295U);
    assert_false(stringToUint32("4294967296", &uint32Val));
    assert_false(stringToUint32("0x100000000", &uint32Val));
    assert_false(stringToUint32(NULL, &uint32Val));

    uint64_t uint64Val;
    assert_true(stringToUint64("0", &uint64Val));
    assert_int_equal(uint64Val, 0);
    assert_true(stringToUint64("0x0", &uint64Val));
    assert_int_equal(uint64Val, 0);
    assert_true(stringToUint64("18446744073709551615", &uint64Val));
    assert_int_equal(uint64Val, 18446744073709551615UL);
    assert_true(stringToUint64("0xffffffffffffffff", &uint64Val));
    assert_int_equal(uint64Val, 18446744073709551615UL);
    assert_false(stringToUint64("-1", &uint64Val));
    // Should retain its prior value when conversion fails
    assert_int_equal(uint64Val, 18446744073709551615UL);
    assert_false(stringToUint64("18446744073709551616", &uint64Val));
    assert_false(stringToUint64("0x10000000000000000", &uint64Val));
    assert_false(stringToUint64(NULL, &uint64Val));

    assert_true(stringToUnsignedNumberWithinRange("0", &uint64Val, 0, 0, 10));
    assert_int_equal(uint64Val, 0);
    assert_true(stringToUnsignedNumberWithinRange("10", &uint64Val, 0, 0, 10));
    assert_int_equal(uint64Val, 10);
    assert_false(stringToUnsignedNumberWithinRange("0", &uint64Val, 0, 1, 10));
    assert_false(stringToUnsignedNumberWithinRange("11", &uint64Val, 0, 1, 10));
    assert_false(stringToUnsignedNumberWithinRange(NULL, &uint64Val, 0, 1, 10));

    int8_t int8Val;
    assert_true(stringToInt8("-128", &int8Val));
    assert_int_equal(int8Val, -128);
    assert_true(stringToInt8("127", &int8Val));
    assert_int_equal(int8Val, 127);
    assert_true(stringToInt8("0x7f", &int8Val));
    assert_int_equal(int8Val, 127);
    assert_false(stringToInt8("-129", &int8Val));
    // Should retain its prior value when conversion fails
    assert_int_equal(int8Val, 127);
    assert_false(stringToInt8("128", &int8Val));
    assert_false(stringToInt8("0x80", &int8Val));
    assert_false(stringToInt8(NULL, &int8Val));

    int16_t int16Val;
    assert_true(stringToInt16("-32768", &int16Val));
    assert_int_equal(int16Val, -32768);
    assert_true(stringToInt16("32767", &int16Val));
    assert_int_equal(int16Val, 32767);
    assert_true(stringToInt16("0x7fff", &int16Val));
    assert_int_equal(int16Val, 32767);
    assert_false(stringToInt16("-32769", &int16Val));
    // Should retain its prior value when conversion fails
    assert_int_equal(int16Val, 32767);
    assert_false(stringToInt16("32768", &int16Val));
    assert_false(stringToInt16("0x8000", &int16Val));
    assert_false(stringToInt16(NULL, &int16Val));

    int32_t int32Val;
    assert_true(stringToInt32("-2147483648", &int32Val));
    assert_int_equal(int32Val, INT32_MIN);
    assert_true(stringToInt32("2147483647", &int32Val));
    assert_int_equal(int32Val, INT32_MAX);
    assert_true(stringToInt32("0x7fffffff", &int32Val));
    assert_int_equal(int32Val, INT32_MAX);
    assert_false(stringToInt32("-2147483649", &int32Val));
    // Should retain its prior value when conversion fails
    assert_int_equal(int32Val, INT32_MAX);
    assert_false(stringToInt32("2147483648", &int32Val));
    assert_false(stringToInt32("0x80000000", &int32Val));
    assert_false(stringToInt32(NULL, &int32Val));

    int64_t int64Val;
    assert_true(stringToInt64("-9223372036854775808", &int64Val));
    assert_int_equal(int64Val, INT64_MIN);
    assert_true(stringToInt64("9223372036854775807", &int64Val));
    assert_int_equal(int64Val, INT64_MAX);
    assert_true(stringToInt64("0x7fffffffffffffff", &int64Val));
    assert_int_equal(int64Val, INT64_MAX);
    assert_false(stringToInt64("-9223372036854775809", &int64Val));
    // Should retain its prior value when conversion fails
    assert_int_equal(int64Val, INT64_MAX);
    assert_false(stringToInt64("9223372036854775808", &int64Val));
    assert_false(stringToInt64("0x8000000000000000", &int64Val));
    assert_false(stringToInt64(NULL, &int64Val));

    assert_true(stringToNumberWithinRange("0", &int64Val, 0, -10, 10));
    assert_int_equal(int64Val, 0);
    assert_true(stringToNumberWithinRange("10", &int64Val, 0, -10, 10));
    assert_int_equal(int64Val, 10);
    assert_true(stringToNumberWithinRange("-10", &int64Val, 0, -10, 10));
    assert_int_equal(int64Val, -10);
    assert_false(stringToNumberWithinRange("-10", &int64Val, 0, -9, 10));
    assert_false(stringToNumberWithinRange("11", &int64Val, 0, -9, 10));
    assert_false(stringToNumberWithinRange(NULL, &uint64Val, 0, -9, 10));

    assert_false(stringToBool(NULL));
    assert_false(stringToBool(""));
    assert_false(stringToBool("notABool"));
    assert_false(stringToBool("false"));
    assert_false(stringToBool("No"));
    assert_false(stringToBool("0"));
    assert_true(stringToBool("True"));
    assert_true(stringToBool("yes"));
    assert_true(stringToBool("1"));
}

void test_bitmapToString(void **state)
{
    uint64_t val = 0;

    // No bits set
    char *str = bitmapToStr(val, sizeof(val));
    assert_string_equal(str, "(none)");
    free(str);

    // 1 bit set
    val = 1;
    str = bitmapToStr(val, sizeof(val));
    assert_string_equal(str, "|1|");
    free(str);

    // 2 bits set
    val = 1ULL | 1ULL << 63;
    str = bitmapToStr(val, sizeof(val));
    assert_string_equal(str, "|1|64|");
    free(str);

    // all the things...err bits
    val = UINT64_MAX;
    str = bitmapToStr(val, sizeof(val));
    assert_string_equal(str,
                        "|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|"
                        "35|36|37|38|39|40|41|42|43|44|45|46|47|48|49|50|51|52|53|54|55|56|57|58|59|60|61|62|63|64|");
    free(str);
}

void test_stringIsEmpty(void **state)
{
    (void) state;

    char *str = NULL;
    assert_true(stringIsEmpty(str));

    str = "";
    assert_true(stringIsEmpty(str));

    str = "abc";
    assert_false(stringIsEmpty(str));

    str = "\0abc";
    assert_true(stringIsEmpty(str));

    str = " ";
    assert_false(stringIsEmpty(str));

    str = "\t";
    assert_false(stringIsEmpty(str));

    str = malloc(1);
    str[0] = '\0';
    assert_true(stringIsEmpty(str));
    free(str);
}

static void test_stringIsPrintable(void **state)
{
    assert_true(stringIsPrintable("abcdef_ghi!", false));
    assert_false(stringIsPrintable("abcdef ghi!", false));
    assert_true(stringIsPrintable("abcdef ghi!", true));
    assert_false(stringIsPrintable("@X\x0a\x98\x84\x1b\x0a", true));
}

static void test_stringPad(void **state)
{
    char *inputStr = NULL;
    char *outputStr = NULL;

    // NULL string test
    outputStr = stringPad(inputStr, 1, '0', true);
    assert_null(outputStr);

    // Empty string test
    inputStr = strdup("");
    outputStr = stringPad(inputStr, 7, '0', true);
    assert_string_equal(outputStr, "0000000");
    free(inputStr);
    free(outputStr);

    // Left padding with '0'
    inputStr = strdup("123456789");
    outputStr = stringPad(inputStr, 5, '0', true);
    assert_string_equal(outputStr, "00000123456789");
    free(inputStr);
    free(outputStr);

    // Right padding with '#'
    inputStr = strdup("123456789");
    outputStr = stringPad(inputStr, 3, '#', false);
    assert_string_equal(outputStr, "123456789###");
    free(inputStr);
    free(outputStr);

    // Zero padding length
    inputStr = strdup("test string");
    outputStr = stringPad(inputStr, 0, '0', false);
    assert_string_equal(outputStr, "test string");
    free(inputStr);
    free(outputStr);
}

// Old method of running this test. Need to keep defined for utilTest dependency
bool runStringUtilsTest()
{
    return true;
}

int main(int argc, const char **argv)
{
    const struct CMUnitTest tests[] = {cmocka_unit_test(test_strReplace),
                                       cmocka_unit_test(test_generateRandomToken),
                                       cmocka_unit_test(test_safeStringCopy),
                                       cmocka_unit_test(test_safeStringAppend),
                                       cmocka_unit_test(test_stringCompare),
                                       cmocka_unit_test(test_stringStartsWith),
                                       cmocka_unit_test(test_stringEndsWith),
                                       cmocka_unit_test(test_trimString),
                                       cmocka_unit_test(test_stringSplitOnToken),
                                       cmocka_unit_test(test_stringToLowerCase),
                                       cmocka_unit_test(test_stringToUpperCase),
                                       cmocka_unit_test(test_stringToCamelCase),
                                       cmocka_unit_test(test_stringBuilder),
                                       cmocka_unit_test(test_stringEditDelete),
                                       cmocka_unit_test(test_stringEditReplace),
                                       cmocka_unit_test(test_stringEditInvalidBounds),
                                       cmocka_unit_test(test_stringToNumberConversions),
                                       cmocka_unit_test(test_bitmapToString),
                                       cmocka_unit_test(test_stringIsEmpty),
                                       cmocka_unit_test(test_stringIsPrintable),
                                       cmocka_unit_test(test_stringPad)};

    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    return retval;
}
