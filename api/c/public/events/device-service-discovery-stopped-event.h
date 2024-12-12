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
 * Created by Christian Leithner on 7/10/2024.
 */

#pragma once

#include "events/device-service-event.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_TYPE (b_device_service_discovery_stopped_event_get_type())
G_DECLARE_FINAL_TYPE(BDeviceServiceDiscoveryStoppedEvent,
                     b_device_service_discovery_stopped_event,
                     B_DEVICE_SERVICE,
                     DISCOVERY_STOPPED_EVENT,
                     BDeviceServiceEvent)

typedef enum
{
    B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS =
        1, // String describing which device class discovery was stopped for
    N_B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_PROPERTIES
} BDeviceServiceDiscoveryStoppedEventProperty;

static const char *B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_PROPERTY_NAMES[] = {NULL, "device-class"};

/**
 * b_device_service_discovery_stopped_event_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceDiscoveryStoppedEvent*
 */
BDeviceServiceDiscoveryStoppedEvent *b_device_service_discovery_stopped_event_new(void);

G_END_DECLS
