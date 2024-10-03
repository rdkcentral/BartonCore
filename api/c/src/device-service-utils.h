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

// Some helper utilities for translating between interanl/external interface types.
#pragma once

#include "deprecated/device/icDeviceEndpoint.h"
#include "deprecated/device/icDeviceMetadata.h"
#include "deprecated/device/icDeviceResource.h"
#include "deprecated/deviceServiceStatus.h"
#include "device-driver/device-driver.h"
#include "device-service-device-found-details.h"
#include "device-service-discovery-filter.h"
#include "device-service-endpoint.h"
#include "device-service-metadata.h"
#include "device-service-resource.h"
#include "device-service-status.h"
#include "device-service-zigbee-energy-scan-result.h"
#include "events/device-service-status-event.h"
#include "zhal/zhal.h"


BDeviceServiceEndpoint *convertIcDeviceEndpointToGObject(const icDeviceEndpoint *endpoint);

BDeviceServiceResource *convertIcDeviceResourceToGObject(const icDeviceResource *resource);

BDeviceServiceMetadata *convertIcDeviceMetadataToGObject(const icDeviceMetadata *metadata);

BDeviceServiceDevice *convertIcDeviceToGObject(const icDevice *device);

BDeviceServiceZigbeeEnergyScanResult *
convertZhalEnergyScanResultToGObject(const zhalEnergyScanResult *zigbeeEnergyScanResult);

GList *convertIcDeviceEndpointListToGList(const icLinkedList *endpoints);

GList *convertIcDeviceResourceListToGList(const icLinkedList *resources);

GList *convertIcDeviceMetadataListToGList(const icLinkedList *metadata);

GList *convertIcZigbeeEnergyScanResultListToGList(const icLinkedList *zigbeeEnergyScanResults);

BDeviceServiceResourceCachingPolicy convertResourceCachingPolicyToGObject(ResourceCachingPolicy policy);

BDeviceServiceStatus *convertDeviceServiceStatusToGObject(const DeviceServiceStatus *status);

BDeviceServiceStatusChangedReason convertStatusChangedReasonToGObject(DeviceServiceStatusChangedReason reason);

BDeviceServiceDeviceFoundDetails *convertDeviceFoundDetailsToGobject(const DeviceFoundDetails *details);

// Shallow copy, data is shared
icLinkedList *convertGListToLinkedListGeneric(GList *glist);

// Shallow copy, data is shared
GList *convertLinkedListToGListGeneric(icLinkedList *list);

GList *convertIcDeviceListToGList(icLinkedList *devices);

// Deep copy, keys and values must be strings
GHashTable *convertHashMapToStringGHashTable(icHashMap *map);

static inline gpointer glistGobjectDataDeepCopy(gconstpointer src, gpointer data)
{
    return g_object_ref((gpointer) src);
}

static inline gpointer glistStringDataDeepCopy(gconstpointer src, gpointer data)
{
    return g_strdup(src);
}