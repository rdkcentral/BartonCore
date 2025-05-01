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
 * Created by Christian Leithner on 6/6/2024.
 */

#include <events/barton-core-recovery-started-event.h>
#include <glib-object.h>

typedef struct
{
    BCoreRecoveryStartedEvent *event;
} BCoreRecoveryStartedEventTest;

static void setup(BCoreRecoveryStartedEventTest *test, gconstpointer user_data)
{
    test->event = b_core_recovery_started_event_new();
}

// Teardown function called after each test
static void teardown(BCoreRecoveryStartedEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BCoreRecoveryStartedEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BCoreRecoveryStartedEventTest *test, gconstpointer user_data)
{
    g_autoptr(GList) deviceClasses = NULL;
    const gchar *class1 = "myClass";
    deviceClasses = g_list_append(deviceClasses, (gchar *) class1);

    g_object_set(test->event,
                 B_CORE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES
                     [B_CORE_RECOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
                 deviceClasses,
                 NULL);

    g_autoptr(GList) classes_test = NULL;
    g_object_get(test->event,
                 B_CORE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES
                     [B_CORE_RECOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
                 &classes_test,
                 NULL);

    g_assert_nonnull(classes_test);
    g_assert_cmpuint(g_list_length(classes_test), ==, 1);
    g_autofree gchar *class1_test_id = g_list_first(classes_test)->data;
    g_assert_cmpstr(class1_test_id, ==, class1);

    g_object_set(
        test->event,
        B_CORE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES[B_CORE_RECOVERY_STARTED_EVENT_PROP_TIMEOUT],
        10,
        NULL);
    guint timeout = 0;
    g_object_get(
        test->event,
        B_CORE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES[B_CORE_RECOVERY_STARTED_EVENT_PROP_TIMEOUT],
        &timeout,
        NULL);
    g_assert_cmpuint(timeout, ==, 10);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/barton-core-recovery-started-event/event-creation",
               BCoreRecoveryStartedEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/barton-core-recovery-started-event/property-access",
               BCoreRecoveryStartedEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}
