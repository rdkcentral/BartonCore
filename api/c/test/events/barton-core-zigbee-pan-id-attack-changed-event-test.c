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
 * Created by ss411 on 08/08/2024.
 */

#include "events/barton-core-zigbee-pan-id-attack-changed-event.h"
#include "glibconfig.h"
#include <glib-object.h>

typedef struct
{
    BCoreZigbeePanIdAttackChangedEvent *event;
} BCoreZigbeePanIdAttackChangedEventTest;

static void setup(BCoreZigbeePanIdAttackChangedEventTest *test, gconstpointer user_data)
{
    test->event = b_core_zigbee_pan_id_attack_changed_event_new();
}

// Teardown function called after each test
static void teardown(BCoreZigbeePanIdAttackChangedEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BCoreZigbeePanIdAttackChangedEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BCoreZigbeePanIdAttackChangedEventTest *test, gconstpointer user_data)
{
    gboolean attack_detected = true;
    g_object_set(test->event,
                 B_CORE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROP_ATTACK_DETECTED],
                 attack_detected,
                 NULL);

    gboolean attack_detected_test = false;
    g_object_get(test->event,
                 B_CORE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROPERTY_NAMES
                     [B_CORE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROP_ATTACK_DETECTED],
                 &attack_detected_test,
                 NULL);

    g_assert_cmpint(attack_detected, ==, attack_detected_test);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/barton-core-zigbee-pan-id-attack-changed-event/event-creation",
               BCoreZigbeePanIdAttackChangedEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/barton-core-zigbee-pan-id-attack-changed-event/property-access",
               BCoreZigbeePanIdAttackChangedEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run all tests in this suite
    return g_test_run();
}
