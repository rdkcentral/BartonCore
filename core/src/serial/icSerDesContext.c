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
//
// Created by mdeleo739 on 6/10/19.
//

#include <icLog/logging.h>
#include <serial/icSerDesContext.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "serDesContext"

icSerDesContext *serDesCreateContext(void)
{
    icSerDesContext *context = (icSerDesContext *) calloc(1, sizeof(icSerDesContext));
    context->props = stringHashMapCreate();
    return context;
}

void serDesDestroyContext(icSerDesContext *context)
{
    if (context != NULL)
    {
        stringHashMapDestroy(context->props, NULL);
        free(context);
    }
}

bool serDesSetContextValue(icSerDesContext *context, const char *key, const char *value)
{
    if (context == NULL)
    {
        icLogWarn(LOG_TAG, "Attempting to set value on null context: \"%s\" -> \"%s\"", key, value);
        return false;
    }
    return stringHashMapPut(context->props, strdup(key), strdup(value));
}

bool serDesHasContextValue(const icSerDesContext *context, const char *key)
{
    if (context == NULL)
    {
        icLogWarn(LOG_TAG, "Attempting to verify value on null context: \"%s\"", key);
        return false;
    }
    return stringHashMapContains(context->props, key);
}

const char *serDesGetContextValue(const icSerDesContext *context, const char *key)
{
    if (context == NULL)
    {
        icLogWarn(LOG_TAG, "Attempting to access value on null context: \"%s\"", key);
        return false;
    }
    return stringHashMapGet(context->props, key);
}
