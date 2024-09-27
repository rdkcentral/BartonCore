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
 * Created by Christian Leithner on 7/16/2024.
 */

#include "device-service-resource.h"
#include "events/device-service-device-removed-event.h"
#include "glibconfig.h"
#include <glib-object.h>

typedef struct
{
    BDeviceServiceDeviceRemovedEvent *event;
} BDeviceServiceDeviceRemovedEventTest;

static void setup(BDeviceServiceDeviceRemovedEventTest *test, gconstpointer user_data)
{
    test->event = b_device_service_device_removed_event_new();
}

// Teardown function called after each test
static void teardown(BDeviceServiceDeviceRemovedEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BDeviceServiceDeviceRemovedEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BDeviceServiceDeviceRemovedEventTest *test, gconstpointer user_data)
{
    const gchar *device_uuid = "myDeviceUuid";
    g_object_set(
        test->event,
        B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_UUID],
        device_uuid,
        NULL);

    g_autofree gchar *device_uuid_test = NULL;
    g_object_get(
        test->event,
        B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_UUID],
        &device_uuid_test,
        NULL);

    g_assert_cmpstr(device_uuid_test, ==, device_uuid);

    const gchar *device_class = "myDeviceClass";
    g_object_set(
        test->event,
        B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_CLASS],
        device_class,
        NULL);

    g_autofree gchar *device_class_test = NULL;
    g_object_get(
        test->event,
        B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_CLASS],
        &device_class_test,
        NULL);

    g_assert_cmpstr(device_class_test, ==, device_class);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/device-service-device-removed-event/event-creation",
               BDeviceServiceDeviceRemovedEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/device-service-device-removed-event/property-access",
               BDeviceServiceDeviceRemovedEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}