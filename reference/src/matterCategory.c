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
 * Created by Micah Koch on 10/31/24.
 */


#include "matterCategory.h"

#include "device-service-client.h"
#include "device-service-reference-io.h"
#include <stdio.h>

static bool commissionDeviceFunc(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    g_return_val_if_fail(argc == 1, false);
    g_return_val_if_fail(argv != NULL, false);
    g_return_val_if_fail(argv[0] != NULL, false);

    bool rc = true;

    g_autoptr(GError) error = NULL;
    rc = b_device_service_client_commission_device(client, argv[0], 120, &error);
    if (rc)
    {
        emitOutput("Attempting to commission device\n");
    }
    else
    {
        if (error != NULL && error->message != NULL)
        {
            emitError("Failed to commission device: %s\n", error->message);
        }
        else
        {
            emitError("Failed to commission device: Unknown error\n");
        }
    }

    return rc;
}

static bool addMatterDeviceFunc(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    g_return_val_if_fail(argc == 1, false);
    g_return_val_if_fail(argv != NULL, false);
    g_return_val_if_fail(argv[0] != NULL, false);

    uint64_t nodeId = g_ascii_strtoull(argv[0], NULL, 10);

    bool rc = true;

    g_autoptr(GError) error = NULL;
    rc = b_device_service_client_add_matter_device(client, nodeId, 120, &error);
    if (rc)
    {
        emitOutput("Attempting to add Matter device\n");
    }
    else
    {
        if (error != NULL && error->message != NULL)
        {
            emitError("Failed to add Matter device: %s\n", error->message);
        }
        else
        {
            emitError("Failed to add Matter device: Unknown error\n");
        }
    }

    return rc;
}

static bool openCommissioningWindow(BDeviceServiceClient *client, gint argc, gchar **argv)
{
    (void) argc; // unused
    bool rc = false;

    guint16 timeoutSeconds = 0; // let device service pick the default
    if (argc == 2)
    {
        timeoutSeconds = (guint16) g_ascii_strtoull(argv[1], NULL, 10);
    }

    BDeviceServiceCommissioningInfo *commissioningInfo =
        b_device_service_client_open_commissioning_window(client, argv[0], timeoutSeconds);

    if (commissioningInfo == NULL)
    {
        emitError("Failed to open commissioning window\n");
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

        emitOutput("Commissioning window opened:\n");
        emitOutput("\tManual code: %s\n", manualCode);
        emitOutput("\tQR code: %s\n", qrCode);

        rc = true;
    }

    return rc;
}

Category *buildMatterCategory(void)
{
    Category *cat = categoryCreate("Matter", "Matter related commands");

    Command *command = commandCreate("commissionDevice",
                                     "cd",
                                     "<setup payload>",
                                     "Commission a specific device with the provided setup payload",
                                     1,
                                     1,
                                     commissionDeviceFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("addMatterDevice",
                            "amd",
                            "<node id>",
                            "Add a Matter device, which has already been commissioned onto the fabric, to the database"
                            "after locating and configuring it",
                            1,
                            1,
                            addMatterDeviceFunc);
    categoryAddCommand(cat, command);

    command = commandCreate(
        "openCommissioningWindow",
        "ocw",
        "<node id> [timeout secs]",
        "Open the commissioning window locally (node id 0) or for a specific node id. Omit timeout for defaults",
        1,
        2,
        openCommissioningWindow);
    categoryAddCommand(cat, command);

    return cat;
}
