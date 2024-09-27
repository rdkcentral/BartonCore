//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2024 Comcast
//
// All rights reserved.
//
// This software is protected by copyright laws of the United States
// and of foreign countries. This material may also be protected by
// patent laws of the United States and of foreign countries.
//
// This software is furnished under a license agreement and/or a
// nondisclosure agreement and may only be used or copied in accordance
// with the terms of those agreements.
//
// The mere transfer of this software does not imply any licenses of trade
// secrets, proprietary technology, copyrights, patents, trademarks, or
// any other form of intellectual property whatsoever.
//
// Comcast retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*
 * Created by Christian Leithner on 5/15/2024.
 */

#include "device-service-resource.h"
#include <glib.h>

// Define a test fixture struct
typedef struct
{
    BDeviceServiceResource *resource;
} BDeviceServiceResourceTest;

// Setup function called before each test
static void setup(BDeviceServiceResourceTest *test, gconstpointer user_data)
{
    test->resource = b_device_service_resource_new();
}

// Teardown function called after each test
static void teardown(BDeviceServiceResourceTest *test, gconstpointer user_data)
{
    g_clear_object(&test->resource);
}

// Test case to check if resource is created successfully
static void test_resource_creation(BDeviceServiceResourceTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->resource);
}

// Test case to check setting and getting properties
static void test_property_access(BDeviceServiceResourceTest *test, gconstpointer user_data)
{
    // Set and get properties and check their values
    g_object_set(
        test->resource, B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_ID], "test-id", NULL);
    gchar *id = NULL;
    g_object_get(
        test->resource, B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_ID], &id, NULL);
    g_assert_cmpstr(id, ==, "test-id");
    g_free(id);

    g_object_set(
        test->resource, B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_URI], "test-uri", NULL);
    gchar *uri = NULL;
    g_object_get(
        test->resource, B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_URI], &uri, NULL);
    g_assert_cmpstr(uri, ==, "test-uri");
    g_free(uri);

    g_object_set(test->resource,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_ENDPOINT_ID],
                 "test-endpoint-id",
                 NULL);
    gchar *endpoint_id = NULL;
    g_object_get(test->resource,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_ENDPOINT_ID],
                 &endpoint_id,
                 NULL);
    g_assert_cmpstr(endpoint_id, ==, "test-endpoint-id");
    g_free(endpoint_id);

    g_object_set(test->resource,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_DEVICE_UUID],
                 "test-device-uuid",
                 NULL);
    gchar *device_uuid = NULL;
    g_object_get(test->resource,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_DEVICE_UUID],
                 &device_uuid,
                 NULL);
    g_assert_cmpstr(device_uuid, ==, "test-device-uuid");
    g_free(device_uuid);

    g_object_set(test->resource,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_VALUE],
                 "test-value",
                 NULL);
    gchar *value = NULL;
    g_object_get(
        test->resource, B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_VALUE], &value, NULL);
    g_assert_cmpstr(value, ==, "test-value");
    g_free(value);

    g_object_set(test->resource,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_TYPE],
                 "test-type",
                 NULL);
    gchar *type = NULL;
    g_object_get(
        test->resource, B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_TYPE], &type, NULL);
    g_assert_cmpstr(type, ==, "test-type");
    g_free(type);

    g_object_set(
        test->resource, B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_MODE], 123, NULL);
    guint mode = 0;
    g_object_get(
        test->resource, B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_MODE], &mode, NULL);
    g_assert_cmpuint(mode, ==, 123);

    g_object_set(test->resource,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_CACHING_POLICY],
                 B_DEVICE_SERVICE_RESOURCE_CACHING_POLICY_ALWAYS,
                 NULL);
    BDeviceServiceResourceCachingPolicy caching_policy = B_DEVICE_SERVICE_RESOURCE_CACHING_POLICY_NEVER;
    g_object_get(test->resource,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_CACHING_POLICY],
                 &caching_policy,
                 NULL);
    g_assert_cmpint(caching_policy, ==, B_DEVICE_SERVICE_RESOURCE_CACHING_POLICY_ALWAYS);

    g_object_set(test->resource,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_DATE_OF_LAST_SYNC_MILLIS],
                 G_MAXUINT64,
                 NULL);
    guint64 date_of_last_sync_millis = 0;
    g_object_get(test->resource,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_DATE_OF_LAST_SYNC_MILLIS],
                 &date_of_last_sync_millis,
                 NULL);
    g_assert_cmpuint(date_of_last_sync_millis, ==, G_MAXUINT64);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/device-service-resource/resource-creation",
               BDeviceServiceResourceTest,
               NULL,
               setup,
               test_resource_creation,
               teardown);
    g_test_add("/device-service-resource/property-access",
               BDeviceServiceResourceTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}
