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
 * queueTest.c
 *
 * Unit test the icQueue object
 *
 * Author: jelderton - 8/25/15
 *-----------------------------------------------*/

#include "queueTest.h"
#include "icTypes/icQueue.h"
#include <icLog/logging.h>
#include <stdlib.h>
#include <string.h>

#define LOG_CAT "logTEST"


static bool canAddItemsToQueue()
{
    icQueue *q = queueCreate();

    // add 3 strings to the queue
    //
    queuePush(q, strdup("abc"));
    queuePush(q, strdup("123"));
    queuePush(q, strdup("xyz"));

    queueDestroy(q, NULL);

    return true;
}

static bool canDelItemsFromQueue()
{
    icQueue *q = queueCreate();

    // add 3 strings to the queue
    //
    queuePush(q, strdup("abc"));
    queuePush(q, strdup("123"));
    queuePush(q, strdup("xyz"));

    while (queueCount(q) > 0)
    {
        // call pop 3 times to see that they come out in the correct order
        //
        char *str = (char *) queuePop(q);
        icLogDebug(LOG_CAT, "got queue item '%s'", str);
        free(str);
    }
    queueDestroy(q, NULL);

    return true;
}

static bool canAddAndDelItemsFromQueue()
{
    icQueue *q = queueCreate();

    // add 3 strings to the queue
    //
    queuePush(q, strdup("abc"));
    queuePush(q, strdup("123"));
    queuePush(q, strdup("xyz"));

    // pull the last one
    //
    char *str = (char *) queuePop(q);
    icLogDebug(LOG_CAT, "got queue item '%s'", str);
    free(str);

    // add 2 more
    //
    queuePush(q, strdup("456"));
    queuePush(q, strdup("789"));

    // pop the stack
    //
    while (queueCount(q) > 0)
    {
        // call pop 3 times to see that they come out in the correct order
        //
        str = (char *) queuePop(q);
        icLogDebug(LOG_CAT, "got queue item '%s'", str);
        free(str);
    }
    queueDestroy(q, NULL);

    return true;
}

static bool queueFindFunc(void *searchVal, void *item)
{
    char *searchStr = (char *) searchVal;
    char *currStr = (char *) item;

    if (strcmp(searchStr, currStr) == 0)
    {
        return true;
    }
    return false;
}

static bool canAddAndFindItemsFromQueue()
{
    icQueue *q = queueCreate();

    // add 3 strings to the queue
    //
    queuePush(q, strdup("abc"));
    queuePush(q, strdup("123"));
    queuePush(q, strdup("xyz"));

    // find each of them
    //
    char *search = NULL;
    if ((search = queueFind(q, "abc", queueFindFunc)) == NULL)
    {
        // oops.
        icLogError(LOG_CAT, "unable to find 'abc' from the queue");
        queueDestroy(q, NULL);
        return false;
    }
    if ((search = queueFind(q, "123", queueFindFunc)) == NULL)
    {
        // oops.
        icLogError(LOG_CAT, "unable to find '123' from the queue");
        queueDestroy(q, NULL);
        return false;
    }
    if ((search = queueFind(q, "xyz", queueFindFunc)) == NULL)
    {
        // oops.
        icLogError(LOG_CAT, "unable to find 'xyz' from the queue");
        queueDestroy(q, NULL);
        return false;
    }

    queueDestroy(q, NULL);
    return true;
}

/*
 *
 */
bool runQueueTests()
{
    if (canAddItemsToQueue() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canAddItemsToQueue");
        return false;
    }

    if (canDelItemsFromQueue() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canDelItemsFromQueue");
        return false;
    }

    if (canAddAndDelItemsFromQueue() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canAddAndDelItemsFromQueue");
        return false;
    }

    if (canAddAndFindItemsFromQueue() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canAddAndDelItemsFromQueue");
        return false;
    }

    return true;
}
