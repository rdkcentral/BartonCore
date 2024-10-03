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
 * Created by Ganesh Induri on 07/26/2024.
 */

#pragma once

#include "events/device-service-event.h"
#include <glib-object.h>
#include <stdbool.h>

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_TYPE (b_device_service_zigbee_channel_changed_event_get_type())

G_DECLARE_FINAL_TYPE(BDeviceServiceZigbeeChannelChangedEvent,
                     b_device_service_zigbee_channel_changed_event,
                     B_DEVICE_SERVICE,
                     ZIGBEE_CHANNEL_CHANGED_EVENT,
                     BDeviceServiceEvent)

typedef enum
{
    B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CHANNEL_CHANGED = 1,
    B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CURRENT_CHANNEL,
    B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_TARGETED_CHANNEL,

    N_B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTIES,
} BDeviceServiceZigbeeChannelChangedEventProperty;

static const char *B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES[] = {NULL,
                                                                                     "channel-changed",
                                                                                     "current-channel",
                                                                                     "targeted-channel"};

/**
 * b_device_service_zigbee_channel_changed_event_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceZigbeeChannelChangedEvent*
 */
BDeviceServiceZigbeeChannelChangedEvent *b_device_service_zigbee_channel_changed_event_new(void);

G_END_DECLS