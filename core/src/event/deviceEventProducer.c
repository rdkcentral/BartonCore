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
 * Created by Thomas Lea on 7/27/2015.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device-service-client.h"
#include "device-service-device-found-details.h"
#include "device-service-discovery-type.h"
#include "device-service-status.h"
#include "device-service-utils.h"
#include "deviceEventProducer.h"
#include "deviceServicePrivate.h" // local header
#include "deviceServiceStatus.h"
#include "events/device-service-device-added-event.h"
#include "events/device-service-device-configuration-completed-event.h"
#include "events/device-service-device-configuration-failed-event.h"
#include "events/device-service-device-configuration-started-event.h"
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
#include "events/device-service-zigbee-channel-changed-event.h"
#include "events/device-service-zigbee-interference-event.h"
#include "events/device-service-zigbee-pan-id-attack-changed-event.h"
#include "icLog/logging.h"
#include <device-driver/device-driver.h>
#include <device/icDeviceEndpoint.h>
#include <device/icDeviceResource.h>
#include <deviceService.h>
#include <icConcurrent/threadUtils.h>
#include <icTypes/icHashMap.h>
#include <jsonHelper/jsonHelper.h>

#define LOG_TAG "deviceServiceEventProducer"
#define G_LOG_DOMAIN_NAME LOG_TAG

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*a))

typedef enum
{
    EARLY_DEVICE_EVENT_TYPE_DEVICE_DISCOVERED,
    EARLY_DEVICE_EVENT_TYPE_DEVICE_DISCOVERY_FAILED,
    EARLY_DEVICE_EVENT_TYPE_DEVICE_REJECTED
} EarlyDeviceEventType;

static BDeviceServiceClient *service = NULL;
enum DEVICE_SERVICE_SIGNALS
{
    SIGNAL_STATUS_CHANGED,
    SIGNAL_DISCOVERY_STARTED,
    SIGNAL_RECOVERY_STARTED,
    SIGNAL_DEVICE_DISCOVERY_FAILED,
    SIGNAL_DEVICE_DISCOVERED,
    SIGNAL_DEVICE_REJECTED,
    SIGNAL_DEVICE_CONFIGURATION_STARTED,
    SIGNAL_DEVICE_CONFIGURATION_COMPLETED,
    SIGNAL_DEVICE_CONFIGURATION_FAILED,
    SIGNAL_DEVICE_ADDED,
    SIGNAL_DEVICE_RECOVERED,
    SIGNAL_DEVICE_DISCOVERY_COMPLETED,
    SIGNAL_DISCOVERY_STOPPED,
    SIGNAL_RECOVERY_STOPPED,
    SIGNAL_RESOURCE_UPDATED,
    SIGNAL_DEVICE_REMOVED,
    SIGNAL_ENDPOINT_REMOVED,
    SIGNAL_ENDPOINT_ADDED,
    SIGNAL_ZIGBEE_CHANNEL_CHANGED,
    SIGNAL_STORAGE_CHANGED,
    SIGNAL_ZIGBEE_INTERFERENCE,
    SIGNAL_PAN_ID_ATTACK_CHANGED,
    SIGNAL_DEVICE_DATABASE_FAILURE,

    SIGNAL_MAX
};

static guint signals[SIGNAL_MAX];

void deviceEventProducerClassInit(BDeviceServiceClientClass *deviceServiceClass)
{
    signals[SIGNAL_STATUS_CHANGED] = g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_STATUS_CHANGED,
                                                   G_TYPE_FROM_CLASS(deviceServiceClass),
                                                   G_SIGNAL_RUN_LAST,
                                                   0,
                                                   NULL,
                                                   NULL,
                                                   NULL,
                                                   G_TYPE_NONE,
                                                   1,
                                                   B_DEVICE_SERVICE_STATUS_EVENT_TYPE);

    signals[SIGNAL_DISCOVERY_STARTED] = g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DISCOVERY_STARTED,
                                                     G_TYPE_FROM_CLASS(deviceServiceClass),
                                                     G_SIGNAL_RUN_LAST,
                                                     0,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     G_TYPE_NONE,
                                                     1,
                                                     B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_TYPE);

    signals[SIGNAL_RECOVERY_STARTED] = g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_RECOVERY_STARTED,
                                                    G_TYPE_FROM_CLASS(deviceServiceClass),
                                                    G_SIGNAL_RUN_LAST,
                                                    0,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    G_TYPE_NONE,
                                                    1,
                                                    B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_DISCOVERY_FAILED] = g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_DISCOVERY_FAILED,
                                                           G_TYPE_FROM_CLASS(deviceServiceClass),
                                                           G_SIGNAL_RUN_LAST,
                                                           0,
                                                           NULL,
                                                           NULL,
                                                           NULL,
                                                           G_TYPE_NONE,
                                                           1,
                                                           B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_DISCOVERED] = g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_DISCOVERED,
                                                     G_TYPE_FROM_CLASS(deviceServiceClass),
                                                     G_SIGNAL_RUN_LAST,
                                                     0,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     G_TYPE_NONE,
                                                     1,
                                                     B_DEVICE_SERVICE_DEVICE_DISCOVERED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_REJECTED] = g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_REJECTED,
                                                   G_TYPE_FROM_CLASS(deviceServiceClass),
                                                   G_SIGNAL_RUN_LAST,
                                                   0,
                                                   NULL,
                                                   NULL,
                                                   NULL,
                                                   G_TYPE_NONE,
                                                   1,
                                                   B_DEVICE_SERVICE_DEVICE_REJECTED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_CONFIGURATION_STARTED] =
        g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_CONFIGURATION_STARTED,
                     G_TYPE_FROM_CLASS(deviceServiceClass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     G_TYPE_NONE,
                     1,
                     B_DEVICE_SERVICE_DEVICE_CONFIGURATION_STARTED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_CONFIGURATION_COMPLETED] =
        g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_CONFIGURATION_COMPLETED,
                     G_TYPE_FROM_CLASS(deviceServiceClass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     G_TYPE_NONE,
                     1,
                     B_DEVICE_SERVICE_DEVICE_CONFIGURATION_COMPLETED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_CONFIGURATION_FAILED] =
        g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_CONFIGURATION_FAILED,
                     G_TYPE_FROM_CLASS(deviceServiceClass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     G_TYPE_NONE,
                     1,
                     B_DEVICE_SERVICE_DEVICE_CONFIGURATION_FAILED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_ADDED] = g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_ADDED,
                                                G_TYPE_FROM_CLASS(deviceServiceClass),
                                                G_SIGNAL_RUN_LAST,
                                                0,
                                                NULL,
                                                NULL,
                                                NULL,
                                                G_TYPE_NONE,
                                                1,
                                                B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_RECOVERED] = g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_RECOVERED,
                                                    G_TYPE_FROM_CLASS(deviceServiceClass),
                                                    G_SIGNAL_RUN_LAST,
                                                    0,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    G_TYPE_NONE,
                                                    1,
                                                    B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_DISCOVERY_COMPLETED] =
        g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_DISCOVERY_COMPLETED,
                     G_TYPE_FROM_CLASS(deviceServiceClass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     G_TYPE_NONE,
                     1,
                     B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_TYPE);

    signals[SIGNAL_DISCOVERY_STOPPED] = g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DISCOVERY_STOPPED,
                                                     G_TYPE_FROM_CLASS(deviceServiceClass),
                                                     G_SIGNAL_RUN_LAST,
                                                     0,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     G_TYPE_NONE,
                                                     1,
                                                     B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_TYPE);

    signals[SIGNAL_RECOVERY_STOPPED] = g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_RECOVERY_STOPPED,
                                                    G_TYPE_FROM_CLASS(deviceServiceClass),
                                                    G_SIGNAL_RUN_LAST,
                                                    0,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    G_TYPE_NONE,
                                                    1,
                                                    B_DEVICE_SERVICE_RECOVERY_STOPPED_EVENT_TYPE);

    signals[SIGNAL_RESOURCE_UPDATED] = g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_RESOURCE_UPDATED,
                                                    G_TYPE_FROM_CLASS(deviceServiceClass),
                                                    G_SIGNAL_RUN_LAST,
                                                    0,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    G_TYPE_NONE,
                                                    1,
                                                    B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_REMOVED] = g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_REMOVED,
                                                  G_TYPE_FROM_CLASS(deviceServiceClass),
                                                  G_SIGNAL_RUN_LAST,
                                                  0,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  G_TYPE_NONE,
                                                  1,
                                                  B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_TYPE);

    signals[SIGNAL_ENDPOINT_REMOVED] = g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_ENDPOINT_REMOVED,
                                                    G_TYPE_FROM_CLASS(deviceServiceClass),
                                                    G_SIGNAL_RUN_LAST,
                                                    0,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    G_TYPE_NONE,
                                                    1,
                                                    B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_TYPE);

    signals[SIGNAL_ENDPOINT_ADDED] = g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_ENDPOINT_ADDED,
                                                  G_TYPE_FROM_CLASS(deviceServiceClass),
                                                  G_SIGNAL_RUN_LAST,
                                                  0,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  G_TYPE_NONE,
                                                  1,
                                                  B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_TYPE);

    signals[SIGNAL_ZIGBEE_CHANNEL_CHANGED] = g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_ZIGBEE_CHANNEL_CHANGED,
                                                          G_TYPE_FROM_CLASS(deviceServiceClass),
                                                          G_SIGNAL_RUN_LAST,
                                                          0,
                                                          NULL,
                                                          NULL,
                                                          NULL,
                                                          G_TYPE_NONE,
                                                          1,
                                                          B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_TYPE);

    signals[SIGNAL_STORAGE_CHANGED] = g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_STORAGE_CHANGED,
                                                   G_TYPE_FROM_CLASS(deviceServiceClass),
                                                   G_SIGNAL_RUN_LAST,
                                                   0,
                                                   NULL,
                                                   NULL,
                                                   NULL,
                                                   G_TYPE_NONE,
                                                   1,
                                                   B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_TYPE);

    signals[SIGNAL_ZIGBEE_INTERFERENCE] = g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_ZIGBEE_INTERFERENCE,
                                                       G_TYPE_FROM_CLASS(deviceServiceClass),
                                                       G_SIGNAL_RUN_LAST,
                                                       0,
                                                       NULL,
                                                       NULL,
                                                       NULL,
                                                       G_TYPE_NONE,
                                                       1,
                                                       B_DEVICE_SERVICE_ZIGBEE_INTERFERENCE_EVENT_TYPE);

    signals[SIGNAL_PAN_ID_ATTACK_CHANGED] =
        g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_ZIGBEE_PAN_ID_ATTACK_CHANGED,
                     G_TYPE_FROM_CLASS(deviceServiceClass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     G_TYPE_NONE,
                     1,
                     B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_DATABASE_FAILURE] = g_signal_new(B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_DATABASE_FAILURE,
                                                           G_TYPE_FROM_CLASS(deviceServiceClass),
                                                           G_SIGNAL_RUN_LAST,
                                                           0,
                                                           NULL,
                                                           NULL,
                                                           NULL,
                                                           G_TYPE_NONE,
                                                           1,
                                                           B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_TYPE);
}

void deviceEventProducerStartup(BDeviceServiceClient *_service)
{
    service = g_object_ref(_service);
}

void deviceEventProducerShutdown(void)
{
    if (service != NULL)
    {
        g_object_unref(service);
        service = NULL;
    }
}

void sendDiscoveryStartedEvent(const icLinkedList *deviceClasses, uint16_t timeoutSeconds, bool findOrphanedDevices)
{
    g_autoptr(GList) deviceClassesList = convertLinkedListToGListGeneric((icLinkedList *) deviceClasses);

    if (findOrphanedDevices)
    {
        g_autoptr(BDeviceServiceRecoveryStartedEvent) event = b_device_service_recovery_started_event_new();
        g_object_set(event,
                     B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES
                         [B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
                     deviceClassesList,
                     B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES
                         [B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROP_TIMEOUT],
                     timeoutSeconds,
                     NULL);
        g_signal_emit(service, signals[SIGNAL_RECOVERY_STARTED], 0, event);
    }
    else
    {
        g_autoptr(BDeviceServiceDiscoveryStartedEvent) event = b_device_service_discovery_started_event_new();
        g_object_set(event,
                     B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROPERTY_NAMES
                         [B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
                     deviceClassesList,
                     B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROPERTY_NAMES
                         [B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROP_TIMEOUT],
                     timeoutSeconds,
                     NULL);
        g_signal_emit(service, signals[SIGNAL_DISCOVERY_STARTED], 0, event);
    }
}

void sendDiscoveryStoppedEvent(const char *deviceClass)
{
    g_autoptr(BDeviceServiceDiscoveryStoppedEvent) event = b_device_service_discovery_stopped_event_new();
    g_object_set(event,
                 B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 NULL);
    g_signal_emit(service, signals[SIGNAL_DISCOVERY_STOPPED], 0, event);
}

void sendRecoveryStoppedEvent(const char *deviceClass)
{
    g_autoptr(BDeviceServiceRecoveryStoppedEvent) event = b_device_service_recovery_stopped_event_new();
    g_object_set(event,
                 B_DEVICE_SERVICE_RECOVERY_STOPPED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_RECOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 NULL);
    g_signal_emit(service, signals[SIGNAL_RECOVERY_STOPPED], 0, event);
}

static void
sendEarlyDeviceEvent(EarlyDeviceEventType eventType, const DeviceFoundDetails *deviceFoundDetails, bool forRecovery)
{
    g_autoptr(BDeviceServiceDeviceFoundDetails) details = convertDeviceFoundDetailsToGobject(deviceFoundDetails);
    BDeviceServiceDiscoveryType discoveryType =
        forRecovery ? B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY : B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY;

    switch (eventType)
    {
        case EARLY_DEVICE_EVENT_TYPE_DEVICE_DISCOVERED:
        {
            g_autoptr(BDeviceServiceDeviceDiscoveredEvent) event = b_device_service_device_discovered_event_new();
            g_object_set(event,
                         B_DEVICE_SERVICE_DEVICE_DISCOVERED_EVENT_PROPERTY_NAMES
                             [B_DEVICE_SERVICE_DEVICE_DISCOVERED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                         details,
                         B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                             [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                         discoveryType,
                         NULL);

            g_signal_emit(service, signals[SIGNAL_DEVICE_DISCOVERED], 0, event);
            break;
        }
        case EARLY_DEVICE_EVENT_TYPE_DEVICE_DISCOVERY_FAILED:
        {
            g_autoptr(BDeviceServiceDeviceDiscoveryFailedEvent) event =
                b_device_service_device_discovery_failed_event_new();
            g_object_set(event,
                         B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT_PROPERTY_NAMES
                             [B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                         details,
                         B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                             [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                         discoveryType,
                         NULL);

            g_signal_emit(service, signals[SIGNAL_DEVICE_DISCOVERY_FAILED], 0, event);
            break;
        }
        case EARLY_DEVICE_EVENT_TYPE_DEVICE_REJECTED:
        {
            g_autoptr(BDeviceServiceDeviceRejectedEvent) event = b_device_service_device_rejected_event_new();
            g_object_set(event,
                         B_DEVICE_SERVICE_DEVICE_REJECTED_EVENT_PROPERTY_NAMES
                             [B_DEVICE_SERVICE_DEVICE_REJECTED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                         details,
                         B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                             [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                         discoveryType,
                         NULL);

            g_signal_emit(service, signals[SIGNAL_DEVICE_REJECTED], 0, event);
            break;
        }
    }
}

void sendDeviceDiscoveredEvent(const DeviceFoundDetails *deviceFoundDetails, bool forRecovery)
{
    sendEarlyDeviceEvent(EARLY_DEVICE_EVENT_TYPE_DEVICE_DISCOVERED, deviceFoundDetails, forRecovery);
}

void sendDeviceDiscoveryFailedEvent(const DeviceFoundDetails *deviceFoundDetails, bool forRecovery)
{
    sendEarlyDeviceEvent(EARLY_DEVICE_EVENT_TYPE_DEVICE_DISCOVERY_FAILED, deviceFoundDetails, forRecovery);
}

void sendDeviceRejectedEvent(const DeviceFoundDetails *deviceFoundDetails, bool forRecovery)
{
    sendEarlyDeviceEvent(EARLY_DEVICE_EVENT_TYPE_DEVICE_REJECTED, deviceFoundDetails, forRecovery);
}

void sendDeviceDiscoveryCompletedEvent(icDevice *device, bool forRecovery)
{
    g_autoptr(BDeviceServiceDevice) deviceGObject = convertIcDeviceToGObject(device);
    BDeviceServiceDiscoveryType discoveryType =
        forRecovery ? B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY : B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY;

    g_autoptr(BDeviceServiceDeviceDiscoveryCompletedEvent) event =
        b_device_service_device_discovery_completed_event_new();
    g_object_set(event,
                 B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROP_DEVICE],
                 deviceGObject,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 discoveryType,
                 NULL);

    g_signal_emit(service, signals[SIGNAL_DEVICE_DISCOVERY_COMPLETED], 0, event);
}

void sendDeviceAddedEvent(const char *uuid)
{
    g_autoptr(BDeviceServiceDevice) device = b_device_service_client_get_device_by_id(service, uuid);

    if (device)
    {
        g_autofree gchar *uri = NULL;
        g_autofree gchar *deviceClass = NULL;
        guint8 deviceClassVersion = 0;

        g_object_get(device,
                     B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_URI],
                     &uri,
                     B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS],
                     &deviceClass,
                     B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS_VERSION],
                     &deviceClassVersion,
                     NULL);

        g_autoptr(BDeviceServiceDeviceAddedEvent) event = b_device_service_device_added_event_new();
        g_object_set(
            event,
            B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_UUID],
            uuid,
            B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_URI],
            uri,
            B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS],
            deviceClass,
            B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES
                [B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS_VERSION],
            deviceClassVersion,
            NULL);
        g_signal_emit(service, signals[SIGNAL_DEVICE_ADDED], 0, event);
    }
}

void sendDeviceRecoveredEvent(const char *uuid)
{
    g_autoptr(BDeviceServiceDevice) device = b_device_service_client_get_device_by_id(service, uuid);

    if (device)
    {
        g_autofree gchar *uri = NULL;
        g_autofree gchar *deviceClass = NULL;
        guint8 deviceClassVersion = 0;

        g_object_get(device,
                     B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_URI],
                     &uri,
                     B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS],
                     &deviceClass,
                     B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS_VERSION],
                     &deviceClassVersion,
                     NULL);

        g_autoptr(BDeviceServiceDeviceRecoveredEvent) event = b_device_service_device_recovered_event_new();
        g_object_set(
            event,
            B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROP_UUID],
            uuid,
            B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROP_URI],
            uri,
            B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROPERTY_NAMES
                [B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROP_DEVICE_CLASS],
            deviceClass,
            B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROPERTY_NAMES
                [B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROP_DEVICE_CLASS_VERSION],
            deviceClassVersion,
            NULL);
        g_signal_emit(service, signals[SIGNAL_DEVICE_RECOVERED], 0, event);
    }
}

void sendDeviceRemovedEvent(const char *uuid, const char *deviceClass)
{
    g_autoptr(BDeviceServiceDeviceRemovedEvent) event = b_device_service_device_removed_event_new();
    g_object_set(
        event,
        B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_UUID],
        uuid,
        B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_CLASS],
        deviceClass,
        NULL);
    g_signal_emit(service, signals[SIGNAL_DEVICE_REMOVED], 0, event);
}

void sendResourceUpdatedEvent(icDeviceResource *resource, cJSON *metadata)
{
    g_autoptr(BDeviceServiceResource) resourceGObject = convertIcDeviceResourceToGObject(resource);

    g_autofree gchar *metadataStr = NULL;
    if (metadata != NULL)
    {
        metadataStr = cJSON_PrintUnformatted(metadata);
    }

    g_autoptr(BDeviceServiceResourceUpdatedEvent) event = b_device_service_resource_updated_event_new();
    g_object_set(
        event,
        B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_RESOURCE],
        resourceGObject,
        B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_METADATA],
        metadataStr,
        NULL);

    g_signal_emit(service, signals[SIGNAL_RESOURCE_UPDATED], 0, event);
}

void sendEndpointAddedEvent(icDeviceEndpoint *endpoint, const char *deviceClass)
{
    g_autoptr(BDeviceServiceEndpoint) endpointGObject = convertIcDeviceEndpointToGObject(endpoint);
    g_autoptr(BDeviceServiceEndpointAddedEvent) event = b_device_service_endpoint_added_event_new();

    g_object_set(
        event,
        B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROP_ENDPOINT],
        endpointGObject,
        B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROP_DEVICE_CLASS],
        deviceClass,
        NULL);

    g_signal_emit(service, signals[SIGNAL_ENDPOINT_ADDED], 0, event);
}

void sendEndpointRemovedEvent(icDeviceEndpoint *endpoint, const char *deviceClass, bool deviceRemoved)
{
    g_autoptr(BDeviceServiceEndpoint) endpointGObject = convertIcDeviceEndpointToGObject(endpoint);
    g_autoptr(BDeviceServiceEndpointRemovedEvent) event = b_device_service_endpoint_removed_event_new();

    g_object_set(
        event,
        B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_ENDPOINT],
        endpointGObject,
        B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_CLASS],
        deviceClass,
        B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_REMOVED],
        deviceRemoved,
        NULL);

    g_signal_emit(service, signals[SIGNAL_ENDPOINT_REMOVED], 0, event);
}

void sendDeviceServiceStatusEvent(DeviceServiceStatusChangedReason reason)
{
    g_autoptr(BDeviceServiceStatusEvent) event = b_device_service_status_event_new();

    scoped_DeviceServiceStatus *system_status = deviceServiceGetStatus();
    g_autoptr(BDeviceServiceStatus) status = convertDeviceServiceStatusToGObject(system_status);

    g_object_set(
        event,
        B_DEVICE_SERVICE_STATUS_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_EVENT_PROP_STATUS],
        status,
        B_DEVICE_SERVICE_STATUS_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_EVENT_PROP_REASON],
        convertStatusChangedReasonToGObject(reason),
        NULL);

    g_signal_emit(service, signals[SIGNAL_STATUS_CHANGED], 0, event);
}

void sendZigbeeChannelChangedEvent(bool success, uint8_t currentChannel, uint8_t targetedChannel)
{
    g_autoptr(BDeviceServiceZigbeeChannelChangedEvent) event = b_device_service_zigbee_channel_changed_event_new();

    g_object_set(event,
                 B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CHANNEL_CHANGED],
                     success,
                 B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CURRENT_CHANNEL],
                     currentChannel,
                 B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_TARGETED_CHANNEL],
                     targetedChannel,
                     NULL);

    g_signal_emit(service, signals[SIGNAL_ZIGBEE_CHANNEL_CHANGED], 0, event);
}

void sendZigbeeNetworkInterferenceEvent(bool interferenceDetected)
{
    g_autoptr(BDeviceServiceZigbeeInterferenceEvent) event = b_device_service_zigbee_interference_event_new();

    g_object_set(event,
                 B_DEVICE_SERVICE_ZIGBEE_INTERFERENCE_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_INTERFERENCE_EVENT_PROP_INTERFERENCE_DETECTED],
                     interferenceDetected,
                     NULL);

    g_signal_emit(service, signals[SIGNAL_ZIGBEE_INTERFERENCE], 0, event);
}

void sendZigbeePanIdAttackEvent(bool attackDetected)
{
    g_autoptr(BDeviceServiceZigbeePanIdAttackChangedEvent) event =
        b_device_service_zigbee_pan_id_attack_changed_event_new();

    g_object_set(event,
                 B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROP_ATTACK_DETECTED],
                 attackDetected,
                 NULL);

    g_signal_emit(service, signals[SIGNAL_PAN_ID_ATTACK_CHANGED], 0, event);
}

void sendZigbeeRemoteCliCommandResponseReceivedEvent(const char *uuid, const char *commandResponse) {}

void sendDeviceConfigureStartedEvent(const char *deviceClass, const char *uuid, bool forRecovery)
{
    g_autoptr(BDeviceServiceDeviceConfigurationStartedEvent) event =
        b_device_service_device_configuration_started_event_new();
    BDeviceServiceDiscoveryType discoveryType =
        forRecovery ? B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY : B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY;

    g_object_set(event,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 uuid,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 discoveryType,
                 NULL);

    g_signal_emit(service, signals[SIGNAL_DEVICE_CONFIGURATION_STARTED], 0, event);
}

void sendDeviceConfigureCompletedEvent(const char *deviceClass, const char *uuid, bool forRecovery)
{
    g_autoptr(BDeviceServiceDeviceConfigurationCompletedEvent) event =
        b_device_service_device_configuration_completed_event_new();
    BDeviceServiceDiscoveryType discoveryType =
        forRecovery ? B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY : B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY;

    g_object_set(event,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 uuid,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 discoveryType,
                 NULL);

    g_signal_emit(service, signals[SIGNAL_DEVICE_CONFIGURATION_COMPLETED], 0, event);
}

void sendDeviceConfigureFailedEvent(const char *deviceClass, const char *uuid, bool forRecovery)
{
    g_autoptr(BDeviceServiceDeviceConfigurationFailedEvent) event =
        b_device_service_device_configuration_failed_event_new();
    BDeviceServiceDiscoveryType discoveryType =
        forRecovery ? B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY : B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY;

    g_object_set(event,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 uuid,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 discoveryType,
                 NULL);

    g_signal_emit(service, signals[SIGNAL_DEVICE_CONFIGURATION_FAILED], 0, event);
}

void sendStorageChangedEvent(GFileMonitorEvent whatChanged)
{
    g_autoptr(BDeviceServiceStorageChangedEvent) event = b_device_service_storage_changed_event_new();
    g_object_set(
        event,
        B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROP_WHAT_CHANGED],
        whatChanged,
        NULL);

    g_signal_emit(service, signals[SIGNAL_STORAGE_CHANGED], 0, event);
}

void sendDeviceDatabaseFailureEvent(BDeviceServiceDeviceDatabaseFailureType failureType, const char *deviceId)
{
    g_autoptr(BDeviceServiceDeviceDatabaseFailureEvent) event = b_device_service_device_database_failure_event_new();
    g_object_set(event,
                 B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE],
                 failureType,
                 B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_DEVICE_ID],
                 deviceId,
                 NULL);

    g_signal_emit(service, signals[SIGNAL_DEVICE_DATABASE_FAILURE], 0, event);
}