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
 * Created by Christian Leithner on 6/7/2024.
 */

#include "device-service-discovery-type.h"
#include "device-service-status.h"
#include "events/device-service-status-event.h"
#include "glib-object.h"
#include <glib.h>

typedef struct
{
    BDeviceServiceStatus *status;
    BDeviceServiceStatusEvent *event;
} BDeviceServiceStatusTest;

// Setup function called before each test
static void setup(BDeviceServiceStatusTest *test, gconstpointer user_data)
{
    test->status = b_device_service_status_new();
    test->event = b_device_service_status_event_new();
}

// Teardown function called after each test
static void teardown(BDeviceServiceStatusTest *test, gconstpointer user_data)
{
    g_clear_object(&test->status);
    g_clear_object(&test->event);
}

// Test case to check if status is created successfully
static void test_status_creation(BDeviceServiceStatusTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->status);
    g_assert_nonnull(test->event);
}

// Test case to check setting and getting properties
static void test_property_access(BDeviceServiceStatusTest *test, gconstpointer user_data)
{
    // test device class
    g_autoptr(GList) device_classes = g_list_append(NULL, "test-device-class");
    g_object_set(test->status,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_DEVICE_CLASSES],
                 device_classes,
                 NULL);
    GList *device_classes_test = NULL;
    g_object_get(test->status,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_DEVICE_CLASSES],
                 &device_classes_test,
                 NULL);
    g_assert_nonnull(device_classes_test);
    g_assert_cmpuint(g_list_length(device_classes_test), ==, 1);
    g_assert_cmpstr(g_list_first(device_classes_test)->data, ==, "test-device-class");
    g_list_free_full(device_classes_test, g_free);

    // test discovery type
    g_object_set(test->status,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_DISCOVERY_TYPE],
                 B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY,
                 NULL);
    BDeviceServiceDiscoveryType discovery_type = B_DEVICE_SERVICE_DISCOVERY_TYPE_NONE;
    g_object_get(test->status,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_DISCOVERY_TYPE],
                 &discovery_type,
                 NULL);
    g_assert_cmpuint(discovery_type, ==, B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY);


    // test searching device classes
    g_autoptr(GList) searching_device_classes = g_list_append(NULL, "test-searching-device-class");
    g_object_set(test->status,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_SEARCHING_DEVICE_CLASSES],
                 searching_device_classes,
                 NULL);
    GList *searching_device_classes_test = NULL;
    g_object_get(test->status,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_SEARCHING_DEVICE_CLASSES],
                 &searching_device_classes_test,
                 NULL);
    g_assert_nonnull(searching_device_classes_test);
    g_assert_cmpuint(g_list_length(searching_device_classes_test), ==, 1);
    g_assert_cmpstr(g_list_first(searching_device_classes_test)->data, ==, "test-searching-device-class");
    g_list_free_full(searching_device_classes_test, g_free);

    // test discovery seconds
    g_object_set(test->status,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_DISCOVERY_SECONDS],
                 G_MAXUINT,
                 NULL);
    guint discovery_seconds = 0;
    g_object_get(test->status,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_DISCOVERY_SECONDS],
                 &discovery_seconds,
                 NULL);
    g_assert_cmpuint(discovery_seconds, ==, G_MAXUINT);

    // test ready for operation
    g_object_set(test->status,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_READY_FOR_OPERATION],
                 TRUE,
                 NULL);
    gboolean ready_for_operation = FALSE;
    g_object_get(test->status,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_READY_FOR_OPERATION],
                 &ready_for_operation,
                 NULL);
    g_assert_cmpuint(ready_for_operation, ==, TRUE);

    // test ready for pairing
    g_object_set(test->status,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_READY_FOR_PAIRING],
                 TRUE,
                 NULL);
    gboolean ready_for_pairing = FALSE;
    g_object_get(test->status,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_READY_FOR_PAIRING],
                 &ready_for_pairing,
                 NULL);
    g_assert_cmpuint(ready_for_pairing, ==, TRUE);

    // test subsystems
    g_autoptr(GHashTable) subsystems = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(subsystems, g_strdup("test-subsystem"), g_strdup("test-status"));
    g_object_set(test->status,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_SUBSYSTEMS],
                 subsystems,
                 NULL);
    g_autoptr(GHashTable) subsystems_test = NULL;
    g_object_get(test->status,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_SUBSYSTEMS],
                 &subsystems_test,
                 NULL);
    g_assert_nonnull(subsystems_test);
    g_assert_cmpuint(g_hash_table_size(subsystems_test), ==, 1);
    g_assert_cmpstr(g_hash_table_lookup(subsystems_test, "test-subsystem"), ==, "test-status");

    // test json
    g_object_set(
        test->status, B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_JSON], "test-json", NULL);
    gchar *json = NULL;
    g_object_get(test->status, B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_JSON], &json, NULL);
    g_assert_nonnull(json);
    g_assert_cmpstr(json, ==, "test-json");
    g_free(json);
}

// Test get and set on the event object
static void test_event_object(BDeviceServiceStatusTest *test, gconstpointer user_data)
{
    BDeviceServiceStatus *status = b_device_service_status_new();
    g_assert_nonnull(status);

    // add an entry to device_classes
    g_autoptr(GList) device_classes = g_list_append(NULL, "test-device-class");
    g_object_set(status,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_DEVICE_CLASSES],
                 device_classes,
                 NULL);

    // set the status on the event object
    g_object_set(test->event,
                 B_DEVICE_SERVICE_STATUS_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_EVENT_PROP_STATUS],
                 status,
                 NULL);

    // get the status object instance from the event object
    BDeviceServiceStatus *status_test = NULL;
    g_object_get(test->event,
                 B_DEVICE_SERVICE_STATUS_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_EVENT_PROP_STATUS],
                 &status_test,
                 NULL);
    g_assert_nonnull(status_test);

    // check that the device_classes property is set on the status object
    GList *device_classes_test = NULL;
    g_object_get(status_test,
                 B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[B_DEVICE_SERVICE_STATUS_PROP_DEVICE_CLASSES],
                 &device_classes_test,
                 NULL);
    g_assert_nonnull(device_classes_test);
    g_assert_cmpuint(g_list_length(device_classes_test), ==, 1);
    g_assert_cmpstr(g_list_first(device_classes_test)->data, ==, "test-device-class");
    g_list_free_full(device_classes_test, g_free);

    g_clear_object(&status);
    g_clear_object(&status_test);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework

    // FIXME: vsc is sending -h and -c when selecting this as build/run target which causes gtest to puke
    argc = 0;

    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/device-service-status/status-creation",
               BDeviceServiceStatusTest,
               NULL,
               setup,
               test_status_creation,
               teardown);
    g_test_add("/device-service-status/property-access",
               BDeviceServiceStatusTest,
               NULL,
               setup,
               test_property_access,
               teardown);
    g_test_add(
        "/device-service-status/event-object", BDeviceServiceStatusTest, NULL, setup, test_event_object, teardown);

    // Run tests
    return g_test_run();
}
