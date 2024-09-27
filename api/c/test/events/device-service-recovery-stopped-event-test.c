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
 * Created by Christian Leithner on 7/11/2024.
 */

#include <events/device-service-recovery-stopped-event.h>
#include <glib-object.h>

typedef struct
{
    BDeviceServiceRecoveryStoppedEvent *event;
} BDeviceServiceRecoveryStoppedEventTest;

static void setup(BDeviceServiceRecoveryStoppedEventTest *test, gconstpointer user_data)
{
    test->event = b_device_service_recovery_stopped_event_new();
}

// Teardown function called after each test
static void teardown(BDeviceServiceRecoveryStoppedEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BDeviceServiceRecoveryStoppedEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BDeviceServiceRecoveryStoppedEventTest *test, gconstpointer user_data)
{
    const gchar *deviceClass = "myClass";

    g_object_set(test->event,
                 B_DEVICE_SERVICE_RECOVERY_STOPPED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_RECOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS],
                 deviceClass,
                 NULL);

    g_autofree gchar *deviceClassTest = NULL;
    g_object_get(test->event,
                 B_DEVICE_SERVICE_RECOVERY_STOPPED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_RECOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS],
                 &deviceClassTest,
                 NULL);

    g_assert_cmpstr(deviceClassTest, ==, deviceClass);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/device-service-recovery-stopped-event/event-creation",
               BDeviceServiceRecoveryStoppedEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/device-service-recovery-stopped-event/property-access",
               BDeviceServiceRecoveryStoppedEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}