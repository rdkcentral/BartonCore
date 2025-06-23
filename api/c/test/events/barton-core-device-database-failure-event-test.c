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
 * Created by Christian Leithner on 8/19/2024.
 */

#include "events/barton-core-device-database-failure-event.h"

typedef struct
{
    BCoreDeviceDatabaseFailureEvent *event;
} BCoreDeviceDatabaseFailureEventTest;

static void setup(BCoreDeviceDatabaseFailureEventTest *test, gconstpointer user_data)
{
    test->event = b_core_device_database_failure_event_new();
}

// Teardown function called after each test
static void teardown(BCoreDeviceDatabaseFailureEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BCoreDeviceDatabaseFailureEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BCoreDeviceDatabaseFailureEventTest *test, gconstpointer user_data)
{
    BCoreDeviceDatabaseFailureType failure_type =
        B_CORE_DEVICE_DATABASE_FAILURE_TYPE_DEVICE_LOAD_FAILURE;
    g_object_set(test->event,
                 B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE],
                 failure_type,
                 NULL);

    BCoreDeviceDatabaseFailureType failure_type_test = -1;
    g_object_get(test->event,
                 B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE],
                 &failure_type_test,
                 NULL);

    g_assert_cmpint(failure_type, ==, failure_type_test);

    const gchar *device_id = "device-id";
    g_object_set(test->event,
                 B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROP_DEVICE_ID],
                 device_id,
                 NULL);

    g_autofree gchar *device_id_test = NULL;
    g_object_get(test->event,
                 B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROP_DEVICE_ID],
                 &device_id_test,
                 NULL);

    g_assert_cmpstr(device_id, ==, device_id_test);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/barton-core-device-database-failure-event/event-creation",
               BCoreDeviceDatabaseFailureEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/barton-core-device-database-failure-event/property-access",
               BCoreDeviceDatabaseFailureEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}
