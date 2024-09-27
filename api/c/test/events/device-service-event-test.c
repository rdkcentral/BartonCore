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
 * Created by Christian Leithner on 6/5/2024.
 */

#include <events/device-service-event.h>
#include <glib-object.h>

#define B_DEVICE_SERVICE_TEST_EVENT_TYPE (b_device_service_test_event_get_type())
G_DECLARE_FINAL_TYPE(BDeviceServiceTestEvent,
                     b_device_service_test_event,
                     B_DEVICE_SERVICE,
                     TEST_EVENT,
                     BDeviceServiceEvent);

struct _BDeviceServiceTestEvent
{
    BDeviceServiceEvent parent_instance;
};

G_DEFINE_TYPE(BDeviceServiceTestEvent, b_device_service_test_event, B_DEVICE_SERVICE_EVENT_TYPE);

static void b_device_service_test_event_class_init(BDeviceServiceTestEventClass *klass) {}

static void b_device_service_test_event_init(BDeviceServiceTestEvent *self) {}

BDeviceServiceTestEvent *b_device_service_test_event_new(void)
{
    return g_object_new(B_DEVICE_SERVICE_TEST_EVENT_TYPE, NULL);
}

typedef struct
{
    BDeviceServiceTestEvent *testEvent;
    guint64 timeBeforeCreation;
} BDeviceServiceEventTest;

static void setup(BDeviceServiceEventTest *test, gconstpointer user_data)
{
    test->timeBeforeCreation = g_get_real_time();
    test->testEvent = b_device_service_test_event_new();
}

// Teardown function called after each test
static void teardown(BDeviceServiceEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->testEvent);
}

// Properties are read only and mocking would pretty much defeat the purpose of
// what's under test. We'll just keep it really simple, we're not trying to put
// glib APIs under test so much as are we calling the right ones.

static void test_property_id_starts_0(BDeviceServiceEventTest *test, gconstpointer user_data)
{
    guint id = 0;
    g_object_get(test->testEvent, B_DEVICE_SERVICE_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_EVENT_PROP_ID], &id, NULL);
    g_assert_cmpuint(id, ==, 0);
}

static void test_property_id_incs_1(BDeviceServiceEventTest *test, gconstpointer user_data)
{
    guint id = 0;
    g_object_get(test->testEvent, B_DEVICE_SERVICE_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_EVENT_PROP_ID], &id, NULL);
    g_assert_cmpuint(id, ==, 1);
}

static void test_property_timestamp(BDeviceServiceEventTest *test, gconstpointer user_data)
{
    guint64 timestamp = 0;
    g_object_get(test->testEvent,
                 B_DEVICE_SERVICE_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_EVENT_PROP_TIMESTAMP],
                 &timestamp,
                 NULL);
    g_assert_cmpuint(timestamp, >=, test->timeBeforeCreation);
    g_assert_cmpuint(timestamp, <=, g_get_real_time());
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/device-service-event/property-id-starts-0",
               BDeviceServiceEventTest,
               NULL,
               setup,
               test_property_id_starts_0,
               teardown);
    g_test_add("/device-service-event/property-id-incs-1",
               BDeviceServiceEventTest,
               NULL,
               setup,
               test_property_id_incs_1,
               teardown);
    g_test_add("/device-service-event/property-timestamp",
               BDeviceServiceEventTest,
               NULL,
               setup,
               test_property_timestamp,
               teardown);

    // Run tests
    return g_test_run();
}