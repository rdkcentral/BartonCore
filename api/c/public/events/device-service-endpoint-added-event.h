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
 * Created by Christian Leithner on 7/16/2024.
 */

#pragma once

#include "events/device-service-event.h"

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_TYPE (b_device_service_endpoint_added_event_get_type())

G_DECLARE_FINAL_TYPE(BDeviceServiceEndpointAddedEvent,
                     b_device_service_endpoint_added_event,
                     B_DEVICE_SERVICE,
                     ENDPOINT_ADDED_EVENT,
                     BDeviceServiceEvent)

typedef enum
{
    B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROP_ENDPOINT = 1, // The endpoint that was added
    B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROP_DEVICE_CLASS, // The device class of the device the endpoint belongs
                                                             // to
    N_B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROPERTIES,
} BDeviceServiceEndpointAddedEventProperty;

static const char *B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[] = {NULL, "endpoint", "device-class"};

/**
 * b_device_service_endpoint_added_event_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceEndpointAddedEvent*
 */
BDeviceServiceEndpointAddedEvent *b_device_service_endpoint_added_event_new(void);

G_END_DECLS
