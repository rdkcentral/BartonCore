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

#include "deviceServicePrivate.h"
#include "icTypes/icLinkedList.h"
#include <device/icDeviceEndpoint.h>
#include <device/icDeviceMetadata.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <jsonHelper/jsonHelper.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG                      "deviceService"

/*
 * Created by Thomas Lea on 8/10/15.
 */

// Keys for endpoint json representation
#define ENDPOINT_URI_KEY             "uri"
#define ENDPOINT_ID_KEY              "id"
#define ENDPOINT_PROFILE_KEY         "profile"
#define ENDPOINT_PROFILE_VERSION_KEY "profileVersion"
#define ENDPOINT_ENABLED_KEY         "enabled"
#define ENDPOINT_RESOURCES_KEY       "resources"
#define ENDPOINT_METADATAS_KEY       "metadatas"

extern inline void endpointDestroy__auto(icDeviceEndpoint **endpoint);

void endpointDestroy(icDeviceEndpoint *endpoint)
{
    if (endpoint != NULL)
    {
        free(endpoint->id);
        free(endpoint->profile);
        free(endpoint->deviceUuid);
        free(endpoint->uri);
        linkedListDestroy(endpoint->resources, (linkedListItemFreeFunc) resourceDestroy);
        linkedListDestroy(endpoint->metadata, (linkedListItemFreeFunc) metadataDestroy);

        free(endpoint);
    }
}

void endpointPrint(icDeviceEndpoint *endpoint, const char *prefix)
{
    if (endpoint == NULL)
    {
        icLogDebug(LOG_TAG, "%sEndpoint [NULL!]", prefix);
    }
    else
    {
        icLogDebug(LOG_TAG, "%sEndpoint", prefix);
        icLogDebug(LOG_TAG, "%s\tid=%s", prefix, endpoint->id);
        icLogDebug(LOG_TAG, "%s\turi=%s", prefix, endpoint->uri);
        icLogDebug(LOG_TAG, "%s\tprofile=%s", prefix, endpoint->profile);
        icLogDebug(LOG_TAG, "%s\tprofileVersion=%d", prefix, endpoint->profileVersion);
        icLogDebug(LOG_TAG, "%s\tdeviceUuid=%s", prefix, endpoint->deviceUuid);
        icLogDebug(LOG_TAG, "%s\tenabled=%s", prefix, (endpoint->enabled == true) ? "true" : "false");


        // add indentation to our lower level stuff
        char *newPrefix;
        if (prefix == NULL)
        {
            newPrefix = (char *) malloc(3); // \t\t\0
            sprintf(newPrefix, "\t\t");
        }
        else
        {
            newPrefix = (char *) malloc(strlen(prefix) + 3); //\t\t\0
            sprintf(newPrefix, "%s\t\t", prefix);
        }

        icLogDebug(LOG_TAG, "%s\tresources:", prefix);
        icLinkedListIterator *iterator = linkedListIteratorCreate(endpoint->resources);
        while (linkedListIteratorHasNext(iterator))
        {
            icDeviceResource *resource = (icDeviceResource *) linkedListIteratorGetNext(iterator);
            resourcePrint(resource, newPrefix);
        }
        linkedListIteratorDestroy(iterator);

        icLogDebug(LOG_TAG, "%s\tmetadata:", prefix);
        iterator = linkedListIteratorCreate(endpoint->metadata);
        while (linkedListIteratorHasNext(iterator))
        {
            icDeviceMetadata *metadata = (icDeviceMetadata *) linkedListIteratorGetNext(iterator);
            metadataPrint(metadata, newPrefix);
        }
        linkedListIteratorDestroy(iterator);

        free(newPrefix);
    }
}

/**
 * Metadata clone function for linked list
 *
 * @param item the item to clone
 * @param context the context
 * @return the cloned metadata object
 */
static void *cloneMetadataWithContext(void *item, void *context)
{
    (void) context;
    return metadataClone((icDeviceMetadata *) item);
}

/**
 * Resource clone function for linked list
 *
 * @param item the item to clone
 * @param context the context
 * @return the cloned resource object
 */
static void *cloneResourceWithContext(void *item, void *context)
{
    (void) context;
    return resourceClone((icDeviceResource *) item);
}

/**
 * Clone a endpoint
 *
 * @param endpoint the endpoint to clone
 * @return the cloned endpoint object
 */
icDeviceEndpoint *endpointClone(const icDeviceEndpoint *endpoint)
{
    // Clone endpoint info
    icDeviceEndpoint *clone = NULL;
    if (endpoint != NULL)
    {
        clone = calloc(1, sizeof(icDeviceEndpoint));
        if (endpoint->uri != NULL)
        {
            clone->uri = strdup(endpoint->uri);
        }
        if (endpoint->id != NULL)
        {
            clone->id = strdup(endpoint->id);
        }
        clone->profileVersion = endpoint->profileVersion;
        if (endpoint->profile != NULL)
        {
            clone->profile = strdup(endpoint->profile);
        }
        if (endpoint->deviceUuid != NULL)
        {
            clone->deviceUuid = strdup(endpoint->deviceUuid);
        }
        clone->enabled = endpoint->enabled;
        // Clone resources
        clone->resources = linkedListDeepClone(endpoint->resources, cloneResourceWithContext, NULL);
        // Clone metadata
        clone->metadata = linkedListDeepClone(endpoint->metadata, cloneMetadataWithContext, NULL);
    }
    else
    {
        icLogWarn(LOG_TAG, "Attempt to clone NULL endpoint");
    }
    return clone;
}

/**
 * Convert endpoint object to JSON
 *
 * @param endpoint the endpoint to convert
 * @return the JSON object
 */
cJSON *endpointToJSON(const icDeviceEndpoint *endpoint, const icSerDesContext *context)
{
    cJSON *json = cJSON_CreateObject();
    // Add endpoint info
    cJSON_AddStringToObject(json, ENDPOINT_URI_KEY, endpoint->uri);
    cJSON_AddStringToObject(json, ENDPOINT_ID_KEY, endpoint->id);
    cJSON_AddStringToObject(json, ENDPOINT_PROFILE_KEY, endpoint->profile);
    cJSON_AddBoolToObject(json, ENDPOINT_ENABLED_KEY, endpoint->enabled);
    cJSON_AddNumberToObject(json, ENDPOINT_PROFILE_VERSION_KEY, endpoint->profileVersion);

    // Add resources by id
    cJSON *resources = resourcesToJSON(endpoint->resources, context);
    cJSON_AddItemToObject(json, ENDPOINT_RESOURCES_KEY, resources);

    // Add metadata by id
    cJSON *metadatasJson = metadatasToJSON(endpoint->metadata, context);
    cJSON_AddItemToObject(json, ENDPOINT_METADATAS_KEY, metadatasJson);

    return json;
}

/**
 * Convert a list of endpoint objects to a JSON object with id as key
 *
 * @param endpoints the list of endpoints
 * @return the JSON object
 */
cJSON *endpointsToJSON(icLinkedList *endpoints, const icSerDesContext *context)
{
    // Add endpoints by id
    cJSON *endpointsJson = cJSON_CreateObject();
    icLinkedListIterator *iter = linkedListIteratorCreate(endpoints);
    while (linkedListIteratorHasNext(iter))
    {
        icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(iter);
        cJSON *endpointJson = endpointToJSON(endpoint, context);
        cJSON_AddItemToObject(endpointsJson, endpoint->id, endpointJson);
    }
    linkedListIteratorDestroy(iter);

    return endpointsJson;
}

/**
 * Load a device endpoint into memory from JSON
 *
 * @param deviceUUID the device UUID for which we are loading the metadata
 * @param endpointJSON the JSON to load
 * @return the endpoint object or NULL if there is an error
 */
icDeviceEndpoint *endpointFromJSON(const char *deviceUUID, cJSON *endpointJSON, const icSerDesContext *context)
{
    icDeviceEndpoint *endpoint = NULL;

    if (endpointJSON != NULL && deviceUUID != NULL)
    {
        scoped_icDeviceEndpoint *tempEndpoint = (icDeviceEndpoint *) calloc(1, sizeof(icDeviceEndpoint));
        if (stringIsEmpty(deviceUUID) == true)
        {
            icLogError(LOG_TAG, "Invalid endpoint no deviceUUID!");
            return NULL;
        }
        else
        {
            tempEndpoint->deviceUuid = strdup(deviceUUID);
        }

        tempEndpoint->uri = getCJSONString(endpointJSON, ENDPOINT_URI_KEY);
        const char *deviceEndpointUri = stringCoalesceAlt(tempEndpoint->uri, "");

        // validate the endpoint id for valid alphanumeric value.
        //
        tempEndpoint->id = getCJSONString(endpointJSON, ENDPOINT_ID_KEY);
        if (stringIsPrintable(tempEndpoint->id, false) == false)
        {
            icLogError(LOG_TAG, "Invalid endpoint %s: invalid id!", deviceEndpointUri);
            return NULL;
        }

        if (endpointUriIsValid(tempEndpoint->uri, deviceUUID, tempEndpoint->id) == false)
        {
            icLogError(LOG_TAG, "Invalid endpoint %s: invalid uri!", deviceEndpointUri);
            return NULL;
        }

        if (getCJSONBool(endpointJSON, ENDPOINT_ENABLED_KEY, &(tempEndpoint->enabled)) == false)
        {
            icLogError(LOG_TAG, "Invalid endpoint %s: invalid value enabled!", deviceEndpointUri);
            return NULL;
        }

        tempEndpoint->profile = getCJSONString(endpointJSON, ENDPOINT_PROFILE_KEY);
        if (stringIsEmpty(tempEndpoint->profile) == true)
        {
            icLogError(LOG_TAG, "Invalid endpoint %s: no profile!", deviceEndpointUri);
            return NULL;
        }

        uint8_t profileVersion = 1;
        getCJSONUInt8(endpointJSON, ENDPOINT_PROFILE_VERSION_KEY, &profileVersion);
        tempEndpoint->profileVersion = profileVersion;

        tempEndpoint->resources = resourcesFromJSON(
            deviceUUID, tempEndpoint->id, cJSON_GetObjectItem(endpointJSON, ENDPOINT_RESOURCES_KEY), context, false);
        if (tempEndpoint->resources == NULL)
        {
            icLogError(LOG_TAG, "Invalid endpoint %s: invalid resources!", deviceEndpointUri);
            return NULL;
        }

        tempEndpoint->metadata = metadatasFromJSON(
            deviceUUID, tempEndpoint->id, cJSON_GetObjectItem(endpointJSON, ENDPOINT_METADATAS_KEY), false);
        if (tempEndpoint->metadata == NULL)
        {
            icLogError(LOG_TAG, "Invalid endpoint %s: invalid metadatas!", deviceEndpointUri);
            return NULL;
        }

        // at this stage we have valid endpoint, create a reference of tempEndpoint and make it NULL.
        // so that auto clean have no effect.
        //
        endpoint = tempEndpoint;
        tempEndpoint = NULL;
    }
    else
    {
        icLogError(LOG_TAG, "Failed to find endpoint json for device %s", stringCoalesceAlt(deviceUUID, ""));
    }

    return endpoint;
}

icLinkedList *
endpointsFromJSON(const char *deviceUUID, cJSON *endpointsJSON, const icSerDesContext *context, bool permissive)
{
    icLinkedList *endpoints = NULL;

    if (endpointsJSON != NULL)
    {
        endpoints = linkedListCreate();
        int endpointCount = cJSON_GetArraySize(endpointsJSON);

        for (int i = 0; i < endpointCount; ++i)
        {
            cJSON *endpointJson = cJSON_GetArrayItem(endpointsJSON, i);
            icDeviceEndpoint *endpoint = endpointFromJSON(deviceUUID, endpointJson, context);

            if (endpoint != NULL)
            {
                linkedListAppend(endpoints, endpoint);
            }
            else
            {
                icLogWarn(LOG_TAG, "Failed to read endpoint %s from device %s", endpointJson->string, deviceUUID);

                if (permissive == false)
                {
                    linkedListDestroy(endpoints, (linkedListItemFreeFunc) endpointDestroy);
                    endpoints = NULL;
                    break;
                }
            }
        }
    }
    else
    {
        icLogError(LOG_TAG, "Unable to find endpoints entry in device JSON for device %s", deviceUUID);
    }

    return endpoints;
}

extern inline char *endpointUriCreate(const char *deviceUuid, const char *endpointId);

bool endpointUriIsValid(const char *deviceEndpointUri, const char *deviceUuid, const char *endpointId)
{
    bool retValue = false;

    if ((stringIsEmpty(deviceEndpointUri) == false) && (stringIsEmpty(deviceUuid) == false) &&
        (stringIsEmpty(endpointId) == false))
    {
        scoped_generic char *tempUri = endpointUriCreate(deviceUuid, endpointId);

        if (stringCompare(deviceEndpointUri, tempUri, true) == 0)
        {
            retValue = true;
        }
    }

    return retValue;
}

bool endpointsSetProfileVersion(icLinkedList *endpoints, uint8_t profileVersion)
{
    if (endpoints == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid argument", __func__);
        return false;
    }

    scoped_icLinkedListIterator *iter = linkedListIteratorCreate(endpoints);
    while (linkedListIteratorHasNext(iter))
    {
        icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(iter);
        endpoint->profileVersion = profileVersion;
    }

    return true;
}
