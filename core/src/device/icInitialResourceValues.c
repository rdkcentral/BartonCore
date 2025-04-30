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
//
// Created by mkoch201 on 4/15/19.
//

#include <icLog/logging.h>
#include <icTypes/icStringHashMap.h>
#include <icUtil/stringUtils.h>
#include <stdlib.h>
#include <string.h>

#include "device/icInitialResourceValues.h"

#define ENDPOINT_FORMAT "%s/%s"
#define LOG_TAG         "initialResourceValues"

/**
 * This is just an abstraction around a stringHashMap, hiding from the client the way we form the keys so we can
 * distinguish between device and endpoint resources
 */

/**
 * Create a new instance
 * @return the created instance
 */
icInitialResourceValues *initialResourceValuesCreate()
{
    return (icInitialResourceValues *) stringHashMapCreate();
}

/**
 * Destroy an instance
 * @param initialResourceValues the instance to destroy
 */
void initialResourceValuesDestroy(icInitialResourceValues *initialResourceValues)
{
    stringHashMapDestroy((icStringHashMap *) initialResourceValues, NULL);
}

/**
 * Internal helper that will handle the put/replace as well as a regular put
 * @param values the set of values
 * @param resourceId the resource id
 * @param value the value
 * @param allowReplace true to allow replace, false otherwise
 * @return true if the value was put
 */
static bool
internalDeviceValuePut(icInitialResourceValues *values, const char *resourceId, const char *value, bool allowReplace)
{
    bool success = false;
    if (values != NULL && resourceId != NULL)
    {
        if (allowReplace)
        {
            // To support replace, we first delete whatever is there
            stringHashMapDelete((icStringHashMap *) values, resourceId, NULL);
        }
        char *key = strdup(resourceId);
        char *valueToPut = value != NULL ? strdup(value) : NULL;
        // Now put the new value
        success = stringHashMapPut((icStringHashMap *) values, key, valueToPut);
        if (!success)
        {
            free(key);
            free(valueToPut);
        }
    }

    return success;
}

/**
 * Put/Replace an initial value for a device resource
 *
 * @param values the set of values
 * @param resourceId the resource id
 * @param value the value
 * @return true if successful, false otherwise(invalid inputs)
 */
bool initialResourceValuesPutDeviceValue(icInitialResourceValues *values, const char *resourceId, const char *value)
{
    return internalDeviceValuePut(values, resourceId, value, true);
}

/**
 * Put an initial value for a device resource if none already exists
 * @param values the value to put
 * @param resourceId the resource id
 * @param value the value
 * @return true if value was put, false otherwise
 */
bool initialResourceValuesPutDeviceValueIfNotExists(icInitialResourceValues *values,
                                                    const char *resourceId,
                                                    const char *value)
{
    return internalDeviceValuePut(values, resourceId, value, false);
}

/**
 * Internal helper that will handle the put/replace as well as a regular put
 * @param values the set of values
 * @param resourceId the resource id
 * @param value the value
 * @param allowReplace true to allow replace, false otherwise
 * @return true if the value was put
 */
static bool internalEndpointValuePut(icInitialResourceValues *values,
                                     const char *endpointId,
                                     const char *resourceId,
                                     const char *value,
                                     bool allowReplace)
{
    bool success = false;
    if (values != NULL && endpointId != NULL && resourceId != NULL)
    {
        char *key = stringBuilder(ENDPOINT_FORMAT, endpointId, resourceId);
        char *valueToPut = value != NULL ? strdup(value) : NULL;
        if (allowReplace)
        {
            // To support replace, we first delete whatever is there
            stringHashMapDelete((icStringHashMap *) values, key, NULL);
        }
        // Now put the new value
        success = stringHashMapPut((icStringHashMap *) values, key, valueToPut);
        if (!success)
        {
            free(key);
            free(valueToPut);
        }
    }

    return success;
}

/**
 * Put/Replace an initial value for an endpoint resource
 *
 * @param values the set of values
 * @param endpointId the endpoint id
 * @param resourceId the resource id
 * @param value the value
 * @return true if successful, false otherwise(invalid inputs)
 */
bool initialResourceValuesPutEndpointValue(icInitialResourceValues *values,
                                           const char *endpointId,
                                           const char *resourceId,
                                           const char *value)
{
    return internalEndpointValuePut(values, endpointId, resourceId, value, true);
}

/**
 * Put an initial value for an endpoint resource if none already exists
 *
 * @param values the set of values
 * @param endpointId the endpoint id
 * @param resourceId the resource id
 * @param value the value
 * @return true if value was put, false otherwise
 */
bool initialResourceValuesPutEndpointValueIfNotExists(icInitialResourceValues *values,
                                                      const char *endpointId,
                                                      const char *resourceId,
                                                      const char *value)
{
    return internalEndpointValuePut(values, endpointId, resourceId, value, false);
}

/**
 * Check if an initial value exists for a device resource
 * @param values the set of values
 * @param resourceId the resource id
 * @return true if an entry for the initial value exists(even if the value is NULL), false if it does not exist
 */
bool initialResourceValuesHasDeviceValue(icInitialResourceValues *values, const char *resourceId)
{
    bool hasValue = false;
    if (values != NULL && resourceId != NULL)
    {
        hasValue = stringHashMapContains((icStringHashMap *) values, resourceId);
    }

    return hasValue;
}

/**
 * Check if an initial value exists for an endpoint resource
 * @param values the set of values
 * @param resourceId the resource id
 * @return true if an entry for the initial value exists(even if the value is NULL), false if it does not exist
 */
bool initialResourceValuesHasEndpointValue(icInitialResourceValues *values,
                                           const char *endpointId,
                                           const char *resourceId)
{
    bool hasValue = false;
    if (values != NULL && endpointId != NULL && resourceId != NULL)
    {
        char *key = stringBuilder(ENDPOINT_FORMAT, endpointId, resourceId);

        hasValue = stringHashMapContains((icStringHashMap *) values, key);

        free(key);
    }

    return hasValue;
}

/**
 * Get the initial value for a device resource.  Using this function you cannot distinguish between a non-existant
 * value and NULL value.
 * @param values the set of values
 * @param resourceId the resource id
 * @return the initial value, which may be NULL
 */
const char *initialResourceValuesGetDeviceValue(icInitialResourceValues *values, const char *resourceId)
{
    char *val = NULL;
    if (values != NULL && resourceId != NULL)
    {
        val = stringHashMapGet((icStringHashMap *) values, resourceId);
    }

    return val;
}

/**
 * Get the initial value for an endpoint resource.  Using this function you cannot distinguish between a non-existant
 * value and NULL value.
 * @param values the set of values
 * @param endpointId the endpoint id
 * @param resourceId the resource id
 * @return the initial value, which may be NULL
 */
const char *
initialResourceValuesGetEndpointValue(icInitialResourceValues *values, const char *endpointId, const char *resourceId)
{
    char *val = NULL;
    if (values != NULL && endpointId != NULL && resourceId != NULL)
    {
        char *key = stringBuilder(ENDPOINT_FORMAT, endpointId, resourceId);

        val = stringHashMapGet((icStringHashMap *) values, key);

        free(key);
    }

    return val;
}

/**
 * Log all the initial resources values that have been set
 * @param values the initial values
 */
void initialResourcesValuesLogValues(icInitialResourceValues *values)
{
    if (values != NULL)
    {
        icLogDebug(LOG_TAG, "Initial Resource Values:");
        icStringHashMapIterator *iter = stringHashMapIteratorCreate((icStringHashMap *) values);
        while (stringHashMapIteratorHasNext(iter))
        {
            char *key;
            char *value;
            stringHashMapIteratorGetNext(iter, &key, &value);

            // Not very flexible, but hide some common sensitive values
            if (strstr(key, "Password") != NULL || strstr(key, "UserId") != NULL)
            {
                value = "<Sensitive Value>";
            }

            icLogDebug(LOG_TAG, "   %s=%s", key, value == NULL ? "NULL" : value);
        }
        stringHashMapIteratorDestroy(iter);
    }
}
