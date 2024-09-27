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
 * Created by Christian Leithner on 6/10/2024.
 */

#include "device-service-device-found-details.h"

typedef struct
{
    BDeviceServiceDeviceFoundDetails *details;
} BDeviceServiceDeviceFoundDetailsTest;

// Setup function called before each test
static void setup(BDeviceServiceDeviceFoundDetailsTest *test, gconstpointer user_data)
{
    test->details = b_device_service_device_found_details_new();
}

// Teardown function called after each test
static void teardown(BDeviceServiceDeviceFoundDetailsTest *test, gconstpointer user_data)
{
    g_clear_object(&test->details);
}

// Test case to check if device is created successfully
static void test_discovery_details_creation(BDeviceServiceDeviceFoundDetailsTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->details);
}

// Test case to check setting and getting properties
static void test_property_access(BDeviceServiceDeviceFoundDetailsTest *test, gconstpointer user_data)
{
    g_object_set(test->details,
                 B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_UUID],
                 "test-id",
                 NULL);
    g_autofree gchar *uuid = NULL;
    g_object_get(test->details,
                 B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_UUID],
                 &uuid,
                 NULL);
    g_assert_cmpstr(uuid, ==, "test-id");

    g_object_set(
        test->details,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_MANUFACTURER],
        "test-manufacturer",
        NULL);
    g_autofree gchar *manufacturer = NULL;
    g_object_get(
        test->details,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_MANUFACTURER],
        &manufacturer,
        NULL);
    g_assert_cmpstr(manufacturer, ==, "test-manufacturer");

    g_object_set(test->details,
                 B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_MODEL],
                 "test-model",
                 NULL);
    g_autofree gchar *model = NULL;
    g_object_get(test->details,
                 B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_MODEL],
                 &model,
                 NULL);
    g_assert_cmpstr(model, ==, "test-model");

    g_object_set(test->details,
                 B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_HARDWARE_VERSION],
                 "test-hw-version",
                 NULL);
    g_autofree gchar *hardwareVersion = NULL;
    g_object_get(test->details,
                 B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_HARDWARE_VERSION],
                 &hardwareVersion,
                 NULL);
    g_assert_cmpstr(hardwareVersion, ==, "test-hw-version");

    g_object_set(test->details,
                 B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_FIRMWARE_VERSION],
                 "test-fw-version",
                 NULL);
    g_autofree gchar *firmwareVersion = NULL;
    g_object_get(test->details,
                 B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_FIRMWARE_VERSION],
                 &firmwareVersion,
                 NULL);
    g_assert_cmpstr(firmwareVersion, ==, "test-fw-version");

    g_object_set(
        test->details,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_DEVICE_CLASS],
        "test-class",
        NULL);
    g_autofree gchar *deviceClass = NULL;
    g_object_get(
        test->details,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_DEVICE_CLASS],
        &deviceClass,
        NULL);
    g_assert_cmpstr(deviceClass, ==, "test-class");

    g_autoptr(GHashTable) metadata = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(metadata, g_strdup("key1"), g_strdup("value1"));
    g_hash_table_insert(metadata, g_strdup("key2"), g_strdup("value2"));

    g_object_set(
        test->details,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_METADATA],
        metadata,
        NULL);

    g_autoptr(GHashTable) metadata_test = NULL;
    g_object_get(
        test->details,
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_METADATA],
        &metadata_test,
        NULL);

    g_assert_nonnull(metadata_test);
    g_assert_cmpuint(g_hash_table_size(metadata_test), ==, g_hash_table_size(metadata));
    g_assert_cmpstr(g_hash_table_lookup(metadata_test, "key1"), ==, "value1");
    g_assert_cmpstr(g_hash_table_lookup(metadata_test, "key2"), ==, "value2");

    g_autoptr(GHashTable) endpointProfiles = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(endpointProfiles, g_strdup("endpointId1"), g_strdup("profile1"));
    g_hash_table_insert(endpointProfiles, g_strdup("endpointId2"), g_strdup("profile2"));

    g_object_set(test->details,
                 B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_ENDPOINT_PROFILES],
                 endpointProfiles,
                 NULL);

    g_autoptr(GHashTable) endpointProfiles_test = NULL;
    g_object_get(test->details,
                 B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_ENDPOINT_PROFILES],
                 &endpointProfiles_test,
                 NULL);

    g_assert_nonnull(endpointProfiles_test);
    g_assert_cmpuint(g_hash_table_size(endpointProfiles_test), ==, g_hash_table_size(endpointProfiles));
    g_assert_cmpstr(g_hash_table_lookup(endpointProfiles_test, "endpointId1"), ==, "profile1");
    g_assert_cmpstr(g_hash_table_lookup(endpointProfiles_test, "endpointId2"), ==, "profile2");
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/device-service-device-found-details/device-creation",
               BDeviceServiceDeviceFoundDetailsTest,
               NULL,
               setup,
               test_discovery_details_creation,
               teardown);
    g_test_add("/device-service-device-found-details/property-access",
               BDeviceServiceDeviceFoundDetailsTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}