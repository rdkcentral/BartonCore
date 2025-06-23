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
 * Created by Ganesh Induri on 07/26/2024.
 */

#include "events/barton-core-zigbee-channel-changed-event.h"
#include "glibconfig.h"
#include <glib-object.h>

typedef struct
{
    BCoreZigbeeChannelChangedEvent *event;
} BCoreZigbeeChannelChangedEventTest;

static void setup(BCoreZigbeeChannelChangedEventTest *test, gconstpointer user_data)
{
    test->event = b_core_zigbee_channel_changed_event_new();
}

// Teardown function called after each test
static void teardown(BCoreZigbeeChannelChangedEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BCoreZigbeeChannelChangedEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BCoreZigbeeChannelChangedEventTest *test, gconstpointer user_data)
{
    gboolean channel_changed = true;
    g_object_set(test->event,
                 B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CHANNEL_CHANGED],
                 channel_changed,
                 NULL);

    gboolean channel_changed_test = false;
    g_object_get(test->event,
                 B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CHANNEL_CHANGED],
                 &channel_changed_test,
                 NULL);

    g_assert_cmpint(channel_changed_test, ==, channel_changed);

    guint current_channel = 25;
    g_object_set(test->event,
                 B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CURRENT_CHANNEL],
                 current_channel,
                 NULL);

    guint current_channel_test = 0;
    g_object_get(test->event,
                 B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CURRENT_CHANNEL],
                 &current_channel_test,
                 NULL);

    g_assert_cmpint(current_channel, ==, current_channel_test);

    guint targeted_channel = 16;
    g_object_set(test->event,
                 B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_TARGETED_CHANNEL],
                 targeted_channel,
                 NULL);

    guint targeted_channel_test = 0;
    g_object_get(test->event,
                 B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_TARGETED_CHANNEL],
                 &targeted_channel_test,
                 NULL);

    g_assert_cmpuint(targeted_channel, ==, targeted_channel_test);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/barton-core-zigbee-channel-changed-event/event-creation",
               BCoreZigbeeChannelChangedEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/barton-core-zigbee-channel-changed-event/property-access",
               BCoreZigbeeChannelChangedEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}
