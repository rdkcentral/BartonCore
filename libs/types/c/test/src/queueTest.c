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
 * queueTest.c
 *
 * Unit test the icQueue object
 *
 * Author: jelderton - 8/25/15
 *-----------------------------------------------*/

#include <icLog/logging.h>
#include <string.h>
#include <stdlib.h>
#include "queueTest.h"
#include "icTypes/icQueue.h"

#define LOG_CAT     "logTEST"


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
    char *searchStr = (char *)searchVal;
    char *currStr = (char *)item;

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


