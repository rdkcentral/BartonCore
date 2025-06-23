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
 * Created by Ganesh Induri on 07/26/2024.
 */

#include "events/barton-core-zigbee-channel-changed-event.h"

struct _BCoreZigbeeChannelChangedEvent
{
    BCoreEvent parent_instance;

    gboolean channel_changed;
    guint current_channel;
    guint targeted_channel;
};

G_DEFINE_TYPE(BCoreZigbeeChannelChangedEvent,
              b_core_zigbee_channel_changed_event,
              B_CORE_EVENT_TYPE)

static GParamSpec *properties[N_B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTIES];

static void b_core_zigbee_channel_changed_event_init(BCoreZigbeeChannelChangedEvent *self)
{
    self->channel_changed = false;
    self->current_channel = 0;
    self->targeted_channel = 0;
}

static void b_core_zigbee_channel_changed_event_finalize(GObject *object)
{

    G_OBJECT_CLASS(b_core_zigbee_channel_changed_event_parent_class)->finalize(object);
}

static void b_core_zigbee_channel_changed_event_get_property(GObject *object,
                                                                       guint property_id,
                                                                       GValue *value,
                                                                       GParamSpec *pspec)
{
    BCoreZigbeeChannelChangedEvent *self = B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT(object);

    switch (property_id)
    {
        case B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CHANNEL_CHANGED:
            g_value_set_boolean(value, self->channel_changed);
            break;
        case B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CURRENT_CHANNEL:
            g_value_set_uint(value, self->current_channel);
            break;
        case B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_TARGETED_CHANNEL:
            g_value_set_uint(value, self->targeted_channel);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_zigbee_channel_changed_event_set_property(GObject *object,
                                                                       guint property_id,
                                                                       const GValue *value,
                                                                       GParamSpec *pspec)
{
    BCoreZigbeeChannelChangedEvent *self = B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT(object);

    switch (property_id)
    {
        case B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CHANNEL_CHANGED:
            self->channel_changed = g_value_get_boolean(value);
            break;
        case B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CURRENT_CHANNEL:
            self->current_channel = g_value_get_uint(value);
            break;
        case B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_TARGETED_CHANNEL:
            self->targeted_channel = g_value_get_uint(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
b_core_zigbee_channel_changed_event_class_init(BCoreZigbeeChannelChangedEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_core_zigbee_channel_changed_event_finalize;
    object_class->get_property = b_core_zigbee_channel_changed_event_get_property;
    object_class->set_property = b_core_zigbee_channel_changed_event_set_property;

    properties[B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CHANNEL_CHANGED] =
        g_param_spec_boolean(B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                                 [B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CHANNEL_CHANGED],
                             "channel-changed",
                             "Indicates if the channel actually changed (true) or failed (false)",
                             FALSE,
                             G_PARAM_READWRITE);

    properties[B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CURRENT_CHANNEL] =
        g_param_spec_uint(B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                              [B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CURRENT_CHANNEL],
                          "current-channel",
                          "The current Zigbee channel",
                          0,
                          G_MAXUINT8,
                          0,
                          G_PARAM_READWRITE);

    properties[B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_TARGETED_CHANNEL] =
        g_param_spec_uint(B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                              [B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_TARGETED_CHANNEL],
                          "targeted-channel",
                          "Indicates targeted Zigbee channel",
                          0,
                          G_MAXUINT8,
                          0,
                          G_PARAM_READWRITE);

    g_object_class_install_properties(
        object_class, N_B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTIES, properties);
}

BCoreZigbeeChannelChangedEvent *b_core_zigbee_channel_changed_event_new(void)
{
    return g_object_new(B_CORE_ZIGBEE_CHANNEL_CHANGED_EVENT_TYPE, NULL);
}
