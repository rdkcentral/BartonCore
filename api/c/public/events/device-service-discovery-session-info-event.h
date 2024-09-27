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
 * Created by Christian Leithner on 6/11/2024.
 */

#pragma once

#include "device-service-event.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_TYPE (b_device_service_discovery_session_info_event_get_type())
G_DECLARE_DERIVABLE_TYPE(BDeviceServiceDiscoverySessionInfoEvent,
                         b_device_service_discovery_session_info_event,
                         B_DEVICE_SERVICE,
                         DISCOVERY_SESSION_INFO_EVENT,
                         BDeviceServiceEvent);

typedef enum
{
    B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE =
        1, // The BDeviceServiceDiscoveryType of the session
    N_B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTIES
} BDeviceServiceDiscoverySessionInfoEventProperty;

static const char *B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES[] = {NULL, "session-discovery-type"};

struct _BDeviceServiceDiscoverySessionInfoEventClass
{
    BDeviceServiceEventClass parent_class;

    gpointer padding[12];
};

G_END_DECLS