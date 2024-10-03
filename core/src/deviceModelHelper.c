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
/*
 * Created by Thomas Lea on 8/15/15.
 */

#include "device/deviceModelHelper.h"
#include "device-driver/device-driver.h"
#include "deviceServicePrivate.h"
#include <device/icDevice.h>
#include <device/icDeviceEndpoint.h>
#include <device/icDeviceMetadata.h>
#include <icLog/logging.h>
#include <stdio.h>
#include <string.h>

#define LOG_TAG "deviceModelHelper"

static icDeviceMetadata *createDeviceMetadataOnList(icLinkedList *metadataList,
                                                    const char *endpointId,
                                                    const char *metadataId,
                                                    const char *deviceUuid,
                                                    const char *value);

icDevice *createDevice(const char *uuid,
                       const char *deviceClass,
                       uint8_t deviceClassVersion,
                       const char *deviceDriverName,
                       const DeviceDescriptor *dd)
{
    icDevice *result = NULL;

    if (uuid == NULL || deviceClass == NULL || deviceDriverName == NULL)
    {
        icLogError(LOG_TAG, "createDevice: invalid arguments");
        return NULL;
    }

    result = (icDevice *) calloc(1, sizeof(icDevice));
    if (result == NULL)
    {
        icLogError(LOG_TAG, "failed to allocate icDevice");
        return NULL;
    }

    result->uuid = strdup(uuid);
    result->deviceClass = strdup(deviceClass);
    result->deviceClassVersion = deviceClassVersion;
    result->managingDeviceDriver = strdup(deviceDriverName);
    result->resources = linkedListCreate();
    result->endpoints = linkedListCreate();
    result->metadata = linkedListCreate();

    // add any metadata provided from the device descriptor
    if (dd != NULL && dd->metadata != NULL)
    {
        icStringHashMapIterator *it = stringHashMapIteratorCreate(dd->metadata);
        while (stringHashMapIteratorHasNext(it))
        {
            char *key;
            char *value;
            stringHashMapIteratorGetNext(it, &key, &value);
            icLogDebug(LOG_TAG, "%s: adding metadata %s %s", __FUNCTION__, key, value);
            createDeviceMetadata(result, key, value);
        }
        stringHashMapIteratorDestroy(it);
    }

    return result;
}

icDeviceMetadata *createDeviceMetadata(icDevice *device, const char *metadataId, const char *value)
{
    icDeviceMetadata *result = createDeviceMetadataOnList(device->metadata, NULL, metadataId, device->uuid, value);
    result->uri = metadataUriCreate(device->uuid, NULL, metadataId);
    return result;
}

icDeviceEndpoint *createEndpoint(icDevice *device, const char *id, const char *profile, bool enabled)
{
    icDeviceEndpoint *result = NULL;

    if (device == NULL || profile == NULL || id == NULL)
    {
        icLogError(LOG_TAG, "createEndpoint: invalid arguments");
        return NULL;
    }

    result = (icDeviceEndpoint *) calloc(1, sizeof(icDeviceEndpoint));
    if (result == NULL)
    {
        icLogError(LOG_TAG, "failed to allocate icDeviceEndpoint");
        return NULL;
    }

    result->id = strdup(id);
    result->deviceUuid = strdup(device->uuid);
    result->profile = strdup(profile);
    result->resources = linkedListCreate();
    result->enabled = enabled;
    result->metadata = linkedListCreate();

    linkedListAppend(device->endpoints, result);

    return result;
}

static icDeviceResource *createDeviceResourceOnList(icLinkedList *resourceList,
                                                    const char *endpointId,
                                                    const char *resourceId,
                                                    const char *deviceUuid,
                                                    const char *value,
                                                    const char *type,
                                                    uint8_t mode,
                                                    ResourceCachingPolicy cachingPolicy)
{
    icDeviceResource *result = NULL;

    if (resourceList == NULL || resourceId == NULL || type == NULL || mode == 0)
    {
        icLogError(LOG_TAG, "createDeviceResourceOnList: invalid arguments");
        return NULL;
    }

    result = (icDeviceResource *) calloc(1, sizeof(icDeviceResource));
    if (result == NULL)
    {
        icLogError(LOG_TAG, "failed to allocate icDeviceResource");
        return NULL;
    }

    result->id = strdup(resourceId);
    if (endpointId != NULL)
    {
        result->endpointId = strdup(endpointId);
    }
    result->deviceUuid = strdup(deviceUuid);

    if (value != NULL)
    {
        result->value = strdup(value);
    }

    result->type = strdup(type);
    result->mode = mode;

    // if a resource is created with DYNAMIC, we can go ahead and safely set the DYNAMIC_CAPABLE bit
    if (mode & RESOURCE_MODE_DYNAMIC)
    {
        result->mode |= RESOURCE_MODE_DYNAMIC_CAPABLE;
    }

    result->cachingPolicy = cachingPolicy;

    linkedListAppend(resourceList, result);

    return result;
}

icDeviceResource *createDeviceResource(icDevice *device,
                                       const char *resourceId,
                                       const char *value,
                                       const char *type,
                                       uint8_t mode,
                                       ResourceCachingPolicy cachingPolicy)
{
    return createDeviceResourceOnList(
        device->resources, NULL, resourceId, device->uuid, value, type, mode, cachingPolicy);
}

icDeviceResource *createDeviceResourceIfAvailable(icDevice *device,
                                                  const char *resourceId,
                                                  icInitialResourceValues *initialResourceValues,
                                                  const char *type,
                                                  uint8_t mode,
                                                  ResourceCachingPolicy cachingPolicy)
{
    if (initialResourceValuesHasDeviceValue(initialResourceValues, resourceId))
    {
        return createDeviceResourceOnList(device->resources,
                                          NULL,
                                          resourceId,
                                          device->uuid,
                                          initialResourceValuesGetDeviceValue(initialResourceValues, resourceId),
                                          type,
                                          mode,
                                          cachingPolicy);
    }
    else
    {
        icLogInfo(LOG_TAG,
                  "createDeviceResourceIfAvailable: Skipping resource creation, no value for resource %s on device %s",
                  resourceId,
                  device->uuid);
    }

    return NULL;
}

icDeviceResource *createEndpointResource(icDeviceEndpoint *endpoint,
                                         const char *resourceId,
                                         const char *value,
                                         const char *type,
                                         uint8_t mode,
                                         ResourceCachingPolicy cachingPolicy)
{
    return createDeviceResourceOnList(
        endpoint->resources, endpoint->id, resourceId, endpoint->deviceUuid, value, type, mode, cachingPolicy);
}

icDeviceResource *createEndpointResourceIfAvailable(icDeviceEndpoint *endpoint,
                                                    const char *resourceId,
                                                    icInitialResourceValues *initialResourceValues,
                                                    const char *type,
                                                    uint8_t mode,
                                                    ResourceCachingPolicy cachingPolicy)
{
    if (initialResourceValuesHasEndpointValue(initialResourceValues, endpoint->id, resourceId))
    {
        return createDeviceResourceOnList(
            endpoint->resources,
            endpoint->id,
            resourceId,
            endpoint->deviceUuid,
            initialResourceValuesGetEndpointValue(initialResourceValues, endpoint->id, resourceId),
            type,
            mode,
            cachingPolicy);
    }
    else
    {
        icLogInfo(LOG_TAG,
                  "createEndpointResourceIfAvailable: Skipping resource creation, no value for resource %s on device "
                  "%s, endpoint %s",
                  resourceId,
                  endpoint->deviceUuid,
                  endpoint->id);
    }

    return NULL;
}

static icDeviceMetadata *createDeviceMetadataOnList(icLinkedList *metadataList,
                                                    const char *endpointId,
                                                    const char *metadataId,
                                                    const char *deviceUuid,
                                                    const char *value)
{
    icDeviceMetadata *result = NULL;

    if (metadataList == NULL || metadataId == NULL)
    {
        icLogError(LOG_TAG, "createDeviceMetadataOnList: invalid arguments");
        return NULL;
    }

    result = (icDeviceMetadata *) calloc(1, sizeof(icDeviceMetadata));
    if (result == NULL)
    {
        icLogError(LOG_TAG, "failed to allocate icDeviceMetadata");
        return NULL;
    }

    result->id = strdup(metadataId);
    if (endpointId != NULL)
    {
        result->endpointId = strdup(endpointId);
    }
    result->deviceUuid = strdup(deviceUuid);

    if (value != NULL)
    {
        result->value = strdup(value);
    }

    linkedListAppend(metadataList, result);

    return result;
}

/*
 * metadata on devices not currently supported
icDeviceMetadata *createDeviceMetadata(icDevice *device, const char *metadataId, const char *value)
{
    return createDeviceResourceOnList(device->metadata, NULL, metadataId, device->uuid, value);
}
 */

icDeviceMetadata *createEndpointMetadata(icDeviceEndpoint *endpoint, const char *metadataId, const char *value)
{
    icDeviceMetadata *result =
        createDeviceMetadataOnList(endpoint->metadata, endpoint->id, metadataId, endpoint->deviceUuid, value);
    result->uri = metadataUriCreate(endpoint->deviceUuid, endpoint->id, metadataId);
    return result;
}
