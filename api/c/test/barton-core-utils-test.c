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

#include "cjson/cJSON.h"
#include "barton-core-discovery-type.h"
#include "barton-core-endpoint.h"
#include "barton-core-resource.h"
#include "barton-core-status.h"
#include "barton-core-utils.h"
#include "device/icDeviceMetadata.h"
#include "device/icDeviceResource.h"
#include "deviceServiceStatus.h"
#include "icTypes/icLinkedList.h"

#ifdef BARTON_CONFIG_ZIGBEE
#include "zhal/zhal.h"
#endif

#include <glib.h>

static icDeviceResource resource = {.id = "resourceId",
                                    .uri = "resourceUri",
                                    .endpointId = "resourceEndpointId",
                                    .deviceUuid = "resourceDeviceUuid",
                                    .value = "resourceValue",
                                    .type = "resourceType",
                                    .mode = 1,
                                    .cachingPolicy = CACHING_POLICY_ALWAYS,
                                    .dateOfLastSyncMillis = 1};

static icDeviceMetadata metadata = {.id = "metadataId",
                                    .uri = "metadataUri",
                                    .endpointId = "metadataEndpointId",
                                    .deviceUuid = "metadataDeviceUuid",
                                    .value = "metadataValue"};

static icDeviceEndpoint endpoint = {.id = "endpointId",
                                    .uri = "endpointUri",
                                    .profile = "endpointProfile",
                                    .profileVersion = 1,
                                    .deviceUuid = "endpointDeviceUuid",
                                    .enabled = true,
                                    .resources = NULL,
                                    .metadata = NULL};

static icDevice device = {.uuid = "deviceUuid",
                          .deviceClass = "deviceClass",
                          .deviceClassVersion = 1,
                          .uri = "deviceUri",
                          .managingDeviceDriver = "managingDeviceDriver",
                          .endpoints = NULL,
                          .resources = NULL,
                          .metadata = NULL};

#ifdef BARTON_CONFIG_ZIGBEE
static zhalEnergyScanResult zigbeeEnergyScanResult = {.channel = 14,
                                                      .maxRssi = -10,
                                                      .minRssi = -50,
                                                      .averageRssi = -30,
                                                      .score = 98};
#endif

static void verifyBCoreResource(BCoreResource *dsResource)
{
    g_assert_nonnull(dsResource);

    g_autofree gchar *resourceId = NULL;
    g_autofree gchar *resourceUri = NULL;
    g_autofree gchar *resourceEndpointId = NULL;
    g_autofree gchar *resourceDeviceUuid = NULL;
    g_autofree gchar *resourceValue = NULL;
    g_autofree gchar *resourceType = NULL;
    guint resourceMode = 0;
    BCoreResourceCachingPolicy resourceCachingPolicy = B_CORE_RESOURCE_CACHING_POLICY_NEVER;

    g_object_get(dsResource,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_ID],
                 &resourceId,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_URI],
                 &resourceUri,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_ENDPOINT_ID],
                 &resourceEndpointId,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_DEVICE_UUID],
                 &resourceDeviceUuid,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_VALUE],
                 &resourceValue,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_TYPE],
                 &resourceType,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_MODE],
                 &resourceMode,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_CACHING_POLICY],
                 &resourceCachingPolicy,
                 NULL);

    g_assert_cmpstr(resourceId, ==, resource.id);
    g_assert_cmpstr(resourceUri, ==, resource.uri);
    g_assert_cmpstr(resourceEndpointId, ==, resource.endpointId);
    g_assert_cmpstr(resourceDeviceUuid, ==, resource.deviceUuid);
    g_assert_cmpstr(resourceValue, ==, resource.value);
    g_assert_cmpstr(resourceType, ==, resource.type);
    g_assert_cmpuint(resourceMode, ==, resource.mode);
    // Assume convertResourceCachingPolicyToGObject is correct, covered in another test
    g_assert_cmpint(resourceCachingPolicy, ==, convertResourceCachingPolicyToGObject(resource.cachingPolicy));
}

static void verifyBCoreMetadata(BCoreMetadata *dsMetadata)
{
    g_assert_nonnull(dsMetadata);

    g_autofree gchar *metadataId = NULL;
    g_autofree gchar *metadataUri = NULL;
    g_autofree gchar *metadataEndpointId = NULL;
    g_autofree gchar *metadataDeviceUuid = NULL;
    g_autofree gchar *metadataValue = NULL;

    g_object_get(dsMetadata,
                 B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_ID],
                 &metadataId,
                 B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_URI],
                 &metadataUri,
                 B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_ENDPOINT_ID],
                 &metadataEndpointId,
                 B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_DEVICE_UUID],
                 &metadataDeviceUuid,
                 B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_VALUE],
                 &metadataValue,
                 NULL);

    g_assert_cmpstr(metadataId, ==, metadata.id);
    g_assert_cmpstr(metadataUri, ==, metadata.uri);
    g_assert_cmpstr(metadataEndpointId, ==, metadata.endpointId);
    g_assert_cmpstr(metadataDeviceUuid, ==, metadata.deviceUuid);
    g_assert_cmpstr(metadataValue, ==, metadata.value);
}

static void verifyBCoreEndpoint(BCoreEndpoint *dsEndpoint)
{
    g_assert_nonnull(dsEndpoint);

    g_autofree gchar *endpointId = NULL;
    g_autofree gchar *endpointUri = NULL;
    g_autofree gchar *endpointProfile = NULL;
    guint endpointProfileVersion = 0;
    g_autofree gchar *endpointDeviceUuid = NULL;
    gboolean endpointEnabled = false;
    g_autolist(BCoreResource) endpointResources = NULL;
    g_autolist(BCoreMetadata) endpointMetadata = NULL;

    g_object_get(dsEndpoint,
                 B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_ID],
                 &endpointId,
                 B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_URI],
                 &endpointUri,
                 B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_PROFILE],
                 &endpointProfile,
                 B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_PROFILE_VERSION],
                 &endpointProfileVersion,
                 B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_DEVICE_UUID],
                 &endpointDeviceUuid,
                 B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_ENABLED],
                 &endpointEnabled,
                 B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_RESOURCES],
                 &endpointResources,
                 B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_METADATA],
                 &endpointMetadata,
                 NULL);

    g_assert_cmpstr(endpointId, ==, endpoint.id);
    g_assert_cmpstr(endpointUri, ==, endpoint.uri);
    g_assert_cmpstr(endpointProfile, ==, endpoint.profile);
    g_assert_cmpuint(endpointProfileVersion, ==, endpoint.profileVersion);
    g_assert_cmpstr(endpointDeviceUuid, ==, endpoint.deviceUuid);
    g_assert_cmpuint(endpointEnabled, ==, endpoint.enabled);
    g_assert_cmpint(g_list_length(endpointResources), ==, linkedListCount(endpoint.resources));
    g_assert_cmpint(g_list_length(endpointMetadata), ==, linkedListCount(endpoint.metadata));

    BCoreResource *dsResource = g_list_first(endpointResources)->data;
    verifyBCoreResource(dsResource);

    BCoreMetadata *dsMetadata = g_list_first(endpointMetadata)->data;
    verifyBCoreMetadata(dsMetadata);
}

static void verifyBCoreDevice(BCoreDevice *dsDevice)
{
    g_assert_nonnull(dsDevice);

    g_autofree gchar *deviceUuid = NULL;
    g_autofree gchar *deviceClass = NULL;
    guint deviceClassVersion = 0;
    g_autofree gchar *uri = NULL;
    g_autofree gchar *managingDeviceDriver = NULL;
    g_autolist(BCoreEndpoint) endpoints = NULL;
    g_autolist(BCoreResource) resources = NULL;
    g_autolist(BCoreMetadata) metadata = NULL;

    g_object_get(dsDevice,
                 B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_UUID],
                 &deviceUuid,
                 B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_DEVICE_CLASS],
                 &deviceClass,
                 B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_DEVICE_CLASS_VERSION],
                 &deviceClassVersion,
                 B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_URI],
                 &uri,
                 B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_MANAGING_DEVICE_DRIVER],
                 &managingDeviceDriver,
                 B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_ENDPOINTS],
                 &endpoints,
                 B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_RESOURCES],
                 &resources,
                 B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_METADATA],
                 &metadata,
                 NULL);

    g_assert_cmpstr(deviceUuid, ==, device.uuid);
    g_assert_cmpstr(deviceClass, ==, device.deviceClass);
    g_assert_cmpuint(deviceClassVersion, ==, device.deviceClassVersion);
    g_assert_cmpstr(uri, ==, device.uri);
    g_assert_cmpstr(managingDeviceDriver, ==, device.managingDeviceDriver);
    g_assert_cmpint(g_list_length(endpoints), ==, linkedListCount(device.endpoints));
    g_assert_cmpint(g_list_length(resources), ==, linkedListCount(device.resources));
    g_assert_cmpint(g_list_length(metadata), ==, linkedListCount(device.metadata));

    BCoreEndpoint *dsEndpoint = g_list_first(endpoints)->data;
    verifyBCoreEndpoint(dsEndpoint);

    BCoreResource *dsResource = g_list_first(resources)->data;
    verifyBCoreResource(dsResource);

    BCoreMetadata *dsMetadata = g_list_first(metadata)->data;
    verifyBCoreMetadata(dsMetadata);
}

#ifdef BARTON_CONFIG_ZIGBEE
static void verifyBCoreZigbeeEnergyScanResults(BCoreZigbeeEnergyScanResult *dsZigbeeEnergyScanResult)
{
    g_assert_nonnull(dsZigbeeEnergyScanResult);

    guint channel = 0;
    gint maxRssi = 0;
    gint minRssi = 0;
    gint avgRssi = 0;
    guint score = 0;

    g_object_get(dsZigbeeEnergyScanResult,
                 B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_CHANNEL],
                 &channel,
                 B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MAX],
                 &maxRssi,
                 B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MIN],
                 &minRssi,
                 B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_AVG],
                 &avgRssi,
                 B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_SCORE],
                 &score,
                 NULL);

    g_assert_cmpuint(channel, ==, zigbeeEnergyScanResult.channel);
    g_assert_cmpint(maxRssi, ==, zigbeeEnergyScanResult.maxRssi);
    g_assert_cmpint(minRssi, ==, zigbeeEnergyScanResult.minRssi);
    g_assert_cmpint(avgRssi, ==, zigbeeEnergyScanResult.averageRssi);
    g_assert_cmpuint(score, ==, zigbeeEnergyScanResult.score);
}
#endif

static void test_convertIcDeviceEndpointToGObject(void)
{
    g_autoptr(BCoreEndpoint) dsEndpoint = convertIcDeviceEndpointToGObject(&endpoint);
    verifyBCoreEndpoint(dsEndpoint);
}

static void test_convertIcDeviceResourceToGObject(void)
{
    g_autoptr(BCoreResource) dsResource = convertIcDeviceResourceToGObject(&resource);
    verifyBCoreResource(dsResource);
}

static void test_convertIcDeviceMetadataToGObject(void)
{
    g_autoptr(BCoreMetadata) dsMetadata = convertIcDeviceMetadataToGObject(&metadata);
    verifyBCoreMetadata(dsMetadata);
}

static void test_convertIcDeviceToGObject(void)
{
    g_autoptr(BCoreDevice) dsDevice = convertIcDeviceToGObject(&device);
    verifyBCoreDevice(dsDevice);
}

#ifdef BARTON_CONFIG_ZIGBEE
static void test_convertIcZigbeeEnergyScanResultToGObject(void)
{
    g_autoptr(BCoreZigbeeEnergyScanResult) dsZigbeeEnergyScanResult =
        convertZhalEnergyScanResultToGObject(&zigbeeEnergyScanResult);
    verifyBCoreZigbeeEnergyScanResults(dsZigbeeEnergyScanResult);
}
#endif

static void test_convertIcDeviceEndpointListToGList(void)
{
    scoped_icLinkedListNofree *endpoints = linkedListCreate();
    linkedListAppend(endpoints, &endpoint);

    g_autolist(BCoreEndpoint) dsEndpoints = convertIcDeviceEndpointListToGList(endpoints);
    g_assert_cmpint(g_list_length(dsEndpoints), ==, linkedListCount(endpoints));

    BCoreEndpoint *dsEndpoint = g_list_first(dsEndpoints)->data;
    verifyBCoreEndpoint(dsEndpoint);
}

static void test_convertIcDeviceResourceListToGList(void)
{
    scoped_icLinkedListNofree *resources = linkedListCreate();
    linkedListAppend(resources, &resource);

    g_autolist(BCoreResource) dsResources = convertIcDeviceResourceListToGList(resources);
    g_assert_cmpint(g_list_length(dsResources), ==, linkedListCount(resources));

    BCoreResource *dsResource = g_list_first(dsResources)->data;
    verifyBCoreResource(dsResource);
}

static void test_convertIcDeviceMetadataListToGList(void)
{
    scoped_icLinkedListNofree *metadataList = linkedListCreate();
    linkedListAppend(metadataList, &metadata);

    g_autolist(BCoreMetadata) dsMetadataList = convertIcDeviceMetadataListToGList(metadataList);
    g_assert_cmpint(g_list_length(dsMetadataList), ==, linkedListCount(metadataList));

    BCoreMetadata *dsMetadata = g_list_first(dsMetadataList)->data;
    verifyBCoreMetadata(dsMetadata);
}

#ifdef BARTON_CONFIG_ZIGBEE
static void test_convertIcZigbeeEnergyScanResultListToGList(void)
{
    scoped_icLinkedListNofree *zigbeeEnergyScanResults = linkedListCreate();
    linkedListAppend(zigbeeEnergyScanResults, &zigbeeEnergyScanResult);

    g_autolist(BCoreZigbeeEnergyScanResult) dsZigbeeEnergyScanResults =
        convertIcZigbeeEnergyScanResultListToGList(zigbeeEnergyScanResults);
    g_assert_cmpint(g_list_length(dsZigbeeEnergyScanResults), ==, linkedListCount(zigbeeEnergyScanResults));

    BCoreZigbeeEnergyScanResult *dsZigbeeEnergyScanResult = g_list_first(dsZigbeeEnergyScanResults)->data;
    verifyBCoreZigbeeEnergyScanResults(dsZigbeeEnergyScanResult);
}
#endif

static void test_convertResourceCachingPolicyToGObject(void)
{
    g_assert_cmpint(convertResourceCachingPolicyToGObject(CACHING_POLICY_ALWAYS),
                    ==,
                    B_CORE_RESOURCE_CACHING_POLICY_ALWAYS);
    g_assert_cmpint(convertResourceCachingPolicyToGObject(CACHING_POLICY_NEVER),
                    ==,
                    B_CORE_RESOURCE_CACHING_POLICY_NEVER);
}

static void test_convertDeviceServiceStatusToGObject(void)
{
    scoped_DeviceServiceStatus *original_status = calloc(1, sizeof(DeviceServiceStatus));
    BCoreStatus *converted_status = NULL;

    // fill in some test data
    original_status->supportedDeviceClasses = linkedListCreate();
    linkedListAppend(original_status->supportedDeviceClasses, strdup("test-device-class"));
    original_status->discoveryRunning = true;
    original_status->discoveringDeviceClasses = linkedListCreate();
    linkedListAppend(original_status->discoveringDeviceClasses, strdup("test-searching-device-class"));
    original_status->discoveryTimeoutSeconds = G_MAXUINT;
    original_status->findingOrphanedDevices =
        false; // only this or discoveryRunning can be true at the same time... will have to test this one below
    original_status->isReadyForDeviceOperation = true;
    original_status->isReadyForPairing = true;
    original_status->subsystemsJsonStatus = hashMapCreate();
    cJSON *jsonStatus = cJSON_CreateObject();
    cJSON_AddStringToObject(jsonStatus, "testKey", "testValue");
    hashMapPut(original_status->subsystemsJsonStatus, strdup("testSubsystem"), strlen("testSubsystem"), jsonStatus);

    // Convert DeviceServiceStatus to BCoreStatus
    converted_status = convertDeviceServiceStatusToGObject(original_status);

    // Verify the conversion
    g_assert_nonnull(converted_status);

    // test device classes
    GList *device_classes_test = NULL;
    g_object_get(converted_status,
                 B_CORE_STATUS_PROPERTY_NAMES[B_CORE_STATUS_PROP_DEVICE_CLASSES],
                 &device_classes_test,
                 NULL);
    g_assert_nonnull(device_classes_test);
    g_assert_cmpuint(g_list_length(device_classes_test), ==, 1);
    g_assert_cmpstr(g_list_first(device_classes_test)->data, ==, "test-device-class");
    g_list_free_full(device_classes_test, g_free);

    // test discovery type
    BCoreDiscoveryType discovery_type = B_CORE_DISCOVERY_TYPE_NONE;
    g_object_get(converted_status,
                 B_CORE_STATUS_PROPERTY_NAMES[B_CORE_STATUS_PROP_DISCOVERY_TYPE],
                 &discovery_type,
                 NULL);
    g_assert_cmpuint(discovery_type, ==, B_CORE_DISCOVERY_TYPE_DISCOVERY);

    // test searching device classes
    GList *searching_device_classes_test = NULL;
    g_object_get(converted_status,
                 B_CORE_STATUS_PROPERTY_NAMES[B_CORE_STATUS_PROP_SEARCHING_DEVICE_CLASSES],
                 &searching_device_classes_test,
                 NULL);
    g_assert_nonnull(searching_device_classes_test);
    g_assert_cmpuint(g_list_length(searching_device_classes_test), ==, 1);
    g_assert_cmpstr(g_list_first(searching_device_classes_test)->data, ==, "test-searching-device-class");
    g_list_free_full(searching_device_classes_test, g_free);

    // test discovery seconds
    guint discovery_seconds = 0;
    g_object_get(converted_status,
                 B_CORE_STATUS_PROPERTY_NAMES[B_CORE_STATUS_PROP_DISCOVERY_SECONDS],
                 &discovery_seconds,
                 NULL);
    g_assert_cmpuint(discovery_seconds, ==, G_MAXUINT);

    // test ready for operation
    gboolean ready_for_operation = FALSE;
    g_object_get(converted_status,
                 B_CORE_STATUS_PROPERTY_NAMES[B_CORE_STATUS_PROP_READY_FOR_OPERATION],
                 &ready_for_operation,
                 NULL);
    g_assert_cmpuint(ready_for_operation, ==, TRUE);

    // test ready for pairing
    gboolean ready_for_pairing = FALSE;
    g_object_get(converted_status,
                 B_CORE_STATUS_PROPERTY_NAMES[B_CORE_STATUS_PROP_READY_FOR_PAIRING],
                 &ready_for_pairing,
                 NULL);
    g_assert_cmpuint(ready_for_pairing, ==, TRUE);

    // test subsystems
    g_autoptr(GHashTable) subsystems_test = NULL;
    g_object_get(converted_status,
                 B_CORE_STATUS_PROPERTY_NAMES[B_CORE_STATUS_PROP_SUBSYSTEMS],
                 &subsystems_test,
                 NULL);
    g_assert_nonnull(subsystems_test);
    g_assert_cmpuint(g_hash_table_size(subsystems_test), ==, 1);
    g_autofree gchar *json_subsystems = cJSON_PrintUnformatted(jsonStatus);
    g_assert_cmpstr(g_hash_table_lookup(subsystems_test, "testSubsystem"), ==, json_subsystems);

    // test json
    gchar *json = NULL;
    g_object_get(
        converted_status, B_CORE_STATUS_PROPERTY_NAMES[B_CORE_STATUS_PROP_JSON], &json, NULL);
    g_assert_nonnull(json);
    g_autofree gchar *json_complete_status = deviceServiceStatusToJson(original_status);
    g_assert_cmpstr(json, ==, json_complete_status);
    g_free(json);

    // finally test findingOrphanedDevices
    g_clear_object(&converted_status);
    original_status->findingOrphanedDevices = true;
    original_status->discoveryRunning = false;
    converted_status = convertDeviceServiceStatusToGObject(original_status);
    discovery_type = B_CORE_DISCOVERY_TYPE_NONE;
    g_object_get(converted_status,
                 B_CORE_STATUS_PROPERTY_NAMES[B_CORE_STATUS_PROP_DISCOVERY_TYPE],
                 &discovery_type,
                 NULL);
    g_assert_cmpuint(discovery_type, ==, B_CORE_DISCOVERY_TYPE_RECOVERY);

    // Clean up
    g_clear_object(&converted_status);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    argc = 0;
    g_test_init(&argc, &argv, NULL);

    // Some global setup
    endpoint.resources = linkedListCreate();
    linkedListAppend(endpoint.resources, &resource);
    endpoint.metadata = linkedListCreate();
    linkedListAppend(endpoint.metadata, &metadata);
    device.endpoints = linkedListCreate();
    linkedListAppend(device.endpoints, &endpoint);
    // Although we are re-using the same resource and metadata for both the endpoint and device (and that would
    // not be the case in a real device model), we are just doing so for simple testing purposes.
    device.resources = linkedListCreate();
    linkedListAppend(device.resources, &resource);
    device.metadata = linkedListCreate();
    linkedListAppend(device.metadata, &metadata);

    g_test_add_func("/barton-core-utils/convertIcDeviceEndpointToGObject", test_convertIcDeviceEndpointToGObject);
    g_test_add_func("/barton-core-utils/convertIcDeviceResourceToGObject", test_convertIcDeviceResourceToGObject);
    g_test_add_func("/barton-core-utils/convertIcDeviceMetadataToGObject", test_convertIcDeviceMetadataToGObject);
    g_test_add_func("/barton-core-utils/convertIcDeviceToGObject", test_convertIcDeviceToGObject);
    g_test_add_func("/barton-core-utils/convertIcDeviceEndpointListToGList",
                    test_convertIcDeviceEndpointListToGList);
    g_test_add_func("/barton-core-utils/convertIcDeviceResourceListToGList",
                    test_convertIcDeviceResourceListToGList);
    g_test_add_func("/barton-core-utils/convertIcDeviceMetadataListToGList",
                    test_convertIcDeviceMetadataListToGList);
    g_test_add_func("/barton-core-utils/convertResourceCachingPolicyToGObject",
                    test_convertResourceCachingPolicyToGObject);
    g_test_add_func("/barton-core-utils/convertDeviceServiceStatusToGObject",
                    test_convertDeviceServiceStatusToGObject);
#ifdef BARTON_CONFIG_ZIGBEE
    g_test_add_func("/barton-core-utils/convertIcZigbeeEnergyScanResultToGObject",
                    test_convertIcZigbeeEnergyScanResultToGObject);
    g_test_add_func("/barton-core-utils/convertIcZigbeeEnergyScanResultListToGList",
                    test_convertIcZigbeeEnergyScanResultListToGList);
#endif

    // Run tests
    return g_test_run();
}
