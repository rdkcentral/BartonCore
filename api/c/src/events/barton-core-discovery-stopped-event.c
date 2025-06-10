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

#include "events/barton-core-discovery-stopped-event.h"

#include <glib.h>

struct _BCoreDiscoveryStoppedEvent
{
    BCoreEvent parent_instance;

    gchar *device_class;
};

G_DEFINE_TYPE(BCoreDiscoveryStoppedEvent,
              b_core_discovery_stopped_event,
              B_CORE_EVENT_TYPE);

static GParamSpec *properties[N_B_CORE_DISCOVERY_STOPPED_EVENT_PROPERTIES];

static void b_core_discovery_stopped_event_init(BCoreDiscoveryStoppedEvent *self)
{
    self->device_class = NULL;
}

static void b_core_discovery_stopped_event_finalize(GObject *object)
{
    BCoreDiscoveryStoppedEvent *self = B_CORE_DISCOVERY_STOPPED_EVENT(object);

    g_free(self->device_class);

    G_OBJECT_CLASS(b_core_discovery_stopped_event_parent_class)->finalize(object);
}

static void b_core_discovery_stopped_event_get_property(GObject *object,
                                                                  guint property_id,
                                                                  GValue *value,
                                                                  GParamSpec *pspec)
{
    BCoreDiscoveryStoppedEvent *self = B_CORE_DISCOVERY_STOPPED_EVENT(object);

    switch (property_id)
    {
        case B_CORE_DISCOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS:
            g_value_set_string(value, self->device_class);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_discovery_stopped_event_set_property(GObject *object,
                                                                  guint property_id,
                                                                  const GValue *value,
                                                                  GParamSpec *pspec)
{
    BCoreDiscoveryStoppedEvent *self = B_CORE_DISCOVERY_STOPPED_EVENT(object);

    switch (property_id)
    {
        case B_CORE_DISCOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS:
            g_free(self->device_class);
            self->device_class = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_discovery_stopped_event_class_init(BCoreDiscoveryStoppedEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_core_discovery_stopped_event_finalize;
    object_class->get_property = b_core_discovery_stopped_event_get_property;
    object_class->set_property = b_core_discovery_stopped_event_set_property;

    properties[B_CORE_DISCOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS] =
        g_param_spec_string(B_CORE_DISCOVERY_STOPPED_EVENT_PROPERTY_NAMES
                                [B_CORE_DISCOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS],
                            "Device class",
                            "The device class discovery was stopped for",
                            NULL,
                            G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_B_CORE_DISCOVERY_STOPPED_EVENT_PROPERTIES, properties);
}

BCoreDiscoveryStoppedEvent *b_core_discovery_stopped_event_new(void)
{
    return g_object_new(B_CORE_DISCOVERY_STOPPED_EVENT_TYPE, NULL);
}
