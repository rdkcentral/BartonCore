//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2024 Comcast Corporation
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
// Comcast Corporation retains all ownership rights.
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