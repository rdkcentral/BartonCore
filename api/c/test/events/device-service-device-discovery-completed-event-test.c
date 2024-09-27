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
 * Created by Christian Leithner on 7/8/2024.
 */

#include "device-service-device.h"
#include <device-service-device-found-details.h>
#include <events/device-service-device-discovery-completed-event.h>
#include <glib-object.h>

typedef struct
{
    BDeviceServiceDeviceDiscoveryCompletedEvent *event;
} BDeviceServiceDeviceDiscoveryCompletedEventTest;

static void setup(BDeviceServiceDeviceDiscoveryCompletedEventTest *test, gconstpointer user_data)
{
    test->event = b_device_service_device_discovery_completed_event_new();
}

// Teardown function called after each test
static void teardown(BDeviceServiceDeviceDiscoveryCompletedEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BDeviceServiceDeviceDiscoveryCompletedEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BDeviceServiceDeviceDiscoveryCompletedEventTest *test, gconstpointer user_data)
{
    g_autoptr(BDeviceServiceDevice) device = b_device_service_device_new();
    const gchar *deviceId = "myDeviceId";
    g_object_set(device, B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_UUID], deviceId, NULL);

    g_object_set(test->event,
                 B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROP_DEVICE],
                 device,
                 NULL);

    g_autoptr(BDeviceServiceDevice) deviceTest = NULL;
    g_object_get(test->event,
                 B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROP_DEVICE],
                 &deviceTest,
                 NULL);

    g_assert_nonnull(deviceTest);

    g_autofree gchar *deviceIdTest = NULL;
    g_object_get(
        deviceTest, B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_PROP_UUID], &deviceIdTest, NULL);

    g_assert_cmpstr(deviceIdTest, ==, deviceId);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/device-service-device-discovery-completed-event/event-creation",
               BDeviceServiceDeviceDiscoveryCompletedEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/device-service-device-discovery-completed-event/property-access",
               BDeviceServiceDeviceDiscoveryCompletedEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}