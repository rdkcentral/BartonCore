//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2019 Comcast Corporation
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
// Comcast Corporation retains all ownership rights.
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
#include "device-service-resource.h"
#include "glib-object.h"
#include "glib.h"
#include "device-service-reference-io.h"
#include "icUtil/stringUtils.h"
#include <linenoise.h>
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
    g_autolist(BDeviceServiceResource) sortedResources = NULL;
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
    g_autolist(BDeviceServiceResource) sortedEndpoints = NULL;
    for (GList *endpointsIter = endpoints; endpointsIter != NULL; endpointsIter = endpointsIter->next)
    {
        BDeviceServiceEndpoint *endpoint = B_DEVICE_SERVICE_ENDPOINT(endpointsIter->data);
        g_autolist(BDeviceServiceResource) endpointResources = NULL;
        g_object_get(endpoint,
                     B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_RESOURCES],
                     &endpointResources,
                     NULL);

        // print endpoint resources
        g_autolist(BDeviceServiceResource) sortedEndpointResources = NULL;
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

    g_autofree gchar *value = b_device_service_client_read_metadata(client, argv[0]);

    emitOutput("%s\n", stringCoalesce(value));

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

    return cat;
}
