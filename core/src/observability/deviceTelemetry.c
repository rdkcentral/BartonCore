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

#ifdef BARTON_CONFIG_OBSERVABILITY

#include "deviceTelemetry.h"

#include <stdlib.h>
#include <string.h>

#include <commonDeviceDefs.h>
#include <device/icDevice.h>
#include <device/icDeviceEndpoint.h>
#include <device/icDeviceResource.h>
#include <deviceService.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <icTypes/icHashMap.h>
#include <observability/observabilityMetrics.h>
#include <resourceTypes.h>

#define LOG_TAG "deviceTelemetry"

// ---------------------------------------------------------------------------
// Lazy meter creation
// ---------------------------------------------------------------------------

static ObservabilityGauge *resourceValueGauge = NULL;
static ObservabilityCounter *stateChangeCounter = NULL;
static ObservabilityCounter *resourceUpdateCounter = NULL;

static void ensureDeviceTelemetryMetersCreated(void)
{
    static bool metersCreated = false;
    if (!metersCreated)
    {
        resourceValueGauge = observabilityGaugeCreate(
            "device.resource.value", "Current numeric value of a device resource", "{value}");

        stateChangeCounter = observabilityCounterCreate(
            "device.resource.state_change", "Boolean state transitions on a device resource", "{transition}");

        resourceUpdateCounter = observabilityCounterCreate(
            "device.resource.update", "Resource value changes reported by devices", "{update}");

        metersCreated = true;
    }
}

// ---------------------------------------------------------------------------
// Per-device attribute cache
// ---------------------------------------------------------------------------

typedef struct
{
    char *deviceClass;
    char *manufacturer;
    char *model;
    char *hardwareVersion;
    char *firmwareVersion;
    char *driverType;
} DeviceAttrCache;

static icHashMap *deviceAttrCacheMap = NULL; // deviceUuid -> DeviceAttrCache*

static void deviceAttrCacheFree(DeviceAttrCache *entry)
{
    if (entry == NULL)
    {
        return;
    }
    free(entry->deviceClass);
    free(entry->manufacturer);
    free(entry->model);
    free(entry->hardwareVersion);
    free(entry->firmwareVersion);
    free(entry->driverType);
    free(entry);
}

static void deviceAttrCacheFreeMapValue(void *key, void *value)
{
    free(key);
    deviceAttrCacheFree((DeviceAttrCache *) value);
}

static char *safeStrdup(const char *s)
{
    return s ? strdup(s) : strdup("");
}

static char *getDeviceResourceValue(icDevice *device, const char *resourceId)
{
    icDeviceResource *res = deviceServiceFindDeviceResourceById(device, resourceId);
    if (res != NULL && res->value != NULL)
    {
        return strdup(res->value);
    }
    return strdup("");
}

static DeviceAttrCache *populateDeviceAttributes(const char *deviceUuid)
{
    icDevice *device = deviceServiceGetDevice(deviceUuid);
    if (device == NULL)
    {
        return NULL;
    }

    DeviceAttrCache *entry = (DeviceAttrCache *) calloc(1, sizeof(DeviceAttrCache));
    entry->deviceClass = safeStrdup(device->deviceClass);
    entry->driverType = safeStrdup(device->managingDeviceDriver);
    entry->manufacturer = getDeviceResourceValue(device, COMMON_DEVICE_RESOURCE_MANUFACTURER);
    entry->model = getDeviceResourceValue(device, COMMON_DEVICE_RESOURCE_MODEL);
    entry->hardwareVersion = getDeviceResourceValue(device, COMMON_DEVICE_RESOURCE_HARDWARE_VERSION);
    entry->firmwareVersion = getDeviceResourceValue(device, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION);

    deviceDestroy(device);
    return entry;
}

/**
 * Check whether the cached attributes look complete enough to persist.
 * During device-add sequences, updateResource() fires before metadata
 * resources (manufacturer, model, etc.) are populated. We avoid caching
 * that incomplete snapshot so subsequent calls after setup finishes will
 * re-populate with full data.
 */
static bool deviceAttrCacheIsComplete(const DeviceAttrCache *entry)
{
    // deviceClass is set on the icDevice struct at creation time, so it is
    // the earliest reliable signal that the device record is fully formed.
    // manufacturer is populated from BasicInformation / Basic cluster and
    // is one of the last resources written during pairing.
    return entry != NULL && entry->deviceClass != NULL && entry->deviceClass[0] != '\0' &&
           entry->manufacturer != NULL && entry->manufacturer[0] != '\0';
}

static DeviceAttrCache *getOrPopulateDeviceAttributes(const char *deviceUuid, bool *outNeedsFree)
{
    *outNeedsFree = false;

    if (deviceUuid == NULL)
    {
        return NULL;
    }

    if (deviceAttrCacheMap == NULL)
    {
        deviceAttrCacheMap = hashMapCreate();
    }

    uint16_t keyLen = (uint16_t) strlen(deviceUuid);
    DeviceAttrCache *entry = (DeviceAttrCache *) hashMapGet(deviceAttrCacheMap, (void *) deviceUuid, keyLen);
    if (entry != NULL)
    {
        return entry;
    }

    entry = populateDeviceAttributes(deviceUuid);
    if (entry != NULL)
    {
        if (deviceAttrCacheIsComplete(entry))
        {
            // Device metadata is fully populated — safe to cache.
            hashMapPut(deviceAttrCacheMap, strdup(deviceUuid), keyLen, entry);
        }
        else
        {
            // Partial entry during device-add sequence — return for this call
            // but don't cache. Caller is responsible for freeing.
            *outNeedsFree = true;
        }
    }
    return entry;
}

// ---------------------------------------------------------------------------
// Endpoint profile lookup (not cached — few endpoints, infrequent calls)
// ---------------------------------------------------------------------------

static char *getEndpointProfile(const char *deviceUuid, const char *endpointId)
{
    if (deviceUuid == NULL || endpointId == NULL)
    {
        return strdup("");
    }

    icDeviceEndpoint *ep = deviceServiceGetEndpointById(deviceUuid, endpointId);
    if (ep == NULL)
    {
        return strdup("");
    }

    char *profile = safeStrdup(ep->profile);
    endpointDestroy(ep);
    return profile;
}

// ---------------------------------------------------------------------------
// Resource update recording
// ---------------------------------------------------------------------------

void deviceTelemetryRecordResourceUpdate(const char *deviceUuid,
                                         const char *endpointId,
                                         const char *resourceId,
                                         const char *resourceType,
                                         const char *newValue,
                                         bool didChange)
{
    if (deviceUuid == NULL || resourceId == NULL)
    {
        return;
    }

    // Skip high-frequency metadata resources that provide no telemetry value
    if (strcmp(resourceId, COMMON_DEVICE_RESOURCE_DATE_LAST_CONTACTED) == 0)
    {
        return;
    }

    ensureDeviceTelemetryMetersCreated();

    bool attrNeedsFree = false;
    DeviceAttrCache *attrs = getOrPopulateDeviceAttributes(deviceUuid, &attrNeedsFree);
    char *endpointProfile = getEndpointProfile(deviceUuid, endpointId);

    const char *devClass = attrs ? attrs->deviceClass : "";
    const char *manufacturer = attrs ? attrs->manufacturer : "";
    const char *model = attrs ? attrs->model : "";
    const char *hwVersion = attrs ? attrs->hardwareVersion : "";
    const char *fwVersion = attrs ? attrs->firmwareVersion : "";
    const char *drvType = attrs ? attrs->driverType : "";

    // Always emit update counter for value changes
    observabilityCounterAddWithAttrs(resourceUpdateCounter,
                                     1,
                                     "device.id",
                                     deviceUuid,
                                     "device.class",
                                     devClass,
                                     "endpoint.profile",
                                     endpointProfile,
                                     "resource.name",
                                     resourceId,
                                     "device.manufacturer",
                                     manufacturer,
                                     "device.model",
                                     model,
                                     "device.hardware_version",
                                     hwVersion,
                                     "device.firmware_version",
                                     fwVersion,
                                     "driver.type",
                                     drvType,
                                     NULL);

    // Type-specific metrics: only when value actually changed and is non-NULL
    if (didChange && newValue != NULL && resourceType != NULL)
    {
        if (strcmp(resourceType, RESOURCE_TYPE_INTEGER) == 0 ||
            strcmp(resourceType, RESOURCE_TYPE_PERCENTAGE) == 0)
        {
            char *endptr = NULL;
            long numValue = strtol(newValue, &endptr, 10);
            if (endptr != newValue)
            {
                observabilityGaugeRecordWithAttrs(resourceValueGauge,
                                                  (int64_t) numValue,
                                                  "device.id",
                                                  deviceUuid,
                                                  "device.class",
                                                  devClass,
                                                  "endpoint.profile",
                                                  endpointProfile,
                                                  "resource.name",
                                                  resourceId,
                                                  "device.manufacturer",
                                                  manufacturer,
                                                  "device.model",
                                                  model,
                                                  "device.hardware_version",
                                                  hwVersion,
                                                  "device.firmware_version",
                                                  fwVersion,
                                                  "driver.type",
                                                  drvType,
                                                  NULL);
            }
        }
        else if (strcmp(resourceType, RESOURCE_TYPE_BOOLEAN) == 0)
        {
            observabilityCounterAddWithAttrs(stateChangeCounter,
                                             1,
                                             "device.id",
                                             deviceUuid,
                                             "device.class",
                                             devClass,
                                             "endpoint.profile",
                                             endpointProfile,
                                             "resource.name",
                                             resourceId,
                                             "device.manufacturer",
                                             manufacturer,
                                             "device.model",
                                             model,
                                             "device.hardware_version",
                                             hwVersion,
                                             "device.firmware_version",
                                             fwVersion,
                                             "driver.type",
                                             drvType,
                                             "state",
                                             newValue,
                                             NULL);
        }
    }

    // Invalidate cache if firmware version was just updated
    if (strcmp(resourceId, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION) == 0 && didChange)
    {
        deviceTelemetryInvalidateCache(deviceUuid);
    }

    free(endpointProfile);

    if (attrNeedsFree)
    {
        deviceAttrCacheFree(attrs);
    }
}

// ---------------------------------------------------------------------------
// Cache invalidation
// ---------------------------------------------------------------------------

void deviceTelemetryInvalidateCache(const char *deviceUuid)
{
    if (deviceAttrCacheMap == NULL)
    {
        return;
    }

    if (deviceUuid == NULL)
    {
        // Evict all entries
        hashMapDestroy(deviceAttrCacheMap, deviceAttrCacheFreeMapValue);
        deviceAttrCacheMap = NULL;
    }
    else
    {
        uint16_t keyLen = (uint16_t) strlen(deviceUuid);
        hashMapDelete(deviceAttrCacheMap, (void *) deviceUuid, keyLen, deviceAttrCacheFreeMapValue);
    }
}

#endif /* BARTON_CONFIG_OBSERVABILITY */
