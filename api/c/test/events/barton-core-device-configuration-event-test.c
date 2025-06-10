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
 * Created by Christian Leithner on 7/1/2024.
 */

#include <events/barton-core-device-configuration-event.h>
#include <glib-object.h>

#define B_CORE_TEST_EVENT_TYPE (b_core_test_event_get_type())
G_DECLARE_FINAL_TYPE(BCoreTestEvent,
                     b_core_test_event,
                     B_CORE,
                     TEST_EVENT,
                     BCoreDeviceConfigurationEvent);

struct _BCoreTestEvent
{
    BCoreDeviceConfigurationEvent parent_instance;
};

G_DEFINE_TYPE(BCoreTestEvent, b_core_test_event, B_CORE_DEVICE_CONFIGURATION_EVENT_TYPE);

static void b_core_test_event_class_init(BCoreTestEventClass *klass) {}

static void b_core_test_event_init(BCoreTestEvent *self) {}

BCoreTestEvent *b_core_test_event_new(void)
{
    return g_object_new(B_CORE_TEST_EVENT_TYPE, NULL);
}

typedef struct
{
    BCoreTestEvent *testEvent;
} BCoreEventTest;

static void setup(BCoreEventTest *test, gconstpointer user_data)
{
    test->testEvent = b_core_test_event_new();
}

// Teardown function called after each test
static void teardown(BCoreEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->testEvent);
}

static void test_property_access(BCoreEventTest *test, gconstpointer user_data)
{
    const gchar *expectedUUID = NULL;
    const gchar *expectedClass = NULL;
    g_autofree gchar *actualUUID = NULL;
    g_autofree gchar *actualClass = NULL;

    g_object_get(test->testEvent,
                 B_CORE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 &actualUUID,
                 B_CORE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 &actualClass,
                 NULL);

    g_assert_cmpstr(actualUUID, ==, expectedUUID);
    g_assert_cmpstr(actualClass, ==, expectedClass);

    expectedUUID = "test-uuid";
    expectedClass = "test-class";

    g_object_set(test->testEvent,
                 B_CORE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 expectedUUID,
                 B_CORE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 expectedClass,
                 NULL);

    g_object_get(test->testEvent,
                 B_CORE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                 &actualUUID,
                 B_CORE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                 &actualClass,
                 NULL);

    g_assert_cmpstr(actualUUID, ==, expectedUUID);
    g_assert_cmpstr(actualClass, ==, expectedClass);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    g_test_add("/barton-core-device-configuration-event/property-access",
               BCoreEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}
