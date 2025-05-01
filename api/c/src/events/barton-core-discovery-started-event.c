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
 * Created by Christian Leithner on 6/6/2024.
 */

#include "events/barton-core-discovery-started-event.h"
#include "barton-core-utils.h"
#include "events/barton-core-event.h"

#include <glib.h>

struct _BCoreDiscoveryStartedEvent
{
    BCoreEvent parent_instance;

    GList *device_classes;
    guint timeout;
};

G_DEFINE_TYPE(BCoreDiscoveryStartedEvent,
              b_core_discovery_started_event,
              B_CORE_EVENT_TYPE);

static GParamSpec *properties[N_B_CORE_DISCOVERY_STARTED_EVENT_PROPERTIES];

static void b_core_discovery_started_event_init(BCoreDiscoveryStartedEvent *self)
{
    self->device_classes = NULL;
    self->timeout = 0;
}

static void b_core_discovery_started_event_finalize(GObject *object)
{
    BCoreDiscoveryStartedEvent *self = B_CORE_DISCOVERY_STARTED_EVENT(object);

    g_list_free_full(self->device_classes, g_free);

    G_OBJECT_CLASS(b_core_discovery_started_event_parent_class)->finalize(object);
}

static void b_core_discovery_started_event_get_property(GObject *object,
                                                                  guint property_id,
                                                                  GValue *value,
                                                                  GParamSpec *pspec)
{
    BCoreDiscoveryStartedEvent *self = B_CORE_DISCOVERY_STARTED_EVENT(object);

    switch (property_id)
    {
        case B_CORE_DISCOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES:
            g_value_set_pointer(value, g_list_copy_deep(self->device_classes, glistStringDataDeepCopy, NULL));
            break;
        case B_CORE_DISCOVERY_STARTED_EVENT_PROP_TIMEOUT:
            g_value_set_uint(value, self->timeout);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_discovery_started_event_set_property(GObject *object,
                                                                  guint property_id,
                                                                  const GValue *value,
                                                                  GParamSpec *pspec)
{
    BCoreDiscoveryStartedEvent *self = B_CORE_DISCOVERY_STARTED_EVENT(object);

    switch (property_id)
    {
        case B_CORE_DISCOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES:
            g_list_free_full(self->device_classes, g_free);
            self->device_classes = g_list_copy_deep(g_value_get_pointer(value), glistStringDataDeepCopy, NULL);
            break;
        case B_CORE_DISCOVERY_STARTED_EVENT_PROP_TIMEOUT:
            self->timeout = g_value_get_uint(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_discovery_started_event_class_init(BCoreDiscoveryStartedEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_core_discovery_started_event_finalize;

    object_class->get_property = b_core_discovery_started_event_get_property;
    object_class->set_property = b_core_discovery_started_event_set_property;

    /**
     * BCoreDiscoveryStartedEvent:device-classes: (type GList(utf8))
     *
     * GList of strings describing which device classes discovery was started for
     */
    properties[B_CORE_DISCOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES] =
        g_param_spec_pointer(B_CORE_DISCOVERY_STARTED_EVENT_PROPERTY_NAMES
                                 [B_CORE_DISCOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES],
                             "Device Classes",
                             "GList of strings describing which device classes discovery was started for",
                             G_PARAM_READWRITE);

    /**
     * BCoreDiscoveryStartedEvent:timeout: (type guint16)
     *
     * Timeout for discovery in seconds
     */
    properties[B_CORE_DISCOVERY_STARTED_EVENT_PROP_TIMEOUT] = g_param_spec_uint(
        B_CORE_DISCOVERY_STARTED_EVENT_PROPERTY_NAMES[B_CORE_DISCOVERY_STARTED_EVENT_PROP_TIMEOUT],
        "Timeout",
        "Timeout for discovery in seconds",
        0,
        G_MAXUINT16,
        0,
        G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_B_CORE_DISCOVERY_STARTED_EVENT_PROPERTIES, properties);
}

BCoreDiscoveryStartedEvent *b_core_discovery_started_event_new(void)
{
    return B_CORE_DISCOVERY_STARTED_EVENT(g_object_new(B_CORE_DISCOVERY_STARTED_EVENT_TYPE, NULL));
}
