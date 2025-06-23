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
 * Created by Christian Leithner on 8/19/2024.
 */

#include "events/barton-core-device-database-failure-event.h"

struct _BCoreDeviceDatabaseFailureEvent
{
    BCoreEvent parent_instance;

    BCoreDeviceDatabaseFailureType failure_type;
    gchar *device_id;
};

G_DEFINE_TYPE(BCoreDeviceDatabaseFailureEvent,
              b_core_device_database_failure_event,
              B_CORE_EVENT_TYPE);

static GParamSpec *properties[N_B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTIES];

static void b_core_device_database_failure_event_init(BCoreDeviceDatabaseFailureEvent *self)
{
    self->failure_type = B_CORE_DEVICE_DATABASE_FAILURE_TYPE_DEVICE_LOAD_FAILURE;
    self->device_id = NULL;
}

static void b_core_device_database_failure_event_finalize(GObject *object)
{
    BCoreDeviceDatabaseFailureEvent *self = B_CORE_DEVICE_DATABASE_FAILURE_EVENT(object);

    g_clear_pointer(&self->device_id, g_free);

    G_OBJECT_CLASS(b_core_device_database_failure_event_parent_class)->finalize(object);
}

static void b_core_device_database_failure_event_get_property(GObject *object,
                                                                        guint property_id,
                                                                        GValue *value,
                                                                        GParamSpec *pspec)
{
    BCoreDeviceDatabaseFailureEvent *self = B_CORE_DEVICE_DATABASE_FAILURE_EVENT(object);

    switch (property_id)
    {
        case B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE:
            g_value_set_enum(value, self->failure_type);
            break;
        case B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROP_DEVICE_ID:
            g_value_set_string(value, self->device_id);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_device_database_failure_event_set_property(GObject *object,
                                                                        guint property_id,
                                                                        const GValue *value,
                                                                        GParamSpec *pspec)
{
    BCoreDeviceDatabaseFailureEvent *self = B_CORE_DEVICE_DATABASE_FAILURE_EVENT(object);

    switch (property_id)
    {
        case B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE:
            self->failure_type = g_value_get_enum(value);
            break;
        case B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROP_DEVICE_ID:
            g_clear_pointer(&self->device_id, g_free);
            self->device_id = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
b_core_device_database_failure_event_class_init(BCoreDeviceDatabaseFailureEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_core_device_database_failure_event_finalize;
    object_class->get_property = b_core_device_database_failure_event_get_property;
    object_class->set_property = b_core_device_database_failure_event_set_property;

    properties[B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE] =
        g_param_spec_enum(B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                              [B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE],
                          "Failure Type",
                          "The type of failure that occurred",
                          B_CORE_DEVICE_DATABASE_FAILURE_TYPE_TYPE,
                          B_CORE_DEVICE_DATABASE_FAILURE_TYPE_DEVICE_LOAD_FAILURE,
                          G_PARAM_READWRITE);

    properties[B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROP_DEVICE_ID] =
        g_param_spec_string(B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                                [B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROP_DEVICE_ID],
                            "Device ID",
                            "The ID of the device that failed to load",
                            NULL,
                            G_PARAM_READWRITE);

    g_object_class_install_properties(
        object_class, N_B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTIES, properties);
}

BCoreDeviceDatabaseFailureEvent *b_core_device_database_failure_event_new(void)
{
    return g_object_new(B_CORE_DEVICE_DATABASE_FAILURE_EVENT_TYPE, NULL);
}

GType b_core_device_database_failure_type_type(void)
{
    static GType type = 0;

    if (type == 0)
    {
        static const GEnumValue values[] = {
            {B_CORE_DEVICE_DATABASE_FAILURE_TYPE_DEVICE_LOAD_FAILURE,
             "B_CORE_DEVICE_DATABASE_FAILURE_TYPE_DEVICE_LOAD_FAILURE", "device-load-failure"    },
            {                                                                0, NULL,                  NULL},
        };

        type = g_enum_register_static("BCoreDeviceDatabaseFailureType", values);
    }

    return type;
}
