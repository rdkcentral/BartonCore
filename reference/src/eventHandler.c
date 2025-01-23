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
 * Created by Micah Koch on 09/17/24.
 */

#include "eventHandler.h"
#include "device-service-client.h"
#include "device-service-device-found-details.h"
#include "device-service-metadata.h"
#include "device-service-reference-io.h"
#include "events/device-service-device-added-event.h"
#include "events/device-service-device-discovered-event.h"
#include "events/device-service-device-discovery-completed-event.h"
#include "events/device-service-device-discovery-failed-event.h"
#include "events/device-service-device-rejected-event.h"
#include "events/device-service-discovery-started-event.h"
#include "events/device-service-discovery-stopped-event.h"
#include "events/device-service-endpoint-added-event.h"
#include "events/device-service-metadata-updated-event.h"
#include <stdio.h>

static void discoveryStartedHandler(BDeviceServiceClient *source, BDeviceServiceDiscoveryStartedEvent *event);
static void discoveryStoppedHandler(BDeviceServiceClient *source, BDeviceServiceDiscoveryStoppedEvent *event);
static void deviceDiscoveredHandler(BDeviceServiceClient *source, BDeviceServiceDeviceDiscoveredEvent *event);
static void deviceRejectedHandler(BDeviceServiceClient *source, BDeviceServiceDeviceRejectedEvent *event);
static void deviceAddedHandler(BDeviceServiceClient *source, BDeviceServiceDeviceAddedEvent *event);
static void endpointAddedHandler(BDeviceServiceClient *source, BDeviceServiceEndpointAddedEvent *event);
static void deviceDiscoveryCompletedHandler(BDeviceServiceClient *source,
                                            BDeviceServiceDeviceDiscoveryCompletedEvent *event);
static void deviceDiscoveryFailedHandler(BDeviceServiceClient *source, BDeviceServiceDeviceDiscoveryFailedEvent *event);
static void metadataUpdated(BDeviceServiceClient *source, BDeviceServiceMetadataUpdatedEvent *event);


void registerEventHandlers(BDeviceServiceClient *client)
{
    g_signal_connect(
        client, B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DISCOVERY_STARTED, G_CALLBACK(discoveryStartedHandler), NULL);
    g_signal_connect(
        client, B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DISCOVERY_STOPPED, G_CALLBACK(discoveryStoppedHandler), NULL);
    g_signal_connect(
        client, B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_DISCOVERED, G_CALLBACK(deviceDiscoveredHandler), NULL);
    g_signal_connect(
        client, B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_REJECTED, G_CALLBACK(deviceRejectedHandler), NULL);
    g_signal_connect(client, B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_ADDED, G_CALLBACK(deviceAddedHandler), NULL);
    g_signal_connect(
        client, B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_ENDPOINT_ADDED, G_CALLBACK(endpointAddedHandler), NULL);
    g_signal_connect(client,
                     B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_DISCOVERY_COMPLETED,
                     G_CALLBACK(deviceDiscoveryCompletedHandler),
                     NULL);
    g_signal_connect(client,
                     B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_DEVICE_DISCOVERY_FAILED,
                     G_CALLBACK(deviceDiscoveryFailedHandler),
                     NULL);
    g_signal_connect(client, B_DEVICE_SERVICE_CLIENT_SIGNAL_NAME_METADATA_UPDATED, G_CALLBACK(metadataUpdated), NULL);
}

void unregisterEventHandlers(void)
{
    // Nothing to do
}

static void discoveryStartedHandler(BDeviceServiceClient *source, BDeviceServiceDiscoveryStartedEvent *event)
{
    emitOutput("discoveryStarted [");

    g_autoptr(GList) deviceClasses = NULL;
    g_object_get(G_OBJECT(event),
                 B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
                 &deviceClasses,
                 NULL);
    while (deviceClasses != NULL)
    {
        const char *deviceClass = (const char *) deviceClasses->data;
        emitOutput("%s", (char *) deviceClasses->data);
        deviceClasses = deviceClasses->next;
        if (deviceClasses)
        {
            emitOutput(", ");
        }
    }

    emitOutput("]\n");
}

static void discoveryStoppedHandler(BDeviceServiceClient *source, BDeviceServiceDiscoveryStoppedEvent *event)
{
    emitOutput("discoveryStopped\n");
}

static void printDeviceFoundDetails(const char *printPrefix, BDeviceServiceDeviceFoundDetails *deviceFoundDetails)
{
    g_autofree gchar *deviceId = NULL;
    g_autofree gchar *manufacturer = NULL;
    g_autofree gchar *model = NULL;
    g_autofree gchar *hardwareVersion = NULL;
    g_autofree gchar *firmwareVersion = NULL;
    g_object_get(
        G_OBJECT(deviceFoundDetails),
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_UUID],
        &deviceId,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_MANUFACTURER],
        &manufacturer,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_MODEL],
        &model,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_HARDWARE_VERSION],
        &hardwareVersion,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_FIRMWARE_VERSION],
        &firmwareVersion,
        NULL);

    emitOutput("%s uuid=%s, manufacturer=%s, model=%s, hardwareVersion=%s, firmwareVersion=%s\n",
               printPrefix,
               deviceId,
               manufacturer,
               model,
               hardwareVersion,
               firmwareVersion);
}

static void deviceDiscoveredHandler(BDeviceServiceClient *source, BDeviceServiceDeviceDiscoveredEvent *event)
{
    g_autoptr(BDeviceServiceDeviceFoundDetails) deviceFoundDetails = NULL;

    g_object_get(G_OBJECT(event),
                 B_DEVICE_SERVICE_DEVICE_DISCOVERED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DISCOVERED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 &deviceFoundDetails,
                 NULL);

    g_return_if_fail(deviceFoundDetails != NULL);

    printDeviceFoundDetails("device discovered!", deviceFoundDetails);
}

static void deviceRejectedHandler(BDeviceServiceClient *source, BDeviceServiceDeviceRejectedEvent *event)
{
    g_autoptr(BDeviceServiceDeviceFoundDetails) deviceFoundDetails = NULL;

    g_object_get(G_OBJECT(event),
                 B_DEVICE_SERVICE_DEVICE_REJECTED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_REJECTED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 &deviceFoundDetails,
                 NULL);

    g_return_if_fail(deviceFoundDetails != NULL);

    printDeviceFoundDetails("device rejected!", deviceFoundDetails);
}

static void deviceAddedHandler(BDeviceServiceClient *source, BDeviceServiceDeviceAddedEvent *event)
{
    g_autofree gchar *deviceId = NULL;
    g_autofree gchar *uri = NULL;
    g_autofree gchar *deviceClass = NULL;
    guint deviceClassVersion = 0;
    g_object_get(
        G_OBJECT(event),
        B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_UUID],
        &deviceId,
        B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_URI],
        &uri,
        B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS],
        &deviceClass,
        B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS_VERSION],
        &deviceClassVersion,
        NULL);

    emitOutput("device added! deviceId=%s, uri=%s, deviceClass=%s, deviceClassVersion=%d\n",
               deviceId,
               uri,
               deviceClass,
               deviceClassVersion);
}

static void endpointAddedHandler(BDeviceServiceClient *source, BDeviceServiceEndpointAddedEvent *event)
{
    g_autoptr(BDeviceServiceEndpoint) endpoint = NULL;
    g_object_get(
        G_OBJECT(event),
        B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROP_ENDPOINT],
        &endpoint,
        NULL);

    g_return_if_fail(endpoint != NULL);

    g_autofree gchar *deviceUuid = NULL;
    g_autofree gchar *id = NULL;
    g_autofree gchar *uri = NULL;
    g_autofree gchar *profile = NULL;
    guint profileVersion = 0;
    g_object_get(G_OBJECT(endpoint),
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_DEVICE_UUID],
                 &deviceUuid,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_ID],
                 &id,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_URI],
                 &uri,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE],
                 &profile,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE_VERSION],
                 &profileVersion,
                 NULL);
    emitOutput("endpoint added! deviceUuid=%s, id=%s, uri=%s, profile=%s, profileVersion=%d\n",
               deviceUuid,
               id,
               uri,
               profile,
               profileVersion);
}

static void deviceDiscoveryCompletedHandler(BDeviceServiceClient *source,
                                            BDeviceServiceDeviceDiscoveryCompletedEvent *event)
{
    g_autoptr(BDeviceServiceDevice) device = NULL;
    g_object_get(G_OBJECT(event),
                 B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROP_DEVICE],
                 &device,
                 NULL);

    g_return_if_fail(device != NULL);

    g_autofree gchar *uuid = NULL;
    g_autofree gchar *deviceClass = NULL;
    g_object_get(G_OBJECT(device),
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_UUID],
                 &uuid,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS],
                 &deviceClass,
                 NULL);

    emitOutput("device discovery completed! uuid=%s, class=%s\n", uuid, deviceClass);
}

static void deviceDiscoveryFailedHandler(BDeviceServiceClient *source, BDeviceServiceDeviceDiscoveryFailedEvent *event)
{
    g_autoptr(BDeviceServiceDeviceFoundDetails) deviceFoundDetails = NULL;
    g_object_get(G_OBJECT(event),
                 B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 &deviceFoundDetails,
                 NULL);

    g_return_if_fail(deviceFoundDetails != NULL);

    printDeviceFoundDetails("device discovery failed!", deviceFoundDetails);
}

static void metadataUpdated(BDeviceServiceClient *source, BDeviceServiceMetadataUpdatedEvent *event)
{
    g_autoptr(BDeviceServiceMetadata) metadata = NULL;
    g_object_get(
        G_OBJECT(event),
        B_DEVICE_SERVICE_METADATA_UPDATED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_UPDATED_EVENT_PROP_METADATA],
        &metadata,
        NULL);

    g_return_if_fail(metadata != NULL);

    g_autofree gchar *uri = NULL;
    g_autofree gchar *value = NULL;

    g_object_get(metadata,
                 B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_URI],
                 &uri,
                 B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_VALUE],
                 &value,
                 NULL);

    emitOutput("metadata updated: uri=%s, value=%s\n", uri, value);
}
