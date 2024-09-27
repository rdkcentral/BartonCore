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
 * Created by Christian Leithner on 8/19/2024.
 */

#include "events/device-service-device-database-failure-event.h"

typedef struct
{
    BDeviceServiceDeviceDatabaseFailureEvent *event;
} BDeviceServiceDeviceDatabaseFailureEventTest;

static void setup(BDeviceServiceDeviceDatabaseFailureEventTest *test, gconstpointer user_data)
{
    test->event = b_device_service_device_database_failure_event_new();
}

// Teardown function called after each test
static void teardown(BDeviceServiceDeviceDatabaseFailureEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BDeviceServiceDeviceDatabaseFailureEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BDeviceServiceDeviceDatabaseFailureEventTest *test, gconstpointer user_data)
{
    BDeviceServiceDeviceDatabaseFailureType failure_type =
        B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_TYPE_DEVICE_LOAD_FAILURE;
    g_object_set(test->event,
                 B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE],
                 failure_type,
                 NULL);

    BDeviceServiceDeviceDatabaseFailureType failure_type_test = -1;
    g_object_get(test->event,
                 B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE],
                 &failure_type_test,
                 NULL);

    g_assert_cmpint(failure_type, ==, failure_type_test);

    const gchar *device_id = "device-id";
    g_object_set(test->event,
                 B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_DEVICE_ID],
                 device_id,
                 NULL);

    g_autofree gchar *device_id_test = NULL;
    g_object_get(test->event,
                 B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_DEVICE_ID],
                 &device_id_test,
                 NULL);

    g_assert_cmpstr(device_id, ==, device_id_test);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/device-service-device-database-failure-event/event-creation",
               BDeviceServiceDeviceDatabaseFailureEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/device-service-device-database-failure-event/property-access",
               BDeviceServiceDeviceDatabaseFailureEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}