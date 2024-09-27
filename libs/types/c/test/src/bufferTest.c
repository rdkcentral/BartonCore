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
 * bufferTest.c
 *
 * Unit test the icSimpleBuffer object
 *
 * Author: jelderton - 4/3/18
 *-----------------------------------------------*/

#include <icLog/logging.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "queueTest.h"
#include "icTypes/icFifoBuffer.h"

#define LOG_CAT     "buffTEST"


static bool canAddSmallStrings()
{
    icFifoBuff *buff = fifoBuffCreate(1024);

    // add 3 strings to the buffer
    //
    fifoBuffPush(buff, "ABC", 3);
    fifoBuffPush(buff, "123", 3);
    fifoBuffPush(buff, "xyz", 3);

    uint32_t len = fifoBuffGetPullAvailable(buff);
    if (len != 9)
    {
        // expecting 9, got something else
        icLogError(LOG_CAT, "add: expected length of 9, got %"PRIu32, len);
        fifoBuffDestroy(buff);
        return false;
    }

    fifoBuffDestroy(buff);
    return true;
}

static bool canReadSmallStrings()
{
    icFifoBuff *buff = fifoBuffCreate(1024);

    // add a small string to the buffer
    //
    const char *msg = "this is a test of the icBuffer object";
    fifoBuffPush(buff, (void *)msg, (uint32_t)strlen(msg));

    // pull 7 chars
    char sample[10];
    memset(sample, 0, 10);
    fifoBuffPull(buff, sample, 7);
    icLogDebug(LOG_CAT, "retrieved partial buffer string '%s'", sample);

    // make sure add again works
    const char *msg2 = " another string to append";
    fifoBuffPush(buff, (void *)msg2, (uint32_t)strlen(msg2));

    // len should be the length of both strings, minus the 7 chars read before
    //
    uint32_t len = fifoBuffGetPullAvailable(buff);
    uint32_t expect = (strlen(msg) + strlen(msg2) - 7);
    if (len != expect)
    {
        // expecting 9, got something else
        icLogError(LOG_CAT, "read: expected length of %"PRIu32", got %"PRIu32, expect, len);
        fifoBuffDestroy(buff);
        return false;
    }

    fifoBuffDestroy(buff);
    return true;
}

/*
 *
 */
bool runBufferTests()
{
    if (canAddSmallStrings() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canAddSmallStrings");
        return false;
    }
    if (canReadSmallStrings() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canReadSmallStrings");
        return false;
    }

    return true;
}


