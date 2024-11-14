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
#include "device-service-resource.h"
#include "glib-object.h"
#include "glib.h"
#include "device-service-reference-io.h"
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
    bool rc = false;
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

        rc = true;
    }
    else
    {
        emitOutput("No devices\n");
    }

    return rc;
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

    return cat;
}
