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

#pragma once

#include "device-service-event.h"
#include "events/device-service-device-discovered-event.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_TYPE (b_device_service_device_configuration_event_get_type())
G_DECLARE_DERIVABLE_TYPE(BDeviceServiceDeviceConfigurationEvent,
                         b_device_service_device_configuration_event,
                         B_DEVICE_SERVICE,
                         DEVICE_CONFIGURATION_EVENT,
                         BDeviceServiceDiscoverySessionInfoEvent);

typedef enum
{
    B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID = 1,     // The UUID of the device
    B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS, // The device class of the device
    N_B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTIES
} BDeviceServiceDeviceConfigurationEventProperty;

static const char *B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES[] = {NULL, "uuid", "device-class"};

struct _BDeviceServiceDeviceConfigurationEventClass
{
    BDeviceServiceEventClass parent_class;

    gpointer padding[12];
};

G_END_DECLS