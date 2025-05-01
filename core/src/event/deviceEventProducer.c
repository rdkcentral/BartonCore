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
 * Created by Thomas Lea on 7/27/2015.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "barton-core-client.h"
#include "barton-core-device-found-details.h"
#include "barton-core-discovery-type.h"
#include "barton-core-status.h"
#include "barton-core-utils.h"
#include "deviceEventProducer.h"
#include "deviceServicePrivate.h" // local header
#include "deviceServiceStatus.h"
#include "events/barton-core-device-added-event.h"
#include "events/barton-core-device-configuration-completed-event.h"
#include "events/barton-core-device-configuration-failed-event.h"
#include "events/barton-core-device-configuration-started-event.h"
#include "events/barton-core-device-discovered-event.h"
#include "events/barton-core-device-discovery-completed-event.h"
#include "events/barton-core-device-discovery-failed-event.h"
#include "events/barton-core-device-recovered-event.h"
#include "events/barton-core-device-rejected-event.h"
#include "events/barton-core-device-removed-event.h"
#include "events/barton-core-discovery-session-info-event.h"
#include "events/barton-core-discovery-started-event.h"
#include "events/barton-core-discovery-stopped-event.h"
#include "events/barton-core-endpoint-added-event.h"
#include "events/barton-core-endpoint-removed-event.h"
#include "events/barton-core-metadata-updated-event.h"
#include "events/barton-core-recovery-started-event.h"
#include "events/barton-core-recovery-stopped-event.h"
#include "events/barton-core-resource-updated-event.h"
#include "events/barton-core-status-event.h"
#include "events/barton-core-zigbee-channel-changed-event.h"
#include "events/barton-core-zigbee-interference-event.h"
#include "events/barton-core-zigbee-pan-id-attack-changed-event.h"
#include "events/barton-core-zigbee-remote-cli-command-response-received-event.h"
#include "icLog/logging.h"
#include <device-driver/device-driver.h>
#include <device/icDeviceEndpoint.h>
#include <device/icDeviceResource.h>
#include <deviceService.h>
#include <icConcurrent/threadUtils.h>
#include <icTypes/icHashMap.h>
#include <jsonHelper/jsonHelper.h>

#define LOG_TAG           "deviceServiceEventProducer"
#define G_LOG_DOMAIN_NAME LOG_TAG

#define ARRAY_SIZE(a)     (sizeof(a) / sizeof(*a))

typedef enum
{
    EARLY_DEVICE_EVENT_TYPE_DEVICE_DISCOVERED,
    EARLY_DEVICE_EVENT_TYPE_DEVICE_DISCOVERY_FAILED,
    EARLY_DEVICE_EVENT_TYPE_DEVICE_REJECTED
} EarlyDeviceEventType;

static BCoreClient *service = NULL;
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
    SIGNAL_METADATA_UPDATED,
    SIGNAL_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED,

    SIGNAL_MAX
};

static guint signals[SIGNAL_MAX];

void deviceEventProducerClassInit(BCoreClientClass *deviceServiceClass)
{
    signals[SIGNAL_STATUS_CHANGED] = g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_STATUS_CHANGED,
                                                  G_TYPE_FROM_CLASS(deviceServiceClass),
                                                  G_SIGNAL_RUN_LAST,
                                                  0,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  G_TYPE_NONE,
                                                  1,
                                                  B_CORE_STATUS_EVENT_TYPE);

    signals[SIGNAL_DISCOVERY_STARTED] = g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_DISCOVERY_STARTED,
                                                     G_TYPE_FROM_CLASS(deviceServiceClass),
                                                     G_SIGNAL_RUN_LAST,
                                                     0,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     G_TYPE_NONE,
                                                     1,
                                                     B_CORE_DISCOVERY_STARTED_EVENT_TYPE);

    signals[SIGNAL_RECOVERY_STARTED] = g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_RECOVERY_STARTED,
                                                    G_TYPE_FROM_CLASS(deviceServiceClass),
                                                    G_SIGNAL_RUN_LAST,
                                                    0,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    G_TYPE_NONE,
                                                    1,
                                                    B_CORE_RECOVERY_STARTED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_DISCOVERY_FAILED] = g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_DEVICE_DISCOVERY_FAILED,
                                                           G_TYPE_FROM_CLASS(deviceServiceClass),
                                                           G_SIGNAL_RUN_LAST,
                                                           0,
                                                           NULL,
                                                           NULL,
                                                           NULL,
                                                           G_TYPE_NONE,
                                                           1,
                                                           B_CORE_DEVICE_DISCOVERY_FAILED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_DISCOVERED] = g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_DEVICE_DISCOVERED,
                                                     G_TYPE_FROM_CLASS(deviceServiceClass),
                                                     G_SIGNAL_RUN_LAST,
                                                     0,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     G_TYPE_NONE,
                                                     1,
                                                     B_CORE_DEVICE_DISCOVERED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_REJECTED] = g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_DEVICE_REJECTED,
                                                   G_TYPE_FROM_CLASS(deviceServiceClass),
                                                   G_SIGNAL_RUN_LAST,
                                                   0,
                                                   NULL,
                                                   NULL,
                                                   NULL,
                                                   G_TYPE_NONE,
                                                   1,
                                                   B_CORE_DEVICE_REJECTED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_CONFIGURATION_STARTED] =
        g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_DEVICE_CONFIGURATION_STARTED,
                     G_TYPE_FROM_CLASS(deviceServiceClass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     G_TYPE_NONE,
                     1,
                     B_CORE_DEVICE_CONFIGURATION_STARTED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_CONFIGURATION_COMPLETED] =
        g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_DEVICE_CONFIGURATION_COMPLETED,
                     G_TYPE_FROM_CLASS(deviceServiceClass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     G_TYPE_NONE,
                     1,
                     B_CORE_DEVICE_CONFIGURATION_COMPLETED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_CONFIGURATION_FAILED] =
        g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_DEVICE_CONFIGURATION_FAILED,
                     G_TYPE_FROM_CLASS(deviceServiceClass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     G_TYPE_NONE,
                     1,
                     B_CORE_DEVICE_CONFIGURATION_FAILED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_ADDED] = g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_DEVICE_ADDED,
                                                G_TYPE_FROM_CLASS(deviceServiceClass),
                                                G_SIGNAL_RUN_LAST,
                                                0,
                                                NULL,
                                                NULL,
                                                NULL,
                                                G_TYPE_NONE,
                                                1,
                                                B_CORE_DEVICE_ADDED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_RECOVERED] = g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_DEVICE_RECOVERED,
                                                    G_TYPE_FROM_CLASS(deviceServiceClass),
                                                    G_SIGNAL_RUN_LAST,
                                                    0,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    G_TYPE_NONE,
                                                    1,
                                                    B_CORE_DEVICE_RECOVERED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_DISCOVERY_COMPLETED] =
        g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_DEVICE_DISCOVERY_COMPLETED,
                     G_TYPE_FROM_CLASS(deviceServiceClass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     G_TYPE_NONE,
                     1,
                     B_CORE_DEVICE_DISCOVERY_COMPLETED_EVENT_TYPE);

    signals[SIGNAL_DISCOVERY_STOPPED] = g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_DISCOVERY_STOPPED,
                                                     G_TYPE_FROM_CLASS(deviceServiceClass),
                                                     G_SIGNAL_RUN_LAST,
                                                     0,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     G_TYPE_NONE,
                                                     1,
                                                     B_CORE_DISCOVERY_STOPPED_EVENT_TYPE);

    signals[SIGNAL_RECOVERY_STOPPED] = g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_RECOVERY_STOPPED,
                                                    G_TYPE_FROM_CLASS(deviceServiceClass),
                                                    G_SIGNAL_RUN_LAST,
                                                    0,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    G_TYPE_NONE,
                                                    1,
                                                    B_CORE_RECOVERY_STOPPED_EVENT_TYPE);

    signals[SIGNAL_RESOURCE_UPDATED] = g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_RESOURCE_UPDATED,
                                                    G_TYPE_FROM_CLASS(deviceServiceClass),
                                                    G_SIGNAL_RUN_LAST,
                                                    0,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    G_TYPE_NONE,
                                                    1,
                                                    B_CORE_RESOURCE_UPDATED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_REMOVED] = g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_DEVICE_REMOVED,
                                                  G_TYPE_FROM_CLASS(deviceServiceClass),
                                                  G_SIGNAL_RUN_LAST,
                                                  0,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  G_TYPE_NONE,
                                                  1,
                                                  B_CORE_DEVICE_REMOVED_EVENT_TYPE);

    signals[SIGNAL_ENDPOINT_REMOVED] = g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_ENDPOINT_REMOVED,
                                                    G_TYPE_FROM_CLASS(deviceServiceClass),
                                                    G_SIGNAL_RUN_LAST,
                                                    0,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    G_TYPE_NONE,
                                                    1,
                                                    B_CORE_ENDPOINT_REMOVED_EVENT_TYPE);

    signals[SIGNAL_ENDPOINT_ADDED] = g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_ENDPOINT_ADDED,
                                                  G_TYPE_FROM_CLASS(deviceServiceClass),
                                                  G_SIGNAL_RUN_LAST,
                                                  0,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  G_TYPE_NONE,
                                                  1,
                                                  B_CORE_ENDPOINT_ADDED_EVENT_TYPE);

    signals[SIGNAL_ZIGBEE_CHANNEL_CHANGED] = g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_ZIGBEE_CHANNEL_CHANGED,
                                                          G_TYPE_FROM_CLASS(deviceServiceClass),
                                                          G_SIGNAL_RUN_LAST,
                                                          0,
                                                          NULL,
                                                          NULL,
                                                          NULL,
                                                          G_TYPE_NONE,
                                                          1,
                                                          B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_TYPE);

    signals[SIGNAL_STORAGE_CHANGED] = g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_STORAGE_CHANGED,
                                                   G_TYPE_FROM_CLASS(deviceServiceClass),
                                                   G_SIGNAL_RUN_LAST,
                                                   0,
                                                   NULL,
                                                   NULL,
                                                   NULL,
                                                   G_TYPE_NONE,
                                                   1,
                                                   B_CORE_STORAGE_CHANGED_EVENT_TYPE);

    signals[SIGNAL_ZIGBEE_INTERFERENCE] = g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_ZIGBEE_INTERFERENCE,
                                                       G_TYPE_FROM_CLASS(deviceServiceClass),
                                                       G_SIGNAL_RUN_LAST,
                                                       0,
                                                       NULL,
                                                       NULL,
                                                       NULL,
                                                       G_TYPE_NONE,
                                                       1,
                                                       B_CORE_ZIGBEE_INTERFERENCE_EVENT_TYPE);

    signals[SIGNAL_PAN_ID_ATTACK_CHANGED] =
        g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_ZIGBEE_PAN_ID_ATTACK_CHANGED,
                     G_TYPE_FROM_CLASS(deviceServiceClass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     G_TYPE_NONE,
                     1,
                     B_CORE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_TYPE);

    signals[SIGNAL_DEVICE_DATABASE_FAILURE] = g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_DEVICE_DATABASE_FAILURE,
                                                           G_TYPE_FROM_CLASS(deviceServiceClass),
                                                           G_SIGNAL_RUN_LAST,
                                                           0,
                                                           NULL,
                                                           NULL,
                                                           NULL,
                                                           G_TYPE_NONE,
                                                           1,
                                                           B_CORE_DEVICE_DATABASE_FAILURE_EVENT_TYPE);

    signals[SIGNAL_METADATA_UPDATED] = g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_METADATA_UPDATED,
                                                    G_TYPE_FROM_CLASS(deviceServiceClass),
                                                    G_SIGNAL_RUN_LAST,
                                                    0,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    G_TYPE_NONE,
                                                    1,
                                                    B_CORE_METADATA_UPDATED_EVENT_TYPE);

    signals[SIGNAL_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED] =
        g_signal_new(B_CORE_CLIENT_SIGNAL_NAME_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED,
                     G_TYPE_FROM_CLASS(deviceServiceClass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     G_TYPE_NONE,
                     1,
                     B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_TYPE);
}

void deviceEventProducerStartup(BCoreClient *_service)
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

    guint timeout = timeoutSeconds;
    if (findOrphanedDevices)
    {
        g_autoptr(BCoreRecoveryStartedEvent) event = b_core_recovery_started_event_new();
        g_object_set(event,
                     B_CORE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES
                         [B_CORE_RECOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
                     deviceClassesList,
                     B_CORE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES
                         [B_CORE_RECOVERY_STARTED_EVENT_PROP_TIMEOUT],
                     timeout,
                     NULL);
        g_signal_emit(service, signals[SIGNAL_RECOVERY_STARTED], 0, event);
    }
    else
    {
        g_autoptr(BCoreDiscoveryStartedEvent) event = b_core_discovery_started_event_new();
        g_object_set(event,
                     B_CORE_DISCOVERY_STARTED_EVENT_PROPERTY_NAMES
                         [B_CORE_DISCOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
                     deviceClassesList,
                     B_CORE_DISCOVERY_STARTED_EVENT_PROPERTY_NAMES
                         [B_CORE_DISCOVERY_STARTED_EVENT_PROP_TIMEOUT],
                     timeout,
                     NULL);
        g_signal_emit(service, signals[SIGNAL_DISCOVERY_STARTED], 0, event);
    }
}

void sendDiscoveryStoppedEvent(const char *deviceClass)
{
    g_autoptr(BCoreDiscoveryStoppedEvent) event = b_core_discovery_stopped_event_new();
    g_object_set(event,
                 B_CORE_DISCOVERY_STOPPED_EVENT_PROPERTY_NAMES
                     [B_CORE_DISCOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 NULL);
    g_signal_emit(service, signals[SIGNAL_DISCOVERY_STOPPED], 0, event);
}

void sendRecoveryStoppedEvent(const char *deviceClass)
{
    g_autoptr(BCoreRecoveryStoppedEvent) event = b_core_recovery_stopped_event_new();
    g_object_set(event,
                 B_CORE_RECOVERY_STOPPED_EVENT_PROPERTY_NAMES
                     [B_CORE_RECOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 NULL);
    g_signal_emit(service, signals[SIGNAL_RECOVERY_STOPPED], 0, event);
}

static void
sendEarlyDeviceEvent(EarlyDeviceEventType eventType, const DeviceFoundDetails *deviceFoundDetails, bool forRecovery)
{
    g_autoptr(BCoreDeviceFoundDetails) details = convertDeviceFoundDetailsToGobject(deviceFoundDetails);
    BCoreDiscoveryType discoveryType =
        forRecovery ? B_CORE_DISCOVERY_TYPE_RECOVERY : B_CORE_DISCOVERY_TYPE_DISCOVERY;

    switch (eventType)
    {
        case EARLY_DEVICE_EVENT_TYPE_DEVICE_DISCOVERED:
        {
            g_autoptr(BCoreDeviceDiscoveredEvent) event = b_core_device_discovered_event_new();
            g_object_set(event,
                         B_CORE_DEVICE_DISCOVERED_EVENT_PROPERTY_NAMES
                             [B_CORE_DEVICE_DISCOVERED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                         details,
                         B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                             [B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                         discoveryType,
                         NULL);

            g_signal_emit(service, signals[SIGNAL_DEVICE_DISCOVERED], 0, event);
            break;
        }
        case EARLY_DEVICE_EVENT_TYPE_DEVICE_DISCOVERY_FAILED:
        {
            g_autoptr(BCoreDeviceDiscoveryFailedEvent) event =
                b_core_device_discovery_failed_event_new();
            g_object_set(event,
                         B_CORE_DEVICE_DISCOVERY_FAILED_EVENT_PROPERTY_NAMES
                             [B_CORE_DEVICE_DISCOVERY_FAILED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                         details,
                         B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                             [B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                         discoveryType,
                         NULL);

            g_signal_emit(service, signals[SIGNAL_DEVICE_DISCOVERY_FAILED], 0, event);
            break;
        }
        case EARLY_DEVICE_EVENT_TYPE_DEVICE_REJECTED:
        {
            g_autoptr(BCoreDeviceRejectedEvent) event = b_core_device_rejected_event_new();
            g_object_set(event,
                         B_CORE_DEVICE_REJECTED_EVENT_PROPERTY_NAMES
                             [B_CORE_DEVICE_REJECTED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                         details,
                         B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                             [B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
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
    g_autoptr(BCoreDevice) deviceGObject = convertIcDeviceToGObject(device);
    BCoreDiscoveryType discoveryType =
        forRecovery ? B_CORE_DISCOVERY_TYPE_RECOVERY : B_CORE_DISCOVERY_TYPE_DISCOVERY;

    g_autoptr(BCoreDeviceDiscoveryCompletedEvent) event =
        b_core_device_discovery_completed_event_new();
    g_object_set(event,
                 B_CORE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROP_DEVICE],
                 deviceGObject,
                 B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 discoveryType,
                 NULL);

    g_signal_emit(service, signals[SIGNAL_DEVICE_DISCOVERY_COMPLETED], 0, event);
}

void sendDeviceAddedEvent(const char *uuid)
{
    g_autoptr(BCoreDevice) device = b_core_client_get_device_by_id(service, uuid);

    if (device)
    {
        g_autofree gchar *uri = NULL;
        g_autofree gchar *deviceClass = NULL;
        guint deviceClassVersion = 0;

        g_object_get(device,
                     B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_URI],
                     &uri,
                     B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_DEVICE_CLASS],
                     &deviceClass,
                     B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_DEVICE_CLASS_VERSION],
                     &deviceClassVersion,
                     NULL);

        g_autoptr(BCoreDeviceAddedEvent) event = b_core_device_added_event_new();
        g_object_set(
            event,
            B_CORE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_ADDED_EVENT_PROP_UUID],
            uuid,
            B_CORE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_ADDED_EVENT_PROP_URI],
            uri,
            B_CORE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS],
            deviceClass,
            B_CORE_DEVICE_ADDED_EVENT_PROPERTY_NAMES
                [B_CORE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS_VERSION],
            deviceClassVersion,
            NULL);
        g_signal_emit(service, signals[SIGNAL_DEVICE_ADDED], 0, event);
    }
}

void sendDeviceRecoveredEvent(const char *uuid)
{
    g_autoptr(BCoreDevice) device = b_core_client_get_device_by_id(service, uuid);

    if (device)
    {
        g_autofree gchar *uri = NULL;
        g_autofree gchar *deviceClass = NULL;
        guint deviceClassVersion = 0;

        g_object_get(device,
                     B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_URI],
                     &uri,
                     B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_DEVICE_CLASS],
                     &deviceClass,
                     B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_DEVICE_CLASS_VERSION],
                     &deviceClassVersion,
                     NULL);

        g_autoptr(BCoreDeviceRecoveredEvent) event = b_core_device_recovered_event_new();
        g_object_set(
            event,
            B_CORE_DEVICE_RECOVERED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_RECOVERED_EVENT_PROP_UUID],
            uuid,
            B_CORE_DEVICE_RECOVERED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_RECOVERED_EVENT_PROP_URI],
            uri,
            B_CORE_DEVICE_RECOVERED_EVENT_PROPERTY_NAMES
                [B_CORE_DEVICE_RECOVERED_EVENT_PROP_DEVICE_CLASS],
            deviceClass,
            B_CORE_DEVICE_RECOVERED_EVENT_PROPERTY_NAMES
                [B_CORE_DEVICE_RECOVERED_EVENT_PROP_DEVICE_CLASS_VERSION],
            deviceClassVersion,
            NULL);
        g_signal_emit(service, signals[SIGNAL_DEVICE_RECOVERED], 0, event);
    }
}

void sendDeviceRemovedEvent(const char *uuid, const char *deviceClass)
{
    g_autoptr(BCoreDeviceRemovedEvent) event = b_core_device_removed_event_new();
    g_object_set(
        event,
        B_CORE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_REMOVED_EVENT_PROP_DEVICE_UUID],
        uuid,
        B_CORE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_REMOVED_EVENT_PROP_DEVICE_CLASS],
        deviceClass,
        NULL);
    g_signal_emit(service, signals[SIGNAL_DEVICE_REMOVED], 0, event);
}

void sendResourceUpdatedEvent(icDeviceResource *resource, cJSON *metadata)
{
    g_autoptr(BCoreResource) resourceGObject = convertIcDeviceResourceToGObject(resource);

    g_autofree gchar *metadataStr = NULL;
    if (metadata != NULL)
    {
        metadataStr = cJSON_PrintUnformatted(metadata);
    }

    g_autoptr(BCoreResourceUpdatedEvent) event = b_core_resource_updated_event_new();
    g_object_set(
        event,
        B_CORE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_CORE_RESOURCE_UPDATED_EVENT_PROP_RESOURCE],
        resourceGObject,
        B_CORE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_CORE_RESOURCE_UPDATED_EVENT_PROP_METADATA],
        metadataStr,
        NULL);

    g_signal_emit(service, signals[SIGNAL_RESOURCE_UPDATED], 0, event);
}

void sendEndpointAddedEvent(icDeviceEndpoint *endpoint, const char *deviceClass)
{
    g_autoptr(BCoreEndpoint) endpointGObject = convertIcDeviceEndpointToGObject(endpoint);
    g_autoptr(BCoreEndpointAddedEvent) event = b_core_endpoint_added_event_new();

    g_object_set(
        event,
        B_CORE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_CORE_ENDPOINT_ADDED_EVENT_PROP_ENDPOINT],
        endpointGObject,
        B_CORE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_CORE_ENDPOINT_ADDED_EVENT_PROP_DEVICE_CLASS],
        deviceClass,
        NULL);

    g_signal_emit(service, signals[SIGNAL_ENDPOINT_ADDED], 0, event);
}

void sendEndpointRemovedEvent(icDeviceEndpoint *endpoint, const char *deviceClass, bool deviceRemoved)
{
    g_autoptr(BCoreEndpoint) endpointGObject = convertIcDeviceEndpointToGObject(endpoint);
    g_autoptr(BCoreEndpointRemovedEvent) event = b_core_endpoint_removed_event_new();

    g_object_set(
        event,
        B_CORE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES[B_CORE_ENDPOINT_REMOVED_EVENT_PROP_ENDPOINT],
        endpointGObject,
        B_CORE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES
            [B_CORE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_CLASS],
        deviceClass,
        B_CORE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES
            [B_CORE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_REMOVED],
        deviceRemoved,
        NULL);

    g_signal_emit(service, signals[SIGNAL_ENDPOINT_REMOVED], 0, event);
}

void sendDeviceServiceStatusEvent(DeviceServiceStatusChangedReason reason)
{
    g_autoptr(BCoreStatusEvent) event = b_core_status_event_new();

    scoped_DeviceServiceStatus *system_status = deviceServiceGetStatus();
    g_autoptr(BCoreStatus) status = convertDeviceServiceStatusToGObject(system_status);

    g_object_set(event,
                 B_CORE_STATUS_EVENT_PROPERTY_NAMES[B_CORE_STATUS_EVENT_PROP_STATUS],
                 status,
                 B_CORE_STATUS_EVENT_PROPERTY_NAMES[B_CORE_STATUS_EVENT_PROP_REASON],
                 convertStatusChangedReasonToGObject(reason),
                 NULL);

    g_signal_emit(service, signals[SIGNAL_STATUS_CHANGED], 0, event);
}

void sendZigbeeChannelChangedEvent(bool success, uint8_t currentChannel, uint8_t targetedChannel)
{
    g_autoptr(BCoreZigbeeChannelChangedEvent) event = b_core_zigbee_channel_changed_event_new();

    guint current = currentChannel;
    guint targeted = targetedChannel;
    g_object_set(event,
                 B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CHANNEL_CHANGED],
                 success,
                 B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CURRENT_CHANNEL],
                 current,
                 B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_TARGETED_CHANNEL],
                 targeted,
                 NULL);

    g_signal_emit(service, signals[SIGNAL_ZIGBEE_CHANNEL_CHANGED], 0, event);
}

void sendZigbeeNetworkInterferenceEvent(bool interferenceDetected)
{
    g_autoptr(BCoreZigbeeInterferenceEvent) event = b_core_zigbee_interference_event_new();

    g_object_set(event,
                 B_CORE_ZIGBEE_INTERFERENCE_EVENT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_INTERFERENCE_EVENT_PROP_INTERFERENCE_DETECTED],
                 interferenceDetected,
                 NULL);

    g_signal_emit(service, signals[SIGNAL_ZIGBEE_INTERFERENCE], 0, event);
}

void sendZigbeePanIdAttackEvent(bool attackDetected)
{
    g_autoptr(BCoreZigbeePanIdAttackChangedEvent) event =
        b_core_zigbee_pan_id_attack_changed_event_new();

    g_object_set(event,
                 B_CORE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROP_ATTACK_DETECTED],
                 attackDetected,
                 NULL);

    g_signal_emit(service, signals[SIGNAL_PAN_ID_ATTACK_CHANGED], 0, event);
}

void sendZigbeeRemoteCliCommandResponseReceivedEvent(const char *uuid, const char *commandResponse)
{
    g_autoptr(BCoreZigbeeRemoteCliCommandResponseReceivedEvent) event =
        b_core_zigbee_remote_cli_command_response_received_event_new();

    g_object_set(event,
                 B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROP_UUID],
                 uuid,
                 B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROP_COMMAND_RESPONSE],
                 commandResponse,
                 NULL);

    g_signal_emit(service, signals[SIGNAL_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED], 0, event);
}

void sendDeviceConfigureStartedEvent(const char *deviceClass, const char *uuid, bool forRecovery)
{
    g_autoptr(BCoreDeviceConfigurationStartedEvent) event =
        b_core_device_configuration_started_event_new();
    BCoreDiscoveryType discoveryType =
        forRecovery ? B_CORE_DISCOVERY_TYPE_RECOVERY : B_CORE_DISCOVERY_TYPE_DISCOVERY;

    g_object_set(event,
                 B_CORE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 B_CORE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 uuid,
                 B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 discoveryType,
                 NULL);

    g_signal_emit(service, signals[SIGNAL_DEVICE_CONFIGURATION_STARTED], 0, event);
}

void sendDeviceConfigureCompletedEvent(const char *deviceClass, const char *uuid, bool forRecovery)
{
    g_autoptr(BCoreDeviceConfigurationCompletedEvent) event =
        b_core_device_configuration_completed_event_new();
    BCoreDiscoveryType discoveryType =
        forRecovery ? B_CORE_DISCOVERY_TYPE_RECOVERY : B_CORE_DISCOVERY_TYPE_DISCOVERY;

    g_object_set(event,
                 B_CORE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 B_CORE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 uuid,
                 B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 discoveryType,
                 NULL);

    g_signal_emit(service, signals[SIGNAL_DEVICE_CONFIGURATION_COMPLETED], 0, event);
}

void sendDeviceConfigureFailedEvent(const char *deviceClass, const char *uuid, bool forRecovery)
{
    g_autoptr(BCoreDeviceConfigurationFailedEvent) event =
        b_core_device_configuration_failed_event_new();
    BCoreDiscoveryType discoveryType =
        forRecovery ? B_CORE_DISCOVERY_TYPE_RECOVERY : B_CORE_DISCOVERY_TYPE_DISCOVERY;

    g_object_set(event,
                 B_CORE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 B_CORE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 uuid,
                 B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 discoveryType,
                 NULL);

    g_signal_emit(service, signals[SIGNAL_DEVICE_CONFIGURATION_FAILED], 0, event);
}

void sendStorageChangedEvent(GFileMonitorEvent whatChanged)
{
    g_autoptr(BCoreStorageChangedEvent) event = b_core_storage_changed_event_new();
    g_object_set(
        event,
        B_CORE_STORAGE_CHANGED_EVENT_PROPERTY_NAMES[B_CORE_STORAGE_CHANGED_EVENT_PROP_WHAT_CHANGED],
        whatChanged,
        NULL);

    g_signal_emit(service, signals[SIGNAL_STORAGE_CHANGED], 0, event);
}

void sendDeviceDatabaseFailureEvent(BCoreDeviceDatabaseFailureType failureType, const char *deviceId)
{
    g_autoptr(BCoreDeviceDatabaseFailureEvent) event = b_core_device_database_failure_event_new();
    g_object_set(event,
                 B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE],
                 failureType,
                 B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROP_DEVICE_ID],
                 deviceId,
                 NULL);

    g_signal_emit(service, signals[SIGNAL_DEVICE_DATABASE_FAILURE], 0, event);
}

void sendMetadataUpdatedEvent(icDeviceMetadata *metadata)
{
    g_autoptr(BCoreMetadata) metadataGObject = convertIcDeviceMetadataToGObject(metadata);

    g_autoptr(BCoreMetadataUpdatedEvent) event = b_core_metadata_updated_event_new();
    g_object_set(
        event,
        B_CORE_METADATA_UPDATED_EVENT_PROPERTY_NAMES[B_CORE_METADATA_UPDATED_EVENT_PROP_METADATA],
        metadataGObject,
        NULL);

    g_signal_emit(service, signals[SIGNAL_METADATA_UPDATED], 0, event);
}
