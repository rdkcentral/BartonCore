//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2020 Comcast Corporation
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
// Comcast Corporation retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

#include "deviceDiscoveryFilters.h"
#include <deviceService.h>
#include <stdbool.h>

static void destroyDiscoveryFilter(discoveryFilter *filter);

/*
 * clone a device discovery filter items
 *
 * @param item - the discovery filter data
 *
 * @returns a newly created discovery filter.
 */
void *cloneDiscoveryFilterItems(void *item, void *context)
{
    (void) context; // unused

    if (item != NULL)
    {
        discoveryFilter *filter = (discoveryFilter *) item;
        discoveryFilter *retval = discoveryFilterCreate(filter->uriPattern, filter->valuePattern);

        return (void *) retval;
    }
    return NULL;
}

/*
 * create a discovery filter and copy the items to it
 */
discoveryFilter *discoveryFilterCreate(const char *uriPattern, const char *valuePattern)
{
    if (uriPattern != NULL && valuePattern != NULL)
    {
        discoveryFilter *filter = (discoveryFilter *) calloc(1, sizeof(discoveryFilter));
        filter->uriPattern = strdup(uriPattern);
        filter->valuePattern = strdup(valuePattern);

        return filter;
    }

    return NULL;
}

void destroyDiscoveryFilterFromList(void *item)
{
    if (item != NULL)
    {
        destroyDiscoveryFilter((discoveryFilter *) item);
    }
}

static void destroyDiscoveryFilter(discoveryFilter *filter)
{
    if (filter == NULL)
    {
        return;
    }

    free(filter->uriPattern);
    free(filter->valuePattern);

    free(filter);
}
