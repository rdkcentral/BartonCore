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
#include <device/icDeviceMetadata.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <jsonHelper/jsonHelper.h>
#include <serial/icSerDesContext.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG            "deviceService"

/*
 * Created by Thomas Lea on 9/15/15.
 */

// Keys for metadata json representation
#define METADATA_ID_KEY    "id"
#define METADATA_URI_KEY   "uri"
#define METADATA_VALUE_KEY "value"


void metadataDestroy(icDeviceMetadata *metadata)
{
    if (metadata != NULL)
    {
        free(metadata->id);
        free(metadata->endpointId);
        free(metadata->uri);
        free(metadata->deviceUuid);
        free(metadata->value);

        free(metadata);
    }
}

extern inline void metadataDestroy__auto(icDeviceMetadata **metadata);

void metadataPrint(icDeviceMetadata *metadata, const char *prefix)
{
    if (metadata == NULL)
    {
        icLogDebug(LOG_TAG, "%sMetadata [NULL!]", prefix);
    }
    else
    {
        icLogDebug(LOG_TAG,
                   "%sMetadata [uri=%s] [id=%s] [endpointId=%s] [value=%s]",
                   prefix,
                   metadata->uri,
                   metadata->id,
                   metadata->endpointId,
                   metadata->value);
    }
}


icDeviceMetadata *metadataClone(const icDeviceMetadata *metadata)
{
    icDeviceMetadata *clone = NULL;
    if (metadata != NULL)
    {
        clone = (icDeviceMetadata *) calloc(1, sizeof(icDeviceMetadata));
        if (metadata->endpointId != NULL)
        {
            clone->endpointId = strdup(metadata->endpointId);
        }
        if (metadata->uri != NULL)
        {
            clone->uri = strdup(metadata->uri);
        }
        if (metadata->id != NULL)
        {
            clone->id = strdup(metadata->id);
        }
        if (metadata->deviceUuid != NULL)
        {
            clone->deviceUuid = strdup(metadata->deviceUuid);
        }
        if (metadata->value != NULL)
        {
            clone->value = strdup(metadata->value);
        }
    }
    else
    {
        icLogWarn(LOG_TAG, "Attempt to clone NULL metadata");
    }
    return clone;
}

cJSON *metadataToJSON(const icDeviceMetadata *metadata, const icSerDesContext *context)
{
    cJSON *json = cJSON_CreateObject();
    // Add metadata info
    cJSON_AddStringToObject(json, METADATA_ID_KEY, metadata->id);
    cJSON_AddStringToObject(json, METADATA_URI_KEY, metadata->uri);

    // First try to parse the metadata as json, if that works, then store it directly
    cJSON *valueJSON = cJSON_Parse(metadata->value);
    if (valueJSON != NULL && cJSON_IsObject(valueJSON))
    {
        cJSON_AddItemToObject(json, METADATA_VALUE_KEY, valueJSON);
    }
    else
    {
        // Cleanup in case we parsed to something that wasn't an object(NULL is safe to delete)
        cJSON_Delete(valueJSON);
        // Wasn't JSON, so just store it
        cJSON_AddStringToObject(json, METADATA_VALUE_KEY, metadata->value);
    }


    return json;
}

cJSON *metadatasToJSON(icLinkedList *metadatas, const icSerDesContext *context)
{
    // Add metadata by id
    cJSON *metadatasJson = cJSON_CreateObject();
    icLinkedListIterator *metadataIter = linkedListIteratorCreate(metadatas);
    while (linkedListIteratorHasNext(metadataIter))
    {
        icDeviceMetadata *metadata = (icDeviceMetadata *) linkedListIteratorGetNext(metadataIter);
        cJSON *metadataJson = metadataToJSON(metadata, context);
        cJSON_AddItemToObject(metadatasJson, metadata->id, metadataJson);
    }
    linkedListIteratorDestroy(metadataIter);

    return metadatasJson;
}

icDeviceMetadata *metadataFromJSON(const char *deviceUUID, const char *endpointId, cJSON *metadataJSON)
{
    icDeviceMetadata *metadata = NULL;

    if (metadataJSON != NULL && deviceUUID != NULL)
    {
        scoped_icDeviceMetadata *tempMetadata = (icDeviceMetadata *) calloc(1, sizeof(icDeviceMetadata));

        tempMetadata->uri = getCJSONString(metadataJSON, METADATA_URI_KEY);
        tempMetadata->id = getCJSONString(metadataJSON, METADATA_ID_KEY);

        if (stringIsPrintable(tempMetadata->id, false) == false)
        {
            icLogError(LOG_TAG, "Invalid metadata id '%s'!", stringCoalesce(tempMetadata->id));
            return NULL;
        }

        // we have to validate the metadata URI.
        // we will validate metadataId/deviceUuid as part of the metadata URI validation.
        // so not required to have different check for metadataId
        //
        if (metadataUriIsValid(tempMetadata->uri, deviceUUID, endpointId, tempMetadata->id) == false)
        {
            icLogError(LOG_TAG, "Invalid metadata uri!");
            return NULL;
        }

        // Check whether it was an object, or a string
        cJSON *valueJSON = cJSON_GetObjectItem(metadataJSON, METADATA_VALUE_KEY);
        if (cJSON_IsObject(valueJSON))
        {
            // Convert the object back to string for users
            tempMetadata->value = cJSON_Print(valueJSON);
        }
        else
        {
            // Just get the string
            tempMetadata->value = getCJSONString(metadataJSON, METADATA_VALUE_KEY);
        }
        if (stringIsEmpty(tempMetadata->value) == true)
        {
            icLogError(LOG_TAG, "Invalid metadata %s: no value!", tempMetadata->uri);
            return NULL;
        }

        if (stringIsEmpty(deviceUUID) == true)
        {
            icLogError(LOG_TAG, "Invalid metadata %s: no deviceUUID!", tempMetadata->uri);
            return NULL;
        }
        else
        {
            tempMetadata->deviceUuid = strdup(deviceUUID);
        }

        tempMetadata->endpointId = strdupOpt(endpointId);

        // at this stage we have valid metadata object, create a reference of tempMetadata and make it NULL.
        // so that auto clean have no effect.
        //
        metadata = tempMetadata;
        tempMetadata = NULL;
    }
    else
    {
        icLogError(LOG_TAG,
                   "Failed to find metadata json for device %s, endpoint %s",
                   stringCoalesce(deviceUUID),
                   stringCoalesce(endpointId));
    }

    return metadata;
}

icLinkedList *metadatasFromJSON(const char *deviceUUID, const char *endpointId, cJSON *metadatasJSON, bool permissive)
{
    icLinkedList *metadatas = NULL;

    if (metadatasJSON != NULL)
    {
        metadatas = linkedListCreate();
        int metadataCount = cJSON_GetArraySize(metadatasJSON);

        for (int i = 0; i < metadataCount; ++i)
        {
            cJSON *metadataJson = cJSON_GetArrayItem(metadatasJSON, i);
            icDeviceMetadata *metadata = metadataFromJSON(deviceUUID, endpointId, metadataJson);
            if (metadata != NULL)
            {
                linkedListAppend(metadatas, metadata);
            }
            else
            {
                icLogError(LOG_TAG,
                           "Failed to read metadata %s for device %s, endpoint %s",
                           metadataJson->string,
                           deviceUUID,
                           stringCoalesceAlt(endpointId, "[root]"));


                if (permissive == false)
                {
                    linkedListDestroy(metadatas, (linkedListItemFreeFunc) metadataDestroy);
                    metadatas = NULL;
                    break;
                }
            }
        }
    }
    else
    {
        icLogError(LOG_TAG,
                   "Failed to find metadatas json for device %s, endpoint %s",
                   deviceUUID,
                   stringCoalesceAlt(endpointId, "[root]"));
    }

    return metadatas;
}

char *metadataUriCreate(const char *deviceUuid, const char *endpointId, const char *name)
{
    char *uri;

    if (endpointId == NULL)
    {
        /*   / + deviceUuid + /m/ + name  */
        uri = stringBuilder("/%s/m/%s", deviceUuid, name);
    }
    else
    {
        /*   endpointUri + /m/ + name  */
        scoped_generic char *epUri = endpointUriCreate(deviceUuid, endpointId);
        uri = stringBuilder("%s/m/%s", epUri, name);
    }

    return uri;
}

bool metadataUriIsValid(const char *deviceMetadataUri,
                        const char *deviceUuid,
                        const char *endpointId,
                        const char *metadataId)
{
    bool retValue = false;

    if ((stringIsEmpty(deviceMetadataUri) == false) && (stringIsEmpty(deviceUuid) == false) &&
        (stringIsEmpty(metadataId) == false))
    {
        scoped_generic char *tempUri = metadataUriCreate(deviceUuid, endpointId, metadataId);

        if (stringCompare(deviceMetadataUri, tempUri, true) == 0)
        {
            retValue = true;
        }
    }

    return retValue;
}
