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
 * Created by Christian Leithner on 7/15/2024.
 */

#include "barton-core-resource.h"
#include "events/barton-core-resource-updated-event.h"
#include "glibconfig.h"
#include <glib-object.h>

typedef struct
{
    BCoreResourceUpdatedEvent *event;
} BCoreResourceUpdatedEventTest;

static void setup(BCoreResourceUpdatedEventTest *test, gconstpointer user_data)
{
    test->event = b_core_resource_updated_event_new();
}

// Teardown function called after each test
static void teardown(BCoreResourceUpdatedEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BCoreResourceUpdatedEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BCoreResourceUpdatedEventTest *test, gconstpointer user_data)
{
    const gchar *metadata = "myMetadata";
    g_object_set(
        test->event,
        B_CORE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_CORE_RESOURCE_UPDATED_EVENT_PROP_METADATA],
        metadata,
        NULL);

    g_autofree gchar *metadata_test = NULL;
    g_object_get(
        test->event,
        B_CORE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_CORE_RESOURCE_UPDATED_EVENT_PROP_METADATA],
        &metadata_test,
        NULL);

    g_assert_cmpstr(metadata_test, ==, metadata);

    g_autoptr(BCoreResource) resource = b_core_resource_new();
    gchar *id = "myId";
    g_object_set(resource, B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_ID], id, NULL);
    g_object_set(
        test->event,
        B_CORE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_CORE_RESOURCE_UPDATED_EVENT_PROP_RESOURCE],
        resource,
        NULL);

    g_autoptr(BCoreResource) resource_test = NULL;
    g_object_get(
        test->event,
        B_CORE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_CORE_RESOURCE_UPDATED_EVENT_PROP_RESOURCE],
        &resource_test,
        NULL);
    g_autofree gchar *id_test = NULL;
    g_object_get(
        resource_test, B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_ID], &id_test, NULL);

    g_assert_cmpstr(id_test, ==, id);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/barton-core-resource-updated-event/event-creation",
               BCoreResourceUpdatedEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/barton-core-resource-updated-event/property-access",
               BCoreResourceUpdatedEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}
