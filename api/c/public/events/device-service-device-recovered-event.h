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

#pragma once

#include "events/device-service-event.h"
#include "glib-object.h"

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_TYPE (b_device_service_device_recovered_event_get_type())
G_DECLARE_FINAL_TYPE(BDeviceServiceDeviceRecoveredEvent,
                     b_device_service_device_recovered_event,
                     B_DEVICE_SERVICE,
                     DEVICE_RECOVERED_EVENT,
                     BDeviceServiceEvent)

typedef enum
{
    B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROP_UUID = 1,             // Device UUID
    B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROP_URI,                  // Device URI
    B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROP_DEVICE_CLASS,         // Device class
    B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROP_DEVICE_CLASS_VERSION, // Device class version
    N_B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROPERTIES
} BDeviceServiceDeviceRecoveredEventProperty;

static const char *B_DEVICE_SERVICE_DEVICE_RECOVERED_EVENT_PROPERTY_NAMES[] = {NULL,
                                                                               "uuid",
                                                                               "uri",
                                                                               "device-class",
                                                                               "device-class-version"};

/**
 * b_device_service_device_recovered_event_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceDeviceRecoveredEvent*
 */
BDeviceServiceDeviceRecoveredEvent *b_device_service_device_recovered_event_new(void);

G_END_DECLS