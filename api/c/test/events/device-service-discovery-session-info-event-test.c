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

#include "device-service-discovery-type.h"
#include <events/device-service-discovery-session-info-event.h>
#include <glib-object.h>

#define B_DEVICE_SERVICE_TEST_EVENT_TYPE (b_device_service_test_event_get_type())
G_DECLARE_FINAL_TYPE(BDeviceServiceTestEvent,
                     b_device_service_test_event,
                     B_DEVICE_SERVICE,
                     TEST_EVENT,
                     BDeviceServiceDiscoverySessionInfoEvent);

struct _BDeviceServiceTestEvent
{
    BDeviceServiceDiscoverySessionInfoEvent parent_instance;
};

G_DEFINE_TYPE(BDeviceServiceTestEvent, b_device_service_test_event, B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_TYPE);

static void b_device_service_test_event_class_init(BDeviceServiceTestEventClass *klass) {}

static void b_device_service_test_event_init(BDeviceServiceTestEvent *self) {}

BDeviceServiceTestEvent *b_device_service_test_event_new(void)
{
    return g_object_new(B_DEVICE_SERVICE_TEST_EVENT_TYPE, NULL);
}

typedef struct
{
    BDeviceServiceTestEvent *testEvent;
} BDeviceServiceEventTest;

static void setup(BDeviceServiceEventTest *test, gconstpointer user_data)
{
    test->testEvent = b_device_service_test_event_new();
}

// Teardown function called after each test
static void teardown(BDeviceServiceEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->testEvent);
}

static void test_property_access(BDeviceServiceEventTest *test, gconstpointer user_data)
{
    BDeviceServiceDiscoveryType type = 0;

    g_object_get(test->testEvent,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 &type,
                 NULL);

    g_assert_cmpint(type, ==, B_DEVICE_SERVICE_DISCOVERY_TYPE_NONE);

    g_object_set(test->testEvent,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY,
                 NULL);

    g_object_get(test->testEvent,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 &type,
                 NULL);

    g_assert_cmpint(type, ==, B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY);

    g_object_set(test->testEvent,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY,
                 NULL);

    g_object_get(test->testEvent,
                 B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                 &type,
                 NULL);

    g_assert_cmpint(type, ==, B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    g_test_add("/device-service-discovery-session-info-event/property-access",
               BDeviceServiceEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}