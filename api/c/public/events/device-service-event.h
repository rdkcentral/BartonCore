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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_EVENT_TYPE (b_device_service_event_get_type())
G_DECLARE_DERIVABLE_TYPE(BDeviceServiceEvent, b_device_service_event, B_DEVICE_SERVICE, EVENT, GObject);

typedef enum
{
    B_DEVICE_SERVICE_EVENT_PROP_ID = 1,    // Unique identifier for the event
    B_DEVICE_SERVICE_EVENT_PROP_TIMESTAMP, // Realtime unix timestamp in microseconds
    N_B_DEVICE_SERVICE_EVENT_PROPERTIES
} BDeviceServiceEventProperty;

static const char *B_DEVICE_SERVICE_EVENT_PROPERTY_NAMES[] = {NULL, "id", "timestamp"};

struct _BDeviceServiceEventClass
{
    GObjectClass parent_class;

    gpointer padding[12];
};

G_END_DECLS