//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2024 Comcast Corporation
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
// Comcast Corporation retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*
 * Created by Ganesh Induri on 08/01/2024.
 */

#pragma once

#include "events/device-service-event.h"
#include <glib-object.h>
#include <stdbool.h>

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_ZIGBEE_INTERFERENCE_EVENT_TYPE (b_device_service_zigbee_interference_event_get_type())

G_DECLARE_FINAL_TYPE(BDeviceServiceZigbeeInterferenceEvent,
                     b_device_service_zigbee_interference_event,
                     B_DEVICE_SERVICE,
                     ZIGBEE_INTERFERENCE_EVENT,
                     BDeviceServiceEvent)

typedef enum
{
    B_DEVICE_SERVICE_ZIGBEE_INTERFERENCE_EVENT_PROP_INTERFERENCE_DETECTED = 1,

    N_B_DEVICE_SERVICE_ZIGBEE_INTERFERENCE_EVENT_PROPERTIES,
} BDeviceServiceZigbeeInterferenceEventProperty;

static const char *B_DEVICE_SERVICE_ZIGBEE_INTERFERENCE_EVENT_PROPERTY_NAMES[] = {NULL, "interference-detected"};

/**
 * b_device_service_zigbee_interference_event_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceZigbeeInterferenceEvent*
 */
BDeviceServiceZigbeeInterferenceEvent *b_device_service_zigbee_interference_event_new(void);

G_END_DECLS