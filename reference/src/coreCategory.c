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
 * Created by Thomas Lea on 9/27/19.
 */

#include "category.h"
#include "device-service-client.h"
#include "device-service-device.h"
#include "device-service-endpoint.h"
#include "device-service-metadata.h"
#include "device-service-reference-io.h"
#include "device-service-resource.h"
#include "glib-object.h"
#include "glib.h"
#include "icUtil/stringUtils.h"
#include "utils.h"
#include <glib/gstdio.h>
#include <linenoise.h>
#include <private/deviceService/resourceModes.h>
#include <private/resourceTypes.h>
#include <stdio.h>

#define DISCOVERY_SECONDS 60

static bool discoverStartFunc(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    (void) argc; // unused
    bool rc = false;

    emitOutput("Starting discovery of %s for %d seconds\n", argv[0], DISCOVERY_SECONDS);

    g_autoptr(GList) deviceClasses = NULL;
    deviceClasses = g_list_append(deviceClasses, argv[0]);

    g_autoptr(GError) err = NULL;
    if (!b_device_service_client_discover_start(client, deviceClasses, NULL, DISCOVERY_SECONDS, &err))
    {
        if (err != NULL)
        {
            emitError("Failed to start discovery : %d - %s\n", err->code, err->message);
        }
        else
        {
            emitError("Unable to start discovery of %s\n", argv[0]);
        }
    }
    else
    {
        rc = true;
    }

    return rc;
}

static bool discoverStopFunc(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    (void) argc; // unused
    (void) argv; // unused
    bool rc = false;

    g_autoptr(GError) err = NULL;
    if (!b_device_service_client_discover_stop(client, NULL))
    {
        emitError("Failed to stop discovery\n");
    }
    else
    {
        rc = true;
    }

    return rc;
}

static void listDeviceEntry(BDeviceServiceDevice *device)
{
    g_return_if_fail(device);

    g_autofree gchar *deviceId = NULL;
    g_autofree gchar *deviceClass = NULL;
    g_autolist(BDeviceServiceEndpoint) endpoints = NULL;

    g_object_get(device,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_UUID],
                 &deviceId,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS],
                 &deviceClass,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_ENDPOINTS],
                 &endpoints,
                 NULL);

    emitOutput("%s: Class: %s\n", deviceId, deviceClass);


    for (GList *endpointsIter = endpoints; endpointsIter != NULL; endpointsIter = endpointsIter->next)
    {
        BDeviceServiceEndpoint *endpoint = B_DEVICE_SERVICE_ENDPOINT(endpointsIter->data);
        g_autofree gchar *endpointId = NULL;
        g_autofree gchar *endpointProfile = NULL;
        g_autofree gchar *label = NULL;
        g_autolist(BDeviceServiceResource) resources = NULL;
        g_object_get(endpoint,
                     B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_ID],
                     &endpointId,
                     B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE],
                     &endpointProfile,
                     B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_RESOURCES],
                     &resources,
                     NULL);
        if (resources && endpointId && endpointProfile)
        {
            for (GList *resourcesIter = resources; resourcesIter != NULL; resourcesIter = resourcesIter->next)
            {
                BDeviceServiceResource *resource = B_DEVICE_SERVICE_RESOURCE(resourcesIter->data);
                g_autofree gchar *resourceType = NULL;
                g_object_get(resource,
                             B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_TYPE],
                             &resourceType,
                             NULL);
                if (g_ascii_strcasecmp(resourceType, RESOURCE_TYPE_LABEL) == 0)
                {
                    g_object_get(resource,
                                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_VALUE],
                                 &label,
                                 NULL);
                    break;
                }
            }

            emitOutput("\tEndpoint %s: Profile: %s, Label: %s\n", endpointId, endpointProfile, label);
        }
    }
}

static bool listDevicesFunc(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    bool idOnly = false;

    if (argc > 0 && g_ascii_strcasecmp(argv[0], "-i") == 0)
    {
        idOnly = true;

        // adjust args to skip the -i
        argc--;
        argv++;
    }

    g_autolist(BDeviceServiceDevice) devices = NULL;

    if (argc == 0)
    {
        devices = b_device_service_client_get_devices(client);
    }
    else
    {
        devices = b_device_service_client_get_devices_by_device_class(client, argv[0]);
    }

    if (devices)
    {
        for (GList *devicesIter = devices; devicesIter != NULL; devicesIter = devicesIter->next)
        {
            BDeviceServiceDevice *device = B_DEVICE_SERVICE_DEVICE(devicesIter->data);
            if (idOnly)
            {
                g_autofree gchar *deviceId = NULL;
                g_object_get(
                    device, B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_UUID], &deviceId, NULL);
                if (deviceId != NULL)
                {
                    emitOutput("%s\n", deviceId);
                }
            }
            else
            {
                listDeviceEntry(device);
            }
        }
    }
    else
    {
        emitOutput("No devices\n");
    }

    return true;
}

static gchar *getDeviceLabel(BDeviceServiceDevice *device)
{
    gchar *result = NULL;

    g_autolist(BDeviceServiceEndpoint) endpoints = NULL;
    g_object_get(
        device, B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_ENDPOINTS], &endpoints, NULL);

    for (GList *endpointsIter = endpoints; endpointsIter != NULL; endpointsIter = endpointsIter->next)
    {
        BDeviceServiceEndpoint *endpoint = B_DEVICE_SERVICE_ENDPOINT(endpointsIter->data);
        g_autolist(BDeviceServiceResource) resources = NULL;
        g_object_get(endpoint,
                     B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_RESOURCES],
                     &resources,
                     NULL);
        if (resources)
        {
            for (GList *resourcesIter = resources; resourcesIter != NULL; resourcesIter = resourcesIter->next)
            {
                BDeviceServiceResource *resource = B_DEVICE_SERVICE_RESOURCE(resourcesIter->data);
                g_autofree gchar *resourceType = NULL;
                g_object_get(resource,
                             B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_TYPE],
                             &resourceType,
                             NULL);
                if (g_ascii_strcasecmp(resourceType, RESOURCE_TYPE_LABEL) == 0)
                {
                    g_object_get(resource,
                                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_VALUE],
                                 &result,
                                 NULL);
                    break;
                }
            }
        }
    }

    return result;
}

static gint resourceSort(gpointer a, gpointer b)
{
    BDeviceServiceResource *left = B_DEVICE_SERVICE_RESOURCE(a);
    BDeviceServiceResource *right = B_DEVICE_SERVICE_RESOURCE(b);

    g_autofree gchar *leftUri = NULL;
    g_autofree gchar *rightUri = NULL;
    g_object_get(left, B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_URI], &leftUri, NULL);
    g_object_get(right, B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_URI], &rightUri, NULL);


    return g_strcmp0(leftUri, rightUri);
}

static void printDeviceEntry(BDeviceServiceDevice *device)
{
    g_autofree gchar *label = getDeviceLabel(device);
    g_autofree gchar *deviceId = NULL;
    g_autofree gchar *deviceClass = NULL;
    g_autolist(BDeviceServiceResource) resources = NULL;
    g_autolist(BDeviceServiceEndpoint) endpoints = NULL;

    g_object_get(device,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_UUID],
                 &deviceId,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS],
                 &deviceClass,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_RESOURCES],
                 &resources,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_ENDPOINTS],
                 &endpoints,
                 NULL);

    emitOutput("%s: %s, Class: %s\n", deviceId, label == NULL ? "(no label)" : label, deviceClass);

    // gather device level resources
    g_autoptr(GList) sortedResources = NULL;
    for (GList *resourcesIter = resources; resourcesIter != NULL; resourcesIter = resourcesIter->next)
    {
        BDeviceServiceResource *resource = B_DEVICE_SERVICE_RESOURCE(resourcesIter->data);
        sortedResources = g_list_insert_sorted(sortedResources, resource, (GCompareFunc) resourceSort);
    }

    // print device level resources
    for (GList *resourcesIter = sortedResources; resourcesIter != NULL; resourcesIter = resourcesIter->next)
    {
        BDeviceServiceResource *resource = B_DEVICE_SERVICE_RESOURCE(resourcesIter->data);
        g_autofree gchar *resourceUri = NULL;
        g_autofree gchar *resourceValue = NULL;
        g_object_get(resource,
                     B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_URI],
                     &resourceUri,
                     B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_VALUE],
                     &resourceValue,
                     NULL);
        emitOutput("\t%s = %s\n", resourceUri, resourceValue == NULL ? "(null)" : resourceValue);
    }

    // loop through each endpoint
    g_autoptr(GList) sortedEndpoints = NULL;
    for (GList *endpointsIter = endpoints; endpointsIter != NULL; endpointsIter = endpointsIter->next)
    {
        BDeviceServiceEndpoint *endpoint = B_DEVICE_SERVICE_ENDPOINT(endpointsIter->data);
        g_autolist(BDeviceServiceResource) endpointResources = NULL;
        g_object_get(endpoint,
                     B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_RESOURCES],
                     &endpointResources,
                     NULL);

        // print endpoint resources
        g_autoptr(GList) sortedEndpointResources = NULL;
        for (GList *endpointResourcesIter = endpointResources; endpointResourcesIter != NULL;
             endpointResourcesIter = endpointResourcesIter->next)
        {
            BDeviceServiceResource *resource = B_DEVICE_SERVICE_RESOURCE(endpointResourcesIter->data);
            sortedEndpointResources =
                g_list_insert_sorted(sortedEndpointResources, resource, (GCompareFunc) resourceSort);
        }

        g_autofree gchar *endpointId = NULL;
        g_autofree gchar *endpointProfile = NULL;
        g_object_get(endpoint,
                     B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_ID],
                     &endpointId,
                     B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE],
                     &endpointProfile,
                     NULL);
        emitOutput("\tEndpoint %s: Profile: %s\n", endpointId, endpointProfile);

        for (GList *resourcesIter = sortedEndpointResources; resourcesIter != NULL; resourcesIter = resourcesIter->next)
        {
            BDeviceServiceResource *resource = B_DEVICE_SERVICE_RESOURCE(resourcesIter->data);
            g_autofree gchar *resourceUri = NULL;
            g_autofree gchar *resourceValue = NULL;
            g_object_get(resource,
                         B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_URI],
                         &resourceUri,
                         B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_VALUE],
                         &resourceValue,
                         NULL);
            emitOutput("\t\t%s = %s\n", resourceUri, resourceValue == NULL ? "(null)" : resourceValue);
        }
    }
}

static bool printDeviceFunc(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    g_return_val_if_fail(argc == 1, false);

    g_autoptr(BDeviceServiceDevice) device = b_device_service_client_get_device_by_id(client, argv[0]);

    printDeviceEntry(device);

    return true;
}

static bool printAllDevicesFunc(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    bool rc = false;

    g_autolist(BDeviceServiceDevice) devices = NULL;
    if (argc == 0)
    {
        devices = b_device_service_client_get_devices(client);
    }
    else
    {
        devices = b_device_service_client_get_devices_by_device_class(client, argv[0]);
    }

    if (devices)
    {
        for (GList *devicesIter = devices; devicesIter != NULL; devicesIter = devicesIter->next)
        {
            BDeviceServiceDevice *device = B_DEVICE_SERVICE_DEVICE(devicesIter->data);
            printDeviceEntry(device);
        }

        rc = true;
    }
    else
    {
        emitError("Failed to get devices\n");
    }

    return rc;
}

static bool readResourceFunc(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    g_return_val_if_fail(argc == 1, false);
    bool result = true;
    g_autoptr(GError) err = NULL;
    g_autofree gchar *value = b_device_service_client_read_resource(client, argv[0], &err);

    if (err == NULL)
    {
        emitOutput("%s\n", stringCoalesce(value));
    }
    else
    {
        emitError("Failed: %s\n", err->message);

        result = false;
    }

    return result;
}

static bool writeResourceFunc(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    g_return_val_if_fail(argc == 1 || argc == 2, false);
    bool result = false;

    const gchar *resourceValue = NULL;
    if (argc == 2)
    {
        resourceValue = argv[1];
    }

    result = b_device_service_client_write_resource(client, argv[0], resourceValue);
    if (!result)
    {
        emitError("Failed to write resource\n");
    }

    return result;
}

static bool execResourceFunc(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    g_return_val_if_fail(argc == 1 || argc == 2, false);
    bool result;

    const gchar *payload = NULL;
    if (argc == 2)
    {
        payload = argv[1];
    }

    g_autofree gchar *response = NULL;
    result = b_device_service_client_execute_resource(client, argv[0], payload, &response);

    if (!result)
    {
        emitError("Failed to execute resource\n");
    }
    else if (response != NULL)
    {
        emitOutput("Execute Response: %s\n", response);
    }
    else
    {
        emitOutput("Execute Succeeded!\n");
    }

    return result;
}

static bool queryResourcesFunc(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    g_return_val_if_fail(argc == 1, false);

    g_autolist(BDeviceServiceResource) resources = b_device_service_client_query_resources_by_uri(client, argv[0]);
    if (resources)
    {
        emitOutput("resources:\n");

        GList *sortedResources = NULL;
        for (GList *iter = resources; iter != NULL; iter = g_list_next(iter))
        {
            BDeviceServiceResource *resource = B_DEVICE_SERVICE_RESOURCE(iter->data);
            sortedResources = g_list_insert_sorted(sortedResources, resource, (GCompareFunc) resourceSort);
        }

        for (GList *iter = sortedResources; iter != NULL; iter = g_list_next(iter))
        {
            BDeviceServiceResource *resource = B_DEVICE_SERVICE_RESOURCE(iter->data);
            g_autofree gchar *uri = NULL;
            g_autofree gchar *value = NULL;

            g_object_get(resource,
                         B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_URI],
                         &uri,
                         B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_VALUE],
                         &value,
                         NULL);
            emitOutput("\t%s = %s\n", uri, stringCoalesce(value));
        }

        g_list_free(sortedResources);
    }
    else
    {
        emitOutput("No resources found\n");
    }

    return true;
}

static bool readMetadataFunc(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    g_return_val_if_fail(argc == 1, false);

    g_autoptr(GError) err;
    g_autofree gchar *value = b_device_service_client_read_metadata(client, argv[0], &err);

    if (err == NULL)
    {
        emitOutput("%s\n", stringCoalesce(value));
    }
    else
    {
        emitError("Failed to read metadata: %s\n", err->message);
    }

    return true;
}

static bool writeMetadataFunc(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    g_return_val_if_fail(argc == 1 || argc == 2, false);

    bool result = b_device_service_client_write_metadata(client, argv[0], argc == 2 ? argv[1] : NULL);
    if (!result)
    {
        emitError("Failed to write metadata\n");
    }

    return result;
}

static bool queryMetadataFunc(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    g_return_val_if_fail(argc == 1, false);

    g_autolist(BDeviceServiceMetadata) metadata = b_device_service_client_get_metadata_by_uri(client, argv[0]);

    if (metadata)
    {
        emitOutput("metadata:\n");
        for (GList *iter = metadata; iter != NULL; iter = g_list_next(iter))
        {
            BDeviceServiceMetadata *meta = B_DEVICE_SERVICE_METADATA(iter->data);
            g_autofree gchar *uri = NULL;
            g_autofree gchar *value = NULL;
            g_object_get(meta,
                         B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_URI],
                         &uri,
                         B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_VALUE],
                         &value,
                         NULL);
            emitOutput("\t%s=%s\n", uri, stringCoalesce(value));
        }
    }
    else
    {
        emitOutput("No metadata found\n");
    }

    return true;
}

static bool getStatusFunc(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    (void) argc; // unused
    (void) argv; // unused
    bool result = false;

    g_autoptr(BDeviceServiceStatus) status = b_device_service_client_get_status(client);
    g_autofree gchar *statusJson = NULL;
    g_object_get(status, B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_JSON], &statusJson, NULL);

    if (!statusJson)
    {
        emitOutput("Failed to get status.\n");
    }
    else
    {
        emitOutput("Device Service Status:\n%s\n", statusJson);
        result = true;
    }

    return result;
}

static void dumpResource(BDeviceServiceResource *resource, gchar *prefix)
{
    if (resource == NULL)
    {
        return;
    }

    guint resourceMode;
    g_autofree gchar *uri = NULL;
    g_autofree gchar *value = NULL;
    g_autofree gchar *ownerId = NULL;
    g_autofree gchar *type = NULL;

    g_object_get(resource,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_MODE],
                 &resourceMode,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_URI],
                 &uri,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_VALUE],
                 &value,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_DEVICE_UUID],
                 &ownerId,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_TYPE],
                 &type,
                 NULL);


    g_autofree const gchar *modeStr = stringifyMode(resourceMode);

    emitOutput("%s%s\n", prefix, uri);
    emitOutput("%s\tvalue=%s\n", prefix, value);
    emitOutput("%s\townerId=%s\n", prefix, ownerId);
    emitOutput("%s\ttype=%s\n", prefix, type);
    emitOutput("%s\tmode=0x%x (%s)\n", prefix, resourceMode, modeStr);
}

static void dumpMetadata(GList *metadata, gchar *prefix)
{
    if (metadata == NULL)
    {
        return;
    }

    for (GList *metadatIter = metadata; metadatIter != NULL; metadatIter = metadatIter->next)
    {
        g_autofree gchar *value = NULL;
        BDeviceServiceMetadata *metadata = B_DEVICE_SERVICE_METADATA(metadatIter->data);
        g_object_get(
            metadata, B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_VALUE], &value, NULL);
        emitOutput("%s\t%s\n", prefix, value);
    }
}

static void dumpEndpoint(BDeviceServiceEndpoint *endpoint, gchar *prefix)
{
    if (endpoint == NULL)
    {
        return;
    }

    g_autofree gchar *uri = NULL;
    g_autofree gchar *profile = NULL;
    guint profileVersion;
    g_autofree gchar *ownerId = NULL;
    g_autolist(BDeviceServiceResource) resources = NULL;
    g_autolist(BDeviceServiceMetadata) metadata = NULL;

    g_object_get(endpoint,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_URI],
                 &uri,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE],
                 &profile,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE_VERSION],
                 &profileVersion,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_DEVICE_UUID],
                 &ownerId,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_RESOURCES],
                 &resources,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_METADATA],
                 &metadata,
                 NULL);

    emitOutput("%s%s\n", prefix, uri);
    emitOutput("%s\tprofile=%s\n", prefix, profile);
    emitOutput("%s\tprofileVersion=%d\n", prefix, profileVersion);
    emitOutput("%s\townerId=%s\n", prefix, ownerId);

    // resources
    emitOutput("%s\tresources:\n", prefix);
    for (GList *resourcesIter = resources; resourcesIter != NULL; resourcesIter = resourcesIter->next)
    {
        BDeviceServiceResource *resource = B_DEVICE_SERVICE_RESOURCE(resourcesIter->data);
        dumpResource(resource, "\t\t\t\t");
    }

    // metadata
    if (g_list_length(metadata) > 0)
    {
        emitOutput("\t\t\tmetadata:\n");
        dumpMetadata(metadata, prefix);
    }
}

static bool dumpDeviceFunc(BDeviceServiceClient *client, int argc, char **argv)
{
    (void) argc; // unused
    bool rc = false;

    g_autoptr(BDeviceServiceDevice) device = b_device_service_client_get_device_by_id(client, argv[0]);

    g_autofree gchar *deviceUri = NULL;
    g_autofree gchar *deviceId = NULL;
    g_autofree gchar *deviceClass = NULL;
    g_autofree gchar *managingDeviceDriver = NULL;
    guint deviceClassVersion;
    g_autolist(BDeviceServiceResource) resources = NULL;
    g_autolist(BDeviceServiceEndpoint) endpoints = NULL;
    g_autolist(BDeviceServiceMetadata) metadata = NULL;


    g_object_get(device,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_URI],
                 &deviceUri,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS],
                 &deviceClass,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS_VERSION],
                 &deviceClassVersion,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_MANAGING_DEVICE_DRIVER],
                 &managingDeviceDriver,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_RESOURCES],
                 &resources,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_ENDPOINTS],
                 &endpoints,
                 B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_METADATA],
                 &metadata,
                 NULL);

    emitOutput("%s\n", deviceUri);
    emitOutput("\tdeviceClass=%s\n", deviceClass);
    emitOutput("\tdeviceClassVersion=%d\n", deviceClassVersion);
    emitOutput("\tmanagingDeviceDriver=%s\n", managingDeviceDriver);

    // device resources
    emitOutput("\tresources:\n");
    for (GList *resourcesIter = resources; resourcesIter != NULL; resourcesIter = resourcesIter->next)
    {
        BDeviceServiceResource *resource = B_DEVICE_SERVICE_RESOURCE(resourcesIter->data);
        dumpResource(resource, "\t\t");
    }

    // endpoints
    emitOutput("\tendpoints:\n");
    for (GList *endpointsIter = endpoints; endpointsIter != NULL; endpointsIter = endpointsIter->next)
    {
        BDeviceServiceEndpoint *endpoint = B_DEVICE_SERVICE_ENDPOINT(endpointsIter->data);
        dumpEndpoint(endpoint, "\t\t");
    }

    // metadata
    if (g_list_length(metadata) > 0)
    {
        emitOutput("\tmetadata:\n");
        dumpMetadata(metadata, "\t\t");
    }

    return true;
}

static bool dumpDevicesFunc(BDeviceServiceClient *client, int argc, char **argv)
{
    (void) argc; // unused
    (void) argv; // unused

    bool rc = false;

    g_autolist(BDeviceServiceDevice) devices = b_device_service_client_get_devices(client);

    if (devices)
    {
        for (GList *devicesIter = devices; devicesIter != NULL; devicesIter = devicesIter->next)
        {
            g_autofree gchar *deviceId = NULL;
            BDeviceServiceDevice *device = B_DEVICE_SERVICE_DEVICE(devicesIter->data);
            g_object_get(
                device, B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_UUID], &deviceId, NULL);
            dumpDeviceFunc(client, 1, &deviceId);
        }
    }

    return rc;
}

static bool removeDeviceFunc(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    g_return_val_if_fail(argc == 1, false);
    bool result = b_device_service_client_remove_device(client, argv[0]);

    if (!result)
    {
        emitOutput("Failed to remove device.\n");
    }
    return result;
}

static bool removeEndpointFunc(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    g_return_val_if_fail(argc == 1, false);
    bool result = true;
    g_autoptr(BDeviceServiceEndpoint) endpoint = b_device_service_client_get_endpoint_by_uri(client, argv[0]);
    if (endpoint)
    {
        g_autofree gchar *deviceUuid = NULL;
        g_autofree gchar *endpointId = NULL;
        g_object_get(endpoint,
                     B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_DEVICE_UUID],
                     &deviceUuid,
                     B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_ID],
                     &endpointId,
                     NULL);

        result = b_device_service_client_remove_endpoint_by_id(client, deviceUuid, endpointId);

        if (!result)
        {
            emitOutput("Failed to remove endpoint.\n");
        }
    }
    return result;
}

static bool removeDevicesFunc(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    bool rc = true;

    if (argc == 0)
    {
        emitOutput("This will remove ALL devices!  Are you sure? (y/n) \n");
    }
    else
    {
        emitOutput("This will remove ALL %s devices!  Are you sure? (y/n) \n", argv[0]);
    }

    int yn = getchar();
    if (yn != 'y')
    {
        emitOutput("Not removing devices\n");
        return true;
    }
    g_autolist(BDeviceServiceDevice) devices = NULL;
    if (argc == 0)
    {
        devices = b_device_service_client_get_devices(client);
    }
    else
    {
        devices = b_device_service_client_get_devices_by_device_class(client, argv[0]);
    }

    if (devices)
    {
        for (GList *devicesIter = devices; devicesIter != NULL; devicesIter = devicesIter->next)
        {
            g_autoptr(BDeviceServiceDevice) device = B_DEVICE_SERVICE_DEVICE(devicesIter->data);
            g_autofree gchar *uuid = NULL;
            g_object_get(G_OBJECT(device),
                         B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_UUID],
                         &uuid,
                         NULL);
            bool result = b_device_service_client_remove_device(client, uuid);
            if (!result)
            {
                emitOutput("Failed to remove device: %s", uuid);
            }
            rc &= result;
        }
    }
    else
    {
        emitOutput("Failed to get devices\n");
    }
    return rc;
}

static BDeviceServicePropertyProvider *getPropertyProvider(BDeviceServiceClient *self)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_CLIENT(self), NULL);

    BDeviceServiceInitializeParamsContainer *paramsContainer = b_device_service_client_get_initialize_params(self);

    g_return_val_if_fail(B_DEVICE_SERVICE_INITIALIZE_PARAMS_CONTAINER(paramsContainer), NULL);

    BDeviceServicePropertyProvider *propertyProvider = b_device_service_initialize_params_container_get_property_provider(paramsContainer);

    return propertyProvider;
}

static bool getPropertyFunc(BDeviceServiceClient *client, gint argc,
                                   gchar **argv)
{
    (void) argc; //unused
    bool result = true;

    g_return_val_if_fail(B_DEVICE_SERVICE_CLIENT(client) != NULL, FALSE);
    g_return_val_if_fail(argv[0] != NULL, FALSE);

    g_autoptr(BDeviceServicePropertyProvider) propertyProvider = getPropertyProvider(client);

    g_return_val_if_fail(B_DEVICE_SERVICE_PROPERTY_PROVIDER(propertyProvider) != NULL, FALSE);

    g_autoptr(GError) error = NULL;

    g_autofree gchar *value = b_device_service_property_provider_get_property(propertyProvider, argv[0], &error);

    if (error)
    {
        emitError("Failed to get the property '%s': %s\n", argv[0], error->message);
        result = false;
    }
    else
    {
        emitOutput("%s\n", value);
    }

    return result;
}

static bool setPropertyFunc(BDeviceServiceClient *client, gint argc,
                                    gchar **argv)
{
    (void) argc; //unused

    g_return_val_if_fail(B_DEVICE_SERVICE_CLIENT(client) != NULL, FALSE);
    g_return_val_if_fail(argv[0] != NULL, FALSE);
    g_return_val_if_fail(argv[1] != NULL, FALSE);

    g_autoptr(BDeviceServicePropertyProvider) propertyProvider = getPropertyProvider(client);

    g_return_val_if_fail(B_DEVICE_SERVICE_PROPERTY_PROVIDER(propertyProvider) != NULL, FALSE);

    g_autoptr(GError) error = NULL;

    bool result = b_device_service_property_provider_set_property(propertyProvider, argv[0], argv[1], &error);

    if (error)
    {
        emitError("Failed to set the property");
    }

    return result;
}

static bool setDDlOverride(gchar *input, BDeviceServicePropertyProvider *provider)
{
    if (input == NULL)
    {
        emitError("Invalid url for ddl override\n");
        return false;
    }


    bool result = b_device_service_property_provider_set_property_string(
        provider, "deviceDescriptor.whitelist.url.override", input);

    if (result)
    {
        emitError("ddl override set to %s\n", input);
        return true;
    }
    else
    {
        emitError("Failed to set ddl override.\n");
        return false;
    }
}

static bool ddlFunc(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    g_return_val_if_fail(argc == 1 || argc == 2, false);

    bool result = false;

    g_autoptr(BDeviceServicePropertyProvider) propertyProvider = getPropertyProvider(client);

    if (g_ascii_strcasecmp(argv[0], "override") == 0)
    {
        if (argc == 2)
        {
            // see if this is a file
            //
            GStatBuf buf;
            if (g_stat(argv[1], &buf))
            {
                // check to see if the file is not empty
                //
                if (buf.st_size > 0)
                {
                    // need to add "file://" to the front
                    // of the file path to be a valid url request
                    //
                    g_autofree gchar *filePath = g_strdup_printf("file://%s", argv[1]);
                    result = setDDlOverride(filePath, propertyProvider);
                }
                else
                {
                    emitError("File %s is empty\n", argv[1]);
                }
            }
            else if (g_str_has_prefix(argv[1], "http") || g_str_has_prefix(argv[1], "file:///"))
            {
                // since this is a url just set prop
                //
                result = setDDlOverride(argv[1], propertyProvider);
            }
            else
            {
                emitError("Input %s is not a valid url or file request\n", argv[1]);
            }
        }
        else
        {
            emitError("Invalid input for ddl override\n");
        }
    }
    else if (g_ascii_strcasecmp(argv[0], "clearoverride") == 0)
    {
        result = b_device_service_property_provider_set_property_string(
            propertyProvider, "deviceDescriptor.whitelist.url.override", NULL);

        if (result)
        {
            emitOutput("Cleared ddl override (if one was set)\n");
        }
        else
        {
            emitError("Failed to clear any previous ddl override\n");
        }
    }
    else if (g_ascii_strcasecmp(argv[0], "process") == 0)
    {
        b_device_service_process_device_descriptors(client);
        result = true;
    }
    else if (g_ascii_strcasecmp(argv[0], "bypass") == 0 ||
             g_ascii_strcasecmp(argv[0], "clearbypass") == 0)
    {
        bool bypass = g_ascii_strcasecmp(argv[0], "bypass") == 0;

        result =
            b_device_service_property_provider_set_property_bool(propertyProvider, "deviceDescriptorBypass", bypass);
        if (result)
        {
            emitOutput("ddl %s\n", bypass ? "bypassed" : "no longer bypassed");
        }
        else
        {
            result = false;
            emitError("Failed to ddl %s\n", bypass ? "bypass" : "clear ddl bypass");
        }
    }
    else
    {
        emitError("invalid ddl subcommand\n");
    }

    return result;
}

Category *buildCoreCategory(void)
{
    Category *cat = categoryCreate("Core", "Core/standard commands");

    // discover devices
    Command *command = commandCreate("discoverStart",
                                     "dstart",
                                     "<device class> [setup code]",
                                     "Start discovery for a device class with optional setup code for Matter devices",
                                     1,
                                     2,
                                     discoverStartFunc);
    categoryAddCommand(cat, command);

    // stop discovering devices
    command = commandCreate("discoverStop", "dstop", NULL, "Stop device discovery", 0, 0, discoverStopFunc);
    categoryAddCommand(cat, command);

    // list devices
    command = commandCreate("listDevices",
                            "list",
                            "[-i] [device class]",
                            "list all devices, or all devices in a class. Use -i to show device IDs only",
                            0,
                            2,
                            listDevicesFunc);
    categoryAddCommand(cat, command);

    // print a device
    command = commandCreate("printDevice", "pd", "<uuid>", "print information for a device", 1, 1, printDeviceFunc);
    categoryAddCommand(cat, command);

    // print all devices
    command = commandCreate("printAllDevices",
                            "pa",
                            "[device class]",
                            "print information for all devices, or all devices in a class",
                            0,
                            1,
                            printAllDevicesFunc);
    categoryAddCommand(cat, command);

    // read resource
    command = commandCreate("readResource", "rr", "<uri>", "read the value of a resource", 1, 1, readResourceFunc);
    commandAddExample(command, "readResource /000d6f000aae8410/r/communicationFailure");
    categoryAddCommand(cat, command);

    // write resource
    command =
        commandCreate("writeResource", "wr", "<uri> [value]", "write the value of a resource", 1, 2, writeResourceFunc);
    commandAddExample(command, "writeResource /000d6f000aae8410/ep/1/r/label \"Front Door\"");
    categoryAddCommand(cat, command);

    // execute resource
    command = commandCreate("execResource", "er", "<uri> [value]", "execute a resource", 1, 2, execResourceFunc);
    categoryAddCommand(cat, command);

    // query resources
    command = commandCreate(
        "queryResources", "qr", "<uri pattern>", "query resources with a pattern", 1, 1, queryResourcesFunc);
    commandAddExample(command, "qr */lowBatt");
    categoryAddCommand(cat, command);

    // read metadata
    command = commandCreate("readMetadata", "rm", "<uri>", "read metadata", 1, 1, readMetadataFunc);
    commandAddExample(command, "rm /000d6f000aae8410/m/lpmPolicy");
    categoryAddCommand(cat, command);

    // write metadata
    command = commandCreate("writeMetadata", "wm", "<uri>", "write metadata", 1, 2, writeMetadataFunc);
    commandAddExample(command, "wm /000d6f000aae8410/m/lpmPolicy never");
    categoryAddCommand(cat, command);

    // query metadata
    command = commandCreate(
        "queryMetadata", "qm", "<uri pattern>", "query metadata through a uri pattern", 1, 1, queryMetadataFunc);
    commandAddExample(command, "qm */rejoins");
    categoryAddCommand(cat, command);

    // get the status of device service
    command = commandCreate("getStatus", "gs", NULL, "Get the status of device service", 0, 0, getStatusFunc);
    categoryAddCommand(cat, command);

    // dump device
    command = commandCreate("dumpDevice", "dd", "<uuid>", "Dump all details about a device", 1, 1, dumpDeviceFunc);
    categoryAddCommand(cat, command);

    // dump devices
    command = commandCreate("dumpAllDevices", "dump", NULL, "Dump all details about all devices", 0, 0, dumpDevicesFunc);
    categoryAddCommand(cat, command);

    // remove device
    command = commandCreate("removeDevice", "rd", "<uuid>", "Remove a device by uuid", 1, 1, removeDeviceFunc);
    categoryAddCommand(cat, command);

    // remove endpoint
    command = commandCreate("removeEndpoint", "re", "<uri>", "Remove an endpoint by uri", 1, 1, removeEndpointFunc);
    categoryAddCommand(cat, command);

    // remove devices (advanced)
    command = commandCreate(
        "removeDevices", NULL, "[device class]", "Remove devices (all or by class)", 0, 1, removeDevicesFunc);
    commandSetAdvanced(command);
    categoryAddCommand(cat, command);

    //system prop read
    command = commandCreate("getProperty",
                            "gp",
                            "<key>",
                            "Get a property value",
                            1,
                            1,
                            getPropertyFunc);
    categoryAddCommand(cat, command);

    //system prop write
    command = commandCreate("setProperty",
                            "sp",
                            "<key> [value]",
                            "Set a property value",
                            1,
                            2,
                            setPropertyFunc);
    categoryAddCommand(cat, command);

    // device descriptor control
    command = commandCreate("ddl",
                            NULL,
                            "override <path> | clearoverride | process | bypass | clearbypass",
                            "Configure and control device descriptor processing",
                            1,
                            2,
                            ddlFunc);
    commandAddExample(command, "ddl override /opt/etc/AllowList.xml.override");
    commandAddExample(command, "ddl clearoverride");
    commandAddExample(command, "ddl process");
    commandAddExample(command, "ddl bypass");
    commandAddExample(command, "ddl clearbypass");
    categoryAddCommand(cat, command);

    return cat;
}
