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
 * Created by Christian Leithner on 7/3/2024.
 */

#pragma once

#include "events/device-service-event.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_TYPE (b_device_service_device_added_event_get_type())
G_DECLARE_FINAL_TYPE(BDeviceServiceDeviceAddedEvent,
                     b_device_service_device_added_event,
                     B_DEVICE_SERVICE,
                     DEVICE_ADDED_EVENT,
                     BDeviceServiceEvent)

typedef enum
{
    B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_UUID = 1,             // Device UUID
    B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_URI,                  // Device URI
    B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS,         // Device class
    B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS_VERSION, // Device class version
    N_B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTIES
} BDeviceServiceDeviceAddedEventProperty;

static const char *B_DEVICE_SERVICE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[] = {NULL,
                                                                           "uuid",
                                                                           "uri",
                                                                           "device-class",
                                                                           "device-class-version"};

/**
 * b_device_service_device_added_event_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceDeviceAddedEvent*
 */
BDeviceServiceDeviceAddedEvent *b_device_service_device_added_event_new(void);

G_END_DECLS
