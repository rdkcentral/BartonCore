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
 * Created by ss411 on 08/08/2024.
 */

#include "events/device-service-zigbee-pan-id-attack-changed-event.h"
#include "glibconfig.h"
#include <glib-object.h>

typedef struct
{
    BDeviceServiceZigbeePanIdAttackChangedEvent *event;
} BDeviceServiceZigbeePanIdAttackChangedEventTest;

static void setup(BDeviceServiceZigbeePanIdAttackChangedEventTest *test, gconstpointer user_data)
{
    test->event = b_device_service_zigbee_pan_id_attack_changed_event_new();
}

// Teardown function called after each test
static void teardown(BDeviceServiceZigbeePanIdAttackChangedEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BDeviceServiceZigbeePanIdAttackChangedEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BDeviceServiceZigbeePanIdAttackChangedEventTest *test, gconstpointer user_data)
{
    gboolean attack_detected = true;
    g_object_set(test->event,
                 B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROP_ATTACK_DETECTED],
                 attack_detected,
                 NULL);

    gboolean attack_detected_test = false;
    g_object_get(test->event,
                 B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROP_ATTACK_DETECTED],
                 &attack_detected_test,
                 NULL);

    g_assert_cmpint(attack_detected, ==, attack_detected_test);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/device-service-zigbee-pan-id-attack-changed-event/event-creation",
               BDeviceServiceZigbeePanIdAttackChangedEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/device-service-zigbee-pan-id-attack-changed-event/property-access",
               BDeviceServiceZigbeePanIdAttackChangedEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run all tests in this suite
    return g_test_run();
}