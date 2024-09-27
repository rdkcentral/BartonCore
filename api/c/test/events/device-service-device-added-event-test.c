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
 * Created by Christian Leithner on 7/3/2024.
 */

#include "events/device-service-device-added-event.h"
#include "glibconfig.h"
#include <glib-object.h>

typedef struct
{
    BDeviceServiceDeviceAddedEvent *event;
} BDeviceServiceDeviceAddedEventTest;

static void setup(BDeviceServiceDeviceAddedEventTest *test, gconstpointer user_data)
{
    test->event = b_device_service_device_added_event_new();
}

// Teardown function called after each test
static void teardown(BDeviceServiceDeviceAddedEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BDeviceServiceDeviceAddedEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BDeviceServiceDeviceAddedEventTest *test, gconstpointer user_data)
{
    const gchar *uuid = "myUuid";
    g_object_set(test->event,
                 B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_UUID],
                 uuid,
                 NULL);

    g_autofree gchar *uuid_test = NULL;
    g_object_get(test->event,
                 B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_UUID],
                 &uuid_test,
                 NULL);

    g_assert_cmpstr(uuid_test, ==, uuid);

    const gchar *uri = "myUri";
    g_object_set(test->event,
                 B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_URI],
                 uri,
                 NULL);

    g_autofree gchar *uri_test = NULL;
    g_object_get(test->event,
                 B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_URI],
                 &uri_test,
                 NULL);

    g_assert_cmpstr(uri_test, ==, uri);

    const gchar *device_class = "myDeviceClass";
    g_object_set(
        test->event,
        B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS],
        device_class,
        NULL);

    g_autofree gchar *device_class_test = NULL;
    g_object_get(
        test->event,
        B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS],
        &device_class_test,
        NULL);

    g_assert_cmpstr(device_class_test, ==, device_class);

    const guint8 device_class_version = 1;
    g_object_set(test->event,
                 B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS_VERSION],
                 device_class_version,
                 NULL);

    guint8 device_class_version_test = 0;
    g_object_get(test->event,
                 B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS_VERSION],
                 &device_class_version_test,
                 NULL);

    g_assert_cmpuint(device_class_version_test, ==, device_class_version);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/device-service-device-added-event/event-creation",
               BDeviceServiceDeviceAddedEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/device-service-device-added-event/property-access",
               BDeviceServiceDeviceAddedEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}