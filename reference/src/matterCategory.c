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

#include "barton-core-client.h"
#include "barton-core-reference-io.h"
#include "reference-network-credentials-provider.h"
#include <stdio.h>
#include <string.h>

static bool commissionDeviceFunc(BCoreClient *client, gint argc, gchar **argv)
{
    bool rc = true;

    g_autoptr(GError) error = NULL;
    rc = b_core_client_commission_device(client, argv[0], 120, &error);
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

static bool addMatterDeviceFunc(BCoreClient *client, gint argc, gchar **argv)
{
    uint64_t nodeId = g_ascii_strtoull(argv[0], NULL, 10);

    bool rc = true;

    g_autoptr(GError) error = NULL;
    rc = b_core_client_add_matter_device(client, nodeId, 120, &error);
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

static bool openCommissioningWindow(BCoreClient *client, gint argc, gchar **argv)
{
    bool rc = false;

    /* Parse and validate the node ID.
     * Treat nodeId == 0 as local.
     * Otherwise, verify the device exists after normalizing the ID. */
    char *endptr = NULL;
    unsigned long long nodeId = g_ascii_strtoull(argv[0], &endptr, 16);

    if (endptr == argv[0] || *endptr != '\0')
    {
        emitError("Invalid node id '%s'\n", argv[0]);
        return false;
    }
    g_autofree gchar *nodeIdArg = NULL;
    if (nodeId != 0)
    {
        nodeIdArg = g_strdup_printf("%016llx", nodeId);

        /* Verify device exists.
         * NOTE: This lookup assumes Matter node IDs are used as device IDs.
         * If the internal mapping between Matter node IDs and Barton device IDs
         * changes, this lookup method may no longer be reliable. */
        g_autoptr(BCoreDevice) device = b_core_client_get_device_by_id(client, nodeIdArg);
        if (device == NULL)
        {
            emitError("No device with node id '%s' found\n", argv[0]);
            return false;
        }
    }

    guint16 timeoutSeconds = 0; // let device service pick the default
    if (argc == 2)
    {
        char *endptr = NULL;
        unsigned long long timeout = g_ascii_strtoull(argv[1], &endptr, 10);

        if (endptr == argv[1] || *endptr != '\0' || timeout > G_MAXUINT16)
        {
            emitError("Invalid timeout '%s'\n", argv[1]);
            return false;
        }

        timeoutSeconds = (guint16) timeout;
    }

    g_autoptr(BCoreCommissioningInfo) commissioningInfo =
        b_core_client_open_commissioning_window(client, nodeIdArg, timeoutSeconds);

    if (commissioningInfo == NULL)
    {
        emitError("Failed to open commissioning window\n");
    }
    else
    {
        /* print the manual and qr codes to stdout. First get them from the object properties */
        g_autofree gchar *manualCode = NULL;
        g_autofree gchar *qrCode = NULL;
        g_object_get(
            commissioningInfo,
            B_CORE_COMMISSIONING_INFO_PROPERTY_NAMES[B_CORE_COMMISSIONING_INFO_PROP_MANUAL_CODE],
            &manualCode,
            B_CORE_COMMISSIONING_INFO_PROPERTY_NAMES[B_CORE_COMMISSIONING_INFO_PROP_QR_CODE],
            &qrCode,
            NULL);

        if ((manualCode == NULL || manualCode[0] == '\0') && (qrCode == NULL || qrCode[0] == '\0'))
        {
            emitError("Failed to open commissioning window: no pairing data returned\n");
        }
        else
        {
            emitOutput("Commissioning window opened:\n");
            emitOutput("\tManual code: %s\n", manualCode != NULL ? manualCode : "");
            emitOutput("\tQR code: %s\n", qrCode != NULL ? qrCode : "");
            rc = true;
        }
    }

    return rc;
}

static bool setWifiCredsFunc(BCoreClient *client, gint argc, gchar **argv)
{
    b_reference_network_credentials_provider_set_wifi_network_credentials(argv[0], argv[1]);
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

    command = commandCreate("setWifiCreds",
                            "swc",
                            "<ssid> <password>",
                            "Set Wifi credentials username/password",
                            2,
                            2,
                            setWifiCredsFunc);
    categoryAddCommand(cat, command);

    return cat;
}
