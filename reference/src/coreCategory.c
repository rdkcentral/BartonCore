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
#include <linenoise.h>
#include <stdio.h>

#define DISCOVERY_SECONDS 60

static bool discoverStartFunc(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    (void) argc; // unused
    bool rc = false;

    fprintf(stdout, "Starting discovery of %s for %d seconds\n", argv[0], DISCOVERY_SECONDS);

    g_autoptr(GList) deviceClasses = NULL;
    deviceClasses = g_list_append(deviceClasses, argv[0]);

    g_autoptr(GError) err = NULL;
    if (!b_device_service_client_discover_start(client, deviceClasses, NULL, DISCOVERY_SECONDS, &err))
    {
        if (err != NULL)
        {
            fprintf(stderr, "Failed to start discovery : %d - %s\n", err->code, err->message);
        }
        else
        {
            fprintf(stderr, "Unable to start discovery of %s\n", argv[0]);
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
        fprintf(stderr, "Failed to stop discovery\n");
    }
    else
    {
        rc = true;
    }

    return rc;
}

static bool openCommissioningWindow(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    (void) argc; // unused
    bool rc = false;

    guint16 timeoutSeconds = 0; //let device service pick the default
    if (argc == 2)
    {
        timeoutSeconds = (guint16) g_ascii_strtoull(argv[1], NULL, 10);
    }

    BDeviceServiceCommissioningInfo *commissioningInfo = b_device_service_client_open_commissioning_window(client, argv[0], timeoutSeconds);

    if (commissioningInfo == NULL)
    {
        fprintf(stderr, "Failed to open commissioning window\n");
    }
    else
    {
        // print the manual and qr codes to stdout.  First get them from the object properties
        g_autofree gchar *manualCode = NULL;
        g_autofree gchar *qrCode = NULL;
        g_object_get(
            commissioningInfo,
            B_DEVICE_SERVICE_COMMISSIONING_INFO_PROPERTY_NAMES[B_DEVICE_SERVICE_COMMISSIONING_INFO_PROP_MANUAL_CODE],
            &manualCode,
            B_DEVICE_SERVICE_COMMISSIONING_INFO_PROPERTY_NAMES[B_DEVICE_SERVICE_COMMISSIONING_INFO_PROP_QR_CODE],
            &qrCode,
            NULL);

        fprintf(stdout, "Commissioning window opened:\n");
        fprintf(stdout, "\tManual code: %s\n", manualCode);
        fprintf(stdout, "\tQR code: %s\n", qrCode);

        rc = true;
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

    command = commandCreate("openCommissioningWindow",
                            "ocw",
                            "<node id> [timeout secs]",
                            "Open the commissioning window locally (node id 0) or for a specific node id. Omit timeout for defaults",
                            1,
                            2,
                            openCommissioningWindow);
    categoryAddCommand(cat, command);

    return cat;
}
