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
 * Created by Christian Leithner on 6/6/2024.
 */

#include <events/device-service-recovery-started-event.h>
#include <glib-object.h>

typedef struct
{
    BDeviceServiceRecoveryStartedEvent *event;
} BDeviceServiceRecoveryStartedEventTest;

static void setup(BDeviceServiceRecoveryStartedEventTest *test, gconstpointer user_data)
{
    test->event = b_device_service_recovery_started_event_new();
}

// Teardown function called after each test
static void teardown(BDeviceServiceRecoveryStartedEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BDeviceServiceRecoveryStartedEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BDeviceServiceRecoveryStartedEventTest *test, gconstpointer user_data)
{
    g_autoptr(GList) deviceClasses = NULL;
    const gchar *class1 = "myClass";
    deviceClasses = g_list_append(deviceClasses, (gchar *) class1);

    g_object_set(test->event,
                 B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
                 deviceClasses,
                 NULL);

    g_autoptr(GList) classes_test = NULL;
    g_object_get(test->event,
                 B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
                 &classes_test,
                 NULL);

    g_assert_nonnull(classes_test);
    g_assert_cmpuint(g_list_length(classes_test), ==, 1);
    g_autofree gchar *class1_test_id = g_list_first(classes_test)->data;
    g_assert_cmpstr(class1_test_id, ==, class1);

    g_object_set(
        test->event,
        B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROP_TIMEOUT],
        10,
        NULL);
    guint timeout = 0;
    g_object_get(
        test->event,
        B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RECOVERY_STARTED_EVENT_PROP_TIMEOUT],
        &timeout,
        NULL);
    g_assert_cmpuint(timeout, ==, 10);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/device-service-recovery-started-event/event-creation",
               BDeviceServiceRecoveryStartedEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/device-service-recovery-started-event/property-access",
               BDeviceServiceRecoveryStartedEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}