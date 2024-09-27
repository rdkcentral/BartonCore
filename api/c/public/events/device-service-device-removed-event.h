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
 * Created by Christian Leithner on 7/16/2024.
 */

#pragma once

#include "events/device-service-event.h"

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_TYPE (b_device_service_device_removed_event_get_type())
G_DECLARE_FINAL_TYPE(BDeviceServiceDeviceRemovedEvent,
                     b_device_service_device_removed_event,
                     B_DEVICE_SERVICE,
                     DEVICE_REMOVED_EVENT,
                     BDeviceServiceEvent)

typedef enum
{
    B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_UUID = 1, // The device ID of the device that was removed
    B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_CLASS,    // The device class of the device that was removed
    N_B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTIES
} BDeviceServiceDeviceRemovedEventProperty;

static const char *B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[] = {NULL, "device-uuid", "device-class"};

/**
 * b_device_service_device_removed_event_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceDeviceRemovedEvent*
 */
BDeviceServiceDeviceRemovedEvent *b_device_service_device_removed_event_new(void);

G_END_DECLS
