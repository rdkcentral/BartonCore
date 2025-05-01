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
 * Created by Christian Leithner on 7/16/2024.
 */

#include "barton-core-endpoint.h"
#include "barton-core-resource.h"
#include "events/barton-core-endpoint-added-event.h"
#include "glibconfig.h"
#include <glib-object.h>

typedef struct
{
    BCoreEndpointAddedEvent *event;
} BCoreEndpointAddedEventTest;

static void setup(BCoreEndpointAddedEventTest *test, gconstpointer user_data)
{
    test->event = b_core_endpoint_added_event_new();
}

// Teardown function called after each test
static void teardown(BCoreEndpointAddedEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BCoreEndpointAddedEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BCoreEndpointAddedEventTest *test, gconstpointer user_data)
{
    const gchar *id = "myEndpointId";
    g_autoptr(BCoreEndpoint) endpoint = b_core_endpoint_new();
    g_object_set(endpoint, B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_ID], id, NULL);

    g_object_set(
        test->event,
        B_CORE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_CORE_ENDPOINT_ADDED_EVENT_PROP_ENDPOINT],
        endpoint,
        NULL);

    g_autoptr(BCoreEndpoint) endpoint_test = NULL;
    g_object_get(
        test->event,
        B_CORE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_CORE_ENDPOINT_ADDED_EVENT_PROP_ENDPOINT],
        &endpoint_test,
        NULL);

    g_autofree gchar *id_test = NULL;
    g_object_get(
        endpoint_test, B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_ID], &id_test, NULL);

    g_assert_cmpstr(id_test, ==, id);

    const gchar *device_class = "myDeviceClass";
    g_object_set(
        test->event,
        B_CORE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_CORE_ENDPOINT_ADDED_EVENT_PROP_DEVICE_CLASS],
        device_class,
        NULL);

    g_autofree gchar *device_class_test = NULL;
    g_object_get(
        test->event,
        B_CORE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_CORE_ENDPOINT_ADDED_EVENT_PROP_DEVICE_CLASS],
        &device_class_test,
        NULL);

    g_assert_cmpstr(device_class_test, ==, device_class);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/barton-core-endpoint-added-event/event-creation",
               BCoreEndpointAddedEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/barton-core-endpoint-added-event/property-access",
               BCoreEndpointAddedEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}
