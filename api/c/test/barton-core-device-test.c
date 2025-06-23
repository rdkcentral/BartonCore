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
 * Created by Christian Leithner on 5/17/2024.
 */

#include "barton-core-device.h"
#include "barton-core-endpoint.h"
#include "barton-core-metadata.h"
#include "barton-core-resource.h"
#include "glib-object.h"
#include <glib.h>

typedef struct
{
    BCoreDevice *device;
} BCoreDeviceTest;

// Setup function called before each test
static void setup(BCoreDeviceTest *test, gconstpointer user_data)
{
    test->device = b_core_device_new();
}

// Teardown function called after each test
static void teardown(BCoreDeviceTest *test, gconstpointer user_data)
{
    g_clear_object(&test->device);
}

// Test case to check if device is created successfully
static void test_device_creation(BCoreDeviceTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->device);
}

// Test case to check setting and getting properties
static void test_property_access(BCoreDeviceTest *test, gconstpointer user_data)
{
    // Set and get properties and check their values
    g_object_set(
        test->device, B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_UUID], "test-id", NULL);
    gchar *uuid = NULL;
    g_object_get(test->device, B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_UUID], &uuid, NULL);
    g_assert_cmpstr(uuid, ==, "test-id");
    g_free(uuid);

    g_object_set(test->device,
                 B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_DEVICE_CLASS],
                 "test-class",
                 NULL);
    gchar *deviceClass = NULL;
    g_object_get(test->device,
                 B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_DEVICE_CLASS],
                 &deviceClass,
                 NULL);
    g_assert_cmpstr(deviceClass, ==, "test-class");
    g_free(deviceClass);

    g_object_set(test->device,
                 B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_DEVICE_CLASS_VERSION],
                 G_MAXUINT8,
                 NULL);
    guint deviceClassVersion = 0;
    g_object_get(test->device,
                 B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_DEVICE_CLASS_VERSION],
                 &deviceClassVersion,
                 NULL);
    g_assert_cmpuint(deviceClassVersion, ==, G_MAXUINT8);

    g_object_set(
        test->device, B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_URI], "test-uri", NULL);
    gchar *uri = NULL;
    g_object_get(test->device, B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_URI], &uri, NULL);
    g_assert_cmpstr(uri, ==, "test-uri");
    g_free(uri);

    g_object_set(test->device,
                 B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_MANAGING_DEVICE_DRIVER],
                 "test-driver",
                 NULL);
    gchar *managingDeviceDriver = NULL;
    g_object_get(test->device,
                 B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_MANAGING_DEVICE_DRIVER],
                 &managingDeviceDriver,
                 NULL);
    g_assert_cmpstr(managingDeviceDriver, ==, "test-driver");
    g_free(managingDeviceDriver);

    g_autolist(BCoreEndpoint) endpoints = NULL;
    g_autoptr(BCoreEndpoint) endpoint1 = b_core_endpoint_new();
    const gchar *endpoint1_id = "endpoint1";
    g_object_set(
        endpoint1, B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_ID], endpoint1_id, NULL);
    endpoints = g_list_append(endpoints, g_object_ref(endpoint1));

    g_object_set(
        test->device, B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_ENDPOINTS], endpoints, NULL);

    g_autolist(BCoreEndpoint) endpoints_test = NULL;
    g_object_get(test->device,
                 B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_ENDPOINTS],
                 &endpoints_test,
                 NULL);

    g_assert_nonnull(endpoints_test);
    g_assert_cmpuint(g_list_length(endpoints_test), ==, 1);
    g_autoptr(BCoreEndpoint) endpoint1_test = g_object_ref(g_list_first(endpoints_test)->data);
    gchar *endpoint1_test_id = NULL;
    g_object_get(endpoint1_test,
                 B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_ID],
                 &endpoint1_test_id,
                 NULL);
    g_assert_cmpstr(endpoint1_test_id, ==, endpoint1_id);
    g_free(endpoint1_test_id);

    g_autolist(BCoreResource) resources = NULL;
    g_autoptr(BCoreResource) resource1 = b_core_resource_new();
    const gchar *resource1_id = "resource1";
    g_object_set(
        resource1, B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_ID], resource1_id, NULL);
    resources = g_list_append(resources, g_object_ref(resource1));

    g_object_set(
        test->device, B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_RESOURCES], resources, NULL);

    g_autolist(BCoreResource) resources_test = NULL;
    g_object_get(test->device,
                 B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_RESOURCES],
                 &resources_test,
                 NULL);

    g_assert_nonnull(resources_test);
    g_assert_cmpuint(g_list_length(resources_test), ==, 1);
    g_autoptr(BCoreResource) resource1_test = g_object_ref(g_list_first(resources_test)->data);
    gchar *resource1_test_id = NULL;
    g_object_get(resource1_test,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_ID],
                 &resource1_test_id,
                 NULL);
    g_assert_cmpstr(resource1_test_id, ==, resource1_id);
    g_free(resource1_test_id);

    g_autolist(BCoreMetadata) metadata = NULL;
    g_autoptr(BCoreMetadata) metadata1 = b_core_metadata_new();
    const gchar *metadata1_id = "metadata1";
    g_object_set(
        metadata1, B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_ID], metadata1_id, NULL);
    metadata = g_list_append(metadata, g_object_ref(metadata1));

    g_object_set(
        test->device, B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_METADATA], metadata, NULL);

    g_autolist(BCoreMetadata) metadata_test = NULL;
    g_object_get(test->device,
                 B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_METADATA],
                 &metadata_test,
                 NULL);

    g_assert_nonnull(metadata_test);
    g_assert_cmpuint(g_list_length(metadata_test), ==, 1);
    g_autoptr(BCoreMetadata) metadata1_test = g_object_ref(g_list_first(metadata_test)->data);
    gchar *metadata1_test_id = NULL;
    g_object_get(metadata1_test,
                 B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_ID],
                 &metadata1_test_id,
                 NULL);
    g_assert_cmpstr(metadata1_test_id, ==, metadata1_id);
    g_free(metadata1_test_id);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/barton-core-device/device-creation",
               BCoreDeviceTest,
               NULL,
               setup,
               test_device_creation,
               teardown);
    g_test_add("/barton-core-device/property-access",
               BCoreDeviceTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}
