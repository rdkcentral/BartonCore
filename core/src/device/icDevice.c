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
/*
 * Created by Thomas Lea on 8/6/15.
 */

#include "deviceServicePrivate.h"
#include <device-driver/device-driver.h>
#include <device/icDevice.h>
#include <device/icDeviceMetadata.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <jsonHelper/jsonHelper.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG                  "deviceService"

// Keys for device json representation
#define DEVICE_DRIVER_KEY        "deviceDriver"
#define DEVICE_CLASS_KEY         "deviceClass"
#define DEVICE_CLASS_VERSION_KEY "deviceClassVersion"
#define DEVICE_URI_KEY           "uri"
#define DEVICE_UUID_KEY          "uuid"
#define DEVICE_ENDPOINTS_KEY     "deviceEndpoints"
#define DEVICE_RESOURCES_KEY     "deviceResources"
#define DEVICE_METADATAS_KEY     "metadatas"

void deviceDestroy(icDevice *device)
{
    if (device != NULL)
    {
        free(device->deviceClass);
        free(device->managingDeviceDriver);
        free(device->uuid);
        free(device->uri);
        linkedListDestroy(device->endpoints, (linkedListItemFreeFunc) endpointDestroy);
        linkedListDestroy(device->resources, (linkedListItemFreeFunc) resourceDestroy);
        linkedListDestroy(device->metadata, (linkedListItemFreeFunc) metadataDestroy);
        free(device);
    }
}

extern inline void deviceDestroy__auto(icDevice **device);
extern inline void deviceListDestroy(icLinkedList *deviceList);
extern inline void deviceListDestroy__auto(icLinkedList **deviceList);

void devicePrint(icDevice *device, const char *prefix)
{
    if (device == NULL)
    {
        icLogDebug(LOG_TAG, "%sDevice [NULL!]", prefix);
    }
    else
    {
        icLogDebug(LOG_TAG, "%sDevice", prefix);
        icLogDebug(LOG_TAG, "%s\tuuid=%s", prefix, device->uuid);
        icLogDebug(LOG_TAG, "%s\tdeviceClass=%s", prefix, device->deviceClass);
        icLogDebug(LOG_TAG, "%s\tdeviceClassVersion=%d", prefix, device->deviceClassVersion);
        icLogDebug(LOG_TAG, "%s\turi=%s", prefix, device->uri);
        icLogDebug(LOG_TAG, "%s\tmanagingDeviceDriver=%s", prefix, device->managingDeviceDriver);


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
        icLinkedListIterator *iterator = linkedListIteratorCreate(device->resources);
        while (linkedListIteratorHasNext(iterator))
        {
            icDeviceResource *resource = (icDeviceResource *) linkedListIteratorGetNext(iterator);
            resourcePrint(resource, newPrefix);
        }
        linkedListIteratorDestroy(iterator);

        icLogDebug(LOG_TAG, "%s\tendpoints:", prefix);
        iterator = linkedListIteratorCreate(device->endpoints);
        while (linkedListIteratorHasNext(iterator))
        {
            icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(iterator);
            endpointPrint(endpoint, newPrefix);
        }
        linkedListIteratorDestroy(iterator);

        icLogDebug(LOG_TAG, "%s\tmetadata:", prefix);
        iterator = linkedListIteratorCreate(device->metadata);
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
 * Endpoint clone function for linked list
 *
 * @param item the item to clone
 * @param context the context
 * @return the cloned endpoint object
 */
static void *cloneEndpointWithContext(void *item, void *context)
{
    (void) context;
    return endpointClone((icDeviceEndpoint *) item);
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
 * Clone a device
 *
 * @param device the device to clone
 * @return the cloned device object
 */
icDevice *deviceClone(const icDevice *device)
{
    // Clone device info
    icDevice *clone = NULL;
    if (device != NULL)
    {
        clone = calloc(1, sizeof(icDevice));
        if (device->uri != NULL)
        {
            clone->uri = strdup(device->uri);
        }
        clone->deviceClassVersion = device->deviceClassVersion;
        if (device->deviceClass)
        {
            clone->deviceClass = strdup(device->deviceClass);
        }
        if (device->managingDeviceDriver != NULL)
        {
            clone->managingDeviceDriver = strdup(device->managingDeviceDriver);
        }
        if (device->uuid != NULL)
        {
            clone->uuid = strdup(device->uuid);
        }

        // Clone endpoints
        if (device->endpoints != NULL)
        {
            clone->endpoints = linkedListDeepClone(device->endpoints, cloneEndpointWithContext, NULL);
        }

        // Clone resources
        if (device->resources != NULL)
        {
            clone->resources = linkedListDeepClone(device->resources, cloneResourceWithContext, NULL);
        }

        // Clone metadata
        if (device->metadata != NULL)
        {
            clone->metadata = linkedListDeepClone(device->metadata, cloneMetadataWithContext, NULL);
        }
    }
    else
    {
        icLogWarn(LOG_TAG, "Attempt to clone NULL device");
    }
    return clone;
}

/**
 * Convert device object to JSON
 *
 * @param device the device to convert
 * @return the JSON object
 */
cJSON *deviceToJSON(const icDevice *device, const icSerDesContext *context)
{
    cJSON *json = cJSON_CreateObject();

    // Add device info
    cJSON_AddStringToObject(json, DEVICE_DRIVER_KEY, device->managingDeviceDriver);
    cJSON_AddStringToObject(json, DEVICE_CLASS_KEY, device->deviceClass);
    cJSON_AddNumberToObject(json, DEVICE_CLASS_VERSION_KEY, device->deviceClassVersion);
    cJSON_AddStringToObject(json, DEVICE_URI_KEY, device->uri);
    cJSON_AddStringToObject(json, DEVICE_UUID_KEY, device->uuid);

    // Add endpoints by id
    cJSON *endpoints = endpointsToJSON(device->endpoints, context);
    cJSON_AddItemToObject(json, DEVICE_ENDPOINTS_KEY, endpoints);

    // Add resources by id
    cJSON *resources = resourcesToJSON(device->resources, context);
    cJSON_AddItemToObject(json, DEVICE_RESOURCES_KEY, resources);

    // Add metadata by id
    cJSON *metadatasJson = metadatasToJSON(device->metadata, context);
    cJSON_AddItemToObject(json, DEVICE_METADATAS_KEY, metadatasJson);

    return json;
}

/**
 * Retrieve a metadata item from the provided device, if it exists.
 *
 * @param device
 * @param key
 * @return the metadata value or NULL if not found
 */
const char *deviceGetMetadata(const icDevice *device, const char *key)
{
    char *result = NULL;

    if (device != NULL && device->metadata != NULL)
    {
        icLinkedListIterator *it = linkedListIteratorCreate(device->metadata);
        while (linkedListIteratorHasNext(it))
        {
            icDeviceMetadata *metadata = linkedListIteratorGetNext(it);
            if (strcmp(metadata->id, key) == 0)
            {
                result = metadata->value;
                break;
            }
        }
        linkedListIteratorDestroy(it);
    }

    return result;
}

const char *deviceGetResourceValueById(const icDevice *device, const char *resourceId)
{
    const char *value = NULL;

    if (device != NULL)
    {
        value = icDeviceResourceGetValueById(device->resources, resourceId);
    }

    return value;
}

icDevice *deviceFromJSON(cJSON *json, const icSerDesContext *context, bool permissive)
{
    scoped_icDevice *tempDevice = (icDevice *) calloc(1, sizeof(icDevice));

    tempDevice->uri = getCJSONString(json, DEVICE_URI_KEY);
    tempDevice->uuid = getCJSONString(json, DEVICE_UUID_KEY);

    if (stringIsPrintable(tempDevice->uuid, false) == false)
    {
        icLogError(LOG_TAG, "Invalid device: invalid uuid '%s'", stringCoalesce(tempDevice->uuid));
        return NULL;
    }

    // we have to validate the device URI.
    // we will validate uuid as part of the device URI validation.
    // so not required to have different check for uuid
    //
    if (deviceUriIsValid(tempDevice->uri, tempDevice->uuid) == false)
    {
        icLogError(LOG_TAG, "Invalid device: invalid uri!");
        return NULL;
    }

    tempDevice->managingDeviceDriver = getCJSONString(json, DEVICE_DRIVER_KEY);
    if (stringIsEmpty(tempDevice->managingDeviceDriver) == true)
    {
        icLogError(LOG_TAG, "Invalid device %s: no managingDeviceDriver!", tempDevice->uri);
        return NULL;
    }

    tempDevice->deviceClass = getCJSONString(json, DEVICE_CLASS_KEY);
    if (stringIsEmpty(tempDevice->deviceClass) == true)
    {
        icLogError(LOG_TAG, "Invalid device %s: no deviceClass!", tempDevice->uri);
        return NULL;
    }

    uint8_t deviceClassVersion = 1;
    getCJSONUInt8(json, DEVICE_CLASS_VERSION_KEY, &deviceClassVersion);
    tempDevice->deviceClassVersion = deviceClassVersion;

    tempDevice->endpoints =
        endpointsFromJSON(tempDevice->uuid, cJSON_GetObjectItem(json, DEVICE_ENDPOINTS_KEY), context, permissive);
    if (linkedListCount(tempDevice->endpoints) == 0)
    {
        icLogError(LOG_TAG, "Invalid device %s: no valid endpoints!", tempDevice->uri);
        return NULL;
    }

    tempDevice->resources =
        resourcesFromJSON(tempDevice->uuid, NULL, cJSON_GetObjectItem(json, DEVICE_RESOURCES_KEY), context, permissive);
    if (linkedListCount(tempDevice->resources) == 0)
    {
        icLogError(LOG_TAG, "Invalid device %s: no valid resources!", tempDevice->uri);
        return NULL;
    }

    /* This is allowed to be an empty list */
    tempDevice->metadata =
        metadatasFromJSON(tempDevice->uuid, NULL, cJSON_GetObjectItem(json, DEVICE_METADATAS_KEY), permissive);
    if (tempDevice->metadata == NULL)
    {
        icLogError(LOG_TAG, "Invalid device %s: invalid metadatas!", tempDevice->uri);
        return NULL;
    }

    // at this stage we have a valid device, create a reference of tempDevice and make it NULL.
    // so that auto clean have no effect.
    //
    icDevice *device = tempDevice;
    tempDevice = NULL;
    return device;
}

bool deviceUriIsValid(const char *deviceUri, const char *deviceUuid)
{
    bool retValue = false;
    if ((stringIsEmpty(deviceUri) == false) && (stringIsEmpty(deviceUuid) == false))
    {
        scoped_generic char *tempUri = stringBuilder("/%s", deviceUuid);
        if (stringCompare(deviceUri, tempUri, true) == 0)
        {
            retValue = true;
        }
    }
    return retValue;
}
