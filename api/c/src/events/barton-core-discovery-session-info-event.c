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

#include "events/barton-core-discovery-session-info-event.h"
#include "barton-core-discovery-type.h"
#include "events/barton-core-event.h"

typedef struct _BCoreDiscoverySessionInfoEventPrivate
{
    BCoreDiscoveryType sessionDiscoveryType;
} BCoreDiscoverySessionInfoEventPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(BCoreDiscoverySessionInfoEvent,
                                    b_core_discovery_session_info_event,
                                    B_CORE_EVENT_TYPE);

static GParamSpec *properties[N_B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROPERTIES];

static void b_core_discovery_session_info_event_get_property(GObject *object,
                                                                       guint property_id,
                                                                       GValue *value,
                                                                       GParamSpec *pspec)
{
    BCoreDiscoverySessionInfoEvent *self = B_CORE_DISCOVERY_SESSION_INFO_EVENT(object);
    BCoreDiscoverySessionInfoEventPrivate *priv =
        b_core_discovery_session_info_event_get_instance_private(self);

    switch (property_id)
    {
        case B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE:
            g_value_set_enum(value, priv->sessionDiscoveryType);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_discovery_session_info_event_set_property(GObject *object,
                                                                       guint property_id,
                                                                       const GValue *value,
                                                                       GParamSpec *pspec)
{
    BCoreDiscoverySessionInfoEvent *self = B_CORE_DISCOVERY_SESSION_INFO_EVENT(object);
    BCoreDiscoverySessionInfoEventPrivate *priv =
        b_core_discovery_session_info_event_get_instance_private(self);

    switch (property_id)
    {
        case B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE:
            priv->sessionDiscoveryType = g_value_get_enum(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
b_core_discovery_session_info_event_class_init(BCoreDiscoverySessionInfoEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = b_core_discovery_session_info_event_get_property;
    object_class->set_property = b_core_discovery_session_info_event_set_property;

    /**
     * BCoreDiscoverySessionInfoEvent:session-discovery-type: (type BCoreDiscoveryType)
     *
     * The BCoreDiscoveryType of the session
     */
    properties[B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE] =
        g_param_spec_enum(B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROPERTY_NAMES
                              [B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROP_SESSION_DISCOVERY_TYPE],
                          "Session Discovery Type",
                          "The BCoreDiscoveryType of the session",
                          B_CORE_DISCOVERY_TYPE_TYPE,
                          B_CORE_DISCOVERY_TYPE_NONE,
                          G_PARAM_READWRITE);

    g_object_class_install_properties(
        object_class, N_B_CORE_DISCOVERY_SESSION_INFO_EVENT_PROPERTIES, properties);
}

static void b_core_discovery_session_info_event_init(BCoreDiscoverySessionInfoEvent *self)
{
    BCoreDiscoverySessionInfoEventPrivate *priv =
        b_core_discovery_session_info_event_get_instance_private(self);

    priv->sessionDiscoveryType = B_CORE_DISCOVERY_TYPE_NONE;
}
