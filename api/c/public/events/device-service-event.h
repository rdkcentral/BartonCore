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
