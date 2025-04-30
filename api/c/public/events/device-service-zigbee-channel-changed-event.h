//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2024 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
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
