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
 * Created by Christian Leithner on 5/16/2024.
 */

#include "device-service-endpoint.h"
#include "device-service-metadata.h"
#include "device-service-resource.h"
#include "glib-object.h"
#include "glibconfig.h"
#include <glib.h>

typedef struct
{
    BDeviceServiceEndpoint *endpoint;
} BDeviceServiceEndpointTest;

// Setup function called before each test
static void setup(BDeviceServiceEndpointTest *test, gconstpointer user_data)
{
    test->endpoint = b_device_service_endpoint_new();
}

// Teardown function called after each test
static void teardown(BDeviceServiceEndpointTest *test, gconstpointer user_data)
{
    g_clear_object(&test->endpoint);
}

// Test case to check if endpoint is created successfully
static void test_endpoint_creation(BDeviceServiceEndpointTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->endpoint);
}

// Test case to check setting and getting properties
static void test_property_access(BDeviceServiceEndpointTest *test, gconstpointer user_data)
{
    // Set and get properties and check their values
    g_object_set(
        test->endpoint, B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_ID], "test-id", NULL);
    gchar *id = NULL;
    g_object_get(
        test->endpoint, B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_ID], &id, NULL);
    g_assert_cmpstr(id, ==, "test-id");
    g_free(id);

    g_object_set(
        test->endpoint, B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_URI], "test-uri", NULL);
    gchar *uri = NULL;
    g_object_get(
        test->endpoint, B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_URI], &uri, NULL);
    g_assert_cmpstr(uri, ==, "test-uri");
    g_free(uri);

    g_object_set(test->endpoint,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE],
                 "test-profile",
                 NULL);
    gchar *profile = NULL;
    g_object_get(test->endpoint,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE],
                 &profile,
                 NULL);
    g_assert_cmpstr(profile, ==, "test-profile");
    g_free(profile);

    g_object_set(test->endpoint,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE_VERSION],
                 G_MAXUINT8,
                 NULL);
    guint8 profileVersion = 0;
    g_object_get(test->endpoint,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE_VERSION],
                 &profileVersion,
                 NULL);
    g_assert_cmpuint(profileVersion, ==, G_MAXUINT8);

    g_object_set(test->endpoint,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_DEVICE_UUID],
                 "test-device-uuid",
                 NULL);
    gchar *device_uuid = NULL;
    g_object_get(test->endpoint,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_DEVICE_UUID],
                 &device_uuid,
                 NULL);
    g_assert_cmpstr(device_uuid, ==, "test-device-uuid");
    g_free(device_uuid);

    g_object_set(
        test->endpoint, B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_ENABLED], TRUE, NULL);
    gboolean enabled = FALSE;
    g_object_get(test->endpoint,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_ENABLED],
                 &enabled,
                 NULL);
    g_assert_true(enabled);

    g_autoptr(GList) resources = NULL;
    g_autoptr(BDeviceServiceResource) resource1 = b_device_service_resource_new();
    const gchar *resource1_id = "resource1";
    g_object_set(
        resource1, B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_ID], resource1_id, NULL);
    resources = g_list_append(resources, g_object_ref(resource1));

    g_object_set(test->endpoint,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_RESOURCES],
                 resources,
                 NULL);

    g_autoptr(GList) resources_test = NULL;
    g_object_get(test->endpoint,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_RESOURCES],
                 &resources_test,
                 NULL);

    g_assert_nonnull(resources_test);
    g_assert_cmpuint(g_list_length(resources_test), ==, 1);
    g_autoptr(BDeviceServiceResource) resource1_test = g_object_ref(g_list_first(resources_test)->data);
    gchar *resource1_test_id = NULL;
    g_object_get(resource1_test,
                 B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_ID],
                 &resource1_test_id,
                 NULL);
    g_assert_cmpstr(resource1_test_id, ==, resource1_id);
    g_free(resource1_test_id);

    g_autoptr(GList) metadata = NULL;
    g_autoptr(BDeviceServiceMetadata) metadata1 = b_device_service_metadata_new();
    const gchar *metadata1_id = "metadata1";
    g_object_set(
        metadata1, B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_ID], metadata1_id, NULL);
    metadata = g_list_append(metadata, g_object_ref(metadata1));

    g_object_set(test->endpoint,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_METADATA],
                 metadata,
                 NULL);

    g_autoptr(GList) metadata_test = NULL;
    g_object_get(test->endpoint,
                 B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_METADATA],
                 &metadata_test,
                 NULL);

    g_assert_nonnull(metadata_test);
    g_assert_cmpuint(g_list_length(metadata_test), ==, 1);
    g_autoptr(BDeviceServiceMetadata) metadata1_test = g_object_ref(g_list_first(metadata_test)->data);
    gchar *metadata1_test_id = NULL;
    g_object_get(metadata1_test,
                 B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_ID],
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
    g_test_add("/device-service-endpoint/endpoint-creation",
               BDeviceServiceEndpointTest,
               NULL,
               setup,
               test_endpoint_creation,
               teardown);
    g_test_add("/device-service-endpoint/property-access",
               BDeviceServiceEndpointTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}