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
 * Created by Christian Leithner on 6/13/2024.
 */

#include <device-service-device-found-details.h>
#include <events/device-service-device-rejected-event.h>
#include <glib-object.h>

typedef struct
{
    BDeviceServiceDeviceRejectedEvent *event;
} BDeviceServiceDeviceRejectedEventTest;

static void setup(BDeviceServiceDeviceRejectedEventTest *test, gconstpointer user_data)
{
    test->event = b_device_service_device_rejected_event_new();
}

// Teardown function called after each test
static void teardown(BDeviceServiceDeviceRejectedEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BDeviceServiceDeviceRejectedEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BDeviceServiceDeviceRejectedEventTest *test, gconstpointer user_data)
{
    g_autoptr(BDeviceServiceDeviceFoundDetails) deviceFoundDetails = b_device_service_device_found_details_new();
    const gchar *deviceId = "myDeviceId";
    g_object_set(deviceFoundDetails,
                 B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_UUID],
                 deviceId,
                 NULL);

    g_object_set(test->event,
                 B_DEVICE_SERVICE_DEVICE_REJECTED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_REJECTED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 deviceFoundDetails,
                 NULL);

    g_autoptr(BDeviceServiceDeviceFoundDetails) deviceFoundDetailsTest = NULL;
    g_object_get(test->event,
                 B_DEVICE_SERVICE_DEVICE_REJECTED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_REJECTED_EVENT_PROP_DEVICE_FOUND_DETAILS],
                 &deviceFoundDetailsTest,
                 NULL);

    g_assert_nonnull(deviceFoundDetailsTest);

    g_autofree gchar *deviceIdTest = NULL;
    g_object_get(deviceFoundDetailsTest,
                 B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_UUID],
                 &deviceIdTest,
                 NULL);

    g_assert_cmpstr(deviceIdTest, ==, deviceId);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/device-service-device-rejected-event/event-creation",
               BDeviceServiceDeviceRejectedEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/device-service-device-rejected-event/property-access",
               BDeviceServiceDeviceRejectedEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}