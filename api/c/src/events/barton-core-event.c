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

#include "events/barton-core-event.h"

static guint idTracker = 0;

typedef struct _BCoreEventPrivate
{
    guint id;
    guint64 timestamp;
} BCoreEventPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(BCoreEvent, b_core_event, G_TYPE_OBJECT);

static void b_core_event_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    BCoreEvent *self = B_CORE_EVENT(object);
    BCoreEventPrivate *priv = b_core_event_get_instance_private(self);

    switch (property_id)
    {
        case B_CORE_EVENT_PROP_ID:
            g_value_set_uint(value, priv->id);
            break;
        case B_CORE_EVENT_PROP_TIMESTAMP:
            g_value_set_uint64(value, priv->timestamp);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
b_core_event_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    BCoreEvent *self = B_CORE_EVENT(object);

    switch (property_id)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_event_class_init(BCoreEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = b_core_event_get_property;

    /**
     * BCoreEvent:id: (type guint)
     *
     * The unique identifier for the event
     */
    g_object_class_install_property(
        object_class,
        B_CORE_EVENT_PROP_ID,
        g_param_spec_uint(B_CORE_EVENT_PROPERTY_NAMES[B_CORE_EVENT_PROP_ID],
                          "ID",
                          "The unique identifier for the event",
                          0,
                          G_MAXUINT,
                          0,
                          G_PARAM_READABLE));

    /**
     * BCoreEvent:timestamp: (type guint64)
     *
     * Realtime unix timestamp in microseconds describing the time the event was created
     */
    g_object_class_install_property(
        object_class,
        B_CORE_EVENT_PROP_TIMESTAMP,
        g_param_spec_uint64(B_CORE_EVENT_PROPERTY_NAMES[B_CORE_EVENT_PROP_TIMESTAMP],
                            "Timestamp",
                            "The time the event was created",
                            0,
                            G_MAXUINT64,
                            0,
                            G_PARAM_READABLE));
}

static void b_core_event_init(BCoreEvent *self)
{
    BCoreEventPrivate *priv = b_core_event_get_instance_private(self);

    priv->id = g_atomic_int_add(&idTracker, 1);
    priv->timestamp = g_get_real_time();
}
