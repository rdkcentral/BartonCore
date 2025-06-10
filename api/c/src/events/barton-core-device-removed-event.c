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
 * Created by Christian Leithner on 7/16/2024.
 */

#include "events/barton-core-device-removed-event.h"

struct _BCoreDeviceRemovedEvent
{
    BCoreEvent parent_instance;

    gchar *device_uuid;
    gchar *device_class;
};

G_DEFINE_TYPE(BCoreDeviceRemovedEvent, b_core_device_removed_event, B_CORE_EVENT_TYPE);

static GParamSpec *properties[N_B_CORE_DEVICE_REMOVED_EVENT_PROPERTIES];

static void b_core_device_removed_event_init(BCoreDeviceRemovedEvent *self)
{
    self->device_uuid = NULL;
    self->device_class = NULL;
}

static void b_core_device_removed_event_finalize(GObject *object)
{
    BCoreDeviceRemovedEvent *self = B_CORE_DEVICE_REMOVED_EVENT(object);

    g_free(self->device_uuid);
    g_free(self->device_class);

    G_OBJECT_CLASS(b_core_device_removed_event_parent_class)->finalize(object);
}

static void
b_core_device_removed_event_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    BCoreDeviceRemovedEvent *self = B_CORE_DEVICE_REMOVED_EVENT(object);

    switch (property_id)
    {
        case B_CORE_DEVICE_REMOVED_EVENT_PROP_DEVICE_UUID:
            g_value_set_string(value, self->device_uuid);
            break;
        case B_CORE_DEVICE_REMOVED_EVENT_PROP_DEVICE_CLASS:
            g_value_set_string(value, self->device_class);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_device_removed_event_set_property(GObject *object,
                                                               guint property_id,
                                                               const GValue *value,
                                                               GParamSpec *pspec)
{
    BCoreDeviceRemovedEvent *self = B_CORE_DEVICE_REMOVED_EVENT(object);

    switch (property_id)
    {
        case B_CORE_DEVICE_REMOVED_EVENT_PROP_DEVICE_UUID:
            g_free(self->device_uuid);
            self->device_uuid = g_value_dup_string(value);
            break;
        case B_CORE_DEVICE_REMOVED_EVENT_PROP_DEVICE_CLASS:
            g_free(self->device_class);
            self->device_class = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_device_removed_event_class_init(BCoreDeviceRemovedEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_core_device_removed_event_finalize;
    object_class->get_property = b_core_device_removed_event_get_property;
    object_class->set_property = b_core_device_removed_event_set_property;

    properties[B_CORE_DEVICE_REMOVED_EVENT_PROP_DEVICE_UUID] = g_param_spec_string(
        B_CORE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_REMOVED_EVENT_PROP_DEVICE_UUID],
        "Device UUID",
        "The device ID of the device that was removed",
        NULL,
        G_PARAM_READWRITE);

    properties[B_CORE_DEVICE_REMOVED_EVENT_PROP_DEVICE_CLASS] = g_param_spec_string(
        B_CORE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_REMOVED_EVENT_PROP_DEVICE_CLASS],
        "Device Class",
        "The device class of the device that was removed",
        NULL,
        G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_B_CORE_DEVICE_REMOVED_EVENT_PROPERTIES, properties);
}

BCoreDeviceRemovedEvent *b_core_device_removed_event_new(void)
{
    return B_CORE_DEVICE_REMOVED_EVENT(g_object_new(B_CORE_DEVICE_REMOVED_EVENT_TYPE, NULL));
}
