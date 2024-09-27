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
 * Created by Christian Leithner on 6/5/2024.
 */

#include "device-service-discovery-filter.h"

typedef struct
{
    BDeviceServiceDiscoveryFilter *filter;
} BDeviceServiceDiscoveryFilterTest;

static void setup(BDeviceServiceDiscoveryFilterTest *test, gconstpointer user_data)
{
    test->filter = b_device_service_discovery_filter_new();
}

static void teardown(BDeviceServiceDiscoveryFilterTest *test, gconstpointer user_data)
{
    g_clear_object(&test->filter);
}

static void test_discovery_filter_creation(BDeviceServiceDiscoveryFilterTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->filter);
}

static void test_property_access(BDeviceServiceDiscoveryFilterTest *test, gconstpointer user_data)
{
    // Set and get properties and check their values
    g_object_set(test->filter,
                 B_DEVICE_SERVICE_DISCOVERY_FILTER_PROPERTY_NAMES[B_DEVICE_SERVICE_DISCOVERY_FILTER_PROP_URI],
                 "test-uri",
                 NULL);
    gchar *uri = NULL;
    g_object_get(test->filter,
                 B_DEVICE_SERVICE_DISCOVERY_FILTER_PROPERTY_NAMES[B_DEVICE_SERVICE_DISCOVERY_FILTER_PROP_URI],
                 &uri,
                 NULL);
    g_assert_cmpstr(uri, ==, "test-uri");
    g_free(uri);

    g_object_set(test->filter,
                 B_DEVICE_SERVICE_DISCOVERY_FILTER_PROPERTY_NAMES[B_DEVICE_SERVICE_DISCOVERY_FILTER_PROP_VALUE],
                 "test-value",
                 NULL);
    gchar *value = NULL;
    g_object_get(test->filter,
                 B_DEVICE_SERVICE_DISCOVERY_FILTER_PROPERTY_NAMES[B_DEVICE_SERVICE_DISCOVERY_FILTER_PROP_VALUE],
                 &value,
                 NULL);
    g_assert_cmpstr(value, ==, "test-value");
    g_free(value);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/device-service-discovery-filter/discovery-filter-creation",
               BDeviceServiceDiscoveryFilterTest,
               NULL,
               setup,
               test_discovery_filter_creation,
               teardown);
    g_test_add("/device-service-discovery-filter/property-access",
               BDeviceServiceDiscoveryFilterTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}