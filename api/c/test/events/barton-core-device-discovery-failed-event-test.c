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
 * Created by Christian Leithner on 6/13/2024.
 */

#include <barton-core-device-found-details.h>
#include <events/barton-core-device-discovery-failed-event.h>
#include <glib-object.h>

typedef struct
{
    BCoreDeviceDiscoveryFailedEvent *event;
} BCoreDeviceDiscoveryFailedEventTest;

static void setup(BCoreDeviceDiscoveryFailedEventTest *test, gconstpointer user_data)
{
    test->event = b_core_device_discovery_failed_event_new();
}

// Teardown function called after each test
static void teardown(BCoreDeviceDiscoveryFailedEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BCoreDeviceDiscoveryFailedEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BCoreDeviceDiscoveryFailedEventTest *test, gconstpointer user_data)
{
    g_autoptr(BCoreDeviceFoundDetails) deviceFoundDetails = b_core_device_found_details_new();
    const gchar *deviceId = "myDeviceId";
    g_object_set(deviceFoundDetails,
                 B_CORE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_CORE_DEVICE_FOUND_DETAILS_PROP_UUID],
                 deviceId,
                 NULL);

    g_object_set(test->event,
                 B_CORE_DEVICE_DISCOVERY_FAILED_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_DISCOVERY_FAILED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 deviceFoundDetails,
                 NULL);

    g_autoptr(BCoreDeviceFoundDetails) deviceFoundDetailsTest = NULL;
    g_object_get(test->event,
                 B_CORE_DEVICE_DISCOVERY_FAILED_EVENT_PROPERTY_NAMES
                     [B_CORE_DEVICE_DISCOVERY_FAILED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 &deviceFoundDetailsTest,
                 NULL);

    g_assert_nonnull(deviceFoundDetailsTest);

    g_autofree gchar *deviceIdTest = NULL;
    g_object_get(deviceFoundDetailsTest,
                 B_CORE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_CORE_DEVICE_FOUND_DETAILS_PROP_UUID],
                 &deviceIdTest,
                 NULL);

    g_assert_cmpstr(deviceIdTest, ==, deviceId);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/barton-core-device-discovery-failed-event/event-creation",
               BCoreDeviceDiscoveryFailedEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/barton-core-device-discovery-failed-event/property-access",
               BCoreDeviceDiscoveryFailedEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}
