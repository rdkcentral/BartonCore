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
 * encodeTest.h
 *
 * Compare old encode/decode functions to 3rdParty
 * counterparts.  Eventually this can be removed
 * as it is a litmus test so we can find suitable
 * replacements.
 *
 * Author: jelderton - 4/18/16
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <icUtil/base64.h>
#include <icUtil/md5.h>
#include "../../../../config/c/public/icConfig/obfuscation.h"

#include "encodeTest.h"

#define BASE_STR_TO_DECODE  "dGhpcyBpcyB0aGUgMXN0IGZyZWFraW5nIHRlc3Q="
#define BAD_STR_TO_DECODE "notbase64"

bool base64EncodeTest(char *testString)
{
    // Base64 encode 'testString' using both mechanisms
    // first the old way
    //
    size_t buffLen = strlen(testString);
    char *result = icEncodeBase64((uint8_t *)testString, (uint16_t)buffLen);
    if (result == NULL)
    {
        printf("unable to BASE-64 encode '%s'\n", testString);
        return false;
    }

    // cleanup & return
    //
    free(result);
    return true;
}

bool base64DecodeTest(char *testString)
{
    // first, old way
    //
    uint8_t *array = NULL;
    uint16_t arrayLen = 0;
    if (icDecodeBase64(testString, &array, &arrayLen) == false)
    {
        printf("unable to BASE-64 decode\n");
        return false;
    }

    // for our test, this should be a string...so print it
    //
    array[arrayLen] = '\0';
    printf("decoded string to be '%s'\n", (char *)array);
    free(array);
    return true;
}

bool checksumTest()
{
    const char *inputString = "let's get the checksum of this string.";

    char *result = icMd5sum(inputString);
    if (result != NULL)
    {
        free(result);
        return true;
    }

    return false;
}

bool runEncodeTests()
{
    if (base64EncodeTest("this is the 1st freaking test") == false)
    {
        printf("base64 encode test FAILED\n");
        return false;
    }

    if (base64DecodeTest(BASE_STR_TO_DECODE) == false)
    {
        printf("base64 decode test FAILED\n");
        return false;
    }

    if(base64DecodeTest(BAD_STR_TO_DECODE) == true)
    {
        printf("base64 decode test with bad input FAILED\n");
        return false;
    }

    if (checksumTest() == false)
    {
        printf("MD5 test FAILED\n");
        return false;
    }

    return true;
}

