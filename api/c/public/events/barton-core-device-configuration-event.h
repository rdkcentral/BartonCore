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
 * Created by Christian Leithner on 7/1/2024.
 */

#pragma once

#include "barton-core-event.h"
#include "events/barton-core-device-discovered-event.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define B_CORE_DEVICE_CONFIGURATION_EVENT_TYPE (b_core_device_configuration_event_get_type())
G_DECLARE_DERIVABLE_TYPE(BCoreDeviceConfigurationEvent,
                         b_core_device_configuration_event,
                         B_CORE,
                         DEVICE_CONFIGURATION_EVENT,
                         BCoreDiscoverySessionInfoEvent);

typedef enum
{
    B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_UUID = 1,     // The UUID of the device
    B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS, // The device class of the device
    N_B_CORE_DEVICE_CONFIGURATION_EVENT_PROPERTIES
} BCoreDeviceConfigurationEventProperty;

static const char *B_CORE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES[] = {NULL, "uuid", "device-class"};

struct _BCoreDeviceConfigurationEventClass
{
    BCoreEventClass parent_class;

    gpointer padding[12];
};

G_END_DECLS
