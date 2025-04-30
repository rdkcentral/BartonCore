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
 * Created by Christian Leithner on 8/5/2024.
 */

#include "events/device-service-storage-changed-event.h"
#include "gio/gio.h"

struct _BDeviceServiceStorageChangedEvent
{
    BDeviceServiceEvent parent_instance;

    GFileMonitorEvent what_changed;
};

G_DEFINE_TYPE(BDeviceServiceStorageChangedEvent, b_device_service_storage_changed_event, B_DEVICE_SERVICE_EVENT_TYPE);

static GParamSpec *properties[N_B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROPERTIES];

static void b_device_service_storage_changed_event_init(BDeviceServiceStorageChangedEvent *self)
{
    self->what_changed = G_FILE_MONITOR_EVENT_CHANGED;
}

static void b_device_service_storage_changed_event_finalize(GObject *object)
{
    BDeviceServiceStorageChangedEvent *self = B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT(object);

    G_OBJECT_CLASS(b_device_service_storage_changed_event_parent_class)->finalize(object);
}

static void b_device_service_storage_changed_event_get_property(GObject *object,
                                                                guint property_id,
                                                                GValue *value,
                                                                GParamSpec *pspec)
{
    BDeviceServiceStorageChangedEvent *self = B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROP_WHAT_CHANGED:
            g_value_set_enum(value, self->what_changed);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_storage_changed_event_set_property(GObject *object,
                                                                guint property_id,
                                                                const GValue *value,
                                                                GParamSpec *pspec)
{
    BDeviceServiceStorageChangedEvent *self = B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROP_WHAT_CHANGED:
            self->what_changed = g_value_get_enum(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_storage_changed_event_class_init(BDeviceServiceStorageChangedEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_device_service_storage_changed_event_finalize;
    object_class->get_property = b_device_service_storage_changed_event_get_property;
    object_class->set_property = b_device_service_storage_changed_event_set_property;

    properties[B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROP_WHAT_CHANGED] = g_param_spec_enum(
        B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROP_WHAT_CHANGED],
        "What Changed",
        "The type of change that occurred",
        G_TYPE_FILE_MONITOR_EVENT,
        G_FILE_MONITOR_EVENT_CHANGED,
        G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_PROPERTIES, properties);
}

BDeviceServiceStorageChangedEvent *b_device_service_storage_changed_event_new(void)
{
    return g_object_new(B_DEVICE_SERVICE_STORAGE_CHANGED_EVENT_TYPE, NULL);
}
