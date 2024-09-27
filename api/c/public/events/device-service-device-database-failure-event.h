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

#pragma once

#include "events/device-service-event.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_TYPE (b_device_service_device_database_failure_event_get_type())

G_DECLARE_FINAL_TYPE(BDeviceServiceDeviceDatabaseFailureEvent,
                     b_device_service_device_database_failure_event,
                     B_DEVICE_SERVICE,
                     DEVICE_DATABASE_FAILURE_EVENT,
                     BDeviceServiceEvent)

typedef enum
{
    B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_TYPE_DEVICE_LOAD_FAILURE =
        1, // Failed to load a device from the device database
} BDeviceServiceDeviceDatabaseFailureType;

// This the old school way of creating GEnum types. We should be using G_DEFINE_ENUM_TYPE but that requires glib >=2.74
GType b_device_service_device_database_failure_type_type(void);
#define B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_TYPE_TYPE (b_device_service_device_database_failure_type_type())

typedef enum
{
    B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE = 1,

    B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_DEVICE_ID, // Applicable if
                                                                   // B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE
                                                                   // ==
                                                                   // B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_TYPE_DEVICE_LOAD_FAILURE

    N_B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTIES,
} BDeviceServiceDeviceDatabaseFailureEventProperty;

static const char *B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES[] = {
    NULL,
    "failure-type",
    "device-id",
};

/**
 * b_device_service_device_database_failure_event_new
 *
 * Creates a new device database failure event.
 *
 * Returns: (transfer full): A new device database failure event.
 */
BDeviceServiceDeviceDatabaseFailureEvent *b_device_service_device_database_failure_event_new(void);

G_END_DECLS