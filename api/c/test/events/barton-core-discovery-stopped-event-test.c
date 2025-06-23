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
 * Created by Christian Leithner on 7/10/2024.
 */

#include <events/barton-core-discovery-stopped-event.h>
#include <glib-object.h>

typedef struct
{
    BCoreDiscoveryStoppedEvent *event;
} BCoreDiscoveryStoppedEventTest;

static void setup(BCoreDiscoveryStoppedEventTest *test, gconstpointer user_data)
{
    test->event = b_core_discovery_stopped_event_new();
}

// Teardown function called after each test
static void teardown(BCoreDiscoveryStoppedEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BCoreDiscoveryStoppedEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BCoreDiscoveryStoppedEventTest *test, gconstpointer user_data)
{
    const gchar *deviceClass = "myClass";

    g_object_set(test->event,
                 B_CORE_DISCOVERY_STOPPED_EVENT_PROPERTY_NAMES
                     [B_CORE_DISCOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 NULL);

    g_autofree gchar *deviceClassTest = NULL;
    g_object_get(test->event,
                 B_CORE_DISCOVERY_STOPPED_EVENT_PROPERTY_NAMES
                     [B_CORE_DISCOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS],
                 &deviceClassTest,
                 NULL);

    g_assert_cmpstr(deviceClassTest, ==, deviceClass);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/barton-core-discovery-stopped-event/event-creation",
               BCoreDiscoveryStoppedEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/barton-core-discovery-stopped-event/property-access",
               BCoreDiscoveryStoppedEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}
