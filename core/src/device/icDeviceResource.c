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

#include "deviceServicePrivate.h"
#include <device-driver/device-driver.h>
#include <device/icDeviceResource.h>
#include <icConfig/simpleProtectConfig.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <jsonHelper/jsonHelper.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG                               "deviceService"

/*
 * Created by Thomas Lea on 8/10/15.
 */

// Keys for resource json representation
#define RESOURCE_ID_KEY                       "id"
#define RESOURCE_URI_KEY                      "uri"
#define RESOURCE_MODE_KEY                     "mode"
#define RESOURCE_CACHING_POLICY_KEY           "cachingPolicy"
#define RESOURCE_DATE_OF_LAST_SYNC_MILLIS_KEY "dateOfLastSyncMillis"
#define RESOURCE_VALUE_KEY                    "value"
#define RESOURCE_ENCRYPTED_VALUE_KEY          "value_enc"
#define RESOURCE_TYPE_KEY                     "type"
#define RESOURCE_NAMESPACE_KEY                "namespace"


extern inline void resourceDestroy__auto(icDeviceResource **resource);

void resourceDestroy(icDeviceResource *resource)
{
    if (resource != NULL)
    {
        free(resource->id);
        free(resource->endpointId);
        free(resource->uri);
        free(resource->type);
        free(resource->deviceUuid);
        free(resource->value);

        free(resource);
    }
}

void resourcePrint(icDeviceResource *resource, const char *prefix)
{
    if (resource == NULL)
    {
        icLogDebug(LOG_TAG, "%sResource [NULL!]", prefix);
    }
    else
    {
        icLogDebug(LOG_TAG,
                   "%sResource [uri=%s] [id=%s] [endpointId=%s] [type=%s] [mode=0x%x] [cache policy=%d]: %s",
                   prefix,
                   resource->uri,
                   resource->id,
                   resource->endpointId,
                   resource->type,
                   resource->mode,
                   resource->cachingPolicy,
                   (resource->mode & RESOURCE_MODE_SENSITIVE) ? ("(encrypted)") : (resource->value));
    }
}

icDeviceResource *resourceClone(const icDeviceResource *resource)
{
    icDeviceResource *clone = NULL;
    if (resource != NULL)
    {
        clone = (icDeviceResource *) calloc(1, sizeof(icDeviceResource));
        if (resource->deviceUuid != NULL)
        {
            clone->deviceUuid = strdup(resource->deviceUuid);
        }
        if (resource->id != NULL)
        {
            clone->id = strdup(resource->id);
        }
        if (resource->uri != NULL)
        {
            clone->uri = strdup(resource->uri);
        }
        if (resource->type != NULL)
        {
            clone->type = strdup(resource->type);
        }
        if (resource->value != NULL)
        {
            clone->value = strdup(resource->value);
        }
        clone->dateOfLastSyncMillis = resource->dateOfLastSyncMillis;
        clone->cachingPolicy = resource->cachingPolicy;
        clone->mode = resource->mode;
        if (resource->endpointId != NULL)
        {
            clone->endpointId = strdup(resource->endpointId);
        }
    }
    else
    {
        icLogWarn(LOG_TAG, "Attempt to clone NULL resource");
    }
    return clone;
}

cJSON *resourceToJSON(const icDeviceResource *resource, const icSerDesContext *context)
{
    cJSON *json = cJSON_CreateObject();
    char *resourceValue = NULL;
    bool resourceValueEncrypted = false;

    if (resource->mode & RESOURCE_MODE_SENSITIVE)
    {
        if (serDesHasContextValue(context, RESOURCE_NAMESPACE_KEY) == true)
        {
            resourceValue =
                simpleProtectConfigData(serDesGetContextValue(context, RESOURCE_NAMESPACE_KEY), resource->value);
            resourceValueEncrypted = true;
        }
        else
        {
            icLogWarn(LOG_TAG, "Cannot encrypt resource \"%s\": missing namespace context value", resource->id);
        }
    }
    if (resourceValue == NULL && resource->value != NULL)
    {
        resourceValue = strdup(resource->value);
    }

    // Add resource info
    cJSON_AddStringToObject(json, RESOURCE_ID_KEY, resource->id);
    cJSON_AddStringToObject(json, RESOURCE_URI_KEY, resource->uri);
    cJSON_AddNumberToObject(json, RESOURCE_MODE_KEY, resource->mode);
    cJSON_AddNumberToObject(json, RESOURCE_CACHING_POLICY_KEY, resource->cachingPolicy);
    cJSON_AddNumberToObject(json, RESOURCE_DATE_OF_LAST_SYNC_MILLIS_KEY, resource->dateOfLastSyncMillis);

    // Whoever stores in the resource, if there is binary data they have to make sure to encode it, as a resource
    // doesn't support binary data

    if (resourceValueEncrypted == true)
    {
        cJSON_AddStringToObject(json, RESOURCE_ENCRYPTED_VALUE_KEY, resourceValue);
    }
    else
    {
        cJSON_AddStringToObject(json, RESOURCE_VALUE_KEY, resourceValue);
    }
    cJSON_AddStringToObject(json, RESOURCE_TYPE_KEY, resource->type);

    // Cleanup
    free(resourceValue);

    return json;
}

cJSON *resourcesToJSON(icLinkedList *resources, const icSerDesContext *context)
{
    cJSON *resourcesJSON = cJSON_CreateObject();
    icLinkedListIterator *iter = linkedListIteratorCreate(resources);
    while (linkedListIteratorHasNext(iter))
    {
        icDeviceResource *resource = (icDeviceResource *) linkedListIteratorGetNext(iter);
        cJSON *resourceJson = resourceToJSON(resource, context);
        cJSON_AddItemToObject(resourcesJSON, resource->id, resourceJson);
    }
    linkedListIteratorDestroy(iter);

    return resourcesJSON;
}


icDeviceResource *
resourceFromJSON(const char *deviceUUID, const char *endpointId, cJSON *resourceJSON, const icSerDesContext *context)
{
    icDeviceResource *resource = NULL;
    char *resourceValue = NULL;

    if (resourceJSON != NULL && deviceUUID != NULL)
    {
        scoped_icDeviceResource *tempResource = (icDeviceResource *) calloc(1, sizeof(icDeviceResource));

        if (stringIsEmpty(deviceUUID) == true)
        {
            icLogError(LOG_TAG, "Invalid resource: no deviceUUID!");
            return NULL;
        }
        else
        {
            tempResource->deviceUuid = strdup(deviceUUID);
        }

        tempResource->endpointId = strdupOpt(endpointId);

        tempResource->uri = getCJSONString(resourceJSON, RESOURCE_URI_KEY);
        tempResource->id = getCJSONString(resourceJSON, RESOURCE_ID_KEY);

        if (stringIsPrintable(tempResource->id, false) == false)
        {
            icLogError(LOG_TAG, "Invalid resource id '%s'!", stringCoalesce(tempResource->id));
            return NULL;
        }

        // we have to validate the resource URI.
        // we will validate resourceId/deviceUuid as part of the resource URI validation.
        // so not required to have different check for resourceId
        //
        if (resourceUriIsValid(tempResource->uri, deviceUUID, endpointId, tempResource->id) == false)
        {
            icLogError(LOG_TAG, "Invalid resource %s: invalid uri!", stringCoalesce(tempResource->uri));
            return NULL;
        }

        uint8_t mode = RESOURCE_MODE_READABLE;
        getCJSONUInt8(resourceJSON, RESOURCE_MODE_KEY, &mode);
        tempResource->mode = mode;

        if (tempResource->mode & RESOURCE_MODE_SENSITIVE)
        {
            char *encryptedResourceValue = getCJSONString(resourceJSON, RESOURCE_ENCRYPTED_VALUE_KEY);

            if (encryptedResourceValue != NULL)
            {
                if (serDesHasContextValue(context, RESOURCE_NAMESPACE_KEY) == true)
                {
                    resourceValue = simpleUnprotectConfigData(serDesGetContextValue(context, RESOURCE_NAMESPACE_KEY),
                                                              encryptedResourceValue);
                }
                else
                {
                    icLogWarn(LOG_TAG,
                              "Cannot decrypt value for resource \"%s\": missing namespace context value",
                              tempResource->id);
                }
                free(encryptedResourceValue);
            }
            else
            {
                icLogWarn(LOG_TAG,
                          "Cannot find encrypted value for resource \"%s\" (using unencrypted value)",
                          tempResource->id);
            }
        }
        if (resourceValue != NULL)
        {
            tempResource->value = resourceValue;
        }
        else
        {
            tempResource->value = getCJSONString(resourceJSON, RESOURCE_VALUE_KEY);
        }

        tempResource->type = getCJSONString(resourceJSON, RESOURCE_TYPE_KEY);

        if (stringIsEmpty(tempResource->type) == true)
        {
            icLogError(LOG_TAG, "Invalid resource %s: no type!", tempResource->uri);
            return NULL;
        }

        int cachingPolicy = CACHING_POLICY_NEVER;
        getCJSONInt(resourceJSON, RESOURCE_CACHING_POLICY_KEY, &cachingPolicy);

        tempResource->cachingPolicy = cachingPolicy;

        uint64_t dateOfLastSyncMillis = 0;
        // coverity[check_return]
        getCJSONUInt64(resourceJSON, RESOURCE_DATE_OF_LAST_SYNC_MILLIS_KEY, &dateOfLastSyncMillis);
        tempResource->dateOfLastSyncMillis = dateOfLastSyncMillis;

        // at this stage we have valid resource, create a reference of tempResource and make it NULL.
        // so that auto clean have no effect.
        //
        resource = tempResource;
        tempResource = NULL;
    }
    else
    {
        icLogError(LOG_TAG,
                   "Failed to find resource json for device %s, endpoint %s",
                   stringCoalesce(deviceUUID),
                   stringCoalesce(endpointId));
    }

    return resource;
}

icLinkedList *resourcesFromJSON(const char *deviceUUID,
                                const char *endpointId,
                                cJSON *resourcesJSON,
                                const icSerDesContext *context,
                                bool permissive)
{
    icLinkedList *resources = NULL;

    if (resourcesJSON != NULL)
    {
        resources = linkedListCreate();
        int resourceCount = cJSON_GetArraySize(resourcesJSON);

        for (int i = 0; i < resourceCount; ++i)
        {
            cJSON *resourceJson = cJSON_GetArrayItem(resourcesJSON, i);
            icDeviceResource *resource = resourceFromJSON(deviceUUID, endpointId, resourceJson, context);
            if (resource != NULL)
            {
                linkedListAppend(resources, resource);
            }
            else
            {
                icLogError(LOG_TAG,
                           "Failed to read resource %s for device %s, endpoint %s",
                           resourceJson->string,
                           deviceUUID,
                           stringCoalesceAlt(endpointId, "[root]"));

                if (permissive == false)
                {
                    linkedListDestroy(resources, (linkedListItemFreeFunc) resourceDestroy);
                    resources = NULL;
                    break;
                }
            }
        }
    }
    else
    {
        icLogError(LOG_TAG,
                   "Failed to find resources json for device %s, endpoint %s",
                   deviceUUID,
                   stringCoalesceAlt(endpointId, "[root]"));
    }

    return resources;
}

const char *icDeviceResourceGetValueById(const icLinkedList *resources, const char *resourceId)
{
    char *value = NULL;
    if (resources != NULL && resourceId != NULL)
    {
        scoped_icLinkedListIterator *it = linkedListIteratorCreate((icLinkedList *) resources);
        while (linkedListIteratorHasNext(it))
        {
            icDeviceResource *resource = linkedListIteratorGetNext(it);
            if (stringCompare(resource->id, resourceId, false) == 0)
            {
                value = resource->value;
                break;
            }
        }
    }

    return value;
}

char *resourceUriCreate(const char *deviceUuid, const char *endpointId, const char *resourceId)
{
    char *uri = NULL;

    if (stringIsEmpty(endpointId) == true)
    {
        // / + deviceUuid + /r/ + resource id
        uri = stringBuilder("/%s/r/%s", deviceUuid, resourceId);
    }
    else
    {
        // endpointUri + /r/ + resource id
        scoped_generic char *epUri = endpointUriCreate(deviceUuid, endpointId);
        uri = stringBuilder("%s/r/%s", epUri, resourceId);
    }
    return uri;
}

bool resourceUriIsValid(const char *deviceResourceUri,
                        const char *deviceUuid,
                        const char *endpointId,
                        const char *resourceId)
{
    bool retValue = false;

    if ((stringIsEmpty(deviceResourceUri) == false) && (stringIsEmpty(deviceUuid) == false) &&
        (stringIsEmpty(resourceId) == false))
    {
        scoped_generic char *tempUri = resourceUriCreate(deviceUuid, endpointId, resourceId);

        if (stringCompare(deviceResourceUri, tempUri, true) == 0)
        {
            retValue = true;
        }
    }

    return retValue;
}
