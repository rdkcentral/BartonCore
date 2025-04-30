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
