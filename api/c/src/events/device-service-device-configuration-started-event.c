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
 * Created by Christian Leithner on 7/1/2024.
 */

#include "events/device-service-device-configuration-started-event.h"
#include "events/device-service-device-configuration-event.h"

struct _BDeviceServiceDeviceConfigurationStartedEvent
{
    BDeviceServiceDeviceConfigurationEvent parent_instance;

    // No properties. Be sure to add unit testing if you add properties!
};

G_DEFINE_TYPE(BDeviceServiceDeviceConfigurationStartedEvent,
              b_device_service_device_configuration_started_event,
              B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_TYPE)

static void
b_device_service_device_configuration_started_event_init(BDeviceServiceDeviceConfigurationStartedEvent *self)
{
    // No properties
}

static void b_device_service_device_configuration_started_event_finalize(GObject *object)
{
    G_OBJECT_CLASS(b_device_service_device_configuration_started_event_parent_class)->finalize(object);
}

static void b_device_service_device_configuration_started_event_class_init(
    BDeviceServiceDeviceConfigurationStartedEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_device_service_device_configuration_started_event_finalize;
}

BDeviceServiceDeviceConfigurationStartedEvent *b_device_service_device_configuration_started_event_new(void)
{
    return g_object_new(B_DEVICE_SERVICE_DEVICE_CONFIGURATION_STARTED_EVENT_TYPE, NULL);
}