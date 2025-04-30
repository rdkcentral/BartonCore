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
 * Created by Ganesh Induri on 08/06/2024.
 */

#include "events/device-service-zigbee-interference-event.h"
#include "glibconfig.h"
#include <glib-object.h>

typedef struct
{
    BDeviceServiceZigbeeInterferenceEvent *event;
} BDeviceServiceZigbeeInterferenceEventTest;

static void setup(BDeviceServiceZigbeeInterferenceEventTest *test, gconstpointer user_data)
{
    test->event = b_device_service_zigbee_interference_event_new();
}

// Teardown function called after each test
static void teardown(BDeviceServiceZigbeeInterferenceEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BDeviceServiceZigbeeInterferenceEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BDeviceServiceZigbeeInterferenceEventTest *test, gconstpointer user_data)
{
    gboolean interference_detected = true;
    g_object_set(test->event,
                 B_DEVICE_SERVICE_ZIGBEE_INTERFERENCE_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_INTERFERENCE_EVENT_PROP_INTERFERENCE_DETECTED],
                 interference_detected,
                 NULL);

    gboolean interference_detected_test = false;
    g_object_get(test->event,
                 B_DEVICE_SERVICE_ZIGBEE_INTERFERENCE_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_INTERFERENCE_EVENT_PROP_INTERFERENCE_DETECTED],
                 &interference_detected_test,
                 NULL);

    g_assert_cmpint(interference_detected, ==, interference_detected_test);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/device-service-zigbee-interference-event/event-creation",
               BDeviceServiceZigbeeInterferenceEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/device-service-zigbee-interference-event/property-access",
               BDeviceServiceZigbeeInterferenceEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}
