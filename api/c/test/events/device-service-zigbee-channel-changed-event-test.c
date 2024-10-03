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
 * Created by Ganesh Induri on 07/26/2024.
 */

#include "events/device-service-zigbee-channel-changed-event.h"
#include "glibconfig.h"
#include <glib-object.h>

typedef struct
{
    BDeviceServiceZigbeeChannelChangedEvent *event;
} BDeviceServiceZigbeeChannelChangedEventTest;

static void setup(BDeviceServiceZigbeeChannelChangedEventTest *test, gconstpointer user_data)
{
    test->event = b_device_service_zigbee_channel_changed_event_new();
}

// Teardown function called after each test
static void teardown(BDeviceServiceZigbeeChannelChangedEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BDeviceServiceZigbeeChannelChangedEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BDeviceServiceZigbeeChannelChangedEventTest *test, gconstpointer user_data)
{
    gboolean channel_changed = true;
    g_object_set(test->event,
                 B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CHANNEL_CHANGED],
                 channel_changed,
                 NULL);

    gboolean channel_changed_test = false;
    g_object_get(test->event,
                 B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CHANNEL_CHANGED],
                 &channel_changed_test,
                 NULL);

    g_assert_cmpint(channel_changed_test, ==, channel_changed);

    const guint8 current_channel = 25;
    g_object_set(test->event,
                 B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CURRENT_CHANNEL],
                 current_channel,
                 NULL);

    guint8 current_channel_test = 0;
    g_object_get(test->event,
                 B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CURRENT_CHANNEL],
                 &current_channel_test,
                 NULL);

    g_assert_cmpint(current_channel, ==, current_channel_test);

    const guint8 targeted_channel = 16;
    g_object_set(test->event,
                 B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_TARGETED_CHANNEL],
                 targeted_channel,
                 NULL);

    guint8 targeted_channel_test = 0;
    g_object_get(test->event,
                 B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                     [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_TARGETED_CHANNEL],
                 &targeted_channel_test,
                 NULL);

    g_assert_cmpuint(targeted_channel, ==, targeted_channel_test);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/device-service-zigbee-channel-changed-event/event-creation",
               BDeviceServiceZigbeeChannelChangedEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/device-service-zigbee-channel-changed-event/property-access",
               BDeviceServiceZigbeeChannelChangedEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}