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
 * Created by Thomas Lea on 6/10/2024.
 */

#include "events/barton-core-status-event.h"
#include "barton-core-status.h"
#include "barton-core-utils.h"
#include "events/barton-core-event.h"

#include <glib.h>

struct _BCoreStatusEvent
{
    BCoreEvent parent_instance;

    BCoreStatus *status;
    BCoreStatusChangedReason reason;
};

G_DEFINE_TYPE(BCoreStatusEvent, b_core_status_event, B_CORE_EVENT_TYPE);

static GParamSpec *properties[N_B_CORE_STATUS_EVENT_PROPERTIES];

static void b_core_status_event_init(BCoreStatusEvent *self)
{
    self->status = NULL;
    self->reason = B_CORE_STATUS_CHANGED_REASON_INVALID;
}

static void b_core_status_event_finalize(GObject *object)
{
    BCoreStatusEvent *self = B_CORE_STATUS_EVENT(object);

    g_clear_object(&self->status);

    G_OBJECT_CLASS(b_core_status_event_parent_class)->finalize(object);
}

static void
b_core_status_event_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    BCoreStatusEvent *self = B_CORE_STATUS_EVENT(object);

    switch (property_id)
    {
        case B_CORE_STATUS_EVENT_PROP_STATUS:
            g_value_set_object(value, self->status);
            break;
        case B_CORE_STATUS_EVENT_PROP_REASON:
            g_value_set_enum(value, self->reason);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
b_core_status_event_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    BCoreStatusEvent *self = B_CORE_STATUS_EVENT(object);

    switch (property_id)
    {
        case B_CORE_STATUS_EVENT_PROP_STATUS:
            if (self->status)
                g_object_unref(self->status);
            self->status = g_value_dup_object(value);
            break;
        case B_CORE_STATUS_EVENT_PROP_REASON:
            self->reason = g_value_get_enum(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_status_event_class_init(BCoreStatusEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = (GObjectFinalizeFunc) b_core_status_event_finalize;

    object_class->get_property = b_core_status_event_get_property;
    object_class->set_property = b_core_status_event_set_property;

    /**
     * DeviceServiceStatusEvent:status: (type DeviceServiceStatus)
     */
    properties[B_CORE_STATUS_EVENT_PROP_STATUS] =
        g_param_spec_object(B_CORE_STATUS_EVENT_PROPERTY_NAMES[B_CORE_STATUS_EVENT_PROP_STATUS],
                            "Device Service Status",
                            "The overall status of Device Service and its subsystems",
                            B_CORE_STATUS_TYPE,
                            G_PARAM_READWRITE);

    properties[B_CORE_STATUS_EVENT_PROP_REASON] =
        g_param_spec_enum(B_CORE_STATUS_EVENT_PROPERTY_NAMES[B_CORE_STATUS_EVENT_PROP_REASON],
                          "Reason",
                          "The reason for the status change",
                          B_CORE_STATUS_CHANGED_REASON_TYPE,
                          B_CORE_STATUS_CHANGED_REASON_INVALID,
                          G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_B_CORE_STATUS_EVENT_PROPERTIES, properties);
}

BCoreStatusEvent *b_core_status_event_new(void)
{
    return B_CORE_STATUS_EVENT(g_object_new(B_CORE_STATUS_EVENT_TYPE, NULL));
}

GType b_core_status_changed_reason_get_type(void)
{
    static gsize g_define_type_id__volatile = 0;

    // Initializes the BCoreStatusChangedReason enum GType only once.
    // Note: Some tools might match the code below with public code, but this just the standard use of this glib API.
    if (g_once_init_enter(&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            {                   B_CORE_STATUS_CHANGED_REASON_INVALID,
             "B_CORE_STATUS_CHANGED_REASON_INVALID",                    "invalid"                       },
            {         B_CORE_STATUS_CHANGED_REASON_READY_FOR_PAIRING,
             "B_CORE_STATUS_CHANGED_REASON_READY_FOR_PAIRING",          "ready-for-pairing"             },
            {B_CORE_STATUS_CHANGED_REASON_READY_FOR_DEVICE_OPERATION,
             "B_CORE_STATUS_CHANGED_REASON_READY_FOR_DEVICE_OPERATION", "ready-for-device-operation"    },
            {          B_CORE_STATUS_CHANGED_REASON_SUBSYSTEM_STATUS,
             "B_CORE_STATUS_CHANGED_REASON_SUBSYSTEM_STATUS",           "subsystem-status"              },
            {                                                                0, NULL,                         NULL}
        };

        GType g_define_type_id =
            g_enum_register_static(g_intern_static_string("BCoreStatusChangedReason"), values);
        g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}
