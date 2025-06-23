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
 * Created by Thomas Lea on 2/7/25.
 */


#include "threadCategory.h"

#include "barton-core-client.h"
#include "barton-core-reference-io.h"

static bool setNat64Enabled(BCoreClient *client, gint argc, gchar **argv)
{
    gboolean enabled = g_ascii_strcasecmp(argv[0], "true") == 0;

    bool rc = b_core_client_thread_set_nat64_enabled(client, enabled);

    if (rc)
    {
        emitOutput("Success\n");
    }
    else
    {
        emitError("Failed\n");
    }

    return rc;
}

static bool activateEphemeralKeyMode(BCoreClient *client, gint argc, gchar **argv)
{
    bool rc = false;
    gchar *key = b_core_client_thread_activate_ephemeral_key_mode(client);

    if (key)
    {
        emitOutput("Success: ePSKc = %s\n", key);
        rc = true;
    }
    else
    {
        emitError("Failed\n");
    }

    return rc;
}

Category *buildThreadCategory(void)
{
    Category *cat = categoryCreate("Thread", "Thread related commands");

    Command *command = commandCreate("setNat64Enabled",
                                     "sne",
                                     "<true|false>",
                                     "Enable or disable the Thread Border Router's NAT64 system",
                                     1,
                                     1,
                                     setNat64Enabled);
    categoryAddCommand(cat, command);

    command = commandCreate("activateEphemeralKeyMode",
                            "aekm",
                            NULL,
                            "Activate ephemeral key mode and print the ePSKc",
                            0,
                            0,
                            activateEphemeralKeyMode);
    categoryAddCommand(cat, command);

    return cat;
}
