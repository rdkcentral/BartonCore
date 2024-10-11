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

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include "cjson/cJSON.h"
#include "device-driver/device-driver.h"
#include "device-service-client.h"
#include "device-service-device-found-details.h"
#include "device-service-discovery-filter.h"
#include "device-service-discovery-type.h"
#include "device-service-endpoint.h"
#include "device-service-metadata.h"
#include "device-service-resource.h"
#include "device-service-status.h"
#include "device-service-zigbee-energy-scan-result.h"
#include "device/icDevice.h"
#include "device/icDeviceEndpoint.h"
#include "device/icDeviceMetadata.h"
#include "device/icDeviceResource.h"
#include "deviceDescriptor.h"
#include "deviceDiscoveryFilters.h"
#include "deviceServiceStatus.h"
#include "event/deviceEventProducer.h"
#include "events/device-service-device-added-event.h"
#include "events/device-service-device-configuration-completed-event.h"
#include "events/device-service-device-configuration-event.h"
#include "events/device-service-device-configuration-failed-event.h"
#include "events/device-service-device-configuration-started-event.h"
#include "events/device-service-device-database-failure-event.h"
#include "events/device-service-device-discovered-event.h"
#include "events/device-service-device-discovery-completed-event.h"
#include "events/device-service-device-discovery-failed-event.h"
#include "events/device-service-device-recovered-event.h"
#include "events/device-service-device-rejected-event.h"
#include "events/device-service-device-removed-event.h"
#include "events/device-service-discovery-session-info-event.h"
#include "events/device-service-discovery-started-event.h"
#include "events/device-service-discovery-stopped-event.h"
#include "events/device-service-endpoint-added-event.h"
#include "events/device-service-endpoint-removed-event.h"
#include "events/device-service-recovery-started-event.h"
#include "events/device-service-recovery-stopped-event.h"
#include "events/device-service-resource-updated-event.h"
#include "events/device-service-status-event.h"
#include "events/device-service-storage-changed-event.h"
#include "events/device-service-zigbee-channel-changed-event.h"
#include "events/device-service-zigbee-interference-event.h"
#include "events/device-service-zigbee-pan-id-attack-changed-event.h"
#include "icTypes/icHashMap.h"
#include "icTypes/icLinkedList.h"
#include "icTypes/icStringHashMap.h"
#include "jsonHelper/jsonHelper.h"

#ifdef BARTON_CONFIG_ZIGBEE
#include "private/subsystems/zigbee/zigbeeSubsystem.h"
#include "zhal/zhal.h"
#endif

#include <cmocka.h>
#include <string.h>

#define LOG_TAG G_LOG_DOMAIN

// Used as a parameter for expect_check functions when the size of the array
// cannot be determined programmatically.
typedef struct ArrayCompareParams
{
    void *array;
    size_t arraySize;
} ArrayCompareParams;

static bool compareStringGLists(GList *first, GList *second);

// Custom Checkers
static int checkListContents(icLinkedList *input, GList *expected);
static int checkDiscoveryFilterListContents(icLinkedList *input, GList *expected);
static int checkBDeviceServiceDiscoveryStartedEventContents(BDeviceServiceDiscoveryStartedEvent *input,
                                                            BDeviceServiceDiscoveryStartedEvent *expected);
static int checkBDeviceServiceRecoveryStartedEventContents(BDeviceServiceRecoveryStartedEvent *input,
                                                           BDeviceServiceRecoveryStartedEvent *expected);
static bool checkBDeviceServiceStatusContents(BDeviceServiceStatus *input, BDeviceServiceStatus *expected);
static int checkBDeviceServiceStatusEventContents(BDeviceServiceStatusEvent *input,
                                                  BDeviceServiceStatusEvent *expected);
static int checkBDeviceServiceDeviceDiscoveryDetailsContents(BDeviceServiceDeviceFoundDetails *input,
                                                             BDeviceServiceDeviceFoundDetails *expected);
static int checkBDeviceServiceDeviceDiscoveryFailedEventContents(BDeviceServiceDeviceDiscoveryFailedEvent *input,
                                                                 BDeviceServiceDeviceDiscoveryFailedEvent *expected);
static int checkBDeviceServiceDeviceDiscoveredEventContents(BDeviceServiceDeviceDiscoveredEvent *input,
                                                            BDeviceServiceDeviceDiscoveredEvent *expected);
static int checkBDeviceServiceDeviceRejectedEventContents(BDeviceServiceDeviceRejectedEvent *input,
                                                          BDeviceServiceDeviceRejectedEvent *expected);
static int
checkBDeviceServiceDeviceConfigurationStartedEventContents(BDeviceServiceDeviceConfigurationStartedEvent *input,
                                                           BDeviceServiceDeviceConfigurationStartedEvent *expected);
static int
checkBDeviceServiceDeviceConfigurationCompletedEventContents(BDeviceServiceDeviceConfigurationCompletedEvent *input,
                                                             BDeviceServiceDeviceConfigurationCompletedEvent *expected);
static int
checkBDeviceServiceDeviceConfigurationFailedEventContents(BDeviceServiceDeviceConfigurationFailedEvent *input,
                                                          BDeviceServiceDeviceConfigurationFailedEvent *expected);
static int checkBDeviceServiceDeviceAddedEventContents(BDeviceServiceDeviceAddedEvent *input,
                                                       BDeviceServiceDeviceAddedEvent *expected);
static int checkBDeviceServiceDeviceRecoveredEventContents(BDeviceServiceDeviceRecoveredEvent *input,
                                                           BDeviceServiceDeviceRecoveredEvent *expected);
static int
checkBDeviceServiceDeviceDiscoveryCompletedEventContents(BDeviceServiceDeviceDiscoveryCompletedEvent *input,
                                                         BDeviceServiceDeviceDiscoveryCompletedEvent *expected);
static int checkBDeviceServiceDeviceContents(BDeviceServiceDevice *input, BDeviceServiceDevice *expected);
static int checkBDeviceServiceEndpointContents(BDeviceServiceEndpoint *input, BDeviceServiceEndpoint *expected);
static int checkBDeviceServiceResourceContents(BDeviceServiceResource *input, BDeviceServiceResource *expected);
static int checkBDeviceServiceMetadataContents(BDeviceServiceMetadata *input, BDeviceServiceMetadata *expected);
static int checkBDeviceServiceDiscoveryStoppedEventContents(BDeviceServiceDiscoveryStoppedEvent *input,
                                                            BDeviceServiceDiscoveryStoppedEvent *expected);
static int checkBDeviceServiceRecoveryStoppedEventContents(BDeviceServiceRecoveryStoppedEvent *input,
                                                           BDeviceServiceRecoveryStoppedEvent *expected);
static int checkBDeviceServiceResourceUpdatedEventContents(BDeviceServiceResourceUpdatedEvent *input,
                                                           BDeviceServiceResourceUpdatedEvent *expected);
static int checkBDeviceServiceDeviceRemovedEventContents(BDeviceServiceDeviceRemovedEvent *input,
                                                         BDeviceServiceDeviceRemovedEvent *expected);
static int checkBDeviceServiceEndpointRemovedEventContents(BDeviceServiceEndpointRemovedEvent *input,
                                                           BDeviceServiceEndpointRemovedEvent *expected);
static int checkBDeviceServiceEndpointAddedEventContents(BDeviceServiceEndpointAddedEvent *input,
                                                         BDeviceServiceEndpointAddedEvent *expected);
static int checkBDeviceServiceZigbeeChannelChangedEventContents(BDeviceServiceZigbeeChannelChangedEvent *input,
                                                                BDeviceServiceZigbeeChannelChangedEvent *expected);
static int checkBDeviceServiceStorageChangedEventContents(BDeviceServiceStorageChangedEvent *input,
                                                          BDeviceServiceStorageChangedEvent *expected);
static int checkBDeviceServiceZigbeeInterferenceEventContents(BDeviceServiceZigbeeInterferenceEvent *input,
                                                              BDeviceServiceZigbeeInterferenceEvent *expected);
static int
checkBDeviceServiceZigbeePanIdAttackChangedEventContents(BDeviceServiceZigbeePanIdAttackChangedEvent *input,
                                                         BDeviceServiceZigbeePanIdAttackChangedEvent *expected);
static bool checkBDeviceServiceZigbeeEnergyScanResultChannelContents(uint8_t *input, ArrayCompareParams *expected);
static int checkBDeviceServiceDeviceDatabaseFailureEventContents(BDeviceServiceDeviceDatabaseFailureEvent *input,
                                                                 BDeviceServiceDeviceDatabaseFailureEvent *expected);

#ifdef BARTON_CONFIG_ZIGBEE
static void assert_change_zigbee_channel_errors(BDeviceServiceClient *client,
                                                uint8_t channel,
                                                ChannelChangeResponse channelChangeResponse,
                                                BDeviceServiceZigbeeChannelChangeError changeError);
#endif

// Wraps
bool __wrap_deviceServiceInitialize(void);
void __wrap_allServicesAvailableNotify(void);
void __wrap_deviceServiceShutdown(void);
icLinkedList *__wrap_deviceServiceGetEndpointsByProfile(const char *profile);
bool __wrap_deviceServiceDiscoverStart(icLinkedList *deviceClasses,
                                       icLinkedList *filters,
                                       uint16_t timeoutSeconds,
                                       bool findOrphanedDevices);
bool __wrap_deviceServiceDiscoverStop(icLinkedList *deviceClasses);
DeviceServiceStatus *__wrap_deviceServiceGetStatus(void);
icDevice *__wrap_deviceServiceGetDevice(const char *uuid);
bool __wrap_deviceServiceCommissionDevice(const char *setupPayload, uint16_t timeoutSeconds);
bool __wrap_deviceServiceAddMatterDevice(uint64_t nodeId, uint16_t timeoutSeconds);
icDeviceEndpoint *__wrap_deviceServiceGetEndpointById(const char *deviceUuid, const char *endpointId);
icDeviceEndpoint *__wrap_deviceServiceGetEndpointByUri(const char *uri);
icLinkedList *__wrap_deviceServiceGetAllDevices();
icLinkedList *__wrap_deviceServiceGetDevicesByDeviceClass(const char *deviceClass);
icDeviceResource *__wrap_deviceServiceGetResourceByUri(const char *uri);
icLinkedList *__wrap_deviceServiceGetDevicesBySubsystem(const char *subsystem);
icDevice *__wrap_deviceServiceGetDeviceByUri(const char *uri);
bool __wrap_deviceServiceWriteResource(const char *uri, const char *resourceValue);
bool __wrap_deviceServiceExecuteResource(const char *uri);
bool __wrap_deviceServiceRemoveDevice(const char *uuid);
bool __wrap_deviceServiceGetSystemProperty(const char *key, char **value);
bool __wrap_deviceServiceSetSystemProperty(const char *name, const char *value);
bool __wrap_deviceServiceRemoveEndpointById(const char *deviceUuid, const char *endpointId);
bool __wrap_deviceServiceSetMetadata(const char *uri, const char *value);
bool __wrap_deviceServiceGetMetadata(const char *uri, char **value);
icLinkedList *__wrap_deviceServiceGetResourcesByUriPattern(char *uri);
bool __wrap_deviceServiceChangeResourceMode(const char *uri, uint8_t newMode);

#ifdef BARTON_CONFIG_ZIGBEE
ChannelChangeResponse __wrap_zigbeeSubsystemChangeChannel(uint8_t channel, bool dryRun);
#endif

icLinkedList *__wrap_deviceServiceGetMetadataByUriPattern(const char *uriPattern);
void __wrap_deviceServiceProcessDeviceDescriptors(void);
char *__wrap_zhalTest(void);
icLinkedList *__wrap_zigbeeSubsystemPerformEnergyScan(const uint8_t *channelsToScan,
                                                      uint8_t numChannelsToScan,
                                                      uint32_t scanDurationMillis,
                                                      uint32_t numScans);
bool __wrap_deviceServiceRestoreConfig(const char *tempRestoreDir);

// Mock Signal Handlers
#define DECLARE_MOCK_EVENT_HANDLER(eventType)                                                                          \
    static void mockHandle##eventType(GObject *source, BDeviceService##eventType *event);

DECLARE_MOCK_EVENT_HANDLER(DiscoveryStartedEvent)
DECLARE_MOCK_EVENT_HANDLER(RecoveryStartedEvent)
DECLARE_MOCK_EVENT_HANDLER(StatusEvent)
DECLARE_MOCK_EVENT_HANDLER(DeviceDiscoveryFailedEvent)
DECLARE_MOCK_EVENT_HANDLER(DeviceDiscoveredEvent)
DECLARE_MOCK_EVENT_HANDLER(DeviceRejectedEvent)
DECLARE_MOCK_EVENT_HANDLER(DeviceConfigurationStartedEvent)
DECLARE_MOCK_EVENT_HANDLER(DeviceConfigurationCompletedEvent)
DECLARE_MOCK_EVENT_HANDLER(DeviceConfigurationFailedEvent)
DECLARE_MOCK_EVENT_HANDLER(DeviceAddedEvent)
DECLARE_MOCK_EVENT_HANDLER(DeviceRecoveredEvent)
DECLARE_MOCK_EVENT_HANDLER(DeviceDiscoveryCompletedEvent)
DECLARE_MOCK_EVENT_HANDLER(DiscoveryStoppedEvent)
DECLARE_MOCK_EVENT_HANDLER(RecoveryStoppedEvent)
DECLARE_MOCK_EVENT_HANDLER(ResourceUpdatedEvent)
DECLARE_MOCK_EVENT_HANDLER(DeviceRemovedEvent)
DECLARE_MOCK_EVENT_HANDLER(EndpointRemovedEvent)
DECLARE_MOCK_EVENT_HANDLER(EndpointAddedEvent)
DECLARE_MOCK_EVENT_HANDLER(ZigbeeChannelChangedEvent)
DECLARE_MOCK_EVENT_HANDLER(StorageChangedEvent)
DECLARE_MOCK_EVENT_HANDLER(ZigbeeInterferenceEvent)
DECLARE_MOCK_EVENT_HANDLER(ZigbeePanIdAttackChangedEvent)
DECLARE_MOCK_EVENT_HANDLER(DeviceDatabaseFailureEvent)

static void registerSignalHandlers(BDeviceServiceClient *client)
{
    g_signal_connect(client,
                     B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DISCOVERY_STARTED,
                     G_CALLBACK(mockHandleDiscoveryStartedEvent),
                     NULL);

    g_signal_connect(
        client, B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_RECOVERY_STARTED, G_CALLBACK(mockHandleRecoveryStartedEvent), NULL);

    g_signal_connect(
        client, B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_STATUS_CHANGED, G_CALLBACK(mockHandleStatusEvent), NULL);

    g_signal_connect(client,
                     B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_DISCOVERY_FAILED,
                     G_CALLBACK(mockHandleDeviceDiscoveryFailedEvent),
                     NULL);

    g_signal_connect(client,
                     B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_DISCOVERED,
                     G_CALLBACK(mockHandleDeviceDiscoveredEvent),
                     NULL);

    g_signal_connect(
        client, B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_RECOVERED, G_CALLBACK(mockHandleDeviceRecoveredEvent), NULL);

    g_signal_connect(
        client, B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_REJECTED, G_CALLBACK(mockHandleDeviceRejectedEvent), NULL);

    g_signal_connect(client,
                     B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_CONFIGURATION_STARTED,
                     G_CALLBACK(mockHandleDeviceConfigurationStartedEvent),
                     NULL);

    g_signal_connect(client,
                     B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_CONFIGURATION_COMPLETED,
                     G_CALLBACK(mockHandleDeviceConfigurationCompletedEvent),
                     NULL);

    g_signal_connect(client,
                     B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_CONFIGURATION_FAILED,
                     G_CALLBACK(mockHandleDeviceConfigurationFailedEvent),
                     NULL);

    g_signal_connect(
        client, B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_ADDED, G_CALLBACK(mockHandleDeviceAddedEvent), NULL);

    g_signal_connect(client,
                     B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_DISCOVERY_COMPLETED,
                     G_CALLBACK(mockHandleDeviceDiscoveryCompletedEvent),
                     NULL);

    g_signal_connect(client,
                     B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DISCOVERY_STOPPED,
                     G_CALLBACK(mockHandleDiscoveryStoppedEvent),
                     NULL);

    g_signal_connect(
        client, B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_RECOVERY_STOPPED, G_CALLBACK(mockHandleRecoveryStoppedEvent), NULL);

    g_signal_connect(
        client, B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_RESOURCE_UPDATED, G_CALLBACK(mockHandleResourceUpdatedEvent), NULL);

    g_signal_connect(
        client, B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_REMOVED, G_CALLBACK(mockHandleDeviceRemovedEvent), NULL);

    g_signal_connect(
        client, B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_ENDPOINT_REMOVED, G_CALLBACK(mockHandleEndpointRemovedEvent), NULL);

    g_signal_connect(
        client, B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_ENDPOINT_ADDED, G_CALLBACK(mockHandleEndpointAddedEvent), NULL);

    g_signal_connect(client,
                     B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_ZIGBEE_CHANNEL_CHANGED,
                     G_CALLBACK(mockHandleZigbeeChannelChangedEvent),
                     NULL);

    g_signal_connect(
        client, B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_STORAGE_CHANGED, G_CALLBACK(mockHandleStorageChangedEvent), NULL);

    g_signal_connect(client,
                     B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_ZIGBEE_INTERFERENCE,
                     G_CALLBACK(mockHandleZigbeeInterferenceEvent),
                     NULL);

    g_signal_connect(client,
                     B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_ZIGBEE_PAN_ID_ATTACK_CHANGED,
                     G_CALLBACK(mockHandleZigbeePanIdAttackChangedEvent),
                     NULL);

    g_signal_connect(client,
                     B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_DATABASE_FAILURE,
                     G_CALLBACK(mockHandleDeviceDatabaseFailureEvent),
                     NULL);
}

static int setup_b_device_service_client(void **state)
{
    g_autoptr(BDeviceServiceInitializeParamsContainer) params = b_device_service_initialize_params_container_new();

    *state = b_device_service_client_new(params);
    will_return(__wrap_deviceServiceInitialize, true);
    expect_function_call(__wrap_deviceServiceInitialize);
    assert_true(b_device_service_client_start(*state));

    expect_function_call(__wrap_allServicesAvailableNotify);
    b_device_service_client_dependencies_ready(*state);

    deviceEventProducerStartup(*state);
    registerSignalHandlers(*state);

    return 0;
}

static int teardown_b_device_service_client(void **state)
{
    deviceEventProducerShutdown();

    expect_function_call(__wrap_deviceServiceShutdown);
    b_device_service_client_stop(*state);
    g_object_unref(*state);

    return 0;
}

static void *stupidIcEndpointLinkedListCloneFunction(void *input, void *context)
{
    icDeviceEndpoint *inputEndpoint = (icDeviceEndpoint *) input;
    return endpointClone(inputEndpoint);
}

static void *icDevicesLinkedListCloneFunction(void *input, void *context)
{
    (void) context;
    return deviceClone((icDevice *) input);
}

static void *icDeviceResourcesLinkedListCloneFunction(void *input, void *context)
{
    (void) context;
    icDeviceResource *inputResource = (icDeviceResource *) input;
    return resourceClone(inputResource);
}

static void *icMetadataLinkedListCloneFunction(void *input, void *context)
{
    (void) context;
    return metadataClone((icDeviceMetadata *) input);
}

#ifdef BARTON_CONFIG_ZIGBEE
static void *zhalEnergyScanResultsLinkedListCloneFunction(void *input, void *context)
{
    (void) context;

    zhalEnergyScanResult *retVal = NULL;
    zhalEnergyScanResult *inputResult = (zhalEnergyScanResult *) input;

    if (inputResult != NULL)
    {
        retVal = (zhalEnergyScanResult *) calloc(1, sizeof(zhalEnergyScanResult));
        retVal->channel = inputResult->channel;
        retVal->maxRssi = inputResult->maxRssi;
        retVal->minRssi = inputResult->minRssi;
        retVal->averageRssi = inputResult->averageRssi;
        retVal->score = inputResult->score;
    }

    return retVal;
}
#endif

static void test_b_device_service_client_get_endpoints_by_profile(void **state)
{
    BDeviceServiceClient *client = *state;
    const char *profileId = "profileId";
    const char *endpointId = "endpointId";
    icLinkedList *endpoints = linkedListCreate();

    // NULL client, valid profileId
    g_autolist(BDeviceServiceEndpoint) result1a = b_device_service_client_get_endpoints_by_profile(NULL, profileId);
    assert_null(result1a);

    // NULL client, NULL profileId
    g_autolist(BDeviceServiceEndpoint) result1b = b_device_service_client_get_endpoints_by_profile(NULL, NULL);
    assert_null(result1b);

    // valid client, NULL profileId
    g_autolist(BDeviceServiceEndpoint) result1c = b_device_service_client_get_endpoints_by_profile(client, NULL);
    assert_null(result1c);

    // NULL endpoint list
    expect_string(__wrap_deviceServiceGetEndpointsByProfile, profile, profileId);
    will_return(__wrap_deviceServiceGetEndpointsByProfile, NULL);
    expect_function_call(__wrap_deviceServiceGetEndpointsByProfile);

    g_autolist(BDeviceServiceEndpoint) result2 = b_device_service_client_get_endpoints_by_profile(client, profileId);
    assert_null(result2);

    // Empty endpoint list
    expect_string(__wrap_deviceServiceGetEndpointsByProfile, profile, profileId);
    will_return(__wrap_deviceServiceGetEndpointsByProfile, g_steal_pointer(&endpoints));
    expect_function_call(__wrap_deviceServiceGetEndpointsByProfile);

    g_autolist(BDeviceServiceEndpoint) result3 = b_device_service_client_get_endpoints_by_profile(client, profileId);
    assert_null(result3);

    // Non-empty endpoint list
    // Don't need to check every member of icDeviceEndpoint against resulting BDeviceServiceEndpoint;
    // type conversion unit tested elsewhere.
    endpoints = linkedListCreate();
    icDeviceEndpoint *endpoint = calloc(1, sizeof(icDeviceEndpoint));
    endpoint->id = strdup(endpointId);
    linkedListAppend(endpoints, g_steal_pointer(&endpoint));

    icLinkedList *endpointsClone = linkedListDeepClone(endpoints, stupidIcEndpointLinkedListCloneFunction, NULL);
    expect_string(__wrap_deviceServiceGetEndpointsByProfile, profile, profileId);
    will_return(__wrap_deviceServiceGetEndpointsByProfile, g_steal_pointer(&endpoints));
    expect_function_call(__wrap_deviceServiceGetEndpointsByProfile);

    g_autolist(BDeviceServiceEndpoint) result4 = b_device_service_client_get_endpoints_by_profile(client, profileId);
    assert_non_null(result4);
    assert_int_equal(g_list_length(result4), linkedListCount(endpointsClone));

    const GList *curr = result4;
    scoped_icLinkedListIterator *iter = linkedListIteratorCreate(endpointsClone);
    while (linkedListIteratorHasNext(iter))
    {
        BDeviceServiceEndpoint *dsEndpoint = (BDeviceServiceEndpoint *) curr->data;
        icDeviceEndpoint *serviceEndpoint = linkedListIteratorGetNext(iter);

        g_autofree gchar *dsEndpointId = NULL;
        g_object_get(dsEndpoint,
                     B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_ID],
                     &dsEndpointId,
                     NULL);

        assert_string_equal(serviceEndpoint->id, dsEndpointId);

        curr = curr->next;
    }

    linkedListDestroy(endpointsClone, (linkedListItemFreeFunc) endpointDestroy);
}

static void discoverStartCommon(void **state, bool forRecovery)
{
    BDeviceServiceClient *client = *state;
    const char *deviceClass = "deviceClass";
    g_autoptr(GList) deviceClasses = g_list_append(NULL, (gpointer) deviceClass);
    BDeviceServiceDiscoveryFilter *filter = b_device_service_discovery_filter_new();
    g_object_set(filter,
                 B_DEVICE_SERVICE_DISCOVERY_FILTER_PROPERTY_NAMES[B_DEVICE_SERVICE_DISCOVERY_FILTER_PROP_URI],
                 "uri",
                 B_DEVICE_SERVICE_DISCOVERY_FILTER_PROPERTY_NAMES[B_DEVICE_SERVICE_DISCOVERY_FILTER_PROP_VALUE],
                 "value",
                 NULL);
    g_autolist(BDeviceServiceDiscoveryFilter) filters = g_list_append(NULL, (gpointer) filter);
    uint16_t timeout = 10;
    GError *error = NULL;

    // NULL client, valid deviceClasses, valid filters, valid timeout, valid error
    bool result1;
    if (forRecovery)
    {
        result1 = b_device_service_client_recover_start(NULL, deviceClasses, filters, timeout, &error);
    }
    else
    {
        result1 = b_device_service_client_discover_start(NULL, deviceClasses, filters, timeout, &error);
    }
    assert_false(result1);
    assert_null(error);

    // valid client, NULL deviceClasses, valid filters, valid timeout, valid error
    bool result2;
    if (forRecovery)
    {
        result2 = b_device_service_client_recover_start(client, NULL, filters, timeout, &error);
    }
    else
    {
        result2 = b_device_service_client_discover_start(client, NULL, filters, timeout, &error);
    }
    assert_false(result2);
    assert_null(error);

    // valid client, valid deviceClasses, NULL filters (valid), valid timeout, valid error
    expect_function_call(__wrap_deviceServiceDiscoverStart);
    expect_check(
        __wrap_deviceServiceDiscoverStart, deviceClasses, (CheckParameterValue) checkListContents, deviceClasses);
    expect_check(
        __wrap_deviceServiceDiscoverStart, filters, (CheckParameterValue) checkDiscoveryFilterListContents, NULL);
    expect_value(__wrap_deviceServiceDiscoverStart, timeoutSeconds, timeout);
    expect_value(__wrap_deviceServiceDiscoverStart, findOrphanedDevices, forRecovery);
    will_return(__wrap_deviceServiceDiscoverStart, true);

    bool result3;
    if (forRecovery)
    {
        result3 = b_device_service_client_recover_start(client, deviceClasses, NULL, timeout, &error);
    }
    else
    {
        result3 = b_device_service_client_discover_start(client, deviceClasses, NULL, timeout, &error);
    }
    assert_true(result3);
    assert_null(error);

    // valid client, valid deviceClasses, valid filters, 0 timeout (valid), valid error
    expect_function_call(__wrap_deviceServiceDiscoverStart);
    expect_check(
        __wrap_deviceServiceDiscoverStart, deviceClasses, (CheckParameterValue) checkListContents, deviceClasses);
    expect_check(
        __wrap_deviceServiceDiscoverStart, filters, (CheckParameterValue) checkDiscoveryFilterListContents, filters);
    expect_value(__wrap_deviceServiceDiscoverStart, timeoutSeconds, 0);
    expect_value(__wrap_deviceServiceDiscoverStart, findOrphanedDevices, forRecovery);
    will_return(__wrap_deviceServiceDiscoverStart, true);

    bool result4;
    if (forRecovery)
    {
        result4 = b_device_service_client_recover_start(client, deviceClasses, filters, 0, &error);
    }
    else
    {
        result4 = b_device_service_client_discover_start(client, deviceClasses, filters, 0, &error);
    }
    assert_true(result4);
    assert_null(error);

    // valid client, valid deviceClasses, valid filters, valid timeout, NULL error (valid, client ignoring)
    expect_function_call(__wrap_deviceServiceDiscoverStart);
    expect_check(
        __wrap_deviceServiceDiscoverStart, deviceClasses, (CheckParameterValue) checkListContents, deviceClasses);
    expect_check(
        __wrap_deviceServiceDiscoverStart, filters, (CheckParameterValue) checkDiscoveryFilterListContents, filters);
    expect_value(__wrap_deviceServiceDiscoverStart, timeoutSeconds, timeout);
    expect_value(__wrap_deviceServiceDiscoverStart, findOrphanedDevices, forRecovery);
    will_return(__wrap_deviceServiceDiscoverStart, true);

    bool result5;
    if (forRecovery)
    {
        result5 = b_device_service_client_recover_start(client, deviceClasses, filters, timeout, NULL);
    }
    else
    {
        result5 = b_device_service_client_discover_start(client, deviceClasses, filters, timeout, NULL);
    }
    assert_true(result5);
}

static void test_b_device_service_client_discover_start(void **state)
{
    discoverStartCommon(state, false);
}

static void test_b_device_service_client_recover_start(void **state)
{
    discoverStartCommon(state, true);
}

static void test_b_device_service_client_discover_stop(void **state)
{
    BDeviceServiceClient *client = *state;
    const char *deviceClass = "deviceClass";
    g_autoptr(GList) deviceClasses = g_list_append(NULL, (gpointer) deviceClass);

    // NULL client, valid deviceClasses
    bool result1 = b_device_service_client_discover_stop(NULL, deviceClasses);
    assert_false(result1);

    // valid client, NULL deviceClasses
    expect_function_call(__wrap_deviceServiceDiscoverStop);
    expect_check(__wrap_deviceServiceDiscoverStop, deviceClasses, (CheckParameterValue) checkListContents, NULL);
    will_return(__wrap_deviceServiceDiscoverStop, true);
    bool result2 = b_device_service_client_discover_stop(client, NULL);
    assert_true(result2);

    // valid client, valid deviceClasses
    expect_function_call(__wrap_deviceServiceDiscoverStop);
    expect_check(
        __wrap_deviceServiceDiscoverStop, deviceClasses, (CheckParameterValue) checkListContents, deviceClasses);
    will_return(__wrap_deviceServiceDiscoverStop, true);
    bool result3 = b_device_service_client_discover_stop(client, deviceClasses);
    assert_true(result3);

    // internal error (false return)
    expect_function_call(__wrap_deviceServiceDiscoverStop);
    expect_check(
        __wrap_deviceServiceDiscoverStop, deviceClasses, (CheckParameterValue) checkListContents, deviceClasses);
    will_return(__wrap_deviceServiceDiscoverStop, false);
    bool result4 = b_device_service_client_discover_stop(client, deviceClasses);
    assert_false(result4);
}

static void test_b_device_service_client_recover_stop(void **state)
{
    BDeviceServiceClient *client = *state;
    const char *deviceClass = "deviceClass";
    g_autoptr(GList) deviceClasses = g_list_append(NULL, (gpointer) deviceClass);

    // NULL client, valid deviceClasses
    bool result1 = b_device_service_client_recover_stop(NULL, deviceClasses);
    assert_false(result1);

    // valid client, NULL deviceClasses
    expect_function_call(__wrap_deviceServiceDiscoverStop);
    expect_check(__wrap_deviceServiceDiscoverStop, deviceClasses, (CheckParameterValue) checkListContents, NULL);
    will_return(__wrap_deviceServiceDiscoverStop, true);
    bool result2 = b_device_service_client_recover_stop(client, NULL);
    assert_true(result2);

    // valid client, valid deviceClasses
    expect_function_call(__wrap_deviceServiceDiscoverStop);
    expect_check(
        __wrap_deviceServiceDiscoverStop, deviceClasses, (CheckParameterValue) checkListContents, deviceClasses);
    will_return(__wrap_deviceServiceDiscoverStop, true);
    bool result3 = b_device_service_client_recover_stop(client, deviceClasses);
    assert_true(result3);

    // internal error (false return)
    expect_function_call(__wrap_deviceServiceDiscoverStop);
    expect_check(
        __wrap_deviceServiceDiscoverStop, deviceClasses, (CheckParameterValue) checkListContents, deviceClasses);
    will_return(__wrap_deviceServiceDiscoverStop, false);
    bool result4 = b_device_service_client_recover_stop(client, deviceClasses);
    assert_false(result4);
}

static void test_b_device_service_client_commission_device(void **state)
{
    BDeviceServiceClient *client = *state;
    char *setupPayload = "setupPayload";
    uint16_t timeout = 10;
    GError *error = NULL;

    // NULL client, valid setupPayload, valid timeout, valid error
    bool result1 = b_device_service_client_commission_device(NULL, setupPayload, timeout, &error);
    assert_false(result1);
    assert_null(error);

    // valid client, NULL setupPayload, valid timeout, valid error
    bool result2 = b_device_service_client_commission_device(client, NULL, timeout, &error);
    assert_false(result2);
    assert_null(error);

    // valid client, valid setupPayload, 0 timeout (valid), valid error
    expect_function_call(__wrap_deviceServiceCommissionDevice);
    expect_string(__wrap_deviceServiceCommissionDevice, setupPayload, setupPayload);
    expect_value(__wrap_deviceServiceCommissionDevice, timeoutSeconds, 0);
    will_return(__wrap_deviceServiceCommissionDevice, true);

    bool result3 = b_device_service_client_commission_device(client, setupPayload, 0, &error);
    assert_true(result3);
    assert_null(error);

    // valid client, valid setupPayload, valid timeout, NULL error (valid, client ignoring)
    expect_function_call(__wrap_deviceServiceCommissionDevice);
    expect_string(__wrap_deviceServiceCommissionDevice, setupPayload, setupPayload);
    expect_value(__wrap_deviceServiceCommissionDevice, timeoutSeconds, timeout);
    will_return(__wrap_deviceServiceCommissionDevice, true);

    bool result4 = b_device_service_client_commission_device(client, setupPayload, timeout, NULL);
    assert_true(result4);
}

static void test_b_device_service_client_add_matter_device(void **state)
{
    BDeviceServiceClient *client = *state;
    uint64_t nodeId = 1234;
    uint16_t timeout = 10;
    GError *error = NULL;

    // NULL client, valid nodeId, valid timeout, valid error
    bool result1 = b_device_service_client_add_matter_device(NULL, nodeId, timeout, &error);
    assert_false(result1);
    assert_null(error);

    // valid client, 0 nodeId (valid), valid timeout, valid error
    expect_function_call(__wrap_deviceServiceAddMatterDevice);
    expect_value(__wrap_deviceServiceAddMatterDevice, nodeId, 0);
    expect_value(__wrap_deviceServiceAddMatterDevice, timeoutSeconds, timeout);
    will_return(__wrap_deviceServiceAddMatterDevice, true);

    bool result2 = b_device_service_client_add_matter_device(client, 0, timeout, &error);
    assert_true(result2);
    assert_null(error);

    // valid client, valid nodeId, 0 timeout (valid), valid error
    expect_function_call(__wrap_deviceServiceAddMatterDevice);
    expect_value(__wrap_deviceServiceAddMatterDevice, nodeId, nodeId);
    expect_value(__wrap_deviceServiceAddMatterDevice, timeoutSeconds, 0);
    will_return(__wrap_deviceServiceAddMatterDevice, true);

    bool result3 = b_device_service_client_add_matter_device(client, nodeId, 0, &error);
    assert_true(result3);
    assert_null(error);

    // valid client, valid nodeId, valid timeout, NULL error (valid, client ignoring)
    expect_function_call(__wrap_deviceServiceAddMatterDevice);
    expect_value(__wrap_deviceServiceAddMatterDevice, nodeId, nodeId);
    expect_value(__wrap_deviceServiceAddMatterDevice, timeoutSeconds, timeout);
    will_return(__wrap_deviceServiceAddMatterDevice, true);

    bool result4 = b_device_service_client_add_matter_device(client, nodeId, timeout, NULL);
    assert_true(result4);
}

static void test_b_device_service_client_get_device_by_id(void **state)
{
    BDeviceServiceClient *client = *state;

    // NULL client, valid id
    g_autoptr(BDeviceServiceDevice) result1 = b_device_service_client_get_device_by_id(NULL, "id");
    assert_null(result1);

    // valid client, NULL id
    g_autoptr(BDeviceServiceDevice) result2 = b_device_service_client_get_device_by_id(client, NULL);
    assert_null(result2);

    // valid client, valid id
    const char *id = "id";
    icDevice *device = calloc(1, sizeof(icDevice));
    device->uuid = strdup(id);

    expect_function_call(__wrap_deviceServiceGetDevice);
    expect_string(__wrap_deviceServiceGetDevice, uuid, id);
    will_return(__wrap_deviceServiceGetDevice, g_steal_pointer(&device));

    g_autoptr(BDeviceServiceDevice) result3 = b_device_service_client_get_device_by_id(client, id);
    assert_non_null(result3);

    g_autofree gchar *resultId = NULL;
    g_object_get(result3, B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_UUID], &resultId, NULL);
    assert_string_equal(resultId, id);
}

static void test_b_device_service_client_get_endpoint_by_id(void **state)
{
    BDeviceServiceClient *client = *state;

    const gchar *deviceUuid = "deviceUuid";
    const gchar *endpointId = "endpointId";

    // NULL client, valid deviceUuid, valid endpointId
    g_autoptr(BDeviceServiceEndpoint) result1 =
        b_device_service_client_get_endpoint_by_id(NULL, deviceUuid, endpointId);
    assert_null(result1);

    // valid client, NULL deviceUuid, valid endpointId
    g_autoptr(BDeviceServiceEndpoint) result2 = b_device_service_client_get_endpoint_by_id(client, NULL, endpointId);
    assert_null(result2);

    // valid client, valid deviceUuid, NULL endpointId
    g_autoptr(BDeviceServiceEndpoint) result3 = b_device_service_client_get_endpoint_by_id(client, deviceUuid, NULL);
    assert_null(result3);

    // valid client, valid deviceUuid, valid endpointId
    icDeviceEndpoint *endpoint = calloc(1, sizeof(icDeviceEndpoint));
    endpoint->id = strdup(endpointId);
    endpoint->deviceUuid = strdup(deviceUuid);

    expect_function_call(__wrap_deviceServiceGetEndpointById);
    expect_string(__wrap_deviceServiceGetEndpointById, deviceUuid, deviceUuid);
    expect_string(__wrap_deviceServiceGetEndpointById, endpointId, endpointId);
    will_return(__wrap_deviceServiceGetEndpointById, g_steal_pointer(&endpoint));

    g_autoptr(BDeviceServiceEndpoint) result4 =
        b_device_service_client_get_endpoint_by_id(client, deviceUuid, endpointId);
    assert_non_null(result4);

    g_autofree gchar *resultDeviceUuid = NULL;
    g_autofree gchar *resultEndpointId = NULL;
    g_object_get(result4,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_DEVICE_UUID],
                 &resultDeviceUuid,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_ID],
                 &resultEndpointId,
                 NULL);

    assert_string_equal(resultDeviceUuid, deviceUuid);
    assert_string_equal(resultEndpointId, endpointId);
}

static void test_b_device_service_client_get_endpoint_by_uri(void **state)
{
    BDeviceServiceClient *client = *state;

    const gchar *uri = "some/uri";

    // NULL client, NULL uri
    g_autoptr(BDeviceServiceEndpoint) result1 = b_device_service_client_get_endpoint_by_uri(NULL, NULL);
    assert_null(result1);

    // valid client, NULL uri
    g_autoptr(BDeviceServiceEndpoint) result2 = b_device_service_client_get_endpoint_by_uri(client, NULL);
    assert_null(result2);

    // NULL client, valid uri
    g_autoptr(BDeviceServiceEndpoint) result3 = b_device_service_client_get_endpoint_by_uri(NULL, uri);
    assert_null(result3);

    icDeviceEndpoint *endpoint = calloc(1, sizeof(icDeviceEndpoint));
    endpoint->uri = strdup(uri);

    expect_function_call(__wrap_deviceServiceGetEndpointByUri);
    expect_string(__wrap_deviceServiceGetEndpointByUri, uri, uri);
    will_return(__wrap_deviceServiceGetEndpointByUri, g_steal_pointer(&endpoint));

    // valid client, valid uri
    g_autoptr(BDeviceServiceEndpoint) result4 = b_device_service_client_get_endpoint_by_uri(client, uri);
    assert_non_null(result4);

    g_autofree gchar *resultUri = NULL;
    g_object_get(
        result4, B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_URI], &resultUri, NULL);

    assert_string_equal(resultUri, uri);
}

static void test_b_device_service_client_remove_endpoint_by_id(void **state)
{
    BDeviceServiceClient *client = *state;

    const gchar *deviceUuid = "deviceUuid";
    const gchar *endpointId = "endpointId";
    gboolean result = FALSE;

    // NULL client, NULL deviceUuid, NULL endpointId
    result = b_device_service_client_remove_endpoint_by_id(NULL, NULL, NULL);
    assert_false(result);

    // NULL client, valid deviceUuid, NULL endpointId
    result = b_device_service_client_remove_endpoint_by_id(NULL, deviceUuid, NULL);
    assert_false(result);

    // NULL client, NULL deviceUuid, valid endpointId
    result = b_device_service_client_remove_endpoint_by_id(NULL, NULL, endpointId);
    assert_false(result);

    // NULL client, valid deviceUuid, valid endpointId
    result = b_device_service_client_remove_endpoint_by_id(NULL, deviceUuid, endpointId);
    assert_false(result);

    // valid client, NULL deviceUuid, NULL endpointId
    result = b_device_service_client_remove_endpoint_by_id(client, NULL, NULL);
    assert_false(result);

    // valid client, NULL deviceUuid, valid endpointId
    result = b_device_service_client_remove_endpoint_by_id(client, NULL, endpointId);
    assert_false(result);

    // valid client, valid deviceUuid, NULL endpointId
    result = b_device_service_client_remove_endpoint_by_id(client, deviceUuid, NULL);
    assert_false(result);

    // Expecting internal error (false return)
    expect_function_call(__wrap_deviceServiceRemoveEndpointById);
    expect_string(__wrap_deviceServiceRemoveEndpointById, deviceUuid, deviceUuid);
    expect_string(__wrap_deviceServiceRemoveEndpointById, endpointId, endpointId);
    will_return(__wrap_deviceServiceRemoveEndpointById, false);

    // valid client, valid deviceUuid, valid endpointId
    result = b_device_service_client_remove_endpoint_by_id(client, deviceUuid, endpointId);
    assert_false(result);

    // Expecting success (true return)
    expect_function_call(__wrap_deviceServiceRemoveEndpointById);
    expect_string(__wrap_deviceServiceRemoveEndpointById, deviceUuid, deviceUuid);
    expect_string(__wrap_deviceServiceRemoveEndpointById, endpointId, endpointId);
    will_return(__wrap_deviceServiceRemoveEndpointById, true);

    // valid client, valid deviceUuid, valid endpointId
    result = b_device_service_client_remove_endpoint_by_id(client, deviceUuid, endpointId);
    assert_true(result);
}

static void test_b_device_service_client_get_devices(void **state)
{
    BDeviceServiceClient *client = *state;
    icLinkedList *devices = linkedListCreate();
    const char *id = "test-id";

    // NULL client
    g_autolist(BDeviceServiceDevice) result1 = b_device_service_client_get_devices(NULL);
    assert_null(result1);

    // Valid client, NULL devices list
    will_return(__wrap_deviceServiceGetAllDevices, NULL);
    expect_function_call(__wrap_deviceServiceGetAllDevices);

    g_autolist(BDeviceServiceDevice) result2 = b_device_service_client_get_devices(client);
    assert_null(result2);

    // Valid client, Empty devices list
    will_return(__wrap_deviceServiceGetAllDevices, g_steal_pointer(&devices));
    expect_function_call(__wrap_deviceServiceGetAllDevices);

    g_autolist(BDeviceServiceDevice) result3 = b_device_service_client_get_devices(client);
    assert_null(result3);

    if (devices == NULL)
    {
        devices = linkedListCreate();
    }
    icDevice *device = calloc(1, sizeof(icDevice));
    device->uuid = strdup(id);
    linkedListAppend(devices, g_steal_pointer(&device));

    icLinkedList *devicesClone = linkedListDeepClone(devices, icDevicesLinkedListCloneFunction, NULL);
    will_return(__wrap_deviceServiceGetAllDevices, g_steal_pointer(&devices));
    expect_function_call(__wrap_deviceServiceGetAllDevices);

    g_autolist(BDeviceServiceDevice) result4 = b_device_service_client_get_devices(client);
    assert_non_null(result4);
    assert_int_equal(g_list_length(result4), linkedListCount(devicesClone));

    const GList *curr = result4;
    scoped_icLinkedListIterator *iter = linkedListIteratorCreate(devicesClone);
    while (linkedListIteratorHasNext(iter))
    {
        if (curr->data)
        {
            BDeviceServiceDevice *self = (BDeviceServiceDevice *) curr->data;
            icDevice *icdevice = linkedListIteratorGetNext(iter);

            gchar *dsDeviceid = NULL;
            if (self)
            {
                g_object_get(
                    self, B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_UUID], &dsDeviceid, NULL);
            }

            g_assert_cmpstr(icdevice->uuid, ==, dsDeviceid);
            g_free(dsDeviceid);

            curr = curr->next;
        }
    }

    linkedListDestroy(devicesClone, (linkedListItemFreeFunc) deviceDestroy);
}


static void test_b_device_service_client_get_devices_by_device_class(void **state)
{
    BDeviceServiceClient *client = *state;
    icLinkedList *devices = linkedListCreate();
    const char *deviceClass = "test-deviceClass";

    // NULL client, Valid deviceClass
    g_autolist(BDeviceServiceDevice) result1 = b_device_service_client_get_devices_by_device_class(NULL, deviceClass);
    assert_null(result1);

    // valid client, NULL deviceClass
    g_autolist(BDeviceServiceDevice) result2 = b_device_service_client_get_devices_by_device_class(client, NULL);
    assert_null(result2);

    // valid client, valid deviceClass
    icDevice *device = calloc(1, sizeof(icDevice));
    device->deviceClass = strdup(deviceClass);
    linkedListAppend(devices, g_steal_pointer(&device));

    icLinkedList *devicesClone = linkedListDeepClone(devices, icDevicesLinkedListCloneFunction, NULL);

    expect_function_call(__wrap_deviceServiceGetDevicesByDeviceClass);
    expect_string(__wrap_deviceServiceGetDevicesByDeviceClass, deviceClass, deviceClass);
    will_return(__wrap_deviceServiceGetDevicesByDeviceClass, g_steal_pointer(&devices));

    g_autolist(BDeviceServiceDevice) result3 = b_device_service_client_get_devices_by_device_class(client, deviceClass);
    assert_non_null(result3);
    assert_int_equal(g_list_length(result3), linkedListCount(devicesClone));

    const GList *curr = result3;
    scoped_icLinkedListIterator *iter = linkedListIteratorCreate(devicesClone);
    while (linkedListIteratorHasNext(iter))
    {
        if (curr->data)
        {
            BDeviceServiceDevice *self = (BDeviceServiceDevice *) curr->data;
            icDevice *icdevice = linkedListIteratorGetNext(iter);

            gchar *dsDeviceClass = NULL;
            if (self)
            {
                g_object_get(self,
                             B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS],
                             &dsDeviceClass,
                             NULL);
            }

            g_assert_cmpstr(icdevice->deviceClass, ==, dsDeviceClass);
            g_free(dsDeviceClass);

            curr = curr->next;
        }
    }

    linkedListDestroy(devicesClone, (linkedListItemFreeFunc) deviceDestroy);
}

static void test_b_device_service_client_set_system_property(void **state)
{
    BDeviceServiceClient *client = *state;
    char *name = "testPropertyName";
    char *value = "testValue";
    gboolean result = FALSE;

    // NULL client, NULL property name, NULL property value.
    result = b_device_service_client_set_system_property(NULL, NULL, NULL);
    assert_false(result);

    // NULL client, NULL property name, valid property value.
    result = b_device_service_client_set_system_property(NULL, NULL, value);
    assert_false(result);

    // NULL client, valid property name, NULL property value.
    result = b_device_service_client_set_system_property(NULL, name, NULL);
    assert_false(result);

    // NULL client, valid property name, valid property value.
    result = b_device_service_client_set_system_property(NULL, name, value);
    assert_false(result);

    // valid client, NULL property name, NULL property value.
    result = b_device_service_client_set_system_property(client, NULL, NULL);
    assert_false(result);

    // valid client, NULL property name, valid property value.
    result = b_device_service_client_set_system_property(client, NULL, value);
    assert_false(result);

    // valid client, valid property name, NULL property value.
    result = b_device_service_client_set_system_property(client, name, NULL);
    assert_false(result);

    // internal error (false return)
    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, name, name);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, value);
    will_return(__wrap_deviceServiceSetSystemProperty, false);

    // valid client, valid property name, valid property value.
    result = b_device_service_client_set_system_property(client, name, value);
    assert_false(result);

    // expect success (true return)
    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, name, name);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, value);
    will_return(__wrap_deviceServiceSetSystemProperty, true);

    // valid client, valid property name, valid property value.
    result = b_device_service_client_set_system_property(client, name, value);
    assert_true(result);
}

static void test_b_device_service_process_device_descriptors(void **state)
{
    BDeviceServiceClient *client = *state;

    // NULL client
    b_device_service_process_device_descriptors(NULL);

    // Expected function call on valid client.
    expect_function_call(__wrap_deviceServiceProcessDeviceDescriptors);

    // valid client
    b_device_service_process_device_descriptors(client);
}

static char *buildDefaultDeviceServiceStatusJson()
{
    scoped_cJSON *json = cJSON_CreateObject();
    if (json != NULL)
    {
        cJSON_AddArrayToObject(json, "supportedDeviceClasses");
        cJSON_AddFalseToObject(json, "discoveryRunning");
        cJSON_AddArrayToObject(json, "discoveringDeviceClasses");
        cJSON_AddNumberToObject(json, "discoveryTimeoutSeconds", 0);
        cJSON_AddFalseToObject(json, "findingOrphanedDevices");
        cJSON_AddFalseToObject(json, "isReadyForDeviceOperation");
        cJSON_AddFalseToObject(json, "isReadyForPairing");
        cJSON_AddArrayToObject(json, "subsystems");
    }

    return cJSON_Print(json);
}

static void test_b_device_service_client_get_status(void **state)
{
    BDeviceServiceClient *client = *state;

    // NULL client
    g_autoptr(BDeviceServiceStatus) result1 = b_device_service_client_get_status(NULL);
    assert_null(result1);

    scoped_DeviceServiceStatus *dsStatus = calloc(1, sizeof(DeviceServiceStatus));
    expect_function_call(__wrap_deviceServiceGetStatus);
    will_return(__wrap_deviceServiceGetStatus, dsStatus);

    // valid client
    g_autoptr(BDeviceServiceStatus) result2 = b_device_service_client_get_status(client);
    assert_non_null(result2);

    g_autoptr(BDeviceServiceStatus) expectedStatus = b_device_service_status_new();

    // When a BDeviceServiceStatus object is created, it sets the JSON property to NULL, so create the
    // expected JSON string here
    g_autofree gchar *defaultJsonStr = buildDefaultDeviceServiceStatusJson();
    g_object_set(expectedStatus,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_JSON],
                 defaultJsonStr,
                 NULL);

    bool result = checkBDeviceServiceStatusContents(expectedStatus, result2);
    assert_true(result);
}

static void test_b_device_service_client_read_resource(void **state)
{
    BDeviceServiceClient *client = *state;

    const gchar *uri = "some/uri";
    GError *err = NULL;
    gchar *result = NULL;

    // NULL client, NULL uri
    result = b_device_service_client_read_resource(NULL, NULL, &err);
    assert_null(result);

    // NULL client, valid uri
    result = b_device_service_client_read_resource(NULL, uri, &err);
    assert_null(result);

    // Valid client, invalid uri
    result = b_device_service_client_read_resource(client, NULL, &err);
    assert_null(result);

    // test a non-readable resource
    icDeviceResource *resource = calloc(1, sizeof(icDeviceResource));
    resource->mode = RESOURCE_MODE_WRITEABLE;

    expect_function_call(__wrap_deviceServiceGetResourceByUri);
    expect_string(__wrap_deviceServiceGetResourceByUri, uri, uri);
    will_return(__wrap_deviceServiceGetResourceByUri, resource);

    result = b_device_service_client_read_resource(client, uri, &err);
    assert_null(result);
    assert_int_equal(err->code, RESOURCE_NOT_READABLE);

    // test a readable resource
    gchar *expectedValue = "a/wonderful/value";
    resource = calloc(1, sizeof(icDeviceResource));
    resource->mode = RESOURCE_MODE_READABLE;
    resource->value = g_strdup(expectedValue);

    expect_function_call(__wrap_deviceServiceGetResourceByUri);
    expect_string(__wrap_deviceServiceGetResourceByUri, uri, uri);
    will_return(__wrap_deviceServiceGetResourceByUri, resource);

    result = b_device_service_client_read_resource(client, uri, &err);
    assert_non_null(result);
    assert_string_equal(result, expectedValue);
    free(result);
    g_error_free(err);
}

static void test_b_device_service_client_get_devices_by_subsystem(void **state)
{
    BDeviceServiceClient *client = *state;
    icLinkedList *devices = linkedListCreate();
    const char *subsystem = "dummy-subsystem";
    const char *dummyDriver = "dummy-driver";

    // NULL client, Valid subsystem
    g_autolist(BDeviceServiceDevice) result1 = b_device_service_client_get_devices_by_subsystem(NULL, subsystem);
    assert_null(result1);

    // valid client, NULL subsystem
    g_autolist(BDeviceServiceDevice) result2 = b_device_service_client_get_devices_by_subsystem(client, NULL);
    assert_null(result2);

    // valid client, valid subsystem
    icDevice *device = calloc(1, sizeof(icDevice));
    device->managingDeviceDriver = strdup(dummyDriver);
    linkedListAppend(devices, g_steal_pointer(&device));

    icLinkedList *devicesClone = linkedListDeepClone(devices, icDevicesLinkedListCloneFunction, NULL);

    expect_function_call(__wrap_deviceServiceGetDevicesBySubsystem);
    expect_string(__wrap_deviceServiceGetDevicesBySubsystem, subsystem, subsystem);
    will_return(__wrap_deviceServiceGetDevicesBySubsystem, g_steal_pointer(&devices));

    g_autolist(BDeviceServiceDevice) result3 = b_device_service_client_get_devices_by_subsystem(client, subsystem);
    assert_non_null(result3);
    assert_int_equal(g_list_length(result3), linkedListCount(devicesClone));

    const GList *curr = result3;
    scoped_icLinkedListIterator *iter = linkedListIteratorCreate(devicesClone);
    while (linkedListIteratorHasNext(iter))
    {
        if (curr->data)
        {
            BDeviceServiceDevice *self = (BDeviceServiceDevice *) curr->data;
            icDevice *icdevice = linkedListIteratorGetNext(iter);

            gchar *dsDeviceManagingDriver = NULL;
            g_object_get(self,
                         B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_MANAGING_DEVICE_DRIVER],
                         &dsDeviceManagingDriver,
                         NULL);

            g_assert_cmpstr(icdevice->managingDeviceDriver, ==, dsDeviceManagingDriver);
            g_free(dsDeviceManagingDriver);

            curr = curr->next;
        }
    }

    linkedListDestroy(devicesClone, (linkedListItemFreeFunc) deviceDestroy);
}

static void test_b_device_service_client_get_device_by_uri(void **state)
{
    BDeviceServiceClient *client = *state;
    const char *uri = "testUri";

    icDevice *device = calloc(1, sizeof(icDevice));
    device->uri = strdup(uri);

    // NULL client, valid uri
    g_autoptr(BDeviceServiceDevice) result1 = b_device_service_client_get_device_by_uri(NULL, uri);
    assert_null(result1);

    // valid client, NULL uri
    g_autoptr(BDeviceServiceDevice) result2 = b_device_service_client_get_device_by_uri(client, NULL);
    assert_null(result2);

    // valid client, valid uri
    expect_function_call(__wrap_deviceServiceGetDeviceByUri);
    expect_string(__wrap_deviceServiceGetDeviceByUri, uri, uri);
    will_return(__wrap_deviceServiceGetDeviceByUri, g_steal_pointer(&device));

    g_autoptr(BDeviceServiceDevice) result3 = b_device_service_client_get_device_by_uri(client, uri);
    assert_non_null(result3);

    g_autofree gchar *resultUri = NULL;
    g_object_get(result3, B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_URI], &resultUri, NULL);
    assert_string_equal(resultUri, uri);
}

static void test_b_device_service_client_get_resource_by_uri(void **state)
{
    BDeviceServiceClient *client = *state;
    const char *uri = "testUri";

    icDeviceResource *deviceResource = calloc(1, sizeof(icDeviceResource));
    deviceResource->uri = strdup(uri);

    // NULL client, valid uri
    g_autoptr(BDeviceServiceResource) result1 = b_device_service_client_get_resource_by_uri(NULL, uri);
    assert_null(result1);

    // valid client, NULL uri
    g_autoptr(BDeviceServiceResource) result2 = b_device_service_client_get_resource_by_uri(client, NULL);
    assert_null(result2);

    // valid client, valid uri
    expect_function_call(__wrap_deviceServiceGetResourceByUri);
    expect_string(__wrap_deviceServiceGetResourceByUri, uri, uri);
    will_return(__wrap_deviceServiceGetResourceByUri, g_steal_pointer(&deviceResource));

    g_autoptr(BDeviceServiceResource) result3 = b_device_service_client_get_resource_by_uri(client, uri);
    assert_non_null(result3);

    g_autofree gchar *resultUri = NULL;
    g_object_get(
        result3, B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_URI], &resultUri, NULL);
    assert_string_equal(resultUri, uri);
}

static void test_b_device_service_client_write_resource(void **state)
{
    BDeviceServiceClient *client = *state;
    char *uri = "testUri";
    char *resourceValue = "testValue";

    // NULL client, valid uri, valid resource value
    bool result1 = b_device_service_client_write_resource(NULL, uri, resourceValue);
    assert_false(result1);

    // valid client, NULL uri, valid resource value
    bool result2 = b_device_service_client_write_resource(client, NULL, resourceValue);
    assert_false(result2);

    // valid client, valid uri, NULL resource value
    bool result3 = b_device_service_client_write_resource(client, uri, NULL);
    assert_false(result3);

    // valid client, valid uri, valid resource value
    expect_function_call(__wrap_deviceServiceWriteResource);
    expect_string(__wrap_deviceServiceWriteResource, uri, uri);
    expect_string(__wrap_deviceServiceWriteResource, resourceValue, resourceValue);
    will_return(__wrap_deviceServiceWriteResource, true);

    bool result4 = b_device_service_client_write_resource(client, uri, resourceValue);
    assert_true(result4);

    // valid client, valid uri, valid resource value but failed to access the resource
    expect_function_call(__wrap_deviceServiceWriteResource);
    expect_string(__wrap_deviceServiceWriteResource, uri, uri);
    expect_string(__wrap_deviceServiceWriteResource, resourceValue, resourceValue);
    will_return(__wrap_deviceServiceWriteResource, false);

    bool result5 = b_device_service_client_write_resource(client, uri, resourceValue);
    assert_false(result5);
}

static void test_b_device_service_client_execute_resource(void **state)
{
    BDeviceServiceClient *client = *state;
    const char *uri = "testUri";
    const char *value = "testValue";
    char *response = NULL;

    // NULL client, valid uri, valid value
    bool result1 = b_device_service_client_execute_resource(NULL, uri, value, &response);
    assert_false(result1);
    assert_null(response);

    // valid client, NULL uri, valid value
    bool result2 = b_device_service_client_execute_resource(client, NULL, value, &response);
    assert_false(result2);
    assert_null(response);

    // valid client, valid uri, valid value
    expect_function_call(__wrap_deviceServiceExecuteResource);
    expect_string(__wrap_deviceServiceExecuteResource, uri, uri);
    will_return(__wrap_deviceServiceExecuteResource, true);

    bool result3 = b_device_service_client_execute_resource(client, uri, value, &response);
    assert_true(result3);

    // valid client, valid uri, No optional value
    expect_function_call(__wrap_deviceServiceExecuteResource);
    expect_string(__wrap_deviceServiceExecuteResource, uri, uri);
    will_return(__wrap_deviceServiceExecuteResource, true);

    bool result4 = b_device_service_client_execute_resource(client, uri, NULL, &response);
    assert_true(result4);

    // internal error (false return)
    expect_function_call(__wrap_deviceServiceExecuteResource);
    expect_string(__wrap_deviceServiceExecuteResource, uri, uri);
    will_return(__wrap_deviceServiceExecuteResource, false);

    bool result5 = b_device_service_client_execute_resource(client, uri, value, &response);
    assert_false(result5);
}

static void test_b_device_service_client_remove_device(void **state)
{
    BDeviceServiceClient *client = *state;
    const char *id = "test-id";

    // NULL client, valid id
    bool result1 = b_device_service_client_remove_device(NULL, id);
    assert_false(result1);

    // valid client, NULL id
    bool result2 = b_device_service_client_remove_device(client, NULL);
    assert_false(result2);

    // valid client, valid id
    expect_function_call(__wrap_deviceServiceRemoveDevice);
    expect_string(__wrap_deviceServiceRemoveDevice, uuid, id);
    will_return(__wrap_deviceServiceRemoveDevice, true);

    bool result3 = b_device_service_client_remove_device(client, id);
    assert_true(result3);
}

static void test_b_device_service_client_get_system_property(void **state)
{
    BDeviceServiceClient *client = *state;
    const char *key = "testPropName";
    char *expectedValue = "testPropValue";

    // NULL client, NULL property
    g_autofree gchar *result = b_device_service_client_get_system_property(NULL, NULL);
    assert_null(result);

    // NULL client, valid property
    g_autofree gchar *result1 = b_device_service_client_get_system_property(NULL, key);
    assert_null(result1);

    // valid client, NULL property
    g_autofree gchar *result2 = b_device_service_client_get_system_property(client, NULL);
    assert_null(result2);

    // valid client, valid property
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, key, key);
    will_return(__wrap_deviceServiceGetSystemProperty, strdup(expectedValue));

    g_autofree gchar *result3 = b_device_service_client_get_system_property(client, key);
    assert_non_null(result3);
    assert_string_equal(result3, expectedValue);
}

static void test_b_device_service_client_write_metadata(void **state)
{
    BDeviceServiceClient *client = *state;
    const char *uri = "testUri";
    const char *metadata = "testMetadata";
    gboolean result = FALSE;

    // NULL client, NULL uri, NULL metadata
    result = b_device_service_client_write_metadata(NULL, NULL, NULL);
    assert_false(result);

    // NULL client, NULL uri, valid metadata
    result = b_device_service_client_write_metadata(NULL, NULL, metadata);
    assert_false(result);

    // NULL client, valid uri, NULL metadata
    result = b_device_service_client_write_metadata(NULL, uri, NULL);
    assert_false(result);

    // NULL client, valid uri, valid metadata
    result = b_device_service_client_write_metadata(NULL, uri, metadata);
    assert_false(result);

    // valid client, NULL uri, NULL metadata
    result = b_device_service_client_write_metadata(client, NULL, NULL);
    assert_false(result);

    // valid client, NULL uri, valid metadata
    result = b_device_service_client_write_metadata(client, NULL, metadata);
    assert_false(result);

    // valid client, valid uri, NULL metadata
    result = b_device_service_client_write_metadata(client, uri, NULL);
    assert_false(result);

    // internal error (false return)
    expect_function_call(__wrap_deviceServiceSetMetadata);
    expect_string(__wrap_deviceServiceSetMetadata, uri, uri);
    expect_string(__wrap_deviceServiceSetMetadata, value, metadata);
    will_return(__wrap_deviceServiceSetMetadata, false);

    // valid client, valid uri, valid metadata
    result = b_device_service_client_write_metadata(client, uri, metadata);
    assert_false(result);

    // expect success (true return)
    expect_function_call(__wrap_deviceServiceSetMetadata);
    expect_string(__wrap_deviceServiceSetMetadata, uri, uri);
    expect_string(__wrap_deviceServiceSetMetadata, value, metadata);
    will_return(__wrap_deviceServiceSetMetadata, true);

    // valid client, valid uri, valid metadata
    result = b_device_service_client_write_metadata(client, uri, metadata);
    assert_true(result);
}

static void test_b_device_service_client_read_metadata(void **state)
{
    BDeviceServiceClient *client = *state;
    const char *uri = "testUri";
    char *expectedValue = "testMetadataValue";

    // NULL client, NULL uri
    g_autofree gchar *result = b_device_service_client_read_metadata(NULL, NULL);
    assert_null(result);

    // NULL client, valid uri
    g_autofree gchar *result1 = b_device_service_client_read_metadata(NULL, uri);
    assert_null(result1);

    // valid client, NULL uri
    g_autofree gchar *result2 = b_device_service_client_read_metadata(client, NULL);
    assert_null(result2);

    // valid client, valid uri
    expect_function_call(__wrap_deviceServiceGetMetadata);
    expect_string(__wrap_deviceServiceGetMetadata, uri, uri);
    will_return(__wrap_deviceServiceGetMetadata, strdup(expectedValue));

    g_autofree gchar *result3 = b_device_service_client_read_metadata(client, uri);
    assert_non_null(result3);
    assert_string_equal(result3, expectedValue);
}

static void test_b_device_service_client_query_resources_by_uri(void **state)
{
    BDeviceServiceClient *client = *state;
    char *uri = "testUri";
    icLinkedList *resources = linkedListCreate();

    // NULL client, NULL uri
    g_autolist(BDeviceServiceResource) result1 = b_device_service_client_query_resources_by_uri(NULL, NULL);
    assert_null(result1);

    // NULL client, valid uri
    g_autolist(BDeviceServiceResource) result2 = b_device_service_client_query_resources_by_uri(NULL, uri);
    assert_null(result2);

    // valid client, NULL uri
    g_autolist(BDeviceServiceResource) result3 = b_device_service_client_query_resources_by_uri(client, NULL);
    assert_null(result3);

    // valid client, valid uri
    icDeviceResource *resource = calloc(1, sizeof(icDeviceResource));
    resource->uri = strdup(uri);
    linkedListAppend(resources, g_steal_pointer(&resource));

    icLinkedList *resourcesClone = linkedListDeepClone(resources, icDeviceResourcesLinkedListCloneFunction, NULL);

    expect_function_call(__wrap_deviceServiceGetResourcesByUriPattern);
    expect_string(__wrap_deviceServiceGetResourcesByUriPattern, uri, uri);
    will_return(__wrap_deviceServiceGetResourcesByUriPattern, g_steal_pointer(&resources));

    g_autolist(BDeviceServiceResource) result4 = b_device_service_client_query_resources_by_uri(client, uri);
    assert_non_null(result4);
    assert_int_equal(g_list_length(result4), linkedListCount(resourcesClone));

    const GList *curr = result4;
    scoped_icLinkedListIterator *iter = linkedListIteratorCreate(resourcesClone);
    while (linkedListIteratorHasNext(iter))
    {
        if (curr->data)
        {
            BDeviceServiceResource *self = (BDeviceServiceResource *) curr->data;
            icDeviceResource *icResource = linkedListIteratorGetNext(iter);

            gchar *dsResourceUri = NULL;
            if (self)
            {
                g_object_get(self,
                             B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_URI],
                             &dsResourceUri,
                             NULL);
            }

            g_assert_cmpstr(icResource->uri, ==, dsResourceUri);
            g_free(dsResourceUri);

            curr = curr->next;
        }
    }

    linkedListDestroy(resourcesClone, (linkedListItemFreeFunc) resourceDestroy);
}

static void test_b_device_service_client_change_resource_mode(void **state)
{
    BDeviceServiceClient *client = *state;
    const char *uri = "testUri";
    const uint8_t newMode = 1;
    gboolean result = FALSE;

    // NULL client, NULL uri, valid newMode
    result = b_device_service_change_resource_mode(NULL, NULL, newMode);
    assert_false(result);

    // NULL client, valid uri, valid newMode
    result = b_device_service_change_resource_mode(NULL, uri, newMode);
    assert_false(result);

    // valid client, NULL uri, valid newMode
    result = b_device_service_change_resource_mode(client, NULL, newMode);
    assert_false(result);

    // internal error (false return)
    expect_function_call(__wrap_deviceServiceChangeResourceMode);
    expect_string(__wrap_deviceServiceChangeResourceMode, uri, uri);
    expect_value(__wrap_deviceServiceChangeResourceMode, newMode, newMode);
    will_return(__wrap_deviceServiceChangeResourceMode, false);

    // valid client, valid uri, valid newMode
    result = b_device_service_change_resource_mode(client, uri, newMode);
    assert_false(result);

    // expect success (true return)
    expect_function_call(__wrap_deviceServiceChangeResourceMode);
    expect_string(__wrap_deviceServiceChangeResourceMode, uri, uri);
    expect_value(__wrap_deviceServiceChangeResourceMode, newMode, newMode);
    will_return(__wrap_deviceServiceChangeResourceMode, true);

    // valid client, valid uri, valid newMode
    result = b_device_service_change_resource_mode(client, uri, newMode);
    assert_true(result);
}

#ifdef BARTON_CONFIG_ZIGBEE
static void test_b_device_service_client_change_zigbee_channel(void **state)
{
    BDeviceServiceClient *client = *state;
    uint8_t channel = 27;
    bool dryRun = false;

    // NULL client, valid channel.
    g_autoptr(GError) err = NULL;
    guint8 result1 = b_device_service_client_change_zigbee_channel(NULL, 16, false, &err);
    assert_int_equal(result1, 0);
    assert_null(err);
    g_clear_error(&err);


    ChannelChangeResponse channelChangeResponse = {.channelNumber = 27, .responseCode = channelChangeFailed};

    // Assert channel change failed error.
    assert_change_zigbee_channel_errors(client, channel, channelChangeResponse, ZIGBEE_CHANNEL_CHANGE_FAILED);

    // Assert channel change not allowed error.
    channelChangeResponse.responseCode = channelChangeNotAllowed;
    assert_change_zigbee_channel_errors(client, channel, channelChangeResponse, ZIGBEE_CHANNEL_CHANGE_NOT_ALLOWED);

    // Assert invalid channel number error.
    channelChangeResponse.responseCode = channelChangeInvalidChannel;
    assert_change_zigbee_channel_errors(client, channel, channelChangeResponse, ZIGBEE_CHANNEL_CHANGE_INVALID_CHANNEL);

    // Assert channel change already in progress error.
    channelChangeResponse.responseCode = channelChangeInProgress;
    assert_change_zigbee_channel_errors(client, channel, channelChangeResponse, ZIGBEE_CHANNEL_CHANGE_IN_PROGRESS);

    // Assert unable to calculate the channel error.
    channelChangeResponse.responseCode = channelChangeUnableToCalculate;
    assert_change_zigbee_channel_errors(
        client, channel, channelChangeResponse, ZIGBEE_CHANNEL_CHANGE_UNABLE_TO_CALCULATE);

    // Valid client, valid channel.
    channel = 16;
    channelChangeResponse.channelNumber = channel;
    channelChangeResponse.responseCode = channelChangeSuccess;
    expect_function_call(__wrap_zigbeeSubsystemChangeChannel);
    expect_value(__wrap_zigbeeSubsystemChangeChannel, channel, channel);
    expect_value(__wrap_zigbeeSubsystemChangeChannel, dryRun, dryRun);
    will_return(__wrap_zigbeeSubsystemChangeChannel, channelChangeResponse.channelNumber);
    will_return(__wrap_zigbeeSubsystemChangeChannel, channelChangeResponse.responseCode);

    err = NULL;
    guint8 result7 = b_device_service_client_change_zigbee_channel(client, 16, false, &err);
    assert_int_equal(result7, 16);
    assert_null(err);
    g_clear_error(&err);

    // Dry run (not a channel change).
    dryRun = true;
    channelChangeResponse.channelNumber = 0;
    channelChangeResponse.responseCode = channelChangeSuccess;
    expect_function_call(__wrap_zigbeeSubsystemChangeChannel);
    expect_value(__wrap_zigbeeSubsystemChangeChannel, channel, channel);
    expect_value(__wrap_zigbeeSubsystemChangeChannel, dryRun, dryRun);
    will_return(__wrap_zigbeeSubsystemChangeChannel, channelChangeResponse.channelNumber);
    will_return(__wrap_zigbeeSubsystemChangeChannel, channelChangeResponse.responseCode);

    err = NULL;
    guint8 result8 = b_device_service_client_change_zigbee_channel(client, 16, true, &err);
    assert_int_equal(result8, 0);
    assert_null(err);
    g_clear_error(&err);
}
#endif

static void test_b_device_service_client_get_metadata_by_uri(void **state)
{
    BDeviceServiceClient *client = *state;
    gchar *uriPattern = "testUriPattern";

    // NULL client, NULL uri pattern
    g_autolist(BDeviceServiceMetadata) result1 = b_device_service_client_get_metadata_by_uri(NULL, NULL);
    assert_null(result1);

    // valid client, NULL uri pattern
    g_autolist(BDeviceServiceMetadata) result2 = b_device_service_client_get_metadata_by_uri(client, NULL);
    assert_null(result2);

    // valid client, NULL uri pattern
    g_autolist(BDeviceServiceMetadata) result3 = b_device_service_client_get_metadata_by_uri(NULL, uriPattern);
    assert_null(result3);

    icLinkedList *metadatas = linkedListCreate();

    icDeviceMetadata *deviceMetadata = calloc(1, sizeof(icDeviceMetadata));
    char *expectedUri = "lebowski";
    deviceMetadata->uri = strdup(expectedUri);
    linkedListAppend(metadatas, g_steal_pointer(&deviceMetadata));

    // Clone the list as the original will be destroyed before comparison
    icLinkedList *metadatasClone = linkedListDeepClone(metadatas, icMetadataLinkedListCloneFunction, NULL);

    expect_function_call(__wrap_deviceServiceGetMetadataByUriPattern);
    expect_string(__wrap_deviceServiceGetMetadataByUriPattern, uri, uriPattern);
    will_return(__wrap_deviceServiceGetMetadataByUriPattern, g_steal_pointer(&metadatas));

    // valid client, valid uri
    g_autolist(BDeviceServiceMetadata) result4 = b_device_service_client_get_metadata_by_uri(client, uriPattern);
    assert_non_null(result4);
    assert_int_equal(g_list_length(result4), linkedListCount(metadatasClone));

    const GList *curr = result4;
    scoped_icLinkedListIterator *iter = linkedListIteratorCreate(metadatasClone);
    while (linkedListIteratorHasNext(iter))
    {
        if (curr->data)
        {
            BDeviceServiceMetadata *self = (BDeviceServiceMetadata *) curr->data;
            icDeviceMetadata *icMetadata = linkedListIteratorGetNext(iter);

            g_autofree gchar *dsUri = NULL;
            g_object_get(
                self, B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_URI], &dsUri, NULL);

            g_assert_cmpstr(icMetadata->uri, ==, dsUri);

            curr = curr->next;
        }
    }

    linkedListDestroy(metadatasClone, (linkedListItemFreeFunc) metadataDestroy);
}

#ifdef BARTON_CONFIG_ZIGBEE
static void test_b_device_service_client_zigbee_test(void **state)
{
    BDeviceServiceClient *client = *state;

    // NULL client
    g_autofree gchar *result1 = b_device_service_client_zigbee_test(NULL);
    assert_null(result1);

    // Create the expected JSON string
    scoped_cJSON *expectedJson = cJSON_CreateObject();
    cJSON_AddStringToObject(expectedJson, "someKey", "someValue");
    g_autofree gchar *expectedJsonStr = cJSON_Print(expectedJson);

    expect_function_call(__wrap_zhalTest);
    will_return(__wrap_zhalTest, strdup(expectedJsonStr));

    // valid client
    g_autofree gchar *result2 = b_device_service_client_zigbee_test(client);
    assert_non_null(result2);
    assert_string_equal(expectedJsonStr, result2);
}

static void test_b_device_service_client_zigbee_energy_scan(void **state)
{
    BDeviceServiceClient *client = *state;

    guint32 maxDuration = 50;
    guint32 scanCount = 1;

    // NULL client, NULL channelsToScan, valid maxDuration, valid scanCount
    GList *result1 = b_device_service_client_zigbee_energy_scan(NULL, NULL, maxDuration, scanCount);
    assert_null(result1);

    // NULL client, valid channelsToScan, valid maxDuration, valid scanCount
    GList *result2 = b_device_service_client_zigbee_energy_scan(NULL, NULL, maxDuration, scanCount);
    assert_null(result2);

    uint8_t expectedChannel = 11;
    uint8_t *channelsArray = (uint8_t *) malloc(1 * sizeof(uint8_t));
    channelsArray[0] = expectedChannel;

    zhalEnergyScanResult *scanResult = calloc(1, sizeof(zhalEnergyScanResult));
    scanResult->channel = expectedChannel;
    scanResult->maxRssi = -10;
    scanResult->minRssi = -80;
    scanResult->averageRssi = -40;
    scanResult->score = 50;

    icLinkedList *energyScanResults = linkedListCreate();
    linkedListAppend(energyScanResults, scanResult);

    // Clone the list since original will be destroyed before comparison
    icLinkedList *energyScanResultsClone =
        linkedListDeepClone(energyScanResults, zhalEnergyScanResultsLinkedListCloneFunction, NULL);

    // Create the GList to pass to the function under test
    GList *channelsToScan = NULL;
    channelsToScan = g_list_append(channelsToScan, GUINT_TO_POINTER(expectedChannel));

    ArrayCompareParams *arrayCompareParams = calloc(1, sizeof(ArrayCompareParams));
    arrayCompareParams->array = channelsArray;
    arrayCompareParams->arraySize = g_list_length(channelsToScan);

    expect_function_call(__wrap_zigbeeSubsystemPerformEnergyScan);
    expect_check(__wrap_zigbeeSubsystemPerformEnergyScan,
                 channelsToScan,
                 (CheckParameterValue) checkBDeviceServiceZigbeeEnergyScanResultChannelContents,
                 arrayCompareParams);
    expect_value(__wrap_zigbeeSubsystemPerformEnergyScan, numChannelsToScan, g_list_length(channelsToScan));
    expect_value(__wrap_zigbeeSubsystemPerformEnergyScan, scanDurationMillis, maxDuration);
    expect_value(__wrap_zigbeeSubsystemPerformEnergyScan, numScans, scanCount);
    will_return(__wrap_zigbeeSubsystemPerformEnergyScan, g_steal_pointer(&energyScanResults));

    // valid client, valid channelsToScan, valid maxDuration, valid scanCount
    g_autolist(BDeviceServiceZigbeeEnergyScanResult) result3 =
        b_device_service_client_zigbee_energy_scan(client, channelsToScan, maxDuration, scanCount);
    assert_non_null(result3);

    const GList *curr = result3;
    scoped_icLinkedListIterator *iter = linkedListIteratorCreate(energyScanResultsClone);
    while (linkedListIteratorHasNext(iter))
    {
        if (curr->data)
        {
            BDeviceServiceZigbeeEnergyScanResult *self = (BDeviceServiceZigbeeEnergyScanResult *) curr->data;
            zhalEnergyScanResult *icResult = linkedListIteratorGetNext(iter);

            guint8 dsChannel = 0;
            gint8 dsMaxRssi = 0;
            gint8 dsMinRssi = 0;
            gint8 dsAverageRssi = 0;
            guint32 dsScore = 0;

            if (self)
            {
                g_object_get(self,
                             B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                                 [B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_CHANNEL],
                             &dsChannel,
                             B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                                 [B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MAX],
                             &dsMaxRssi,
                             B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                                 [B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MIN],
                             &dsMinRssi,
                             B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                                 [B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_AVG],
                             &dsAverageRssi,
                             B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                                 [B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_SCORE],
                             &dsScore,
                             NULL);
            }

            g_assert_cmpuint(icResult->channel, ==, dsChannel);
            g_assert_cmpint(icResult->maxRssi, ==, dsMaxRssi);
            g_assert_cmpint(icResult->minRssi, ==, dsMinRssi);
            g_assert_cmpint(icResult->averageRssi, ==, dsAverageRssi);
            g_assert_cmpuint(icResult->score, ==, dsScore);

            curr = curr->next;
        }
    }

    linkedListDestroy(energyScanResultsClone, NULL);
    free(channelsArray);
    free(arrayCompareParams);
}
#endif

static void test_b_device_service_client_config_restore(void **state)
{
    BDeviceServiceClient *client = *state;
    gboolean result = FALSE;
    gchar *tempRestoreDir = "/tmp/restore";

    // NULL client, NULL tmpRestoreDir
    result = b_device_service_client_config_restore(NULL, NULL);
    assert_false(result);

    // NULL client, valid tmpRestoreDir
    result = b_device_service_client_config_restore(NULL, tempRestoreDir);
    assert_false(result);

    // valid client, NULL tmpRestoreDir
    result = b_device_service_client_config_restore(client, NULL);
    assert_false(result);

    // internal error (false return)
    expect_function_call(__wrap_deviceServiceRestoreConfig);
    expect_string(__wrap_deviceServiceRestoreConfig, tempRestoreDir, tempRestoreDir);
    will_return(__wrap_deviceServiceRestoreConfig, false);

    // valid client, valid tmpRestoreDir
    result = b_device_service_client_config_restore(client, tempRestoreDir);
    assert_false(result);

    // expect success (true return)
    expect_function_call(__wrap_deviceServiceRestoreConfig);
    expect_string(__wrap_deviceServiceRestoreConfig, tempRestoreDir, tempRestoreDir);
    will_return(__wrap_deviceServiceRestoreConfig, true);

    // valid client, valid tmpRestoreDir
    result = b_device_service_client_config_restore(client, tempRestoreDir);
    assert_true(result);
}

static void test_sendDiscoveryStartedEvent(void **state)
{
    scoped_icLinkedListNofree *deviceClasses = NULL;
    g_autoptr(GList) deviceClassesGList = NULL;
    uint16_t timeoutSeconds = 10;
    g_autoptr(BDeviceServiceDiscoveryStartedEvent) comparisonFixture = b_device_service_discovery_started_event_new();

    expect_function_call(mockHandleDiscoveryStartedEvent);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
        deviceClassesGList,
        B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROP_TIMEOUT],
        timeoutSeconds,
        NULL);
    expect_check(mockHandleDiscoveryStartedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDiscoveryStartedEventContents,
                 comparisonFixture);

    // NULL deviceClasses, valid timeoutSeconds
    sendDiscoveryStartedEvent(deviceClasses, timeoutSeconds, false);

    deviceClasses = linkedListCreate();
    expect_function_call(mockHandleDiscoveryStartedEvent);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
        deviceClassesGList,
        B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROP_TIMEOUT],
        timeoutSeconds,
        NULL);
    expect_check(mockHandleDiscoveryStartedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDiscoveryStartedEventContents,
                 comparisonFixture);

    // Empty deviceClasses, valid timeoutSeconds
    sendDiscoveryStartedEvent(deviceClasses, timeoutSeconds, false);

    linkedListAppend(deviceClasses, "myClass");
    deviceClassesGList = g_list_append(deviceClassesGList, "myClass");
    expect_function_call(mockHandleDiscoveryStartedEvent);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
        deviceClassesGList,
        B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROP_TIMEOUT],
        timeoutSeconds,
        NULL);
    expect_check(mockHandleDiscoveryStartedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDiscoveryStartedEventContents,
                 comparisonFixture);

    // One device class, valid timeoutSeconds
    sendDiscoveryStartedEvent(deviceClasses, timeoutSeconds, false);

    linkedListAppend(deviceClasses, "myOtherClass");
    deviceClassesGList = g_list_append(deviceClassesGList, "myOtherClass");
    expect_function_call(mockHandleDiscoveryStartedEvent);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
        deviceClassesGList,
        B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROP_TIMEOUT],
        timeoutSeconds,
        NULL);
    expect_check(mockHandleDiscoveryStartedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDiscoveryStartedEventContents,
                 comparisonFixture);

    // Two device classes, valid timeoutSeconds
    sendDiscoveryStartedEvent(deviceClasses, timeoutSeconds, false);
}

static void test_sendRecoveryStartedEvent(void **state)
{
    scoped_icLinkedListNofree *deviceClasses = NULL;
    g_autoptr(GList) deviceClassesGList = NULL;
    uint16_t timeoutSeconds = 10;
    g_autoptr(BDeviceServiceRecoveryStartedEvent) comparisonFixture = b_device_service_recovery_started_event_new();

    expect_function_call(mockHandleRecoveryStartedEvent);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
        deviceClassesGList,
        B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROP_TIMEOUT],
        timeoutSeconds,
        NULL);
    expect_check(mockHandleRecoveryStartedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceRecoveryStartedEventContents,
                 comparisonFixture);

    // NULL deviceClasses, valid timeoutSeconds
    sendDiscoveryStartedEvent(deviceClasses, timeoutSeconds, true);

    deviceClasses = linkedListCreate();
    expect_function_call(mockHandleRecoveryStartedEvent);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
        deviceClassesGList,
        B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROP_TIMEOUT],
        timeoutSeconds,
        NULL);
    expect_check(mockHandleRecoveryStartedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceRecoveryStartedEventContents,
                 comparisonFixture);

    // Empty deviceClasses, valid timeoutSeconds
    sendDiscoveryStartedEvent(deviceClasses, timeoutSeconds, true);

    linkedListAppend(deviceClasses, "myClass");
    deviceClassesGList = g_list_append(deviceClassesGList, "myClass");
    expect_function_call(mockHandleRecoveryStartedEvent);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
        deviceClassesGList,
        B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROP_TIMEOUT],
        timeoutSeconds,
        NULL);
    expect_check(mockHandleRecoveryStartedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceRecoveryStartedEventContents,
                 comparisonFixture);

    // One device class, valid timeoutSeconds
    sendDiscoveryStartedEvent(deviceClasses, timeoutSeconds, true);

    linkedListAppend(deviceClasses, "myOtherClass");
    deviceClassesGList = g_list_append(deviceClassesGList, "myOtherClass");
    expect_function_call(mockHandleRecoveryStartedEvent);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
        deviceClassesGList,
        B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROP_TIMEOUT],
        timeoutSeconds,
        NULL);
    expect_check(mockHandleRecoveryStartedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceRecoveryStartedEventContents,
                 comparisonFixture);

    // Two device classes, valid timeoutSeconds
    sendDiscoveryStartedEvent(deviceClasses, timeoutSeconds, true);
}

static void test_sendDeviceServiceStatusEvent(void **state)
{
    scoped_DeviceServiceStatus *statusFixture = NULL;
    g_autoptr(BDeviceServiceStatus) status = NULL;
    g_autoptr(BDeviceServiceStatusEvent) comparisonFixture = NULL;

    expect_function_call(__wrap_deviceServiceGetStatus);
    will_return(__wrap_deviceServiceGetStatus, statusFixture);
    expect_function_call(mockHandleStatusEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_STATUS_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_EVENT_PROP_STATUS],
                 status,
                 B_DEVICE_SERVICE_STATUS_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_EVENT_PROP_REASON],
                 B_DEVICE_SERVICE_STATUS_CHANGED_REASON_INVALID,
                 NULL);
    expect_check(
        mockHandleStatusEvent, event, (CheckParameterValue) checkBDeviceServiceStatusEventContents, comparisonFixture);

    sendDeviceServiceStatusEvent(DeviceServiceStatusChangedReasonInvalid);

    statusFixture = calloc(1, sizeof(DeviceServiceStatus));
    status = b_device_service_status_new();
    comparisonFixture = b_device_service_status_event_new();

    expect_function_call(__wrap_deviceServiceGetStatus);
    will_return(__wrap_deviceServiceGetStatus, statusFixture);

    expect_function_call(mockHandleStatusEvent);
    g_autofree gchar *json1 = deviceServiceStatusToJson(statusFixture);
    g_object_set(status, B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_JSON], json1, NULL);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_STATUS_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_EVENT_PROP_STATUS],
                 status,
                 B_DEVICE_SERVICE_STATUS_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_EVENT_PROP_REASON],
                 B_DEVICE_SERVICE_STATUS_CHANGED_REASON_INVALID,
                 NULL);
    expect_check(
        mockHandleStatusEvent, event, (CheckParameterValue) checkBDeviceServiceStatusEventContents, comparisonFixture);

    sendDeviceServiceStatusEvent(DeviceServiceStatusChangedReasonInvalid);

    // We don't need to do every permutation of conversions, the device-service-utils API should be covered in other
    // unit tests. We are just going to check a rudimentary status fixture.

    statusFixture->supportedDeviceClasses = linkedListCreate();
    linkedListAppend(statusFixture->supportedDeviceClasses, strdup("myClass"));
    statusFixture->discoveringDeviceClasses = linkedListCreate();
    linkedListAppend(statusFixture->discoveringDeviceClasses, strdup("myClass"));
    statusFixture->discoveryRunning = true;
    statusFixture->discoveryTimeoutSeconds = 10;
    statusFixture->isReadyForDeviceOperation = true;
    statusFixture->isReadyForPairing = true;
    statusFixture->findingOrphanedDevices = false;
    statusFixture->subsystemsJsonStatus = hashMapCreate();
    scoped_cJSON *subsystemStatus = cJSON_CreateObject();
    cJSON_AddStringToObject(subsystemStatus, "status", "test-status");
    hashMapPut(statusFixture->subsystemsJsonStatus,
               strdup("zigbee"),
               strlen("zigbee"),
               cJSON_Duplicate(subsystemStatus, true));

    expect_function_call(__wrap_deviceServiceGetStatus);
    will_return(__wrap_deviceServiceGetStatus, statusFixture);

    expect_function_call(mockHandleStatusEvent);
    g_autoptr(GList) deviceClasses = g_list_append(NULL, "myClass");
    g_autoptr(GList) searchingDeviceClasses = g_list_append(NULL, "myClass");
    g_autoptr(GHashTable) subsystems = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_autofree gchar *subsystemStatusJson = cJSON_Print(subsystemStatus);
    g_hash_table_insert(subsystems, strdup("zigbee"), strdup(subsystemStatusJson));
    g_autofree gchar *json2 = deviceServiceStatusToJson(statusFixture);
    g_object_set(status,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_DEVICE_CLASSES],
                 deviceClasses,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_DISCOVERY_TYPE],
                 statusFixture->discoveryRunning         ? B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY
                 : statusFixture->findingOrphanedDevices ? B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY
                                                         : B_DEVICE_SERVICE_DISCOVERY_TYPE_NONE,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_SEARCHING_DEVICE_CLASSES],
                 searchingDeviceClasses,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_DISCOVERY_SECONDS],
                 statusFixture->discoveryTimeoutSeconds,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_READY_FOR_OPERATION],
                 statusFixture->isReadyForDeviceOperation,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_READY_FOR_PAIRING],
                 statusFixture->isReadyForPairing,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_SUBSYSTEMS],
                 subsystems,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_JSON],
                 json2,
                 NULL);

    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_STATUS_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_EVENT_PROP_STATUS],
                 status,
                 B_DEVICE_SERVICE_STATUS_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_EVENT_PROP_REASON],
                 B_DEVICE_SERVICE_STATUS_CHANGED_REASON_SUBSYSTEM_STATUS,
                 NULL);
    expect_check(
        mockHandleStatusEvent, event, (CheckParameterValue) checkBDeviceServiceStatusEventContents, comparisonFixture);

    sendDeviceServiceStatusEvent(DeviceServiceStatusChangedReasonSubsystemStatus);
}

static void test_sendDeviceDiscoveryFailedEvent(void **state)
{
    g_autoptr(BDeviceServiceDeviceDiscoveryFailedEvent) comparisonFixture =
        b_device_service_device_discovery_failed_event_new();
    g_autoptr(BDeviceServiceDeviceFoundDetails) deviceFoundDetails = NULL;

    expect_function_call(mockHandleDeviceDiscoveryFailedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 deviceFoundDetails,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY,
                 NULL);
    expect_check(mockHandleDeviceDiscoveryFailedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceDiscoveryFailedEventContents,
                 comparisonFixture);

    // NULL deviceFoundDetails
    sendDeviceDiscoveryFailedEvent(NULL, false);

    scoped_icStringHashMap *metadataMap = stringHashMapCreate();
    scoped_icStringHashMap *endpointProfilesMap = stringHashMapCreate();
    DeviceFoundDetails deviceFoundDetailsFixture = {
        .deviceUuid = "uuid",
        .manufacturer = "manufacturer",
        .model = "model",
        .hardwareVersion = "hardwareVersion",
        .firmwareVersion = "firmwareVersion",
        .deviceClass = "deviceClass",
        .metadata = metadataMap,
        .endpointProfileMap = endpointProfilesMap,
    };
    stringHashMapPutCopy(deviceFoundDetailsFixture.metadata, "test metadata key", "test metadata value");
    stringHashMapPutCopy(deviceFoundDetailsFixture.endpointProfileMap, "test endpoint id", "test endpoint profile");

    g_autoptr(GHashTable) metadata = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_autoptr(GHashTable) endpointProfiles = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(metadata, strdup("test metadata key"), strdup("test metadata value"));
    g_hash_table_insert(endpointProfiles, strdup("test endpoint id"), strdup("test endpoint profile"));

    deviceFoundDetails = b_device_service_device_found_details_new();
    g_object_set(
        deviceFoundDetails,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_UUID],
        deviceFoundDetailsFixture.deviceUuid,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_MANUFACTURER],
        deviceFoundDetailsFixture.manufacturer,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_MODEL],
        deviceFoundDetailsFixture.model,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_HARDWARE_VERSION],
        deviceFoundDetailsFixture.hardwareVersion,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_FIRMWARE_VERSION],
        deviceFoundDetailsFixture.firmwareVersion,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_DEVICE_CLASS],
        deviceFoundDetailsFixture.deviceClass,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_METADATA],
        metadata,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_ENDPOINT_PROFILES],
        endpointProfiles,
        NULL);

    expect_function_call(mockHandleDeviceDiscoveryFailedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 deviceFoundDetails,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY,
                 NULL);
    expect_check(mockHandleDeviceDiscoveryFailedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceDiscoveryFailedEventContents,
                 comparisonFixture);

    sendDeviceDiscoveryFailedEvent(&deviceFoundDetailsFixture, false);

    expect_function_call(mockHandleDeviceDiscoveryFailedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY,
                 NULL);
    expect_check(mockHandleDeviceDiscoveryFailedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceDiscoveryFailedEventContents,
                 comparisonFixture);

    sendDeviceDiscoveryFailedEvent(&deviceFoundDetailsFixture, true);
}

static void test_sendDeviceDiscoveredEvent(void **state)
{
    g_autoptr(BDeviceServiceDeviceDiscoveredEvent) comparisonFixture = b_device_service_device_discovered_event_new();
    g_autoptr(BDeviceServiceDeviceFoundDetails) deviceFoundDetails = NULL;

    expect_function_call(mockHandleDeviceDiscoveredEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DEVICE_DISCOVERED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DISCOVERED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 deviceFoundDetails,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY,
                 NULL);
    expect_check(mockHandleDeviceDiscoveredEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceDiscoveredEventContents,
                 comparisonFixture);

    // NULL deviceFoundDetails
    sendDeviceDiscoveredEvent(NULL, false);

    scoped_icStringHashMap *metadataMap = stringHashMapCreate();
    scoped_icStringHashMap *endpointProfilesMap = stringHashMapCreate();
    DeviceFoundDetails deviceFoundDetailsFixture = {
        .deviceUuid = "uuid",
        .manufacturer = "manufacturer",
        .model = "model",
        .hardwareVersion = "hardwareVersion",
        .firmwareVersion = "firmwareVersion",
        .deviceClass = "deviceClass",
        .metadata = metadataMap,
        .endpointProfileMap = endpointProfilesMap,
    };
    stringHashMapPutCopy(deviceFoundDetailsFixture.metadata, "test metadata key", "test metadata value");
    stringHashMapPutCopy(deviceFoundDetailsFixture.endpointProfileMap, "test endpoint id", "test endpoint profile");

    g_autoptr(GHashTable) metadata = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_autoptr(GHashTable) endpointProfiles = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(metadata, strdup("test metadata key"), strdup("test metadata value"));
    g_hash_table_insert(endpointProfiles, strdup("test endpoint id"), strdup("test endpoint profile"));

    deviceFoundDetails = b_device_service_device_found_details_new();
    g_object_set(
        deviceFoundDetails,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_UUID],
        deviceFoundDetailsFixture.deviceUuid,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_MANUFACTURER],
        deviceFoundDetailsFixture.manufacturer,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_MODEL],
        deviceFoundDetailsFixture.model,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_HARDWARE_VERSION],
        deviceFoundDetailsFixture.hardwareVersion,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_FIRMWARE_VERSION],
        deviceFoundDetailsFixture.firmwareVersion,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_DEVICE_CLASS],
        deviceFoundDetailsFixture.deviceClass,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_METADATA],
        metadata,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_ENDPOINT_PROFILES],
        endpointProfiles,
        NULL);

    expect_function_call(mockHandleDeviceDiscoveredEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DEVICE_DISCOVERED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DISCOVERED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 deviceFoundDetails,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY,
                 NULL);
    expect_check(mockHandleDeviceDiscoveredEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceDiscoveredEventContents,
                 comparisonFixture);

    sendDeviceDiscoveredEvent(&deviceFoundDetailsFixture, false);

    expect_function_call(mockHandleDeviceDiscoveredEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY,
                 NULL);
    expect_check(mockHandleDeviceDiscoveredEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceDiscoveredEventContents,
                 comparisonFixture);

    sendDeviceDiscoveredEvent(&deviceFoundDetailsFixture, true);
}

static void test_sendDeviceRejectedEvent(void **state)
{
    g_autoptr(BDeviceServiceDeviceRejectedEvent) comparisonFixture = b_device_service_device_rejected_event_new();
    g_autoptr(BDeviceServiceDeviceFoundDetails) deviceFoundDetails = NULL;

    expect_function_call(mockHandleDeviceRejectedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DEVICE_REJECTED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_REJECTED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 deviceFoundDetails,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY,
                 NULL);
    expect_check(mockHandleDeviceRejectedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceRejectedEventContents,
                 comparisonFixture);

    // NULL deviceFoundDetails
    sendDeviceRejectedEvent(NULL, false);

    scoped_icStringHashMap *metadataMap = stringHashMapCreate();
    scoped_icStringHashMap *endpointProfilesMap = stringHashMapCreate();
    DeviceFoundDetails deviceFoundDetailsFixture = {
        .deviceUuid = "uuid",
        .manufacturer = "manufacturer",
        .model = "model",
        .hardwareVersion = "hardwareVersion",
        .firmwareVersion = "firmwareVersion",
        .deviceClass = "deviceClass",
        .metadata = metadataMap,
        .endpointProfileMap = endpointProfilesMap,
    };
    stringHashMapPutCopy(deviceFoundDetailsFixture.metadata, "test metadata key", "test metadata value");
    stringHashMapPutCopy(deviceFoundDetailsFixture.endpointProfileMap, "test endpoint id", "test endpoint profile");

    g_autoptr(GHashTable) metadata = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_autoptr(GHashTable) endpointProfiles = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(metadata, strdup("test metadata key"), strdup("test metadata value"));
    g_hash_table_insert(endpointProfiles, strdup("test endpoint id"), strdup("test endpoint profile"));

    deviceFoundDetails = b_device_service_device_found_details_new();
    g_object_set(
        deviceFoundDetails,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_UUID],
        deviceFoundDetailsFixture.deviceUuid,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_MANUFACTURER],
        deviceFoundDetailsFixture.manufacturer,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_MODEL],
        deviceFoundDetailsFixture.model,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_HARDWARE_VERSION],
        deviceFoundDetailsFixture.hardwareVersion,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_FIRMWARE_VERSION],
        deviceFoundDetailsFixture.firmwareVersion,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_DEVICE_CLASS],
        deviceFoundDetailsFixture.deviceClass,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_METADATA],
        metadata,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_ENDPOINT_PROFILES],
        endpointProfiles,
        NULL);

    expect_function_call(mockHandleDeviceRejectedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DEVICE_REJECTED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_REJECTED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 deviceFoundDetails,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY,
                 NULL);
    expect_check(mockHandleDeviceRejectedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceRejectedEventContents,
                 comparisonFixture);

    sendDeviceRejectedEvent(&deviceFoundDetailsFixture, false);

    expect_function_call(mockHandleDeviceRejectedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY,
                 NULL);
    expect_check(mockHandleDeviceRejectedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceRejectedEventContents,
                 comparisonFixture);

    sendDeviceRejectedEvent(&deviceFoundDetailsFixture, true);
}

static void test_sendDeviceConfigureStartedEvent(void **state)
{
    const gchar *deviceUuid = "uuid";
    const gchar *deviceClass = "deviceClass";
    BDeviceServiceDiscoveryType discoveryType = B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY;

    g_autoptr(BDeviceServiceDeviceConfigurationStartedEvent) comparisonFixture =
        b_device_service_device_configuration_started_event_new();

    expect_function_call(mockHandleDeviceConfigurationStartedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 NULL,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 deviceUuid,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 discoveryType,
                 NULL);
    expect_check(mockHandleDeviceConfigurationStartedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceConfigurationStartedEventContents,
                 comparisonFixture);

    // NULL deviceClass, valid uuid, valid discoveryType
    sendDeviceConfigureStartedEvent(NULL, deviceUuid, false);

    expect_function_call(mockHandleDeviceConfigurationStartedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 NULL,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 discoveryType,
                 NULL);
    expect_check(mockHandleDeviceConfigurationStartedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceConfigurationStartedEventContents,
                 comparisonFixture);

    // valid deviceClass, NULL uuid, valid discoveryType
    sendDeviceConfigureStartedEvent(deviceClass, NULL, false);

    expect_function_call(mockHandleDeviceConfigurationStartedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 deviceUuid,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 discoveryType,
                 NULL);
    expect_check(mockHandleDeviceConfigurationStartedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceConfigurationStartedEventContents,
                 comparisonFixture);

    // valid deviceClass, valid uuid, discover discoveryType
    sendDeviceConfigureStartedEvent(deviceClass, deviceUuid, false);

    expect_function_call(mockHandleDeviceConfigurationStartedEvent);
    discoveryType = B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY;
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 deviceUuid,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 discoveryType,
                 NULL);
    expect_check(mockHandleDeviceConfigurationStartedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceConfigurationStartedEventContents,
                 comparisonFixture);

    // valid deviceClass, valid uuid, recovery discoveryType
    sendDeviceConfigureStartedEvent(deviceClass, deviceUuid, true);
}

static void test_sendDeviceConfigureCompletedEvent(void **state)
{
    const gchar *deviceUuid = "uuid";
    const gchar *deviceClass = "deviceClass";
    BDeviceServiceDiscoveryType discoveryType = B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY;

    g_autoptr(BDeviceServiceDeviceConfigurationCompletedEvent) comparisonFixture =
        b_device_service_device_configuration_completed_event_new();

    expect_function_call(mockHandleDeviceConfigurationCompletedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 NULL,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 deviceUuid,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 discoveryType,
                 NULL);
    expect_check(mockHandleDeviceConfigurationCompletedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceConfigurationCompletedEventContents,
                 comparisonFixture);

    // NULL deviceClass, valid uuid, valid discoveryType
    sendDeviceConfigureCompletedEvent(NULL, deviceUuid, false);

    expect_function_call(mockHandleDeviceConfigurationCompletedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 NULL,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 discoveryType,
                 NULL);
    expect_check(mockHandleDeviceConfigurationCompletedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceConfigurationCompletedEventContents,
                 comparisonFixture);

    // valid deviceClass, NULL uuid, valid discoveryType
    sendDeviceConfigureCompletedEvent(deviceClass, NULL, false);

    expect_function_call(mockHandleDeviceConfigurationCompletedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 deviceUuid,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 discoveryType,
                 NULL);
    expect_check(mockHandleDeviceConfigurationCompletedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceConfigurationCompletedEventContents,
                 comparisonFixture);

    // valid deviceClass, valid uuid, discover discoveryType
    sendDeviceConfigureCompletedEvent(deviceClass, deviceUuid, false);

    expect_function_call(mockHandleDeviceConfigurationCompletedEvent);
    discoveryType = B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY;
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 deviceUuid,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 discoveryType,
                 NULL);
    expect_check(mockHandleDeviceConfigurationCompletedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceConfigurationCompletedEventContents,
                 comparisonFixture);

    // valid deviceClass, valid uuid, recovery discoveryType
    sendDeviceConfigureCompletedEvent(deviceClass, deviceUuid, true);
}

static void test_sendDeviceConfigureFailedEvent(void **state)
{
    const gchar *deviceUuid = "uuid";
    const gchar *deviceClass = "deviceClass";
    BDeviceServiceDiscoveryType discoveryType = B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY;

    g_autoptr(BDeviceServiceDeviceConfigurationFailedEvent) comparisonFixture =
        b_device_service_device_configuration_failed_event_new();

    expect_function_call(mockHandleDeviceConfigurationFailedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 NULL,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 deviceUuid,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 discoveryType,
                 NULL);
    expect_check(mockHandleDeviceConfigurationFailedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceConfigurationFailedEventContents,
                 comparisonFixture);

    // NULL deviceClass, valid uuid, valid discoveryType
    sendDeviceConfigureFailedEvent(NULL, deviceUuid, false);

    expect_function_call(mockHandleDeviceConfigurationFailedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 NULL,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 discoveryType,
                 NULL);
    expect_check(mockHandleDeviceConfigurationFailedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceConfigurationFailedEventContents,
                 comparisonFixture);

    // valid deviceClass, NULL uuid, valid discoveryType
    sendDeviceConfigureFailedEvent(deviceClass, NULL, false);

    expect_function_call(mockHandleDeviceConfigurationFailedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 deviceUuid,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 discoveryType,
                 NULL);
    expect_check(mockHandleDeviceConfigurationFailedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceConfigurationFailedEventContents,
                 comparisonFixture);

    // valid deviceClass, valid uuid, discover discoveryType
    sendDeviceConfigureFailedEvent(deviceClass, deviceUuid, false);

    expect_function_call(mockHandleDeviceConfigurationFailedEvent);
    discoveryType = B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY;
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 deviceUuid,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 discoveryType,
                 NULL);
    expect_check(mockHandleDeviceConfigurationFailedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceConfigurationFailedEventContents,
                 comparisonFixture);

    // valid deviceClass, valid uuid, recovery discoveryType
    sendDeviceConfigureFailedEvent(deviceClass, deviceUuid, true);
}

static void test_sendDeviceAddedEvent(void **state)
{
    const gchar *deviceUuid = "uuid";
    const gchar *deviceUri = "uri";
    const gchar *deviceClass = "deviceClass";
    guint8 deviceClassVersion = 1;

    g_autoptr(BDeviceServiceDeviceAddedEvent) comparisonFixture = b_device_service_device_added_event_new();

    // NULL deviceUuid
    sendDeviceAddedEvent(NULL);

    expect_function_call(__wrap_deviceServiceGetDevice);
    expect_string(__wrap_deviceServiceGetDevice, uuid, "deviceDoesntExist");
    will_return(__wrap_deviceServiceGetDevice, NULL);

    // non-existent deviceUuid
    sendDeviceAddedEvent("deviceDoesntExist");

    icDevice *device = calloc(1, sizeof(icDevice));
    device->uuid = strdup(deviceUuid);
    device->uri = strdup(deviceUri);
    device->deviceClass = strdup(deviceClass);
    device->deviceClassVersion = deviceClassVersion;

    expect_function_call(__wrap_deviceServiceGetDevice);
    expect_string(__wrap_deviceServiceGetDevice, uuid, deviceUuid);
    will_return(__wrap_deviceServiceGetDevice, g_steal_pointer(&device));

    expect_function_call(mockHandleDeviceAddedEvent);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_UUID],
        deviceUuid,
        B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_URI],
        deviceUri,
        B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS],
        deviceClass,
        B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS_VERSION],
        deviceClassVersion,
        NULL);

    expect_check(mockHandleDeviceAddedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceAddedEventContents,
                 comparisonFixture);

    // valid deviceUuid
    sendDeviceAddedEvent(deviceUuid);
}

static void test_sendDeviceRecoveredEvent(void **state)
{
    const gchar *deviceUuid = "uuid";
    const gchar *deviceUri = "uri";
    const gchar *deviceClass = "deviceClass";
    guint8 deviceClassVersion = 1;

    g_autoptr(BDeviceServiceDeviceRecoveredEvent) comparisonFixture = b_device_service_device_recovered_event_new();

    // NULL deviceUuid
    sendDeviceRecoveredEvent(NULL);

    expect_function_call(__wrap_deviceServiceGetDevice);
    expect_string(__wrap_deviceServiceGetDevice, uuid, "deviceDoesntExist");
    will_return(__wrap_deviceServiceGetDevice, NULL);

    // non-existent deviceUuid
    sendDeviceRecoveredEvent("deviceDoesntExist");

    icDevice *device = calloc(1, sizeof(icDevice));
    device->uuid = strdup(deviceUuid);
    device->uri = strdup(deviceUri);
    device->deviceClass = strdup(deviceClass);
    device->deviceClassVersion = deviceClassVersion;

    expect_function_call(__wrap_deviceServiceGetDevice);
    expect_string(__wrap_deviceServiceGetDevice, uuid, deviceUuid);
    will_return(__wrap_deviceServiceGetDevice, g_steal_pointer(&device));

    expect_function_call(mockHandleDeviceRecoveredEvent);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROP_UUID],
        deviceUuid,
        B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROP_URI],
        deviceUri,
        B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROP_DEVICE_CLASS],
        deviceClass,
        B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROP_DEVICE_CLASS_VERSION],
        deviceClassVersion,
        NULL);

    expect_check(mockHandleDeviceRecoveredEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceRecoveredEventContents,
                 comparisonFixture);

    // valid deviceUuid
    sendDeviceRecoveredEvent(deviceUuid);
}

static void test_sendDeviceDiscoveryCompletedEvent(void **state)
{
    icDevice discoveredDevice = {
        .uuid = "uuid",
        .deviceClass = "deviceClass",
        .deviceClassVersion = 1,
        .uri = "uri",
        .managingDeviceDriver = "managingDeviceDriver",
    };
    // Don't care too much about deeply comparing objects (i.e. endpoints, resourced, metadata).
    // Underlying translation of icDevice to BDeviceServiceDevice should be unit tested elsewhere.

    g_autoptr(BDeviceServiceDevice) deviceFixture = b_device_service_device_new();
    g_autoptr(BDeviceServiceDeviceDiscoveryCompletedEvent) comparisonFixture =
        b_device_service_device_discovery_completed_event_new();


    expect_function_call(mockHandleDeviceDiscoveryCompletedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY,
                 B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROP_DEVICE],
                 NULL,
                 NULL);
    expect_check(mockHandleDeviceDiscoveryCompletedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceDiscoveryCompletedEventContents,
                 comparisonFixture);

    // NULL device
    sendDeviceDiscoveryCompletedEvent(NULL, false);

    expect_function_call(mockHandleDeviceDiscoveryCompletedEvent);
    g_object_set(deviceFixture,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_UUID],
                 discoveredDevice.uuid,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS],
                 discoveredDevice.deviceClass,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS_VERSION],
                 discoveredDevice.deviceClassVersion,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_URI],
                 discoveredDevice.uri,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_MANAGING_DEVICE_DRIVER],
                 discoveredDevice.managingDeviceDriver,
                 NULL);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY,
                 B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROP_DEVICE],
                 deviceFixture,
                 NULL);
    expect_check(mockHandleDeviceDiscoveryCompletedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceDiscoveryCompletedEventContents,
                 comparisonFixture);

    // discover discoveryType
    sendDeviceDiscoveryCompletedEvent(&discoveredDevice, false);

    expect_function_call(mockHandleDeviceDiscoveryCompletedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY,
                 NULL);
    expect_check(mockHandleDeviceDiscoveryCompletedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceDiscoveryCompletedEventContents,
                 comparisonFixture);

    // recovery discoveryType
    sendDeviceDiscoveryCompletedEvent(&discoveredDevice, true);
}

static void test_sendDiscoveryStoppedEvent(void **state)
{
    const gchar *deviceClass = "deviceClass";
    g_autoptr(BDeviceServiceDiscoveryStoppedEvent) comparisonFixture = b_device_service_discovery_stopped_event_new();

    expect_function_call(mockHandleDiscoveryStoppedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS],
                 NULL,
                 NULL);
    expect_check(mockHandleDiscoveryStoppedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDiscoveryStoppedEventContents,
                 comparisonFixture);

    // NULL deviceClass
    sendDiscoveryStoppedEvent(NULL);

    expect_function_call(mockHandleDiscoveryStoppedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 NULL);
    expect_check(mockHandleDiscoveryStoppedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDiscoveryStoppedEventContents,
                 comparisonFixture);

    // valid deviceClass
    sendDiscoveryStoppedEvent(deviceClass);
}

static void test_sendRecoveryStoppedEvent(void **state)
{
    const gchar *deviceClass = "deviceClass";
    g_autoptr(BDeviceServiceRecoveryStoppedEvent) comparisonFixture = b_device_service_recovery_stopped_event_new();

    expect_function_call(mockHandleRecoveryStoppedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_RECOVERY_STOPPED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_RECOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS],
                 NULL,
                 NULL);
    expect_check(mockHandleRecoveryStoppedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceRecoveryStoppedEventContents,
                 comparisonFixture);

    // NULL deviceClass
    sendRecoveryStoppedEvent(NULL);

    expect_function_call(mockHandleRecoveryStoppedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_RECOVERY_STOPPED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_RECOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 NULL);
    expect_check(mockHandleRecoveryStoppedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceRecoveryStoppedEventContents,
                 comparisonFixture);

    // valid deviceClass
    sendRecoveryStoppedEvent(deviceClass);
}

static void test_sendResourceUpdatedEvent(void **state)
{
    icDeviceResource resource = {
        .id = "id",
        .uri = "uri",
        .endpointId = "endpointId",
        .deviceUuid = "deviceUuid",
        .value = "value",
        .type = "type",
    };

    scoped_cJSON *eventMetadata = cJSON_CreateObject();
    cJSON_AddStringToObject(eventMetadata, "key", "value");
    g_autofree gchar *eventMetadataStringFixture = cJSON_PrintUnformatted(eventMetadata);

    g_autoptr(BDeviceServiceResource) resourceFixture = b_device_service_resource_new();
    g_autoptr(BDeviceServiceResourceUpdatedEvent) comparisonFixture = b_device_service_resource_updated_event_new();

    expect_function_call(mockHandleResourceUpdatedEvent);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_RESOURCE],
        NULL,
        B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_METADATA],
        eventMetadataStringFixture,
        NULL);
    expect_check(mockHandleResourceUpdatedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceResourceUpdatedEventContents,
                 comparisonFixture);

    // NULL resource, valid metadata
    sendResourceUpdatedEvent(NULL, eventMetadata);

    expect_function_call(mockHandleResourceUpdatedEvent);
    g_object_set(resourceFixture,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_ID],
                 resource.id,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_URI],
                 resource.uri,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_ENDPOINT_ID],
                 resource.endpointId,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_DEVICE_UUID],
                 resource.deviceUuid,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_VALUE],
                 resource.value,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_TYPE],
                 resource.type,
                 NULL);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_RESOURCE],
        resourceFixture,
        B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_METADATA],
        NULL,
        NULL);
    expect_check(mockHandleResourceUpdatedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceResourceUpdatedEventContents,
                 comparisonFixture);

    // valid resource, NULL metadata
    sendResourceUpdatedEvent(&resource, NULL);

    expect_function_call(mockHandleResourceUpdatedEvent);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_RESOURCE],
        resourceFixture,
        B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_METADATA],
        eventMetadataStringFixture,
        NULL);
    expect_check(mockHandleResourceUpdatedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceResourceUpdatedEventContents,
                 comparisonFixture);

    // valid resource, valid metadata
    sendResourceUpdatedEvent(&resource, eventMetadata);
}

static void test_sendDeviceRemovedEvent(void **state)
{
    const gchar *deviceUuid = "uuid";
    const gchar *deviceClass = "deviceClass";
    g_autoptr(BDeviceServiceDeviceRemovedEvent) comparisonFixture = b_device_service_device_removed_event_new();

    expect_function_call(mockHandleDeviceRemovedEvent);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_UUID],
        NULL,
        B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_CLASS],
        deviceClass,
        NULL);
    expect_check(mockHandleDeviceRemovedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceRemovedEventContents,
                 comparisonFixture);

    // NULL deviceUuid, valid deviceClass
    sendDeviceRemovedEvent(NULL, deviceClass);

    expect_function_call(mockHandleDeviceRemovedEvent);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_UUID],
        deviceUuid,
        B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_CLASS],
        NULL,
        NULL);
    expect_check(mockHandleDeviceRemovedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceRemovedEventContents,
                 comparisonFixture);

    // valid deviceUuid, NULL deviceClass
    sendDeviceRemovedEvent(deviceUuid, NULL);

    expect_function_call(mockHandleDeviceRemovedEvent);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_UUID],
        deviceUuid,
        B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_CLASS],
        deviceClass,
        NULL);
    expect_check(mockHandleDeviceRemovedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceRemovedEventContents,
                 comparisonFixture);

    // valid deviceUuid, valid deviceClass
    sendDeviceRemovedEvent(deviceUuid, deviceClass);
}

static void test_sendEndpointRemovedEvent(void **state)
{
    icDeviceEndpoint endpoint = {
        .id = "endpointId",
        .uri = "uri",
        .profile = "profile",
        .deviceUuid = "deviceUuid",
    };

    const gchar *deviceClass = "deviceClass";
    gboolean deviceRemoved = FALSE;

    g_autoptr(BDeviceServiceEndpoint) endpointFixture = b_device_service_endpoint_new();
    g_autoptr(BDeviceServiceEndpointRemovedEvent) comparisonFixture = b_device_service_endpoint_removed_event_new();

    expect_function_call(mockHandleEndpointRemovedEvent);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_ENDPOINT],
        NULL,
        B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_CLASS],
        deviceClass,
        B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_REMOVED],
        deviceRemoved,
        NULL);
    expect_check(mockHandleEndpointRemovedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceEndpointRemovedEventContents,
                 comparisonFixture);

    // NULL endpointId, valid deviceClass
    sendEndpointRemovedEvent(NULL, deviceClass, deviceRemoved);

    expect_function_call(mockHandleEndpointRemovedEvent);
    g_object_set(endpointFixture,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_ID],
                 endpoint.id,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_URI],
                 endpoint.uri,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE],
                 endpoint.profile,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_DEVICE_UUID],
                 endpoint.deviceUuid,
                 NULL);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_ENDPOINT],
        endpointFixture,
        B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_CLASS],
        NULL,
        B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_REMOVED],
        deviceRemoved,
        NULL);
    expect_check(mockHandleEndpointRemovedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceEndpointRemovedEventContents,
                 comparisonFixture);

    // valid endpointId, NULL deviceClass
    sendEndpointRemovedEvent(&endpoint, NULL, deviceRemoved);

    expect_function_call(mockHandleEndpointRemovedEvent);
    deviceRemoved = TRUE;
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_ENDPOINT],
        endpointFixture,
        B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_CLASS],
        deviceClass,
        B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_REMOVED],
        deviceRemoved,
        NULL);
    expect_check(mockHandleEndpointRemovedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceEndpointRemovedEventContents,
                 comparisonFixture);

    // valid endpointId, valid deviceClass
    sendEndpointRemovedEvent(&endpoint, deviceClass, deviceRemoved);
}

static void test_sendEndpointAddedEvent(void **state)
{
    icDeviceEndpoint endpoint = {
        .id = "endpointId",
        .uri = "uri",
        .profile = "profile",
        .deviceUuid = "deviceUuid",
    };

    const gchar *deviceClass = "deviceClass";

    g_autoptr(BDeviceServiceEndpoint) endpointFixture = b_device_service_endpoint_new();
    g_autoptr(BDeviceServiceEndpointAddedEvent) comparisonFixture = b_device_service_endpoint_added_event_new();

    expect_function_call(mockHandleEndpointAddedEvent);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROP_ENDPOINT],
        NULL,
        B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROP_DEVICE_CLASS],
        deviceClass,
        NULL);
    expect_check(mockHandleEndpointAddedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceEndpointAddedEventContents,
                 comparisonFixture);

    // NULL endpointId, valid deviceClass
    sendEndpointAddedEvent(NULL, deviceClass);

    expect_function_call(mockHandleEndpointAddedEvent);
    g_object_set(endpointFixture,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_ID],
                 endpoint.id,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_URI],
                 endpoint.uri,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE],
                 endpoint.profile,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_DEVICE_UUID],
                 endpoint.deviceUuid,
                 NULL);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROP_ENDPOINT],
        endpointFixture,
        B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROP_DEVICE_CLASS],
        NULL,
        NULL);
    expect_check(mockHandleEndpointAddedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceEndpointAddedEventContents,
                 comparisonFixture);

    // valid endpointId, NULL deviceClass
    sendEndpointAddedEvent(&endpoint, NULL);

    expect_function_call(mockHandleEndpointAddedEvent);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROP_ENDPOINT],
        endpointFixture,
        B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROP_DEVICE_CLASS],
        deviceClass,
        NULL);
    expect_check(mockHandleEndpointAddedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceEndpointAddedEventContents,
                 comparisonFixture);

    // valid endpointId, valid deviceClass
    sendEndpointAddedEvent(&endpoint, deviceClass);
}

static void test_sendZigbeeChannelChangedEvent(void **state)
{
    gboolean channel_changed = true;
    guint8 current_channel = 16;
    guint8 targeted_channel = 16;

    // When channel is changed
    g_autoptr(BDeviceServiceZigbeeChannelChangedEvent) comparisonFixture =
        b_device_service_zigbee_channel_changed_event_new();
    expect_function_call(mockHandleZigbeeChannelChangedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CHANNEL_CHANGED],
                 channel_changed,
                 B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CURRENT_CHANNEL],
                 current_channel,
                 B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_TARGETED_CHANNEL],
                 targeted_channel,
                 NULL);
    expect_check(mockHandleZigbeeChannelChangedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceZigbeeChannelChangedEventContents,
                 comparisonFixture);

    sendZigbeeChannelChangedEvent(channel_changed, current_channel, targeted_channel);

    expect_function_call(mockHandleZigbeeChannelChangedEvent);
    // Failed to change the channel
    channel_changed = false;
    targeted_channel = 20;
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CHANNEL_CHANGED],
                 channel_changed,
                 B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CURRENT_CHANNEL],
                 current_channel,
                 B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_TARGETED_CHANNEL],
                 targeted_channel,
                 NULL);

    expect_check(mockHandleZigbeeChannelChangedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceZigbeeChannelChangedEventContents,
                 comparisonFixture);

    sendZigbeeChannelChangedEvent(channel_changed, current_channel, targeted_channel);
}

static void test_sendStorageChangedEvent(void **state)
{
    GFileMonitorEvent whatChanged = G_FILE_MONITOR_EVENT_CHANGED;

    g_autoptr(BDeviceServiceStorageChangedEvent) comparisonFixture = b_device_service_storage_changed_event_new();

    expect_function_call(mockHandleStorageChangedEvent);
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROP_WHAT_CHANGED],
        whatChanged,
        NULL);
    expect_check(mockHandleStorageChangedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceStorageChangedEventContents,
                 comparisonFixture);

    sendStorageChangedEvent(whatChanged);

    expect_function_call(mockHandleStorageChangedEvent);
    whatChanged = G_FILE_MONITOR_EVENT_CREATED;
    g_object_set(
        comparisonFixture,
        B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROP_WHAT_CHANGED],
        whatChanged,
        NULL);
    expect_check(mockHandleStorageChangedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceStorageChangedEventContents,
                 comparisonFixture);

    sendStorageChangedEvent(whatChanged);
}

static void test_sendZigbeeInterferenceEvent(void **state)
{
    gboolean interference_detected = true;

    // When interference is detected.
    g_autoptr(BDeviceServiceZigbeeInterferenceEvent) comparisonFixture =
        b_device_service_zigbee_interference_event_new();
    expect_function_call(mockHandleZigbeeInterferenceEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_ZIGBEE_INTERFERENCE_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_INTERFERENCE_EVENT_PROP_INTERFERENCE_DETECTED],
                 interference_detected,
                 NULL);
    expect_check(mockHandleZigbeeInterferenceEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceZigbeeInterferenceEventContents,
                 comparisonFixture);

    sendZigbeeNetworkInterferenceEvent(interference_detected);

    expect_function_call(mockHandleZigbeeInterferenceEvent);
    // Interference had occurred and has subsided.
    interference_detected = false;
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_ZIGBEE_INTERFERENCE_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_INTERFERENCE_EVENT_PROP_INTERFERENCE_DETECTED],
                 interference_detected,
                 NULL);

    expect_check(mockHandleZigbeeInterferenceEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceZigbeeInterferenceEventContents,
                 comparisonFixture);

    sendZigbeeNetworkInterferenceEvent(interference_detected);
}

static void test_sendZigbeePanIdAttackChangedEvent(void **state)
{
    gboolean pan_id_attack_detected = true;

    // When PAN ID attack is detected.
    g_autoptr(BDeviceServiceZigbeePanIdAttackChangedEvent) comparisonFixture =
        b_device_service_zigbee_pan_id_attack_changed_event_new();

    expect_function_call(mockHandleZigbeePanIdAttackChangedEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROP_ATTACK_DETECTED],
                 pan_id_attack_detected,
                 NULL);
    expect_check(mockHandleZigbeePanIdAttackChangedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceZigbeePanIdAttackChangedEventContents,
                 comparisonFixture);

    sendZigbeePanIdAttackEvent(pan_id_attack_detected);

    expect_function_call(mockHandleZigbeePanIdAttackChangedEvent);
    // PAN ID attack has occurred and subsided.
    pan_id_attack_detected = false;
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROP_ATTACK_DETECTED],
                 pan_id_attack_detected,
                 NULL);
    expect_check(mockHandleZigbeePanIdAttackChangedEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceZigbeePanIdAttackChangedEventContents,
                 comparisonFixture);

    sendZigbeePanIdAttackEvent(pan_id_attack_detected);
}

static void test_sendDeviceDatabaseFailureEvent(void **state)
{
    BDeviceServiceDeviceDatabaseFailureType failureType =
        B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_TYPE_DEVICE_LOAD_FAILURE;
    const gchar *deviceUuid = "uuid";
    g_autoptr(BDeviceServiceDeviceDatabaseFailureEvent) comparisonFixture =
        b_device_service_device_database_failure_event_new();

    expect_function_call(mockHandleDeviceDatabaseFailureEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE],
                 failureType,
                 B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_DEVICE_ID],
                 NULL,
                 NULL);
    expect_check(mockHandleDeviceDatabaseFailureEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceDatabaseFailureEventContents,
                 comparisonFixture);

    sendDeviceDatabaseFailureEvent(failureType, NULL);

    expect_function_call(mockHandleDeviceDatabaseFailureEvent);
    g_object_set(comparisonFixture,
                 B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE],
                 failureType,
                 B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_DEVICE_ID],
                 deviceUuid,
                 NULL);
    expect_check(mockHandleDeviceDatabaseFailureEvent,
                 event,
                 (CheckParameterValue) checkBDeviceServiceDeviceDatabaseFailureEventContents,
                 comparisonFixture);

    sendDeviceDatabaseFailureEvent(failureType, deviceUuid);
}

static bool compareStringGLists(GList *first, GList *second)
{
    g_return_val_if_fail(g_list_length(first) == g_list_length(second), false);

    for (GList *curr = first, *curr2 = second; curr != NULL && curr2 != NULL; curr = curr->next, curr2 = curr2->next)
    {
        if (g_strcmp0((gchar *) curr->data, (gchar *) curr2->data) != 0)
        {
            return false;
        }
    }

    return true;
}

static int checkListContents(icLinkedList *input, GList *expected)
{
    g_return_val_if_fail(linkedListCount(input) == g_list_length(expected), 0);

    scoped_icLinkedListIterator *iter = linkedListIteratorCreate(input);
    GList *curr = expected;

    while (linkedListIteratorHasNext(iter) && curr != NULL)
    {
        void *content = linkedListIteratorGetNext(iter);
        if (content != curr->data)
        {
            return 0;
        }

        curr = curr->next;
    }

    return 1;
}

static int checkDiscoveryFilterListContents(icLinkedList *input, GList *expected)
{
    g_return_val_if_fail(linkedListCount(input) == g_list_length(expected), 0);

    scoped_icLinkedListIterator *iter = linkedListIteratorCreate(input);
    GList *current = expected;

    while (linkedListIteratorHasNext(iter) && current != NULL)
    {
        discoveryFilter *filter = linkedListIteratorGetNext(iter);
        BDeviceServiceDiscoveryFilter *filter2 = current->data;

        g_autofree gchar *expectedUriPattern = NULL;
        g_autofree gchar *expectedValuePattern = NULL;
        g_object_get(filter2,
                     B_DEVICE_SERVICE_DISCOVERY_FILTER_PROPERTY_NAMES[B_DEVICE_SERVICE_DISCOVERY_FILTER_PROP_URI],
                     &expectedUriPattern,
                     B_DEVICE_SERVICE_DISCOVERY_FILTER_PROPERTY_NAMES[B_DEVICE_SERVICE_DISCOVERY_FILTER_PROP_VALUE],
                     &expectedValuePattern,
                     NULL);

        if (g_strcmp0(filter->uriPattern, expectedUriPattern) != 0)
        {
            return 0;
        }

        if (g_strcmp0(filter->valuePattern, expectedValuePattern) != 0)
        {
            return 0;
        }

        current = current->next;
    }

    return 1;
}

static int checkBDeviceServiceDiscoveryStartedEventContents(BDeviceServiceDiscoveryStartedEvent *input,
                                                            BDeviceServiceDiscoveryStartedEvent *expected)
{
    GList *inputDeviceClasses = NULL;
    GList *expectedDeviceClasses = NULL;
    guint16 inputTimeoutSeconds = 0;
    guint16 expectedTimeoutSeconds = 0;

    g_object_get(
        input,
        B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
        &inputDeviceClasses,
        B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROP_TIMEOUT],
        &inputTimeoutSeconds,
        NULL);

    g_object_get(
        expected,
        B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
        &expectedDeviceClasses,
        B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROP_TIMEOUT],
        &expectedTimeoutSeconds,
        NULL);

    g_return_val_if_fail(inputTimeoutSeconds == expectedTimeoutSeconds, 0);
    g_return_val_if_fail(g_list_length(inputDeviceClasses) == g_list_length(expectedDeviceClasses), 0);
    g_return_val_if_fail(compareStringGLists(inputDeviceClasses, expectedDeviceClasses), 0);

    g_list_free_full(inputDeviceClasses, g_free);
    g_list_free_full(expectedDeviceClasses, g_free);

    return 1;
}

static int checkBDeviceServiceRecoveryStartedEventContents(BDeviceServiceRecoveryStartedEvent *input,
                                                           BDeviceServiceRecoveryStartedEvent *expected)
{
    GList *inputDeviceClasses = NULL;
    GList *expectedDeviceClasses = NULL;
    guint16 inputTimeoutSeconds = 0;
    guint16 expectedTimeoutSeconds = 0;

    g_object_get(
        input,
        B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
        &inputDeviceClasses,
        B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROP_TIMEOUT],
        &inputTimeoutSeconds,
        NULL);

    g_object_get(
        expected,
        B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
        &expectedDeviceClasses,
        B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROP_TIMEOUT],
        &expectedTimeoutSeconds,
        NULL);

    g_return_val_if_fail(inputTimeoutSeconds == expectedTimeoutSeconds, 0);
    g_return_val_if_fail(g_list_length(inputDeviceClasses) == g_list_length(expectedDeviceClasses), 0);
    g_return_val_if_fail(compareStringGLists(inputDeviceClasses, expectedDeviceClasses), 0);

    g_list_free_full(inputDeviceClasses, g_free);
    g_list_free_full(expectedDeviceClasses, g_free);

    return 1;
}

static bool checkBDeviceServiceStatusContents(BDeviceServiceStatus *inputStatus, BDeviceServiceStatus *expectedStatus)
{
    GList *inputDeviceClasses = NULL;
    GList *expectedDeviceClasses = NULL;
    BDeviceServiceDiscoveryType inputDiscoveryType = 0;
    BDeviceServiceDiscoveryType expectedDiscoveryType = 0;
    GList *inputSearchingDeviceClasses = NULL;
    GList *expectedSearchingDeviceClasses = NULL;
    guint32 inputDiscoverySeconds = 0;
    guint32 expectedDiscoverySeconds = 0;
    gboolean inputReadyForOperation = false;
    gboolean expectedReadyForOperation = false;
    gboolean inputReadyForPairing = false;
    gboolean expectedReadyForPairing = false;
    g_autoptr(GHashTable) inputSubsystems = NULL;
    g_autoptr(GHashTable) expectedSubsystems = NULL;
    g_autofree gchar *inputJson = NULL;
    g_autofree gchar *expectedJson = NULL;

    g_object_get(inputStatus,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_DEVICE_CLASSES],
                 &inputDeviceClasses,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_DISCOVERY_TYPE],
                 &inputDiscoveryType,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_SEARCHING_DEVICE_CLASSES],
                 &inputSearchingDeviceClasses,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_DISCOVERY_SECONDS],
                 &inputDiscoverySeconds,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_READY_FOR_OPERATION],
                 &inputReadyForOperation,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_READY_FOR_PAIRING],
                 &inputReadyForPairing,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_SUBSYSTEMS],
                 &inputSubsystems,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_JSON],
                 &inputJson,
                 NULL);

    g_object_get(expectedStatus,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_DEVICE_CLASSES],
                 &expectedDeviceClasses,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_DISCOVERY_TYPE],
                 &expectedDiscoveryType,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_SEARCHING_DEVICE_CLASSES],
                 &expectedSearchingDeviceClasses,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_DISCOVERY_SECONDS],
                 &expectedDiscoverySeconds,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_READY_FOR_OPERATION],
                 &expectedReadyForOperation,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_READY_FOR_PAIRING],
                 &expectedReadyForPairing,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_SUBSYSTEMS],
                 &expectedSubsystems,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_JSON],
                 &expectedJson,
                 NULL);

    g_return_val_if_fail(inputDiscoveryType == expectedDiscoveryType, false);
    g_return_val_if_fail(inputDiscoverySeconds == expectedDiscoverySeconds, false);
    g_return_val_if_fail(inputReadyForOperation == expectedReadyForOperation, false);
    g_return_val_if_fail(inputReadyForPairing == expectedReadyForPairing, false);
    g_return_val_if_fail(g_hash_table_size(inputSubsystems) == g_hash_table_size(expectedSubsystems), false);
    g_return_val_if_fail(g_strcmp0(inputJson, expectedJson) == 0, false);
    g_return_val_if_fail(g_list_length(inputDeviceClasses) == g_list_length(expectedDeviceClasses), false);
    g_return_val_if_fail(g_list_length(inputSearchingDeviceClasses) == g_list_length(expectedSearchingDeviceClasses),
                         false);
    g_return_val_if_fail(compareStringGLists(inputDeviceClasses, expectedDeviceClasses), false);
    g_return_val_if_fail(compareStringGLists(inputSearchingDeviceClasses, expectedSearchingDeviceClasses), false);

    g_list_free_full(inputDeviceClasses, g_free);
    g_list_free_full(expectedDeviceClasses, g_free);
    g_list_free_full(inputSearchingDeviceClasses, g_free);
    g_list_free_full(expectedSearchingDeviceClasses, g_free);

    return true;
}

static int checkBDeviceServiceStatusEventContents(BDeviceServiceStatusEvent *input, BDeviceServiceStatusEvent *expected)
{
    BDeviceServiceStatus *inputStatus = NULL;
    BDeviceServiceStatus *expectedStatus = NULL;
    BDeviceServiceStatusChangedReason inputReason = 0;
    BDeviceServiceStatusChangedReason expectedReason = 0;

    g_object_get(input,
                 B_DEVICE_SERVICE_STATUS_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_EVENT_PROP_STATUS],
                 &inputStatus,
                 B_DEVICE_SERVICE_STATUS_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_EVENT_PROP_REASON],
                 &inputReason,
                 NULL);
    g_object_get(expected,
                 B_DEVICE_SERVICE_STATUS_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_EVENT_PROP_STATUS],
                 &expectedStatus,
                 B_DEVICE_SERVICE_STATUS_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_EVENT_PROP_REASON],
                 &expectedReason,
                 NULL);

    g_return_val_if_fail(inputReason == expectedReason, 0);
    g_return_val_if_fail(checkBDeviceServiceStatusContents(inputStatus, expectedStatus) == true, 0);

    return 1;
}

static int checkBDeviceServiceDeviceDiscoveryDetailsContents(BDeviceServiceDeviceFoundDetails *input,
                                                             BDeviceServiceDeviceFoundDetails *expected)
{
    if (input == NULL && expected == NULL)
    {
        return 1;
    }

    g_autofree gchar *inputUuid = NULL;
    g_autofree gchar *expectedUuid = NULL;
    g_autofree gchar *inputManufacturer = NULL;
    g_autofree gchar *expectedManufacturer = NULL;
    g_autofree gchar *inputModel = NULL;
    g_autofree gchar *expectedModel = NULL;
    g_autofree gchar *inputHardwareVersion = NULL;
    g_autofree gchar *expectedHardwareVersion = NULL;
    g_autofree gchar *inputFirmwareVersion = NULL;
    g_autofree gchar *expectedFirmwareVersion = NULL;
    g_autofree gchar *inputDeviceClass = NULL;
    g_autofree gchar *expectedDeviceClass = NULL;
    g_autoptr(GHashTable) inputMetadata = NULL;
    g_autoptr(GHashTable) expectedMetadata = NULL;
    g_autoptr(GHashTable) inputEndpointProfiles = NULL;
    g_autoptr(GHashTable) expectedEndpointProfiles = NULL;

    g_object_get(
        input,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_UUID],
        &inputUuid,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_MANUFACTURER],
        &inputManufacturer,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_MODEL],
        &inputModel,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_HARDWARE_VERSION],
        &inputHardwareVersion,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_FIRMWARE_VERSION],
        &inputFirmwareVersion,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_DEVICE_CLASS],
        &inputDeviceClass,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_METADATA],
        &inputMetadata,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_ENDPOINT_PROFILES],
        &inputEndpointProfiles,
        NULL);

    g_object_get(
        expected,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_UUID],
        &expectedUuid,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_MANUFACTURER],
        &expectedManufacturer,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_MODEL],
        &expectedModel,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_HARDWARE_VERSION],
        &expectedHardwareVersion,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_FIRMWARE_VERSION],
        &expectedFirmwareVersion,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_DEVICE_CLASS],
        &expectedDeviceClass,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_METADATA],
        &expectedMetadata,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_ENDPOINT_PROFILES],
        &expectedEndpointProfiles,
        NULL);

    g_return_val_if_fail(g_strcmp0(inputUuid, expectedUuid) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputManufacturer, expectedManufacturer) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputModel, expectedModel) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputHardwareVersion, expectedHardwareVersion) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputFirmwareVersion, expectedFirmwareVersion) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputDeviceClass, expectedDeviceClass) == 0, 0);
    g_return_val_if_fail(g_hash_table_size(inputMetadata) == g_hash_table_size(expectedMetadata), 0);
    g_return_val_if_fail(g_hash_table_size(inputEndpointProfiles) == g_hash_table_size(expectedEndpointProfiles), 0);

    GHashTableIter inputMetadataIter;
    GHashTableIter expectedMetadataIter;
    g_hash_table_iter_init(&inputMetadataIter, inputMetadata);
    const gchar *inputMetadataKey = NULL;
    const gchar *inputMetadataValue = NULL;
    while (g_hash_table_iter_next(&inputMetadataIter, (gpointer *) &inputMetadataKey, (gpointer *) &inputMetadataValue))
    {
        const gchar *expectedMetadataValue = g_hash_table_lookup(expectedMetadata, inputMetadataKey);
        g_return_val_if_fail(g_strcmp0(inputMetadataValue, expectedMetadataValue) == 0, 0);
    }

    GHashTableIter inputEndpointProfilesIter;
    GHashTableIter expectedEndpointProfilesIter;
    g_hash_table_iter_init(&inputEndpointProfilesIter, inputEndpointProfiles);
    const gchar *inputEndpointProfilesKey = NULL;
    const gchar *inputEndpointProfilesValue = NULL;
    while (g_hash_table_iter_next(
        &inputEndpointProfilesIter, (gpointer *) &inputEndpointProfilesKey, (gpointer *) &inputEndpointProfilesValue))
    {
        const gchar *expectedEndpointProfilesValue =
            g_hash_table_lookup(expectedEndpointProfiles, inputEndpointProfilesKey);
        g_return_val_if_fail(g_strcmp0(inputEndpointProfilesValue, expectedEndpointProfilesValue) == 0, 0);
    }

    return 1;
}

static int checkBDeviceServiceDeviceDiscoveryFailedEventContents(BDeviceServiceDeviceDiscoveryFailedEvent *input,
                                                                 BDeviceServiceDeviceDiscoveryFailedEvent *expected)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_DEVICE_DISCOVERY_FAILED_EVENT(input), 0);

    g_autoptr(BDeviceServiceDeviceFoundDetails) inputDeviceFoundDetails = NULL;
    g_autoptr(BDeviceServiceDeviceFoundDetails) expectedDeviceFoundDetails = NULL;
    BDeviceServiceDiscoveryType inputDiscoveryType = 0;
    BDeviceServiceDiscoveryType expectedDiscoveryType = 0;

    g_object_get(input,
                 B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 &inputDeviceFoundDetails,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 &inputDiscoveryType,
                 NULL);
    g_object_get(expected,
                 B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 &expectedDeviceFoundDetails,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 &expectedDiscoveryType,
                 NULL);

    g_return_val_if_fail(inputDiscoveryType == expectedDiscoveryType, 0);

    return checkBDeviceServiceDeviceDiscoveryDetailsContents(inputDeviceFoundDetails, expectedDeviceFoundDetails);
}

static int checkBDeviceServiceDeviceDiscoveredEventContents(BDeviceServiceDeviceDiscoveredEvent *input,
                                                            BDeviceServiceDeviceDiscoveredEvent *expected)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_DEVICE_DISCOVERED_EVENT(input), 0);

    g_autoptr(BDeviceServiceDeviceFoundDetails) inputDeviceFoundDetails = NULL;
    g_autoptr(BDeviceServiceDeviceFoundDetails) expectedDeviceFoundDetails = NULL;
    BDeviceServiceDiscoveryType inputDiscoveryType = 0;
    BDeviceServiceDiscoveryType expectedDiscoveryType = 0;

    g_object_get(input,
                 B_DEVICE_SERVICE_DEVICE_DISCOVERED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DISCOVERED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 &inputDeviceFoundDetails,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 &inputDiscoveryType,
                 NULL);

    g_object_get(expected,
                 B_DEVICE_SERVICE_DEVICE_DISCOVERED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DISCOVERED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 &expectedDeviceFoundDetails,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 &expectedDiscoveryType,
                 NULL);

    g_return_val_if_fail(inputDiscoveryType == expectedDiscoveryType, 0);

    return checkBDeviceServiceDeviceDiscoveryDetailsContents(inputDeviceFoundDetails, expectedDeviceFoundDetails);
}

static int checkBDeviceServiceDeviceRejectedEventContents(BDeviceServiceDeviceRejectedEvent *input,
                                                          BDeviceServiceDeviceRejectedEvent *expected)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_DEVICE_REJECTED_EVENT(input), 0);

    g_autoptr(BDeviceServiceDeviceFoundDetails) inputDeviceFoundDetails = NULL;
    g_autoptr(BDeviceServiceDeviceFoundDetails) expectedDeviceFoundDetails = NULL;
    BDeviceServiceDiscoveryType inputDiscoveryType = 0;
    BDeviceServiceDiscoveryType expectedDiscoveryType = 0;

    g_object_get(input,
                 B_DEVICE_SERVICE_DEVICE_REJECTED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_REJECTED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 &inputDeviceFoundDetails,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 &inputDiscoveryType,
                 NULL);

    g_object_get(expected,
                 B_DEVICE_SERVICE_DEVICE_REJECTED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_REJECTED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 &expectedDeviceFoundDetails,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 &expectedDiscoveryType,
                 NULL);

    g_return_val_if_fail(inputDiscoveryType == expectedDiscoveryType, 0);

    return checkBDeviceServiceDeviceDiscoveryDetailsContents(inputDeviceFoundDetails, expectedDeviceFoundDetails);
}

static int
checkBDeviceServiceDeviceConfigurationStartedEventContents(BDeviceServiceDeviceConfigurationStartedEvent *input,
                                                           BDeviceServiceDeviceConfigurationStartedEvent *expected)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_DEVICE_CONFIGURATION_STARTED_EVENT(input), 0);

    g_autofree gchar *inputUUID = NULL;
    g_autofree gchar *expectedUUID = NULL;
    g_autofree gchar *inputDeviceClass = NULL;
    g_autofree gchar *expectedDeviceClass = NULL;
    BDeviceServiceDiscoveryType inputDiscoveryType = 0;
    BDeviceServiceDiscoveryType expectedDiscoveryType = 0;

    g_object_get(input,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 &inputUUID,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 &inputDeviceClass,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 &inputDiscoveryType,
                 NULL);

    g_object_get(expected,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 &expectedUUID,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 &expectedDeviceClass,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 &expectedDiscoveryType,
                 NULL);

    g_return_val_if_fail(g_strcmp0(inputUUID, expectedUUID) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputDeviceClass, expectedDeviceClass) == 0, 0);
    g_return_val_if_fail(inputDiscoveryType == expectedDiscoveryType, 0);

    return 1;
}

static int
checkBDeviceServiceDeviceConfigurationCompletedEventContents(BDeviceServiceDeviceConfigurationCompletedEvent *input,
                                                             BDeviceServiceDeviceConfigurationCompletedEvent *expected)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_DEVICE_CONFIGURATION_COMPLETED_EVENT(input), 0);

    g_autofree gchar *inputUUID = NULL;
    g_autofree gchar *expectedUUID = NULL;
    g_autofree gchar *inputDeviceClass = NULL;
    g_autofree gchar *expectedDeviceClass = NULL;
    BDeviceServiceDiscoveryType inputDiscoveryType = 0;
    BDeviceServiceDiscoveryType expectedDiscoveryType = 0;

    g_object_get(input,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 &inputUUID,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 &inputDeviceClass,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 &inputDiscoveryType,
                 NULL);

    g_object_get(expected,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 &expectedUUID,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 &expectedDeviceClass,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 &expectedDiscoveryType,
                 NULL);

    g_return_val_if_fail(g_strcmp0(inputUUID, expectedUUID) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputDeviceClass, expectedDeviceClass) == 0, 0);
    g_return_val_if_fail(inputDiscoveryType == expectedDiscoveryType, 0);

    return 1;
}

static int
checkBDeviceServiceDeviceConfigurationFailedEventContents(BDeviceServiceDeviceConfigurationFailedEvent *input,
                                                          BDeviceServiceDeviceConfigurationFailedEvent *expected)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_DEVICE_CONFIGURATION_FAILED_EVENT(input), 0);

    g_autofree gchar *inputUUID = NULL;
    g_autofree gchar *expectedUUID = NULL;
    g_autofree gchar *inputDeviceClass = NULL;
    g_autofree gchar *expectedDeviceClass = NULL;
    BDeviceServiceDiscoveryType inputDiscoveryType = 0;
    BDeviceServiceDiscoveryType expectedDiscoveryType = 0;

    g_object_get(input,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 &inputUUID,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 &inputDeviceClass,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 &inputDiscoveryType,
                 NULL);

    g_object_get(expected,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 &expectedUUID,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 &expectedDeviceClass,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 &expectedDiscoveryType,
                 NULL);

    g_return_val_if_fail(g_strcmp0(inputUUID, expectedUUID) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputDeviceClass, expectedDeviceClass) == 0, 0);
    g_return_val_if_fail(inputDiscoveryType == expectedDiscoveryType, 0);

    return 1;
}

static int checkBDeviceServiceDeviceAddedEventContents(BDeviceServiceDeviceAddedEvent *input,
                                                       BDeviceServiceDeviceAddedEvent *expected)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_DEVICE_ADDED_EVENT(input), 0);

    g_autofree gchar *inputUUID = NULL;
    g_autofree gchar *expectedUUID = NULL;
    g_autofree gchar *inputUri = NULL;
    g_autofree gchar *expectedUri = NULL;
    g_autofree gchar *inputDeviceClass = NULL;
    g_autofree gchar *expectedDeviceClass = NULL;
    guint8 inputDeviceClassVersion = 0;
    guint8 expectedDeviceClassVersion = 0;

    g_object_get(
        input,
        B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_UUID],
        &inputUUID,
        B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_URI],
        &inputUri,
        B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS],
        &inputDeviceClass,
        B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS_VERSION],
        &inputDeviceClassVersion,
        NULL);

    g_object_get(
        expected,
        B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_UUID],
        &expectedUUID,
        B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_URI],
        &expectedUri,
        B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS],
        &expectedDeviceClass,
        B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS_VERSION],
        &expectedDeviceClassVersion,
        NULL);

    g_return_val_if_fail(g_strcmp0(inputUUID, expectedUUID) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputUri, expectedUri) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputDeviceClass, expectedDeviceClass) == 0, 0);
    g_return_val_if_fail(inputDeviceClassVersion == expectedDeviceClassVersion, 0);

    return 1;
}

static int checkBDeviceServiceDeviceRecoveredEventContents(BDeviceServiceDeviceRecoveredEvent *input,
                                                           BDeviceServiceDeviceRecoveredEvent *expected)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_DEVICE_RECOVERED_EVENT(input), 0);

    g_autofree gchar *inputUUID = NULL;
    g_autofree gchar *expectedUUID = NULL;
    g_autofree gchar *inputUri = NULL;
    g_autofree gchar *expectedUri = NULL;
    g_autofree gchar *inputDeviceClass = NULL;
    g_autofree gchar *expectedDeviceClass = NULL;
    guint8 inputDeviceClassVersion = 0;
    guint8 expectedDeviceClassVersion = 0;

    g_object_get(
        input,
        B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROP_UUID],
        &inputUUID,
        B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROP_URI],
        &inputUri,
        B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROP_DEVICE_CLASS],
        &inputDeviceClass,
        B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROP_DEVICE_CLASS_VERSION],
        &inputDeviceClassVersion,
        NULL);

    g_object_get(
        expected,
        B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROP_UUID],
        &expectedUUID,
        B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROP_URI],
        &expectedUri,
        B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROP_DEVICE_CLASS],
        &expectedDeviceClass,
        B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROP_DEVICE_CLASS_VERSION],
        &expectedDeviceClassVersion,
        NULL);

    g_return_val_if_fail(g_strcmp0(inputUUID, expectedUUID) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputUri, expectedUri) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputDeviceClass, expectedDeviceClass) == 0, 0);
    g_return_val_if_fail(inputDeviceClassVersion == expectedDeviceClassVersion, 0);

    return 1;
}

static int
checkBDeviceServiceDeviceDiscoveryCompletedEventContents(BDeviceServiceDeviceDiscoveryCompletedEvent *input,
                                                         BDeviceServiceDeviceDiscoveryCompletedEvent *expected)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_DEVICE_DISCOVERY_COMPLETED_EVENT(input), 0);

    g_autoptr(BDeviceServiceDevice) inputDevice = NULL;
    g_autoptr(BDeviceServiceDevice) expectedDevice = NULL;
    BDeviceServiceDiscoveryType inputDiscoveryType = 0;
    BDeviceServiceDiscoveryType expectedDiscoveryType = 0;

    g_object_get(input,
                 B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROP_DEVICE],
                 &inputDevice,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 &inputDiscoveryType,
                 NULL);

    g_object_get(expected,
                 B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROP_DEVICE],
                 &expectedDevice,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 &expectedDiscoveryType,
                 NULL);

    g_return_val_if_fail(inputDiscoveryType == expectedDiscoveryType, 0);

    return checkBDeviceServiceDeviceContents(inputDevice, expectedDevice);
}

static int checkBDeviceServiceDeviceContents(BDeviceServiceDevice *input, BDeviceServiceDevice *expected)
{
    if (input == NULL && expected == NULL)
    {
        return 1;
    }

    g_return_val_if_fail(B_DEVICE_SERVICE_IS_DEVICE(input), 0);

    g_autofree gchar *inputUuid = NULL;
    g_autofree gchar *expectedUuid = NULL;
    g_autofree gchar *inputDeviceClass = NULL;
    g_autofree gchar *expectedDeviceClass = NULL;
    guint8 inputDeviceClassVersion = 0;
    guint8 expectedDeviceClassVersion = 0;
    g_autofree gchar *inputUri = NULL;
    g_autofree gchar *expectedUri = NULL;
    g_autofree gchar *inputManagingDeviceDriver = NULL;
    g_autofree gchar *expectedManagingDeviceDriver = NULL;
    g_autolist(BDeviceServiceEndpoint) inputEndpoints = NULL;
    g_autolist(BDeviceServiceEndpoint) expectedEndpoints = NULL;
    g_autolist(BDeviceServiceResource) inputResources = NULL;
    g_autolist(BDeviceServiceResource) expectedResources = NULL;
    g_autolist(BDeviceServiceMetadata) inputMetadata = NULL;
    g_autolist(BDeviceServiceMetadata) expectedMetadata = NULL;

    g_object_get(input,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_UUID],
                 &inputUuid,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS],
                 &inputDeviceClass,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS_VERSION],
                 &inputDeviceClassVersion,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_URI],
                 &inputUri,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_MANAGING_DEVICE_DRIVER],
                 &inputManagingDeviceDriver,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_ENDPOINTS],
                 &inputEndpoints,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_RESOURCES],
                 &inputResources,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_METADATA],
                 &inputMetadata,
                 NULL);

    g_object_get(expected,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_UUID],
                 &expectedUuid,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS],
                 &expectedDeviceClass,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS_VERSION],
                 &expectedDeviceClassVersion,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_URI],
                 &expectedUri,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_MANAGING_DEVICE_DRIVER],
                 &expectedManagingDeviceDriver,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_ENDPOINTS],
                 &expectedEndpoints,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_RESOURCES],
                 &expectedResources,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_METADATA],
                 &expectedMetadata,
                 NULL);

    g_return_val_if_fail(g_strcmp0(inputUuid, expectedUuid) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputDeviceClass, expectedDeviceClass) == 0, 0);
    g_return_val_if_fail(inputDeviceClassVersion == expectedDeviceClassVersion, 0);
    g_return_val_if_fail(g_strcmp0(inputUri, expectedUri) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputManagingDeviceDriver, expectedManagingDeviceDriver) == 0, 0);
    g_return_val_if_fail(g_list_length(inputEndpoints) == g_list_length(expectedEndpoints), 0);
    g_return_val_if_fail(g_list_length(inputResources) == g_list_length(expectedResources), 0);
    g_return_val_if_fail(g_list_length(inputMetadata) == g_list_length(expectedMetadata), 0);

    for (GList *currInput = inputEndpoints, *currExpected = expectedEndpoints;
         currInput != NULL && currExpected != NULL;
         currInput = currInput->next, currExpected = currExpected->next)
    {
        BDeviceServiceEndpoint *currInputEndpoint = currInput->data;
        BDeviceServiceEndpoint *currExpectedEndpoint = currExpected->data;

        if (checkBDeviceServiceEndpointContents(currInputEndpoint, currExpectedEndpoint) == 0)
        {
            return 0;
        }
    }

    for (GList *currInput = inputResources, *currExpected = expectedResources;
         currInput != NULL && currExpected != NULL;
         currInput = currInput->next, currExpected = currExpected->next)
    {
        BDeviceServiceResource *currInputResource = currInput->data;
        BDeviceServiceResource *currExpectedResource = currExpected->data;

        if (checkBDeviceServiceResourceContents(currInputResource, currExpectedResource) == 0)
        {
            return 0;
        }
    }

    for (GList *currInput = inputMetadata, *currExpected = expectedMetadata; currInput != NULL && currExpected != NULL;
         currInput = currInput->next, currExpected = currExpected->next)
    {
        BDeviceServiceMetadata *currInputMetadata = currInput->data;
        BDeviceServiceMetadata *currExpectedMetadata = currExpected->data;

        if (checkBDeviceServiceMetadataContents(currInputMetadata, currExpectedMetadata) == 0)
        {
            return 0;
        }
    }

    return 1;
}

static int checkBDeviceServiceEndpointContents(BDeviceServiceEndpoint *input, BDeviceServiceEndpoint *expected)
{
    if (input == NULL && expected == NULL)
    {
        return 1;
    }

    g_return_val_if_fail(B_DEVICE_SERVICE_IS_ENDPOINT(input), 0);

    g_autofree gchar *inputId = NULL;
    g_autofree gchar *expectedId = NULL;
    g_autofree gchar *inputUri = NULL;
    g_autofree gchar *expectedUri = NULL;
    g_autofree gchar *inputProfile = NULL;
    g_autofree gchar *expectedProfile = NULL;
    guint8 inputProfileVersion = 0;
    guint8 expectedProfileVersion = 0;
    g_autofree gchar *inputDeviceUuid = NULL;
    g_autofree gchar *expectedDeviceUuid = NULL;
    gboolean inputEnabled = FALSE;
    gboolean expectedEnabled = FALSE;
    g_autolist(BDeviceServiceResource) inputResources = NULL;
    g_autolist(BDeviceServiceResource) expectedResources = NULL;
    g_autolist(BDeviceServiceMetadata) inputMetadata = NULL;
    g_autolist(BDeviceServiceMetadata) expectedMetadata = NULL;

    g_object_get(input,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_ID],
                 &inputId,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_URI],
                 &inputUri,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE],
                 &inputProfile,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE_VERSION],
                 &inputProfileVersion,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_DEVICE_UUID],
                 &inputDeviceUuid,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_ENABLED],
                 &inputEnabled,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_RESOURCES],
                 &inputResources,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_METADATA],
                 &inputMetadata,
                 NULL);

    g_object_get(expected,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_ID],
                 &expectedId,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_URI],
                 &expectedUri,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE],
                 &expectedProfile,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE_VERSION],
                 &expectedProfileVersion,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_DEVICE_UUID],
                 &expectedDeviceUuid,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_ENABLED],
                 &expectedEnabled,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_RESOURCES],
                 &expectedResources,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_METADATA],
                 &expectedMetadata,
                 NULL);

    g_return_val_if_fail(g_strcmp0(inputId, expectedId) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputUri, expectedUri) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputProfile, expectedProfile) == 0, 0);
    g_return_val_if_fail(inputProfileVersion == expectedProfileVersion, 0);
    g_return_val_if_fail(g_strcmp0(inputDeviceUuid, expectedDeviceUuid) == 0, 0);
    g_return_val_if_fail(inputEnabled == expectedEnabled, 0);
    g_return_val_if_fail(g_list_length(inputResources) == g_list_length(expectedResources), 0);
    g_return_val_if_fail(g_list_length(inputMetadata) == g_list_length(expectedMetadata), 0);

    for (GList *currInput = inputResources, *currExpected = expectedResources;
         currInput != NULL && currExpected != NULL;
         currInput = currInput->next, currExpected = currExpected->next)
    {
        BDeviceServiceResource *currInputResource = currInput->data;
        BDeviceServiceResource *currExpectedResource = currExpected->data;

        if (checkBDeviceServiceResourceContents(currInputResource, currExpectedResource) == 0)
        {
            return 0;
        }
    }

    for (GList *currInput = inputMetadata, *currExpected = expectedMetadata; currInput != NULL && currExpected != NULL;
         currInput = currInput->next, currExpected = currExpected->next)
    {
        BDeviceServiceMetadata *currInputMetadata = currInput->data;
        BDeviceServiceMetadata *currExpectedMetadata = currExpected->data;

        if (checkBDeviceServiceMetadataContents(currInputMetadata, currExpectedMetadata) == 0)
        {
            return 0;
        }
    }

    return 1;
}

static int checkBDeviceServiceResourceContents(BDeviceServiceResource *input, BDeviceServiceResource *expected)
{
    if (input == NULL && expected == NULL)
    {
        return 1;
    }

    g_return_val_if_fail(B_DEVICE_SERVICE_IS_RESOURCE(input), 0);

    g_autofree gchar *inputId = NULL;
    g_autofree gchar *expectedId = NULL;
    g_autofree gchar *inputUri = NULL;
    g_autofree gchar *expectedUri = NULL;
    g_autofree gchar *inputEndpointId = NULL;
    g_autofree gchar *expectedEndpointId = NULL;
    g_autofree gchar *inputDeviceUuid = NULL;
    g_autofree gchar *expectedDeviceUuid = NULL;
    g_autofree gchar *inputValue = NULL;
    g_autofree gchar *expectedValue = NULL;
    g_autofree gchar *inputType = NULL;
    g_autofree gchar *expectedType = NULL;
    guint8 inputMode = 0;
    guint8 expectedMode = 0;
    BDeviceServiceResourceCachingPolicy inputCachingPolicy = 0;
    BDeviceServiceResourceCachingPolicy expectedCachingPolicy = 0;
    guint64 inputDateOfLastSyncMillis = 0;
    guint64 expectedDateOfLastSyncMillis = 0;

    g_object_get(input,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_ID],
                 &inputId,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_URI],
                 &inputUri,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_ENDPOINT_ID],
                 &inputEndpointId,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_DEVICE_UUID],
                 &inputDeviceUuid,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_VALUE],
                 &inputValue,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_TYPE],
                 &inputType,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_MODE],
                 &inputMode,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_CACHING_POLICY],
                 &inputCachingPolicy,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_DATE_OF_LAST_SYNC_MILLIS],
                 &inputDateOfLastSyncMillis,
                 NULL);

    g_object_get(expected,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_ID],
                 &expectedId,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_URI],
                 &expectedUri,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_ENDPOINT_ID],
                 &expectedEndpointId,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_DEVICE_UUID],
                 &expectedDeviceUuid,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_VALUE],
                 &expectedValue,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_TYPE],
                 &expectedType,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_MODE],
                 &expectedMode,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_CACHING_POLICY],
                 &expectedCachingPolicy,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_DATE_OF_LAST_SYNC_MILLIS],
                 &expectedDateOfLastSyncMillis,
                 NULL);

    g_return_val_if_fail(g_strcmp0(inputId, expectedId) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputUri, expectedUri) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputEndpointId, expectedEndpointId) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputDeviceUuid, expectedDeviceUuid) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputValue, expectedValue) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputType, expectedType) == 0, 0);
    g_return_val_if_fail(inputMode == expectedMode, 0);
    g_return_val_if_fail(inputCachingPolicy == expectedCachingPolicy, 0);
    g_return_val_if_fail(inputDateOfLastSyncMillis == expectedDateOfLastSyncMillis, 0);

    return 1;
}

static int checkBDeviceServiceMetadataContents(BDeviceServiceMetadata *input, BDeviceServiceMetadata *expected)
{
    if (input == NULL && expected == NULL)
    {
        return 1;
    }

    g_return_val_if_fail(B_DEVICE_SERVICE_IS_METADATA(input), 0);

    g_autofree gchar *inputId = NULL;
    g_autofree gchar *expectedId = NULL;
    g_autofree gchar *inputUri = NULL;
    g_autofree gchar *expectedUri = NULL;
    g_autofree gchar *inputEndpointId = NULL;
    g_autofree gchar *expectedEndpointId = NULL;
    g_autofree gchar *inputDeviceUuid = NULL;
    g_autofree gchar *expectedDeviceUuid = NULL;
    g_autofree gchar *inputValue = NULL;
    g_autofree gchar *expectedValue = NULL;

    g_object_get(input,
                 B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_ID],
                 &inputId,
                 B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_URI],
                 &inputUri,
                 B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_ENDPOINT_ID],
                 &inputEndpointId,
                 B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_DEVICE_UUID],
                 &inputDeviceUuid,
                 B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_VALUE],
                 &inputValue,
                 NULL);

    g_object_get(expected,
                 B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_ID],
                 &expectedId,
                 B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_URI],
                 &expectedUri,
                 B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_ENDPOINT_ID],
                 &expectedEndpointId,
                 B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_DEVICE_UUID],
                 &expectedDeviceUuid,
                 B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_VALUE],
                 &expectedValue,
                 NULL);

    g_return_val_if_fail(g_strcmp0(inputId, expectedId) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputUri, expectedUri) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputEndpointId, expectedEndpointId) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputDeviceUuid, expectedDeviceUuid) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputValue, expectedValue) == 0, 0);

    return 1;
}

static int checkBDeviceServiceDiscoveryStoppedEventContents(BDeviceServiceDiscoveryStoppedEvent *input,
                                                            BDeviceServiceDiscoveryStoppedEvent *expected)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_DISCOVERY_STOPPED_EVENT(input), 0);

    g_autofree gchar *inputDeviceClass = NULL;
    g_autofree gchar *expectedDeviceClass = NULL;

    g_object_get(input,
                 B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS],
                 &inputDeviceClass,
                 NULL);
    g_object_get(expected,
                 B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS],
                 &expectedDeviceClass,
                 NULL);

    g_return_val_if_fail(g_strcmp0(inputDeviceClass, expectedDeviceClass) == 0, 0);

    return 1;
}

static int checkBDeviceServiceRecoveryStoppedEventContents(BDeviceServiceRecoveryStoppedEvent *input,
                                                           BDeviceServiceRecoveryStoppedEvent *expected)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_RECOVERY_STOPPED_EVENT(input), 0);

    g_autofree gchar *inputDeviceClass = NULL;
    g_autofree gchar *expectedDeviceClass = NULL;

    g_object_get(input,
                 B_DEVICE_SERVICE_RECOVERY_STOPPED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_RECOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS],
                 &inputDeviceClass,
                 NULL);
    g_object_get(expected,
                 B_DEVICE_SERVICE_RECOVERY_STOPPED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_RECOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS],
                 &expectedDeviceClass,
                 NULL);

    g_return_val_if_fail(g_strcmp0(inputDeviceClass, expectedDeviceClass) == 0, 0);

    return 1;
}

static int checkBDeviceServiceResourceUpdatedEventContents(BDeviceServiceResourceUpdatedEvent *input,
                                                           BDeviceServiceResourceUpdatedEvent *expected)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_RESOURCE_UPDATED_EVENT(input), 0);

    g_autoptr(BDeviceServiceResource) inputResource = NULL;
    g_autoptr(BDeviceServiceResource) expectedResource = NULL;
    g_autofree gchar *inputEventMetadata = NULL;
    g_autofree gchar *expectedEventMetadata = NULL;

    g_object_get(
        input,
        B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_RESOURCE],
        &inputResource,
        B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_METADATA],
        &inputEventMetadata,
        NULL);

    g_object_get(
        expected,
        B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_RESOURCE],
        &expectedResource,
        B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_METADATA],
        &expectedEventMetadata,
        NULL);

    g_return_val_if_fail(g_strcmp0(inputEventMetadata, expectedEventMetadata) == 0, 0);

    return checkBDeviceServiceResourceContents(inputResource, expectedResource);
}

static int checkBDeviceServiceDeviceRemovedEventContents(BDeviceServiceDeviceRemovedEvent *input,
                                                         BDeviceServiceDeviceRemovedEvent *expected)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_DEVICE_REMOVED_EVENT(input), 0);

    g_autofree gchar *inputUuid = NULL;
    g_autofree gchar *expectedUuid = NULL;
    g_autofree gchar *inputDeviceClass = NULL;
    g_autofree gchar *expectedDeviceClass = NULL;

    g_object_get(
        input,
        B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_UUID],
        &inputUuid,
        B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_CLASS],
        &inputDeviceClass,
        NULL);
    g_object_get(
        expected,
        B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_UUID],
        &expectedUuid,
        B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_CLASS],
        &expectedDeviceClass,
        NULL);

    g_return_val_if_fail(g_strcmp0(inputUuid, expectedUuid) == 0, 0);
    g_return_val_if_fail(g_strcmp0(inputDeviceClass, expectedDeviceClass) == 0, 0);

    return 1;
}

static int checkBDeviceServiceEndpointRemovedEventContents(BDeviceServiceEndpointRemovedEvent *input,
                                                           BDeviceServiceEndpointRemovedEvent *expected)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_ENDPOINT_REMOVED_EVENT(input), 0);

    g_autoptr(BDeviceServiceEndpoint) inputEndpoint = NULL;
    g_autoptr(BDeviceServiceEndpoint) expectedEndpoint = NULL;
    g_autofree gchar *inputDeviceClass = NULL;
    g_autofree gchar *expectedDeviceClass = NULL;
    gboolean inputDeviceRemoved = FALSE;
    gboolean expectedDeviceRemoved = FALSE;

    g_object_get(
        input,
        B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_ENDPOINT],
        &inputEndpoint,
        B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_CLASS],
        &inputDeviceClass,
        B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_REMOVED],
        &inputDeviceRemoved,
        NULL);
    g_object_get(
        expected,
        B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_ENDPOINT],
        &expectedEndpoint,
        B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_CLASS],
        &expectedDeviceClass,
        B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_REMOVED],
        &expectedDeviceRemoved,
        NULL);

    g_return_val_if_fail(g_strcmp0(inputDeviceClass, expectedDeviceClass) == 0, 0);
    g_return_val_if_fail(inputDeviceRemoved == expectedDeviceRemoved, 0);

    return checkBDeviceServiceEndpointContents(inputEndpoint, expectedEndpoint);
}

static int checkBDeviceServiceEndpointAddedEventContents(BDeviceServiceEndpointAddedEvent *input,
                                                         BDeviceServiceEndpointAddedEvent *expected)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_ENDPOINT_ADDED_EVENT(input), 0);

    g_autoptr(BDeviceServiceEndpoint) inputEndpoint = NULL;
    g_autoptr(BDeviceServiceEndpoint) expectedEndpoint = NULL;
    g_autofree gchar *inputDeviceClass = NULL;
    g_autofree gchar *expectedDeviceClass = NULL;

    g_object_get(
        input,
        B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROP_ENDPOINT],
        &inputEndpoint,
        B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROP_DEVICE_CLASS],
        &inputDeviceClass,
        NULL);

    g_object_get(
        expected,
        B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROP_ENDPOINT],
        &expectedEndpoint,
        B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROP_DEVICE_CLASS],
        &expectedDeviceClass,
        NULL);

    g_return_val_if_fail(g_strcmp0(inputDeviceClass, expectedDeviceClass) == 0, 0);

    return checkBDeviceServiceEndpointContents(inputEndpoint, expectedEndpoint);
}

static int checkBDeviceServiceZigbeeChannelChangedEventContents(BDeviceServiceZigbeeChannelChangedEvent *input,
                                                                BDeviceServiceZigbeeChannelChangedEvent *expected)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_ZIGBEE_CHANNEL_CHANGED_EVENT(input), 0);

    gboolean inputChannelChanged = false;
    gboolean expectedChannelChanged = false;
    guint inputCurrentChannel = 0;
    guint expectedCurrentChannel = 0;
    guint inputNewChannel = 0;
    guint expectedNewChannel = 0;

    g_object_get(input,
                 B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CHANNEL_CHANGED],
                 &inputChannelChanged,
                 B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CURRENT_CHANNEL],
                 &inputCurrentChannel,
                 B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_TARGETED_CHANNEL],
                 &inputNewChannel,
                 NULL);

    g_object_get(expected,
                 B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CHANNEL_CHANGED],
                 &expectedChannelChanged,
                 B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CURRENT_CHANNEL],
                 &expectedCurrentChannel,
                 B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_TARGETED_CHANNEL],
                 &expectedNewChannel,
                 NULL);

    g_return_val_if_fail(inputChannelChanged == expectedChannelChanged, 0);
    g_return_val_if_fail(inputCurrentChannel == expectedCurrentChannel, 0);
    g_return_val_if_fail(inputNewChannel == expectedNewChannel, 0);

    return 1;
}

static int checkBDeviceServiceStorageChangedEventContents(BDeviceServiceStorageChangedEvent *input,
                                                          BDeviceServiceStorageChangedEvent *expected)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_STORAGE_CHANGED_EVENT(input), 0);

    GFileMonitorEvent inputWhatChanged = 0;
    GFileMonitorEvent expectedWhatChanged = 0;

    g_object_get(
        input,
        B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROP_WHAT_CHANGED],
        &inputWhatChanged,
        NULL);

    g_object_get(
        expected,
        B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROP_WHAT_CHANGED],
        &expectedWhatChanged,
        NULL);

    g_return_val_if_fail(inputWhatChanged == expectedWhatChanged, 0);

    return 1;
}

static int checkBDeviceServiceZigbeeInterferenceEventContents(BDeviceServiceZigbeeInterferenceEvent *input,
                                                              BDeviceServiceZigbeeInterferenceEvent *expected)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_ZIGBEE_INTERFERENCE_EVENT(input), 0);

    gboolean inputInterferenceDetected = false;
    gboolean expectedInterferenceDetected = false;

    g_object_get(input,
                 B_DEVICE_SERVICE_ZIGBEE_INTERFERENCE_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_INTERFERENCE_EVENT_PROP_INTERFERENCE_DETECTED],
                 &inputInterferenceDetected,
                 NULL);

    g_object_get(expected,
                 B_DEVICE_SERVICE_ZIGBEE_INTERFERENCE_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_INTERFERENCE_EVENT_PROP_INTERFERENCE_DETECTED],
                 &expectedInterferenceDetected,
                 NULL);

    g_return_val_if_fail(inputInterferenceDetected == expectedInterferenceDetected, 0);

    return 1;
}

static int
checkBDeviceServiceZigbeePanIdAttackChangedEventContents(BDeviceServiceZigbeePanIdAttackChangedEvent *input,
                                                         BDeviceServiceZigbeePanIdAttackChangedEvent *expected)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT(input), 0);

    gboolean inputPanIdAttackDetected = false;
    gboolean expectedPanIdAttackDetected = false;

    g_object_get(input,
                 B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROP_ATTACK_DETECTED],
                 &inputPanIdAttackDetected,
                 NULL);

    g_object_get(expected,
                 B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROP_ATTACK_DETECTED],
                 &expectedPanIdAttackDetected,
                 NULL);

    g_return_val_if_fail(inputPanIdAttackDetected == expectedPanIdAttackDetected, 0);

    return 1;
}

static bool checkBDeviceServiceZigbeeEnergyScanResultChannelContents(uint8_t *input, ArrayCompareParams *expected)
{
    g_return_val_if_fail(input != NULL, false);
    g_return_val_if_fail(expected != NULL, false);

    bool retVal = true;
    uint8_t *expectedArray = (uint8_t *) expected->array;

    for (int i = 0; i < expected->arraySize; i++)
    {
        if (input[i] != expectedArray[i])
        {
            retVal = false;
            break;
        }
    }

    return retVal;
}

static int checkBDeviceServiceDeviceDatabaseFailureEventContents(BDeviceServiceDeviceDatabaseFailureEvent *input,
                                                                 BDeviceServiceDeviceDatabaseFailureEvent *expected)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT(input), 0);

    BDeviceServiceDeviceDatabaseFailureType inputFailureType = false;
    BDeviceServiceDeviceDatabaseFailureType expectedFailureType = false;
    g_autofree gchar *inputDeviceId = NULL;
    g_autofree gchar *expectedDeviceId = NULL;

    g_object_get(input,
                 B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE],
                 &inputFailureType,
                 B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_DEVICE_ID],
                 &inputDeviceId,
                 NULL);

    g_object_get(expected,
                 B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE],
                 &expectedFailureType,
                 B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_DEVICE_ID],
                 &expectedDeviceId,
                 NULL);

    g_return_val_if_fail(inputFailureType == expectedFailureType, 0);
    g_return_val_if_fail(g_strcmp0(inputDeviceId, expectedDeviceId) == 0, 0);

    return 1;
}

#ifdef BARTON_CONFIG_ZIGBEE
static void assert_change_zigbee_channel_errors(BDeviceServiceClient *client,
                                                uint8_t channel,
                                                ChannelChangeResponse channelChangeResponse,
                                                BDeviceServiceZigbeeChannelChangeError changeError)
{
    g_autoptr(GError) err = NULL;
    expect_function_call(__wrap_zigbeeSubsystemChangeChannel);
    expect_value(__wrap_zigbeeSubsystemChangeChannel, channel, channel);
    expect_value(__wrap_zigbeeSubsystemChangeChannel, dryRun, false);
    will_return(__wrap_zigbeeSubsystemChangeChannel, channelChangeResponse.channelNumber);
    will_return(__wrap_zigbeeSubsystemChangeChannel, channelChangeResponse.responseCode);

    guint8 result = b_device_service_client_change_zigbee_channel(client, channel, false, &err);
    assert_int_equal(result, channel);
    assert_non_null(err);
    assert_int_equal(err->code, changeError);
    g_clear_error(&err);
}
#endif

bool __wrap_deviceServiceInitialize(void)
{
    function_called();
    return mock();
}

void __wrap_allServicesAvailableNotify(void)
{
    function_called();
    return;
}

void __wrap_deviceServiceShutdown(void)
{
    function_called();
    return;
}

icLinkedList *__wrap_deviceServiceGetEndpointsByProfile(const char *profile)
{
    function_called();
    check_expected(profile);
    icLinkedList *result = (icLinkedList *) mock();
    return result;
}

bool __wrap_deviceServiceDiscoverStart(icLinkedList *deviceClasses,
                                       icLinkedList *filters,
                                       uint16_t timeoutSeconds,
                                       bool findOrphanedDevices)
{
    function_called();
    check_expected(deviceClasses);
    check_expected(filters);
    check_expected(timeoutSeconds);
    check_expected(findOrphanedDevices);
    return mock();
}

bool __wrap_deviceServiceDiscoverStop(icLinkedList *deviceClasses)
{
    function_called();
    check_expected(deviceClasses);
    return mock();
}

DeviceServiceStatus *__wrap_deviceServiceGetStatus(void)
{
    function_called();
    DeviceServiceStatus *mock = (DeviceServiceStatus *) mock();

    g_return_val_if_fail(mock != NULL, NULL);

    // Need to copy as caller will free and we need to keep our mock fixture around to do checks.
    g_autofree gchar *json = deviceServiceStatusToJson(mock);
    return deviceServiceStatusFromJson(json);
}

icDevice *__wrap_deviceServiceGetDevice(const char *uuid)
{
    function_called();
    check_expected(uuid);
    icDevice *mock = (icDevice *) mock();
    return mock;
}

bool __wrap_deviceServiceCommissionDevice(const char *setupPayload, uint16_t timeoutSeconds)
{
    function_called();
    check_expected(setupPayload);
    check_expected(timeoutSeconds);
    return (bool) mock();
}

bool __wrap_deviceServiceAddMatterDevice(uint64_t nodeId, uint16_t timeoutSeconds)
{
    function_called();
    check_expected(nodeId);
    check_expected(timeoutSeconds);
    return (bool) mock();
}

icDeviceEndpoint *__wrap_deviceServiceGetEndpointById(const char *deviceUuid, const char *endpointId)
{
    function_called();
    check_expected(deviceUuid);
    check_expected(endpointId);
    icDeviceEndpoint *mock = (icDeviceEndpoint *) mock();
    return mock;
}

icDeviceEndpoint *__wrap_deviceServiceGetEndpointByUri(const char *uri)
{
    function_called();
    check_expected(uri);
    icDeviceEndpoint *mock = (icDeviceEndpoint *) mock();
    return mock;
}

icLinkedList *__wrap_deviceServiceGetAllDevices()
{
    function_called();
    icLinkedList *result = (icLinkedList *) mock();
    return result;
}

icLinkedList *__wrap_deviceServiceGetDevicesByDeviceClass(const char *deviceClass)
{
    function_called();
    check_expected(deviceClass);
    icLinkedList *result = (icLinkedList *) mock();
    return result;
}

icDeviceResource *__wrap_deviceServiceGetResourceByUri(const char *uri)
{
    function_called();
    check_expected(uri);
    icDeviceResource *mock = (icDeviceResource *) mock();
    return mock;
}

icLinkedList *__wrap_deviceServiceGetDevicesBySubsystem(const char *subsystem)
{
    function_called();
    check_expected(subsystem);
    icLinkedList *result = (icLinkedList *) mock();
    return result;
}

icDevice *__wrap_deviceServiceGetDeviceByUri(const char *uri)
{
    function_called();
    check_expected(uri);
    icDevice *mock = (icDevice *) mock();
    return mock;
}

bool __wrap_deviceServiceWriteResource(const char *uri, const char *resourceValue)
{
    function_called();
    check_expected(uri);
    check_expected(resourceValue);
    return (bool) mock();
}

bool __wrap_deviceServiceExecuteResource(const char *uri)
{
    function_called();
    check_expected(uri);
    return mock();
}

bool __wrap_deviceServiceRemoveDevice(const char *uuid)
{
    function_called();
    check_expected(uuid);
    return mock();
}

bool __wrap_deviceServiceGetSystemProperty(const char *key, char **value)
{
    function_called();
    check_expected(key);
    *value = (char *) mock();
    return *value != NULL;
}

bool __wrap_deviceServiceGetMetadata(const char *uri, char **value)
{
    function_called();
    check_expected(uri);
    *value = (char *) mock();
    return *value != NULL;
}

bool __wrap_deviceServiceSetSystemProperty(const char *name, const char *value)
{
    function_called();
    check_expected(name);
    check_expected(value);
    return (bool) mock();
}

bool __wrap_deviceServiceRemoveEndpointById(const char *deviceUuid, const char *endpointId)
{
    function_called();
    check_expected(deviceUuid);
    check_expected(endpointId);
    return (bool) mock();
}

bool __wrap_deviceServiceSetMetadata(const char *uri, const char *value)
{
    function_called();
    check_expected(uri);
    check_expected(value);
    return (bool) mock();
}

icLinkedList *__wrap_deviceServiceGetResourcesByUriPattern(char *uri)
{
    function_called();
    check_expected(uri);
    icLinkedList *result = (icLinkedList *) mock();
    return result;
}

icLinkedList *__wrap_deviceServiceGetMetadataByUriPattern(const char *uri)
{
    function_called();
    check_expected(uri);
    icLinkedList *result = (icLinkedList *) mock();
    return result;
}

bool __wrap_deviceServiceChangeResourceMode(const char *uri, u_int8_t newMode)
{
    function_called();
    check_expected(uri);
    check_expected(newMode);
    return (bool) mock();
}

#ifdef BARTON_CONFIG_ZIGBEE
ChannelChangeResponse __wrap_zigbeeSubsystemChangeChannel(uint8_t channel, bool dryRun)
{
    function_called();
    check_expected(channel);
    check_expected(dryRun);

    ChannelChangeResponse mock;
    // retrieve the next mocked value
    mock.channelNumber = (uint8_t) mock();
    mock.responseCode = (ChannelChangeResponseCode) mock();

    return mock;
}
#endif

void __wrap_deviceServiceProcessDeviceDescriptors(void)
{
    function_called();
    return;
}

char *__wrap_zhalTest(void)
{
    function_called();
    return (char *) mock();
}

icLinkedList *__wrap_zigbeeSubsystemPerformEnergyScan(const uint8_t *channelsToScan,
                                                      uint8_t numChannelsToScan,
                                                      uint32_t scanDurationMillis,
                                                      uint32_t numScans)
{
    function_called();
    check_expected(channelsToScan);
    check_expected(numChannelsToScan);
    check_expected(scanDurationMillis);
    check_expected(numScans);
    icLinkedList *result = (icLinkedList *) mock();
    return result;
}

bool __wrap_deviceServiceRestoreConfig(const char *tempRestoreDir)
{
    function_called();
    check_expected(tempRestoreDir);
    return (bool) mock();
}

#define DEFINE_MOCK_EVENT_HANDLER(eventType)                                                                           \
    static void mockHandle##eventType(GObject *source, BDeviceService##eventType *event)                               \
    {                                                                                                                  \
        function_called();                                                                                             \
        check_expected(event);                                                                                         \
        return;                                                                                                        \
    }

DEFINE_MOCK_EVENT_HANDLER(DiscoveryStartedEvent)
DEFINE_MOCK_EVENT_HANDLER(RecoveryStartedEvent)
DEFINE_MOCK_EVENT_HANDLER(StatusEvent)
DEFINE_MOCK_EVENT_HANDLER(DeviceDiscoveryFailedEvent)
DEFINE_MOCK_EVENT_HANDLER(DeviceDiscoveredEvent)
DEFINE_MOCK_EVENT_HANDLER(DeviceRejectedEvent)
DEFINE_MOCK_EVENT_HANDLER(DeviceConfigurationStartedEvent)
DEFINE_MOCK_EVENT_HANDLER(DeviceConfigurationCompletedEvent)
DEFINE_MOCK_EVENT_HANDLER(DeviceConfigurationFailedEvent)
DEFINE_MOCK_EVENT_HANDLER(DeviceAddedEvent)
DEFINE_MOCK_EVENT_HANDLER(DeviceRecoveredEvent)
DEFINE_MOCK_EVENT_HANDLER(DeviceDiscoveryCompletedEvent)
DEFINE_MOCK_EVENT_HANDLER(DiscoveryStoppedEvent)
DEFINE_MOCK_EVENT_HANDLER(RecoveryStoppedEvent)
DEFINE_MOCK_EVENT_HANDLER(ResourceUpdatedEvent)
DEFINE_MOCK_EVENT_HANDLER(DeviceRemovedEvent)
DEFINE_MOCK_EVENT_HANDLER(EndpointRemovedEvent)
DEFINE_MOCK_EVENT_HANDLER(EndpointAddedEvent)
DEFINE_MOCK_EVENT_HANDLER(ZigbeeChannelChangedEvent)
DEFINE_MOCK_EVENT_HANDLER(StorageChangedEvent)
DEFINE_MOCK_EVENT_HANDLER(ZigbeeInterferenceEvent)
DEFINE_MOCK_EVENT_HANDLER(ZigbeePanIdAttackChangedEvent)
DEFINE_MOCK_EVENT_HANDLER(DeviceDatabaseFailureEvent)

int main(int argc, const char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_b_device_service_client_get_endpoints_by_profile),
        cmocka_unit_test(test_b_device_service_client_discover_start),
        cmocka_unit_test(test_b_device_service_client_recover_start),
        cmocka_unit_test(test_b_device_service_client_discover_stop),
        cmocka_unit_test(test_b_device_service_client_recover_stop),
        cmocka_unit_test(test_b_device_service_client_commission_device),
        cmocka_unit_test(test_b_device_service_client_add_matter_device),
        cmocka_unit_test(test_b_device_service_client_get_device_by_id),
        cmocka_unit_test(test_b_device_service_client_get_endpoint_by_id),
        cmocka_unit_test(test_b_device_service_client_get_endpoint_by_uri),
        cmocka_unit_test(test_b_device_service_client_get_devices),
        cmocka_unit_test(test_b_device_service_client_get_devices_by_device_class),
        cmocka_unit_test(test_b_device_service_client_read_resource),
        cmocka_unit_test(test_b_device_service_client_get_status),
        cmocka_unit_test(test_b_device_service_client_get_devices_by_subsystem),
        cmocka_unit_test(test_b_device_service_client_get_device_by_uri),
        cmocka_unit_test(test_b_device_service_client_get_resource_by_uri),
        cmocka_unit_test(test_b_device_service_client_write_resource),
        cmocka_unit_test(test_b_device_service_client_execute_resource),
        cmocka_unit_test(test_b_device_service_client_remove_device),
        cmocka_unit_test(test_b_device_service_client_get_system_property),
        cmocka_unit_test(test_b_device_service_client_set_system_property),
        cmocka_unit_test(test_b_device_service_client_remove_endpoint_by_id),
        cmocka_unit_test(test_b_device_service_client_write_metadata),
        cmocka_unit_test(test_b_device_service_client_read_metadata),
        cmocka_unit_test(test_b_device_service_client_query_resources_by_uri),
        cmocka_unit_test(test_b_device_service_client_change_resource_mode),
        cmocka_unit_test(test_b_device_service_client_get_metadata_by_uri),
        cmocka_unit_test(test_b_device_service_process_device_descriptors),
        cmocka_unit_test(test_b_device_service_client_config_restore),
        cmocka_unit_test(test_sendDiscoveryStartedEvent),
        cmocka_unit_test(test_sendRecoveryStartedEvent),
        cmocka_unit_test(test_sendDeviceServiceStatusEvent),
        cmocka_unit_test(test_sendDeviceDiscoveryFailedEvent),
        cmocka_unit_test(test_sendDeviceDiscoveredEvent),
        cmocka_unit_test(test_sendDeviceRejectedEvent),
        cmocka_unit_test(test_sendDeviceConfigureStartedEvent),
        cmocka_unit_test(test_sendDeviceConfigureCompletedEvent),
        cmocka_unit_test(test_sendDeviceConfigureFailedEvent),
        cmocka_unit_test(test_sendDeviceAddedEvent),
        cmocka_unit_test(test_sendDeviceRecoveredEvent),
        cmocka_unit_test(test_sendDeviceDiscoveryCompletedEvent),
        cmocka_unit_test(test_sendDiscoveryStoppedEvent),
        cmocka_unit_test(test_sendRecoveryStoppedEvent),
        cmocka_unit_test(test_sendResourceUpdatedEvent),
        cmocka_unit_test(test_sendDeviceRemovedEvent),
        cmocka_unit_test(test_sendEndpointRemovedEvent),
        cmocka_unit_test(test_sendEndpointAddedEvent),
        cmocka_unit_test(test_sendZigbeeChannelChangedEvent),
        cmocka_unit_test(test_sendStorageChangedEvent),
        cmocka_unit_test(test_sendZigbeeInterferenceEvent),
        cmocka_unit_test(test_sendZigbeePanIdAttackChangedEvent),
        cmocka_unit_test(test_sendDeviceDatabaseFailureEvent),
#ifdef BARTON_CONFIG_ZIGBEE
        cmocka_unit_test(test_b_device_service_client_zigbee_test),
        cmocka_unit_test(test_b_device_service_client_change_zigbee_channel),
        cmocka_unit_test(test_b_device_service_client_zigbee_energy_scan),
#endif
    };

    int retval = cmocka_run_group_tests(tests, setup_b_device_service_client, teardown_b_device_service_client);

    return retval;
}
