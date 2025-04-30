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
