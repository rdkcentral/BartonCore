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
 * Created by Ganesh Induri on 05/27/2025.
 */

#include "events/barton-core-zigbee-remote-cli-command-response-received-event.h"
#include "glibconfig.h"
#include <glib-object.h>

typedef struct
{
    BCoreZigbeeRemoteCliCommandResponseReceivedEvent *event;
} BCoreZigbeeRemoteCliResponseReceivedEventTest;

static void setup(BCoreZigbeeRemoteCliResponseReceivedEventTest *test, gconstpointer user_data)
{
    test->event = b_core_zigbee_remote_cli_command_response_received_event_new();
}

// Teardown function called after each test
static void teardown(BCoreZigbeeRemoteCliResponseReceivedEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BCoreZigbeeRemoteCliResponseReceivedEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BCoreZigbeeRemoteCliResponseReceivedEventTest *test, gconstpointer user_data)
{
    g_object_set(test->event,
                 B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROP_UUID],
                 "my-uuid",
                 NULL);

    gchar *uuid_test = NULL;
    g_object_get(test->event,
                 B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROP_UUID],
                 &uuid_test,
                 NULL);
    g_assert_cmpstr("my-uuid", ==, uuid_test);
    g_free(uuid_test);

    g_object_set(test->event,
                 B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROP_COMMAND_RESPONSE],
                 "myCommandResponse",
                 NULL);

    gchar *command_response_test = NULL;
    g_object_get(test->event,
                 B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROP_COMMAND_RESPONSE],
                 &command_response_test,
                 NULL);
    g_assert_cmpstr("myCommandResponse", ==, command_response_test);
    g_free(command_response_test);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define the test suite
    g_test_add("/barton-core-zigbee-remote-cli-response-received-event/event-creation",
               BCoreZigbeeRemoteCliResponseReceivedEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/barton-core-zigbee-remote-cli-response-received-event/property-access",
               BCoreZigbeeRemoteCliResponseReceivedEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run the tests
    return g_test_run();
}
