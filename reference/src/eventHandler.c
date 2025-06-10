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
#include "barton-core-client.h"
#include "barton-core-device-found-details.h"
#include "barton-core-metadata.h"
#include "barton-core-reference-io.h"
#include "events/barton-core-device-added-event.h"
#include "events/barton-core-device-discovered-event.h"
#include "events/barton-core-device-discovery-completed-event.h"
#include "events/barton-core-device-discovery-failed-event.h"
#include "events/barton-core-device-rejected-event.h"
#include "events/barton-core-device-removed-event.h"
#include "events/barton-core-discovery-started-event.h"
#include "events/barton-core-discovery-stopped-event.h"
#include "events/barton-core-endpoint-added-event.h"
#include "events/barton-core-endpoint-removed-event.h"
#include "events/barton-core-metadata-updated-event.h"
#include "events/barton-core-resource-updated-event.h"
#include "events/barton-core-zigbee-remote-cli-command-response-received-event.h"
#include "utils.h"
#include <stdio.h>

static void discoveryStartedHandler(BCoreClient *source, BCoreDiscoveryStartedEvent *event);
static void discoveryStoppedHandler(BCoreClient *source, BCoreDiscoveryStoppedEvent *event);
static void deviceDiscoveredHandler(BCoreClient *source, BCoreDeviceDiscoveredEvent *event);
static void deviceRejectedHandler(BCoreClient *source, BCoreDeviceRejectedEvent *event);
static void deviceAddedHandler(BCoreClient *source, BCoreDeviceAddedEvent *event);
static void endpointAddedHandler(BCoreClient *source, BCoreEndpointAddedEvent *event);
static void deviceDiscoveryCompletedHandler(BCoreClient *source,
                                            BCoreDeviceDiscoveryCompletedEvent *event);
static void deviceDiscoveryFailedHandler(BCoreClient *source, BCoreDeviceDiscoveryFailedEvent *event);
static void metadataUpdated(BCoreClient *source, BCoreMetadataUpdatedEvent *event);
static void deviceRemovedEventHandler(BCoreClient *source, BCoreDeviceRemovedEvent *event);
static void endpointRemovedEventHandler(BCoreClient *source, BCoreEndpointRemovedEvent *event);
static void resourceUpdatedHandler(BCoreClient *source, BCoreResourceUpdatedEvent *event);
static void zigbeeRemoteCliResponseEventHandler(BCoreClient *source,
                                                BCoreZigbeeRemoteCliCommandResponseReceivedEvent *event);


void registerEventHandlers(BCoreClient *client)
{
    g_signal_connect(
        client, B_CORE_CLIENT_SIGNAL_NAME_DISCOVERY_STARTED, G_CALLBACK(discoveryStartedHandler), NULL);
    g_signal_connect(
        client, B_CORE_CLIENT_SIGNAL_NAME_DISCOVERY_STOPPED, G_CALLBACK(discoveryStoppedHandler), NULL);
    g_signal_connect(
        client, B_CORE_CLIENT_SIGNAL_NAME_DEVICE_DISCOVERED, G_CALLBACK(deviceDiscoveredHandler), NULL);
    g_signal_connect(
        client, B_CORE_CLIENT_SIGNAL_NAME_DEVICE_REJECTED, G_CALLBACK(deviceRejectedHandler), NULL);
    g_signal_connect(client, B_CORE_CLIENT_SIGNAL_NAME_DEVICE_ADDED, G_CALLBACK(deviceAddedHandler), NULL);
    g_signal_connect(
        client, B_CORE_CLIENT_SIGNAL_NAME_ENDPOINT_ADDED, G_CALLBACK(endpointAddedHandler), NULL);
    g_signal_connect(client,
                     B_CORE_CLIENT_SIGNAL_NAME_DEVICE_DISCOVERY_COMPLETED,
                     G_CALLBACK(deviceDiscoveryCompletedHandler),
                     NULL);
    g_signal_connect(client,
                     B_CORE_CLIENT_SIGNAL_NAME_DEVICE_DISCOVERY_FAILED,
                     G_CALLBACK(deviceDiscoveryFailedHandler),
                     NULL);
    g_signal_connect(client, B_CORE_CLIENT_SIGNAL_NAME_METADATA_UPDATED, G_CALLBACK(metadataUpdated), NULL);
    g_signal_connect(
        client, B_CORE_CLIENT_SIGNAL_NAME_DEVICE_REMOVED, G_CALLBACK(deviceRemovedEventHandler), NULL);
    g_signal_connect(
        client, B_CORE_CLIENT_SIGNAL_NAME_ENDPOINT_REMOVED, G_CALLBACK(endpointRemovedEventHandler), NULL);
    g_signal_connect(
        client, B_CORE_CLIENT_SIGNAL_NAME_RESOURCE_UPDATED, G_CALLBACK(resourceUpdatedHandler), NULL);
    g_signal_connect(client,
                     B_CORE_CLIENT_SIGNAL_NAME_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED,
                     G_CALLBACK(zigbeeRemoteCliResponseEventHandler),
                     NULL);
}

void unregisterEventHandlers(void)
{
    // Nothing to do
}

static void discoveryStartedHandler(BCoreClient *source, BCoreDiscoveryStartedEvent *event)
{
    emitOutput("discoveryStarted [");

    g_autoptr(GList) deviceClasses = NULL;
    g_object_get(G_OBJECT(event),
                 B_CORE_DISCOVERY_STARTED_EVENT_PROPERTY_NAMES
                     [B_CORE_DISCOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
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

static void discoveryStoppedHandler(BCoreClient *source, BCoreDiscoveryStoppedEvent *event)
{
    emitOutput("discoveryStopped\n");
}

static void deviceDiscoveredHandler(BCoreClient *source, BCoreDeviceDiscoveredEvent *event)
{
    g_autoptr(BCoreDeviceFoundDetails) deviceFoundDetails = NULL;

    g_object_get(G_OBJECT(event),
                 B_CORE_DEVICE_DISCOVERED_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_DISCOVERED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 &deviceFoundDetails,
                 NULL);

    g_return_if_fail(deviceFoundDetails != NULL);

    printDeviceFoundDetails("device discovered!", deviceFoundDetails);
}

static void deviceRejectedHandler(BCoreClient *source, BCoreDeviceRejectedEvent *event)
{
    g_autoptr(BCoreDeviceFoundDetails) deviceFoundDetails = NULL;

    g_object_get(G_OBJECT(event),
                 B_CORE_DEVICE_REJECTED_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_REJECTED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 &deviceFoundDetails,
                 NULL);

    g_return_if_fail(deviceFoundDetails != NULL);

    printDeviceFoundDetails("device rejected!", deviceFoundDetails);
}

static void deviceAddedHandler(BCoreClient *source, BCoreDeviceAddedEvent *event)
{
    g_autofree gchar *deviceId = NULL;
    g_autofree gchar *uri = NULL;
    g_autofree gchar *deviceClass = NULL;
    guint deviceClassVersion = 0;
    g_object_get(
        G_OBJECT(event),
        B_CORE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_ADDED_EVENT_PROP_UUID],
        &deviceId,
        B_CORE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_ADDED_EVENT_PROP_URI],
        &uri,
        B_CORE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS],
        &deviceClass,
        B_CORE_DEVICE_ADDED_EVENT_PROPERTY_NAMES
            [B_CORE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS_VERSION],
        &deviceClassVersion,
        NULL);

    emitOutput("device added! deviceId=%s, uri=%s, deviceClass=%s, deviceClassVersion=%d\n",
               deviceId,
               uri,
               deviceClass,
               deviceClassVersion);
}

static void endpointAddedHandler(BCoreClient *source, BCoreEndpointAddedEvent *event)
{
    g_autoptr(BCoreEndpoint) endpoint = NULL;
    g_object_get(
        G_OBJECT(event),
        B_CORE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_CORE_ENDPOINT_ADDED_EVENT_PROP_ENDPOINT],
        &endpoint,
        NULL);

    g_return_if_fail(endpoint != NULL);

    g_autofree gchar *deviceUuid = NULL;
    g_autofree gchar *id = NULL;
    g_autofree gchar *uri = NULL;
    g_autofree gchar *profile = NULL;
    guint profileVersion = 0;
    g_object_get(G_OBJECT(endpoint),
                 B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_DEVICE_UUID],
                 &deviceUuid,
                 B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_ID],
                 &id,
                 B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_URI],
                 &uri,
                 B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_PROFILE],
                 &profile,
                 B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_PROFILE_VERSION],
                 &profileVersion,
                 NULL);
    emitOutput("endpoint added! deviceUuid=%s, id=%s, uri=%s, profile=%s, profileVersion=%d\n",
               deviceUuid,
               id,
               uri,
               profile,
               profileVersion);
}

static void deviceDiscoveryCompletedHandler(BCoreClient *source,
                                            BCoreDeviceDiscoveryCompletedEvent *event)
{
    g_autoptr(BCoreDevice) device = NULL;
    g_object_get(G_OBJECT(event),
                 B_CORE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROP_DEVICE],
                 &device,
                 NULL);

    g_return_if_fail(device != NULL);

    g_autofree gchar *uuid = NULL;
    g_autofree gchar *deviceClass = NULL;
    g_object_get(G_OBJECT(device),
                 B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_UUID],
                 &uuid,
                 B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_DEVICE_CLASS],
                 &deviceClass,
                 NULL);

    emitOutput("device discovery completed! uuid=%s, class=%s\n", uuid, deviceClass);
}

static void deviceDiscoveryFailedHandler(BCoreClient *source, BCoreDeviceDiscoveryFailedEvent *event)
{
    g_autoptr(BCoreDeviceFoundDetails) deviceFoundDetails = NULL;
    g_object_get(G_OBJECT(event),
                 B_CORE_DEVICE_DISCOVERY_FAILED_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_DISCOVERY_FAILED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 &deviceFoundDetails,
                 NULL);

    g_return_if_fail(deviceFoundDetails != NULL);

    printDeviceFoundDetails("device discovery failed!", deviceFoundDetails);
}

static void metadataUpdated(BCoreClient *source, BCoreMetadataUpdatedEvent *event)
{
    g_autoptr(BCoreMetadata) metadata = NULL;
    g_object_get(
        G_OBJECT(event),
        B_CORE_METADATA_UPDATED_EVENT_PROPERTY_NAMES[B_CORE_METADATA_UPDATED_EVENT_PROP_METADATA],
        &metadata,
        NULL);

    g_return_if_fail(metadata != NULL);

    g_autofree gchar *uri = NULL;
    g_autofree gchar *value = NULL;

    g_object_get(metadata,
                 B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_URI],
                 &uri,
                 B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_VALUE],
                 &value,
                 NULL);

    emitOutput("metadata updated: uri=%s, value=%s\n", uri, value);
}

static void deviceRemovedEventHandler(BCoreClient *source, BCoreDeviceRemovedEvent *event)
{
    g_autofree gchar *deviceId = NULL;
    g_autofree gchar *deviceClass = NULL;

    g_object_get(
        G_OBJECT(event),
        B_CORE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_REMOVED_EVENT_PROP_DEVICE_UUID],
        &deviceId,
        B_CORE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_REMOVED_EVENT_PROP_DEVICE_CLASS],
        &deviceClass,
        NULL);
    emitOutput("device removed! deviceId=%s, deviceClass=%s\n", deviceId, deviceClass);
}

static void endpointRemovedEventHandler(BCoreClient *source, BCoreEndpointRemovedEvent *event)
{
    g_autoptr(BCoreEndpoint) endpoint = NULL;
    g_object_get(
        G_OBJECT(event),
        B_CORE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES[B_CORE_ENDPOINT_REMOVED_EVENT_PROP_ENDPOINT],
        &endpoint,
        NULL);

    g_return_if_fail(endpoint != NULL);

    g_autofree gchar *id = NULL;
    g_autofree gchar *profile = NULL;

    g_object_get(G_OBJECT(endpoint),
                 B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_ID],
                 &id,
                 B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_PROFILE],
                 &profile,
                 NULL);
    emitOutput("endpointRemoved: endpointId=%s, profile=%s\n", id, profile);
}

static void resourceUpdatedHandler(BCoreClient *source, BCoreResourceUpdatedEvent *event)
{
    g_autoptr(BCoreResource) resource = NULL;
    g_autofree gchar *metadata = NULL;

    g_object_get(
        G_OBJECT(event),
        B_CORE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_CORE_RESOURCE_UPDATED_EVENT_PROP_RESOURCE],
        &resource,
        B_CORE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_CORE_RESOURCE_UPDATED_EVENT_PROP_METADATA],
        &metadata,
        NULL);

    g_return_if_fail(resource != NULL);

    g_autofree gchar *resourceDump = getResourceDump(resource);

    if (metadata)
    {
        emitOutput("\r\nresourceUpdated: %s (metadata=%s)\n", resourceDump, metadata);
    }
    else
    {
        emitOutput("\r\nresourceUpdated: %s\n", resourceDump);
    }
}

static void zigbeeRemoteCliResponseEventHandler(BCoreClient *source,
                                                BCoreZigbeeRemoteCliCommandResponseReceivedEvent *event)
{
    g_autofree gchar *commandResponse = NULL;

    g_object_get(G_OBJECT(event),
                 B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROP_COMMAND_RESPONSE],
                 &commandResponse,
                 NULL);

    emitOutput("%s\n", commandResponse);
}
