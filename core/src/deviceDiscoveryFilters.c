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
