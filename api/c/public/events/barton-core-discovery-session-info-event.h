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

#include "barton-core-event.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define B_CORE_DISCOVERY_SESSION_INFO_EVENT_TYPE (b_core_discovery_session_info_event_get_type())
G_DECLARE_DERIVABLE_TYPE(BCoreDiscoverySessionInfoEvent,
                         b_core_discovery_session_info_event,
                         B_CORE,
                         DISCOVERY_SESSION_INFO_EVENT,
                         BCoreEvent);

typedef enum
{
    B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE =
        1, // The BCoreDiscoveryType of the session
    N_B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROPERTIES
} BCoreDiscoverySessionInfoEventProperty;

static const char *B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES[] = {NULL, "session-discovery-type"};

struct _BCoreDiscoverySessionInfoEventClass
{
    BCoreEventClass parent_class;

    gpointer padding[12];
};

G_END_DECLS
