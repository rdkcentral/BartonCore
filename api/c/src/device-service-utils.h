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
 * Created by Christian Leithner on 6/2/2024.
 */

// Some helper utilities for translating between interanl/external interface types.
#pragma once

#include "device-driver/device-driver.h"
#include "device-service-device-found-details.h"
#include "device-service-discovery-filter.h"
#include "device-service-endpoint.h"
#include "device-service-metadata.h"
#include "device-service-resource.h"
#include "device-service-status.h"
#include "device-service-zigbee-energy-scan-result.h"
#include "events/device-service-status-event.h"
#include "private/device/icDeviceEndpoint.h"
#include "private/device/icDeviceMetadata.h"
#include "private/device/icDeviceResource.h"
#include "private/deviceServiceStatus.h"

#ifdef BARTON_CONFIG_ZIGBEE
#include "zhal/zhal.h"
#endif


BDeviceServiceEndpoint *convertIcDeviceEndpointToGObject(const icDeviceEndpoint *endpoint);

BDeviceServiceResource *convertIcDeviceResourceToGObject(const icDeviceResource *resource);

BDeviceServiceMetadata *convertIcDeviceMetadataToGObject(const icDeviceMetadata *metadata);

BDeviceServiceDevice *convertIcDeviceToGObject(const icDevice *device);

#ifdef BARTON_CONFIG_ZIGBEE
BDeviceServiceZigbeeEnergyScanResult *
convertZhalEnergyScanResultToGObject(const zhalEnergyScanResult *zigbeeEnergyScanResult);
#endif

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
