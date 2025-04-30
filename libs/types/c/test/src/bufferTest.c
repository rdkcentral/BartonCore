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
 * bufferTest.c
 *
 * Unit test the icSimpleBuffer object
 *
 * Author: jelderton - 4/3/18
 *-----------------------------------------------*/

#include "icTypes/icFifoBuffer.h"
#include "queueTest.h"
#include <icLog/logging.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#define LOG_CAT "buffTEST"


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
        icLogError(LOG_CAT, "add: expected length of 9, got %" PRIu32, len);
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
    fifoBuffPush(buff, (void *) msg, (uint32_t) strlen(msg));

    // pull 7 chars
    char sample[10];
    memset(sample, 0, 10);
    fifoBuffPull(buff, sample, 7);
    icLogDebug(LOG_CAT, "retrieved partial buffer string '%s'", sample);

    // make sure add again works
    const char *msg2 = " another string to append";
    fifoBuffPush(buff, (void *) msg2, (uint32_t) strlen(msg2));

    // len should be the length of both strings, minus the 7 chars read before
    //
    uint32_t len = fifoBuffGetPullAvailable(buff);
    uint32_t expect = (strlen(msg) + strlen(msg2) - 7);
    if (len != expect)
    {
        // expecting 9, got something else
        icLogError(LOG_CAT, "read: expected length of %" PRIu32 ", got %" PRIu32, expect, len);
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
