//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2022 Comcast
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
// Comcast retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------
//
// Created by tlea on 12/1/22.
//

#include "deviceServiceStatus.h"
#include "icTypes/icLinkedListFuncs.h"
#include "jsonHelper/jsonHelper.h"
#include <string.h>
#include "icTypes/icLinkedList.h"
#include "icTypes/icHashMap.h"

static void subsystemStatusCloneFunc(void *key, void *value, void **clonedKey, void **clonedValue, void *context)
{
    if (key && value && clonedKey && clonedValue)
    {
        *clonedKey = strdup((char *) key);
        *clonedValue = cJSON_Duplicate((cJSON *) value, true);
    }
}

static void subsystemStatusFreeFunc(void *key, void *value)
{
    free(key);
    cJSON_Delete((cJSON *) value);
}

DeviceServiceStatus * device_service_status_copy (DeviceServiceStatus *s)
{
    DeviceServiceStatus *retVal = NULL;

    if (s)
    {
        retVal = calloc(1, sizeof(DeviceServiceStatus));

        if (s->supportedDeviceClasses)
        {
            retVal->supportedDeviceClasses = linkedListDeepClone(s->supportedDeviceClasses, linkedListCloneStringItemFunc, NULL);
        }

        if (s->discoveringDeviceClasses)
        {
            retVal->discoveringDeviceClasses = linkedListDeepClone(s->discoveringDeviceClasses, linkedListCloneStringItemFunc, NULL);
        }

        if (s->subsystemsJsonStatus)
        {
            retVal->subsystemsJsonStatus = hashMapDeepClone(s->subsystemsJsonStatus, subsystemStatusCloneFunc, NULL);
        }

        retVal->discoveryRunning = s->discoveryRunning;
        retVal->discoveryTimeoutSeconds = s->discoveryTimeoutSeconds;
        retVal->findingOrphanedDevices = s->findingOrphanedDevices;
        retVal->isReadyForDeviceOperation = s->isReadyForDeviceOperation;
        retVal->isReadyForPairing = s->isReadyForPairing;
    }

    return retVal;
}

void device_service_status_free (DeviceServiceStatus *s)
{
    if(s)
    {
        linkedListDestroy(s->supportedDeviceClasses, free);
        linkedListDestroy(s->discoveringDeviceClasses, free);
        hashMapDestroy(s->subsystemsJsonStatus, subsystemStatusFreeFunc);
        free(s);
    }
}

static void jsonStatusEntryFreeFunc(void *key, void *value)
{
    free(key);
    cJSON_Delete(value);
}

void deviceServiceStatusDestroy(DeviceServiceStatus *status)
{
    if (status != NULL)
    {
        linkedListDestroy(status->supportedDeviceClasses, NULL);
        linkedListDestroy(status->discoveringDeviceClasses, NULL);
        hashMapDestroy(status->subsystemsJsonStatus, jsonStatusEntryFreeFunc);
        free(status);
    }
}

DECLARE_DESTRUCTOR(DeviceServiceStatus, deviceServiceStatusDestroy);

DeviceServiceStatus *deviceServiceStatusFromJson(const char *json)
{
    DeviceServiceStatus *result = NULL;
    bool parseError = false;

    cJSON *parsed = cJSON_Parse(json); // will return NULL if input is NULL
    if (parsed != NULL)
    {
        result = calloc(1, sizeof(DeviceServiceStatus));

        cJSON *array = cJSON_GetObjectItem(parsed, "supportedDeviceClasses");
        if (array == NULL)
        {
            parseError = true;
        }
        else
        {
            result->supportedDeviceClasses = linkedListCreate();
            cJSON *item;
            cJSON_ArrayForEach(item, array)
            {
                linkedListAppend(result->supportedDeviceClasses, strdup(item->valuestring));
            }
        }

        if (!getCJSONBool(parsed, "discoveryRunning", &result->discoveryRunning))
        {
            parseError = true;
        }

        array = cJSON_GetObjectItem(parsed, "discoveringDeviceClasses");
        if (array == NULL)
        {
            parseError = true;
        }
        else
        {
            result->discoveringDeviceClasses = linkedListCreate();
            cJSON *item;
            cJSON_ArrayForEach(item, array)
            {
                linkedListAppend(result->discoveringDeviceClasses, strdup(item->valuestring));
            }
        }

        if (!getCJSONUInt32(parsed, "discoveryTimeoutSeconds", &result->discoveryTimeoutSeconds))
        {
            parseError = true;
        }

        if (!getCJSONBool(parsed, "findingOrphanedDevices", &result->findingOrphanedDevices))
        {
            parseError = true;
        }

        if (!getCJSONBool(parsed, "isReadyForDeviceOperation", &result->isReadyForDeviceOperation))
        {
            parseError = true;
        }

        if (!getCJSONBool(parsed, "isReadyForPairing", &result->isReadyForPairing))
        {
            parseError = true;
        }

        array = cJSON_GetObjectItem(parsed, "subsystems");
        if (array == NULL)
        {
            parseError = true;
        }
        else
        {
            result->subsystemsJsonStatus = hashMapCreate();

            //walk backwards through the array since we are removing elements as we go
            int num = cJSON_GetArraySize(array);
            for (int i = num-1; i >= 0; i--)
            {
                cJSON *item = cJSON_DetachItemFromArray(array, i);

                // we dont want this in returned json
                cJSON *subsystemName = cJSON_DetachItemFromObject(item, "__name");
                if (subsystemName == NULL)
                {
                    parseError = true;
                    cJSON_Delete(item); //since we couldnt put in our map for cleanup
                    break;
                }
                else
                {
                    hashMapPut(result->subsystemsJsonStatus,
                               strdup(subsystemName->valuestring),
                               strlen(subsystemName->valuestring),
                               item);

                    cJSON_Delete(subsystemName); //since we detached it
                }
            }
        }

        cJSON_Delete(parsed);
    }

    if (parseError && result != NULL)
    {
        deviceServiceStatusDestroy(result);
        result = NULL;
    }

    return result;
}

char *deviceServiceStatusToJson(const DeviceServiceStatus *status)
{
    scoped_cJSON *statusJson = cJSON_CreateObject();

    cJSON *array = cJSON_AddArrayToObject(statusJson, "supportedDeviceClasses");
    sbIcLinkedListIterator *supportedIt = linkedListIteratorCreate(status->supportedDeviceClasses);
    while (linkedListIteratorHasNext(supportedIt))
    {
        cJSON_AddItemToArray(array, cJSON_CreateString(linkedListIteratorGetNext(supportedIt)));
    }

    cJSON_AddBoolToObject(statusJson, "discoveryRunning", status->discoveryRunning);

    array = cJSON_AddArrayToObject(statusJson, "discoveringDeviceClasses");
    sbIcLinkedListIterator *discoveringIt = linkedListIteratorCreate(status->discoveringDeviceClasses);
    while (linkedListIteratorHasNext(discoveringIt))
    {
        cJSON_AddItemToArray(array, cJSON_CreateString(linkedListIteratorGetNext(discoveringIt)));
    }

    cJSON_AddNumberToObject(statusJson, "discoveryTimeoutSeconds", status->discoveryTimeoutSeconds);
    cJSON_AddBoolToObject(statusJson, "findingOrphanedDevices", status->findingOrphanedDevices);
    cJSON_AddBoolToObject(statusJson, "isReadyForDeviceOperation", status->isReadyForDeviceOperation);
    cJSON_AddBoolToObject(statusJson, "isReadyForPairing", status->isReadyForPairing);

    cJSON *subsystems = cJSON_AddArrayToObject(statusJson, "subsystems");
    scoped_icHashMapIterator *it = hashMapIteratorCreate((status->subsystemsJsonStatus));
    while (hashMapIteratorHasNext(it))
    {
        void *key = NULL;
        uint16_t keyLen = 0;
        cJSON *subStatus;
        hashMapIteratorGetNext(it, &key, &keyLen, (void **) &(subStatus));

        scoped_generic char *subsystemName = calloc(1, keyLen + 1);
        memcpy(subsystemName, key, keyLen);
        cJSON *subsystem = cJSON_Duplicate(subStatus, true);

        // a good enough effort to avoid conflict with whatever the subsystem has
        cJSON_AddStringToObject(subsystem, "__name", subsystemName);

        cJSON_AddItemToArray(subsystems, subsystem);
    }

    return cJSON_Print(statusJson);
}