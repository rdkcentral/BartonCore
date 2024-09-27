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

#include "device-service-metadata.h"
#include <glib.h>

typedef struct
{
    BDeviceServiceMetadata *metadata;
} BDeviceServiceMetadataTest;

// Setup function called before each test
static void setup(BDeviceServiceMetadataTest *test, gconstpointer user_data)
{
    test->metadata = b_device_service_metadata_new();
}

// Teardown function called after each test
static void teardown(BDeviceServiceMetadataTest *test, gconstpointer user_data)
{
    g_clear_object(&test->metadata);
}

// Test case to check if metadata is created successfully
static void test_metadata_creation(BDeviceServiceMetadataTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->metadata);
}

// Test case to check setting and getting properties
static void test_property_access(BDeviceServiceMetadataTest *test, gconstpointer user_data)
{
    // Set and get properties and check their values
    g_object_set(
        test->metadata, B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_ID], "test-id", NULL);
    gchar *id = NULL;
    g_object_get(
        test->metadata, B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_ID], &id, NULL);
    g_assert_cmpstr(id, ==, "test-id");
    g_free(id);

    g_object_set(
        test->metadata, B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_URI], "test-uri", NULL);
    gchar *uri = NULL;
    g_object_get(
        test->metadata, B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_URI], &uri, NULL);
    g_assert_cmpstr(uri, ==, "test-uri");
    g_free(uri);

    g_object_set(test->metadata,
                 B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_ENDPOINT_ID],
                 "test-endpoint-id",
                 NULL);
    gchar *endpoint_id = NULL;
    g_object_get(test->metadata,
                 B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_ENDPOINT_ID],
                 &endpoint_id,
                 NULL);
    g_assert_cmpstr(endpoint_id, ==, "test-endpoint-id");
    g_free(endpoint_id);

    g_object_set(test->metadata,
                 B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_DEVICE_UUID],
                 "test-device-uuid",
                 NULL);
    gchar *device_uuid = NULL;
    g_object_get(test->metadata,
                 B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_DEVICE_UUID],
                 &device_uuid,
                 NULL);
    g_assert_cmpstr(device_uuid, ==, "test-device-uuid");
    g_free(device_uuid);

    g_object_set(test->metadata,
                 B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_VALUE],
                 "test-value",
                 NULL);
    gchar *value = NULL;
    g_object_get(
        test->metadata, B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_VALUE], &value, NULL);
    g_assert_cmpstr(value, ==, "test-value");
    g_free(value);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/device-service-metadata/metadata-creation",
               BDeviceServiceMetadataTest,
               NULL,
               setup,
               test_metadata_creation,
               teardown);
    g_test_add("/device-service-metadata/property-access",
               BDeviceServiceMetadataTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}