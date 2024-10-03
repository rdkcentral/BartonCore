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

/*-----------------------------------------------
 * stringBufferTest.c
 *
 * Unit test the icStringBuffer object
 *
 * Author: mkoch201 - 5/16/18
 *-----------------------------------------------*/

#include "icTypes/icStringBuffer.h"
#include <icLog/logging.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#define LOG_CAT "stringBufferTEST"


static bool canAppendAndGet()
{
    icStringBuffer *buff = stringBufferCreate(1024);

    stringBufferAppend(buff, "ABC");
    stringBufferAppend(buff, "def");
    stringBufferAppend(buff, "123");

    uint32_t len = stringBufferLength(buff);
    if (len != 9)
    {
        // expecting 9, got something else
        icLogError(LOG_CAT, "appendAndGet: expected length of 9, got %" PRIu32, len);
        stringBufferDestroy(buff);
        return false;
    }

    char *out = stringBufferToString(buff);
    if (strcmp(out, "ABCdef123") != 0)
    {
        icLogError(LOG_CAT, "appendAndGet: expected ABCdef123, got %s", out);
        stringBufferDestroy(buff);
        free(out);
        return false;
    }

    stringBufferDestroy(buff);
    free(out);
    return true;
}

static bool canAppendGetAndAppendAgain()
{
    icStringBuffer *buff = stringBufferCreate(1024);

    stringBufferAppend(buff, "ABC");
    char *out1 = stringBufferToString(buff);
    stringBufferAppend(buff, "def");
    char *out2 = stringBufferToString(buff);

    if (strcmp(out1, "ABC") != 0)
    {
        icLogError(LOG_CAT, "canAppendGetAndAppendAgain: expected ABC, got %s", out1);
        stringBufferDestroy(buff);
        free(out1);
        free(out2);
        return false;
    }

    if (strcmp(out2, "ABCdef") != 0)
    {
        icLogError(LOG_CAT, "canAppendGetAndAppendAgain: expected ABCdef, got %s", out1);
        stringBufferDestroy(buff);
        free(out1);
        free(out2);
        return false;
    }

    stringBufferDestroy(buff);
    free(out1);
    free(out2);
    return true;
}

static bool canAppendFormats()
{
    icStringBuffer *buff = stringBufferCreate(1024);
    stringBufferAppendFormat(buff, "%s %d", "test", 123);

    char *out1 = stringBufferToString(buff);

    if (strcmp(out1, "test 123") != 0)
    {
        icLogError(LOG_CAT, "canAppendFormats: expected tes 123, got %s", out1);
        stringBufferDestroy(buff);
        free(out1);
        return false;
    }

    stringBufferDestroy(buff);
    free(out1);
    return true;
}

static bool canClear()
{
    icStringBuffer *buff = stringBufferCreate(1024);
    stringBufferAppend(buff, "1234");
    stringBufferClear(buff);
    stringBufferAppend(buff, "5678");

    char *out1 = stringBufferToString(buff);

    if (strcmp(out1, "5678") != 0)
    {
        icLogError(LOG_CAT, "canClear: expected 5678, got %s", out1);
        stringBufferDestroy(buff);
        free(out1);
        return false;
    }

    stringBufferDestroy(buff);
    free(out1);
    return true;
}


/*
 *
 */
bool runStringBufferTests()
{
    if (canAppendAndGet() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canAppendAndGet");
        return false;
    }
    if (canAppendGetAndAppendAgain() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canAppendGetAndAppendAgain");
        return false;
    }
    if (canAppendFormats() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canAppendFormats");
        return false;
    }
    if (canClear() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canClear");
        return false;
    }

    return true;
}
