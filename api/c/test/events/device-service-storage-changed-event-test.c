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
 * Created by Christian Leithner on 8/5/2024.
 */

#include "events/device-service-storage-changed-event.h"
#include "gio/gio.h"

typedef struct
{
    BDeviceServiceStorageChangedEvent *event;
} BDeviceServiceStorageChangedEventTest;

static void setup(BDeviceServiceStorageChangedEventTest *test, gconstpointer user_data)
{
    test->event = b_device_service_storage_changed_event_new();
}

// Teardown function called after each test
static void teardown(BDeviceServiceStorageChangedEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BDeviceServiceStorageChangedEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BDeviceServiceStorageChangedEventTest *test, gconstpointer user_data)
{
    GFileMonitorEvent what_changed = G_FILE_MONITOR_EVENT_CREATED;
    g_object_set(
        test->event,
        B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROP_WHAT_CHANGED],
        what_changed,
        NULL);

    GFileMonitorEvent what_changed_test = G_FILE_MONITOR_EVENT_DELETED;

    g_object_get(
        test->event,
        B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROP_WHAT_CHANGED],
        &what_changed_test,
        NULL);

    g_assert_cmpint(what_changed_test, ==, what_changed);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/device-service-storage-changed-event/event-creation",
               BDeviceServiceStorageChangedEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/device-service-storage-changed-event/property-access",
               BDeviceServiceStorageChangedEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}
