//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2024 Comcast
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

/*
 * Created by Christian Leithner on 6/2/2024.
 */

#include "device-service-utils.h"
#include "device-service-discovery-type.h"
#include "device-service-endpoint.h"
#include "device-service-metadata.h"
#include "device-service-resource.h"
#include "device-service-status.h"
#include "events/device-service-status-event.h"
#include "glibconfig.h"
#include "icTypes/icLinkedList.h"

BDeviceServiceEndpoint *convertIcDeviceEndpointToGObject(const icDeviceEndpoint *endpoint)
{
    BDeviceServiceEndpoint *retVal = NULL;

    if (endpoint)
    {
        retVal = b_device_service_endpoint_new();

        g_autolist(BDeviceServiceResource) resources = convertIcDeviceResourceListToGList(endpoint->resources);
        g_autolist(BDeviceServiceMetadata) metadata = convertIcDeviceMetadataListToGList(endpoint->metadata);

        guint profileVersion = endpoint->profileVersion;
        g_object_set(retVal,
                     B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_ID],
                     endpoint->id,
                     B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_URI],
                     endpoint->uri,
                     B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE],
                     endpoint->profile,
                     B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE_VERSION],
                     profileVersion,
                     B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_DEVICE_UUID],
                     endpoint->deviceUuid,
                     B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_ENABLED],
                     endpoint->enabled,
                     B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_RESOURCES],
                     resources,
                     B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_METADATA],
                     metadata,
                     NULL);
    }

    return retVal;
}

BDeviceServiceResource *convertIcDeviceResourceToGObject(const icDeviceResource *resource)
{
    BDeviceServiceResource *retVal = NULL;

    if (resource)
    {
        retVal = b_device_service_resource_new();

        BDeviceServiceResourceCachingPolicy cachingPolicy =
            convertResourceCachingPolicyToGObject(resource->cachingPolicy);

        guint mode = resource->mode;
        guint64 dateOfLastSyncMillis = resource->dateOfLastSyncMillis;
        g_object_set(retVal,
                     B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_ID],
                     resource->id,
                     B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_URI],
                     resource->uri,
                     B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_ENDPOINT_ID],
                     resource->endpointId,
                     B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_DEVICE_UUID],
                     resource->deviceUuid,
                     B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_VALUE],
                     resource->value,
                     B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_TYPE],
                     resource->type,
                     B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_MODE],
                     mode,
                     B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_DATE_OF_LAST_SYNC_MILLIS],
                     dateOfLastSyncMillis,
                     NULL);

        if (cachingPolicy != 0)
        {
            g_object_set(retVal,
                         B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_CACHING_POLICY],
                         cachingPolicy,
                         NULL);
        }
    }

    return retVal;
}

BDeviceServiceMetadata *convertIcDeviceMetadataToGObject(const icDeviceMetadata *metadata)
{
    BDeviceServiceMetadata *retVal = NULL;

    if (metadata)
    {
        retVal = b_device_service_metadata_new();

        g_object_set(retVal,
                     B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_ID],
                     metadata->id,
                     B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_URI],
                     metadata->uri,
                     B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_ENDPOINT_ID],
                     metadata->endpointId,
                     B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_DEVICE_UUID],
                     metadata->deviceUuid,
                     B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_VALUE],
                     metadata->value,
                     NULL);
    }

    return retVal;
}

BDeviceServiceDevice *convertIcDeviceToGObject(const icDevice *device)
{
    BDeviceServiceDevice *retVal = NULL;

    if (device)
    {
        retVal = b_device_service_device_new();

        g_autolist(BDeviceServiceEndpoint) endpoints = convertIcDeviceEndpointListToGList(device->endpoints);
        g_autolist(BDeviceServiceResource) resources = convertIcDeviceResourceListToGList(device->resources);
        g_autolist(BDeviceServiceMetadata) metadata = convertIcDeviceMetadataListToGList(device->metadata);

        guint deviceClassVersion = device->deviceClassVersion;
        g_object_set(retVal,
                     B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_UUID],
                     device->uuid,
                     B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS],
                     device->deviceClass,
                     B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS_VERSION],
                     deviceClassVersion,
                     B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_URI],
                     device->uri,
                     B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_MANAGING_DEVICE_DRIVER],
                     device->managingDeviceDriver,
                     B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_ENDPOINTS],
                     endpoints,
                     B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_RESOURCES],
                     resources,
                     B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_METADATA],
                     metadata,
                     NULL);
    }

    return retVal;
}

#ifdef BARTON_CONFIG_ZIGBEE
BDeviceServiceZigbeeEnergyScanResult *
convertZhalEnergyScanResultToGObject(const zhalEnergyScanResult *zigbeeEnergyScanResult)
{
    BDeviceServiceZigbeeEnergyScanResult *retVal = NULL;

    if (zigbeeEnergyScanResult != NULL)
    {
        retVal = b_device_service_zigbee_energy_scan_result_new();

        guint channel = zigbeeEnergyScanResult->channel;
        gint maxRssi = zigbeeEnergyScanResult->maxRssi;
        gint minRssi = zigbeeEnergyScanResult->minRssi;
        gint averageRssi = zigbeeEnergyScanResult->averageRssi;
        guint score = zigbeeEnergyScanResult->score;
        g_object_set(retVal,
                     B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                         [B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_CHANNEL],
                     channel,
                     B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                         [B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MAX],
                     maxRssi,
                     B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                         [B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MIN],
                     minRssi,
                     B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                         [B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_AVG],
                     averageRssi,
                     B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                         [B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_SCORE],
                     score,
                     NULL);
    }

    return retVal;
}
#endif

GList *convertIcDeviceEndpointListToGList(const icLinkedList *endpoints)
{
    GList *retVal = NULL;

    scoped_icLinkedListIterator *iter = linkedListIteratorCreate((icLinkedList *) endpoints);

    while (linkedListIteratorHasNext(iter))
    {
        const icDeviceEndpoint *endpoint = linkedListIteratorGetNext(iter);
        if (endpoint)
        {
            BDeviceServiceEndpoint *dsEndpoint = convertIcDeviceEndpointToGObject(endpoint);
            if (dsEndpoint)
            {
                retVal = g_list_append(retVal, dsEndpoint);
            }
        }
    }

    return retVal;
}

GList *convertIcDeviceResourceListToGList(const icLinkedList *resources)
{
    GList *retVal = NULL;

    scoped_icLinkedListIterator *iter = linkedListIteratorCreate((icLinkedList *) resources);

    while (linkedListIteratorHasNext(iter))
    {
        const icDeviceResource *resource = linkedListIteratorGetNext(iter);
        if (resource)
        {
            BDeviceServiceResource *dsResource = convertIcDeviceResourceToGObject(resource);
            if (dsResource)
            {
                retVal = g_list_append(retVal, dsResource);
            }
        }
    }

    return retVal;
}

GList *convertIcDeviceMetadataListToGList(const icLinkedList *metadata)
{
    GList *retVal = NULL;

    scoped_icLinkedListIterator *iter = linkedListIteratorCreate((icLinkedList *) metadata);

    while (linkedListIteratorHasNext(iter))
    {
        const icDeviceMetadata *md = linkedListIteratorGetNext(iter);
        if (md)
        {
            BDeviceServiceMetadata *dsMetadata = convertIcDeviceMetadataToGObject(md);
            if (dsMetadata)
            {
                retVal = g_list_append(retVal, dsMetadata);
            }
        }
    }

    return retVal;
}

#ifdef BARTON_CONFIG_ZIGBEE
GList *convertIcZigbeeEnergyScanResultListToGList(const icLinkedList *zigbeeEnergyScanResults)
{
    GList *retVal = NULL;

    scoped_icLinkedListIterator *iter = linkedListIteratorCreate((icLinkedList *) zigbeeEnergyScanResults);

    while (linkedListIteratorHasNext(iter))
    {
        zhalEnergyScanResult *result = linkedListIteratorGetNext(iter);
        if (result != NULL)
        {
            BDeviceServiceZigbeeEnergyScanResult *dsResult = convertZhalEnergyScanResultToGObject(result);
            if (dsResult)
            {
                retVal = g_list_append(retVal, dsResult);
            }
        }
    }

    return retVal;
}
#endif

BDeviceServiceResourceCachingPolicy convertResourceCachingPolicyToGObject(ResourceCachingPolicy policy)
{
    // 0 is invalid in gobject land
    BDeviceServiceResourceCachingPolicy retVal = 0;

    switch (policy)
    {
        case CACHING_POLICY_NEVER:
            retVal = B_DEVICE_SERVICE_RESOURCE_CACHING_POLICY_NEVER;
            break;
        case CACHING_POLICY_ALWAYS:
            retVal = B_DEVICE_SERVICE_RESOURCE_CACHING_POLICY_ALWAYS;
            break;
        default:
            break;
    }

    return retVal;
}

BDeviceServiceStatus *convertDeviceServiceStatusToGObject(const DeviceServiceStatus *status)
{
    g_return_val_if_fail(status != NULL, NULL);

    BDeviceServiceStatus *retVal = b_device_service_status_new();

    g_autoptr(GList) supportedDeviceClassesList =
        convertLinkedListToGListGeneric((icLinkedList *) status->supportedDeviceClasses);
    g_autoptr(GList) discoveringDeviceClassesList =
        convertLinkedListToGListGeneric((icLinkedList *) status->discoveringDeviceClasses);
    g_autoptr(GHashTable) subsystems = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    // add an overall JSON representation of the status. Redundant and may be removed in the future.
    g_autofree gchar *json = deviceServiceStatusToJson(status);

    scoped_icHashMapIterator *iter = hashMapIteratorCreate(status->subsystemsJsonStatus);
    while (hashMapIteratorHasNext(iter))
    {
        char *subsystem_name = NULL;
        uint16_t subsystem_name_len = 0;
        cJSON *subsystem_status_json = NULL;

        hashMapIteratorGetNext(iter, (void **) &subsystem_name, &subsystem_name_len, (void **) &subsystem_status_json);

        char *subsystem_status_json_str = cJSON_PrintUnformatted(subsystem_status_json);
        g_hash_table_insert(subsystems, g_strdup(subsystem_name), subsystem_status_json_str);
    }

    BDeviceServiceDiscoveryType discoveryType = B_DEVICE_SERVICE_DISCOVERY_TYPE_NONE;
    if (status->discoveryRunning)
    {
        discoveryType = B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY;
    }
    else if (status->findingOrphanedDevices)
    {
        discoveryType = B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY;
    }

    g_object_set(retVal,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_DEVICE_CLASSES],
                 supportedDeviceClassesList,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_DISCOVERY_TYPE],
                 discoveryType,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_SEARCHING_DEVICE_CLASSES],
                 discoveringDeviceClassesList,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_DISCOVERY_SECONDS],
                 status->discoveryTimeoutSeconds,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_READY_FOR_OPERATION],
                 status->isReadyForDeviceOperation,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_READY_FOR_PAIRING],
                 status->isReadyForPairing,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_SUBSYSTEMS],
                 subsystems,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_JSON],
                 json,
                 NULL);

    return retVal;
}

BDeviceServiceStatusChangedReason convertStatusChangedReasonToGObject(DeviceServiceStatusChangedReason reason)
{
    switch (reason)
    {
        case DeviceServiceStatusChangedReasonReadyForPairing:
            return B_DEVICE_SERVICE_STATUS_CHANGED_REASON_READY_FOR_PAIRING;
        case DeviceServiceStatusChangedReasonReadyForDeviceOperation:
            return B_DEVICE_SERVICE_STATUS_CHANGED_REASON_READY_FOR_DEVICE_OPERATION;
        case DeviceServiceStatusChangedReasonSubsystemStatus:
            return B_DEVICE_SERVICE_STATUS_CHANGED_REASON_SUBSYSTEM_STATUS;

        case DeviceServiceStatusChangedReasonInvalid:
        default:
            return B_DEVICE_SERVICE_STATUS_CHANGED_REASON_INVALID;
    }
}

BDeviceServiceDeviceFoundDetails *convertDeviceFoundDetailsToGobject(const DeviceFoundDetails *details)
{
    g_return_val_if_fail(details != NULL, NULL);

    BDeviceServiceDeviceFoundDetails *retVal = b_device_service_device_found_details_new();

    g_autoptr(GHashTable) metadata = convertHashMapToStringGHashTable((icHashMap *) details->metadata);
    g_autoptr(GHashTable) endpointProfiles =
        convertHashMapToStringGHashTable((icHashMap *) details->endpointProfileMap);

    g_object_set(
        retVal,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_UUID],
        details->deviceUuid,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_MANUFACTURER],
        details->manufacturer,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_MODEL],
        details->model,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_HARDWARE_VERSION],
        details->hardwareVersion,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_FIRMWARE_VERSION],
        details->firmwareVersion,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_DEVICE_CLASS],
        details->deviceClass,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_METADATA],
        metadata,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_ENDPOINT_PROFILES],
        endpointProfiles,
        NULL);

    return retVal;
}

icLinkedList *convertGListToLinkedListGeneric(GList *glist)
{
    g_return_val_if_fail(glist != NULL, NULL);

    icLinkedList *list = linkedListCreate();
    GList *iter = glist;
    while (iter != NULL)
    {
        linkedListAppend(list, iter->data);
        iter = iter->next;
    }
    return list;
}

GList *convertLinkedListToGListGeneric(icLinkedList *list)
{
    GList *retVal = NULL;

    scoped_icLinkedListIterator *iter = linkedListIteratorCreate(list);

    while (linkedListIteratorHasNext(iter))
    {
        void *data = linkedListIteratorGetNext(iter);
        if (data)
        {
            retVal = g_list_append(retVal, data);
        }
    }

    return retVal;
}

GHashTable *convertHashMapToStringGHashTable(icHashMap *map)
{
    g_return_val_if_fail(map != NULL, NULL);

    GHashTable *retVal = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    scoped_icHashMapIterator *iter = hashMapIteratorCreate(map);
    while (hashMapIteratorHasNext(iter))
    {
        char *key = NULL;
        uint16_t keyLen = 0;
        char *value = NULL;

        hashMapIteratorGetNext(iter, (void **) &key, &keyLen, (void **) &value);

        g_hash_table_insert(retVal, g_strdup(key), g_strdup(value));
    }

    return retVal;
}

GList *convertIcDeviceListToGList(icLinkedList *devices)
{
    GList *retVal = NULL;

    scoped_icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
    while (linkedListIteratorHasNext(iterator))
    {
        icDevice *device = linkedListIteratorGetNext(iterator);
        BDeviceServiceDevice *deviceObject = convertIcDeviceToGObject(device);
        if (deviceObject)
        {
            retVal = g_list_append(retVal, deviceObject);
        }
    }

    return retVal;
}
