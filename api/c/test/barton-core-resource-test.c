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
 * Created by Christian Leithner on 5/15/2024.
 */

#include "barton-core-resource.h"
#include <glib.h>

// Define a test fixture struct
typedef struct
{
    BCoreResource *resource;
} BCoreResourceTest;

// Setup function called before each test
static void setup(BCoreResourceTest *test, gconstpointer user_data)
{
    test->resource = b_core_resource_new();
}

// Teardown function called after each test
static void teardown(BCoreResourceTest *test, gconstpointer user_data)
{
    g_clear_object(&test->resource);
}

// Test case to check if resource is created successfully
static void test_resource_creation(BCoreResourceTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->resource);
}

// Test case to check setting and getting properties
static void test_property_access(BCoreResourceTest *test, gconstpointer user_data)
{
    // Set and get properties and check their values
    g_object_set(
        test->resource, B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_ID], "test-id", NULL);
    gchar *id = NULL;
    g_object_get(
        test->resource, B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_ID], &id, NULL);
    g_assert_cmpstr(id, ==, "test-id");
    g_free(id);

    g_object_set(
        test->resource, B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_URI], "test-uri", NULL);
    gchar *uri = NULL;
    g_object_get(
        test->resource, B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_URI], &uri, NULL);
    g_assert_cmpstr(uri, ==, "test-uri");
    g_free(uri);

    g_object_set(test->resource,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_ENDPOINT_ID],
                 "test-endpoint-id",
                 NULL);
    gchar *endpoint_id = NULL;
    g_object_get(test->resource,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_ENDPOINT_ID],
                 &endpoint_id,
                 NULL);
    g_assert_cmpstr(endpoint_id, ==, "test-endpoint-id");
    g_free(endpoint_id);

    g_object_set(test->resource,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_DEVICE_UUID],
                 "test-device-uuid",
                 NULL);
    gchar *device_uuid = NULL;
    g_object_get(test->resource,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_DEVICE_UUID],
                 &device_uuid,
                 NULL);
    g_assert_cmpstr(device_uuid, ==, "test-device-uuid");
    g_free(device_uuid);

    g_object_set(test->resource,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_VALUE],
                 "test-value",
                 NULL);
    gchar *value = NULL;
    g_object_get(
        test->resource, B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_VALUE], &value, NULL);
    g_assert_cmpstr(value, ==, "test-value");
    g_free(value);

    g_object_set(test->resource,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_TYPE],
                 "test-type",
                 NULL);
    gchar *type = NULL;
    g_object_get(
        test->resource, B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_TYPE], &type, NULL);
    g_assert_cmpstr(type, ==, "test-type");
    g_free(type);

    g_object_set(
        test->resource, B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_MODE], 123, NULL);
    guint mode = 0;
    g_object_get(
        test->resource, B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_MODE], &mode, NULL);
    g_assert_cmpuint(mode, ==, 123);

    g_object_set(test->resource,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_CACHING_POLICY],
                 B_CORE_RESOURCE_CACHING_POLICY_ALWAYS,
                 NULL);
    BCoreResourceCachingPolicy caching_policy = B_CORE_RESOURCE_CACHING_POLICY_NEVER;
    g_object_get(test->resource,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_CACHING_POLICY],
                 &caching_policy,
                 NULL);
    g_assert_cmpint(caching_policy, ==, B_CORE_RESOURCE_CACHING_POLICY_ALWAYS);

    g_object_set(test->resource,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_DATE_OF_LAST_SYNC_MILLIS],
                 G_MAXUINT64,
                 NULL);
    guint64 date_of_last_sync_millis = 0;
    g_object_get(test->resource,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_DATE_OF_LAST_SYNC_MILLIS],
                 &date_of_last_sync_millis,
                 NULL);
    g_assert_cmpuint(date_of_last_sync_millis, ==, G_MAXUINT64);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/barton-core-resource/resource-creation",
               BCoreResourceTest,
               NULL,
               setup,
               test_resource_creation,
               teardown);
    g_test_add("/barton-core-resource/property-access",
               BCoreResourceTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}
