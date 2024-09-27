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
 * Created by Christian Leithner on 8/5/2024.
 */

#pragma once

#include "events/device-service-event.h"
#include "glib-object.h"

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_TYPE (b_device_service_storage_changed_event_get_type())
G_DECLARE_FINAL_TYPE(BDeviceServiceStorageChangedEvent,
                     b_device_service_storage_changed_event,
                     B_DEVICE_SERVICE,
                     STORAGE_CHANGED_EVENT,
                     BDeviceServiceEvent)

typedef enum
{
    B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROP_WHAT_CHANGED = 1, // GFileMonitorEvent enum
    N_B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROPERTIES
} BDeviceServiceStorageChangedEventProperty;

static const char *B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROPERTY_NAMES[] = {NULL, "what-changed"};

/**
 * b_device_service_storage_changed_event_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceStorageChangedEvent*
 */
BDeviceServiceStorageChangedEvent *b_device_service_storage_changed_event_new(void);

G_END_DECLS