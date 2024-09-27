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
 * Created by Christian Leithner on 8/5/2024.
 */

#include "events/device-service-storage-changed-event.h"
#include "gio/gio.h"

typedef struct
{
    BDeviceServiceStorageChangedEvent *event;
} BDeviceServiceStorageChangedEventTest;

static void setup(BDeviceServiceStorageChangedEventTest *test, gconstpointer user_data)
{
    test->event = b_device_service_storage_changed_event_new();
}

// Teardown function called after each test
static void teardown(BDeviceServiceStorageChangedEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BDeviceServiceStorageChangedEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BDeviceServiceStorageChangedEventTest *test, gconstpointer user_data)
{
    GFileMonitorEvent what_changed = G_FILE_MONITOR_EVENT_CREATED;
    g_object_set(
        test->event,
        B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROP_WHAT_CHANGED],
        what_changed,
        NULL);

    GFileMonitorEvent what_changed_test = G_FILE_MONITOR_EVENT_DELETED;

    g_object_get(
        test->event,
        B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROP_WHAT_CHANGED],
        &what_changed_test,
        NULL);

    g_assert_cmpint(what_changed_test, ==, what_changed);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/device-service-storage-changed-event/event-creation",
               BDeviceServiceStorageChangedEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/device-service-storage-changed-event/property-access",
               BDeviceServiceStorageChangedEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}