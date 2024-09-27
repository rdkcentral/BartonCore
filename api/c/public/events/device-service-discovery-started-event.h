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
 * Created by Christian Leithner on 6/6/2024.
 */

#pragma once

#include "events/device-service-event.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_TYPE (b_device_service_discovery_started_event_get_type())
G_DECLARE_FINAL_TYPE(BDeviceServiceDiscoveryStartedEvent,
                     b_device_service_discovery_started_event,
                     B_DEVICE_SERVICE,
                     DISCOVERY_STARTED_EVENT,
                     BDeviceServiceEvent)

typedef enum
{
    B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES =
        1, // GList of strings describing which device classes discovery was started for
    B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROP_TIMEOUT, // Timeout for discovery in seconds
    N_B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROPERTIES
} BDeviceServiceDiscoveryStartedEventProperty;

static const char *B_DEVICE_SERVICE_DISCOVERY_STARTED_EVENT_PROPERTY_NAMES[] = {NULL, "device-classes", "timeout"};

/**
 * b_device_service_discovery_started_event_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceDiscoveryStartedEvent*
 */
BDeviceServiceDiscoveryStartedEvent *b_device_service_discovery_started_event_new(void);

G_END_DECLS