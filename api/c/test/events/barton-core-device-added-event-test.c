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
 * Created by Christian Leithner on 7/3/2024.
 */

#include "events/barton-core-device-added-event.h"
#include "glibconfig.h"
#include <glib-object.h>

typedef struct
{
    BCoreDeviceAddedEvent *event;
} BCoreDeviceAddedEventTest;

static void setup(BCoreDeviceAddedEventTest *test, gconstpointer user_data)
{
    test->event = b_core_device_added_event_new();
}

// Teardown function called after each test
static void teardown(BCoreDeviceAddedEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BCoreDeviceAddedEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BCoreDeviceAddedEventTest *test, gconstpointer user_data)
{
    const gchar *uuid = "myUuid";
    g_object_set(test->event,
                 B_CORE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_ADDED_EVENT_PROP_UUID],
                 uuid,
                 NULL);

    g_autofree gchar *uuid_test = NULL;
    g_object_get(test->event,
                 B_CORE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_ADDED_EVENT_PROP_UUID],
                 &uuid_test,
                 NULL);

    g_assert_cmpstr(uuid_test, ==, uuid);

    const gchar *uri = "myUri";
    g_object_set(test->event,
                 B_CORE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_ADDED_EVENT_PROP_URI],
                 uri,
                 NULL);

    g_autofree gchar *uri_test = NULL;
    g_object_get(test->event,
                 B_CORE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_ADDED_EVENT_PROP_URI],
                 &uri_test,
                 NULL);

    g_assert_cmpstr(uri_test, ==, uri);

    const gchar *device_class = "myDeviceClass";
    g_object_set(
        test->event,
        B_CORE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS],
        device_class,
        NULL);

    g_autofree gchar *device_class_test = NULL;
    g_object_get(
        test->event,
        B_CORE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS],
        &device_class_test,
        NULL);

    g_assert_cmpstr(device_class_test, ==, device_class);

    guint device_class_version = 1;
    g_object_set(test->event,
                 B_CORE_DEVICE_ADDED_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS_VERSION],
                 device_class_version,
                 NULL);

    guint device_class_version_test = 0;
    g_object_get(test->event,
                 B_CORE_DEVICE_ADDED_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS_VERSION],
                 &device_class_version_test,
                 NULL);

    g_assert_cmpuint(device_class_version_test, ==, device_class_version);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/barton-core-device-added-event/event-creation",
               BCoreDeviceAddedEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/barton-core-device-added-event/property-access",
               BCoreDeviceAddedEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}
