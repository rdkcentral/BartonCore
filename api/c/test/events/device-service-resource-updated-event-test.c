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
 * Created by Christian Leithner on 7/15/2024.
 */

#include "device-service-resource.h"
#include "events/device-service-resource-updated-event.h"
#include "glibconfig.h"
#include <glib-object.h>

typedef struct
{
    BDeviceServiceResourceUpdatedEvent *event;
} BDeviceServiceResourceUpdatedEventTest;

static void setup(BDeviceServiceResourceUpdatedEventTest *test, gconstpointer user_data)
{
    test->event = b_device_service_resource_updated_event_new();
}

// Teardown function called after each test
static void teardown(BDeviceServiceResourceUpdatedEventTest *test, gconstpointer user_data)
{
    g_clear_object(&test->event);
}

static void test_event_creation(BDeviceServiceResourceUpdatedEventTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->event);
}

static void test_property_access(BDeviceServiceResourceUpdatedEventTest *test, gconstpointer user_data)
{
    const gchar *metadata = "myMetadata";
    g_object_set(
        test->event,
        B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_METADATA],
        metadata,
        NULL);

    g_autofree gchar *metadata_test = NULL;
    g_object_get(
        test->event,
        B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_METADATA],
        &metadata_test,
        NULL);

    g_assert_cmpstr(metadata_test, ==, metadata);

    BDeviceServiceResource *resource = b_device_service_resource_new();
    gchar *id = "myId";
    g_object_set(resource, B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_ID], id, NULL);
    g_object_set(
        test->event,
        B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_RESOURCE],
        resource,
        NULL);

    BDeviceServiceResource *resource_test = NULL;
    g_object_get(
        test->event,
        B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_RESOURCE],
        &resource_test,
        NULL);
    g_autofree gchar *id_test = NULL;
    g_object_get(
        resource_test, B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_PROP_ID], &id_test, NULL);

    g_assert_cmpstr(id_test, ==, id);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/device-service-resource-updated-event/event-creation",
               BDeviceServiceResourceUpdatedEventTest,
               NULL,
               setup,
               test_event_creation,
               teardown);
    g_test_add("/device-service-resource-updated-event/property-access",
               BDeviceServiceResourceUpdatedEventTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}