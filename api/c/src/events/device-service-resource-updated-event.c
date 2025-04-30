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
 * Created by Christian Leithner on 7/15/2024.
 */

#include "events/device-service-resource-updated-event.h"
#include "device-service-resource.h"

#include <glib-object.h>

struct _BDeviceServiceResourceUpdatedEvent
{
    BDeviceServiceEvent parent_instance;

    BDeviceServiceResource *resource;
    gchar *metadata;
};

G_DEFINE_TYPE(BDeviceServiceResourceUpdatedEvent, b_device_service_resource_updated_event, B_DEVICE_SERVICE_EVENT_TYPE);

static GParamSpec *properties[N_B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTIES];

static void b_device_service_resource_updated_event_init(BDeviceServiceResourceUpdatedEvent *self)
{
    self->resource = NULL;
    self->metadata = NULL;
}

static void b_device_service_resource_updated_event_finalize(GObject *object)
{
    BDeviceServiceResourceUpdatedEvent *self = B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT(object);

    g_clear_object(&self->resource);
    g_free(self->metadata);

    G_OBJECT_CLASS(b_device_service_resource_updated_event_parent_class)->finalize(object);
}

static void b_device_service_resource_updated_event_get_property(GObject *object,
                                                                 guint property_id,
                                                                 GValue *value,
                                                                 GParamSpec *pspec)
{
    BDeviceServiceResourceUpdatedEvent *self = B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_RESOURCE:
            g_value_set_object(value, self->resource);
            break;
        case B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_METADATA:
            g_value_set_string(value, self->metadata);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_resource_updated_event_set_property(GObject *object,
                                                                 guint property_id,
                                                                 const GValue *value,
                                                                 GParamSpec *pspec)
{
    BDeviceServiceResourceUpdatedEvent *self = B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_RESOURCE:
            g_set_object(&self->resource, g_value_get_object(value));
            break;
        case B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_METADATA:
            g_free(self->metadata);
            self->metadata = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_resource_updated_event_class_init(BDeviceServiceResourceUpdatedEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_device_service_resource_updated_event_finalize;
    object_class->get_property = b_device_service_resource_updated_event_get_property;
    object_class->set_property = b_device_service_resource_updated_event_set_property;

    /**
     * BDeviceServiceResourceUpdatedEvent:resource: (type BDeviceServiceResource)
     *
     * The BDeviceServiceResource that was updated
     */
    properties[B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_RESOURCE] = g_param_spec_object(
        B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_RESOURCE],
        "Resource",
        "The BDeviceServiceResource that was updated",
        B_DEVICE_SERVICE_RESOURCE_TYPE,
        G_PARAM_READWRITE);

    /**
     * BDeviceServiceResourceUpdatedEvent:metadata: (type utf8)
     *
     * A json string representing extra metadata describing this event
     */
    properties[B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_METADATA] = g_param_spec_string(
        B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_METADATA],
        "Metadata",
        "A json string representing extra metadata describing this event",
        NULL,
        G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTIES, properties);
}

BDeviceServiceResourceUpdatedEvent *b_device_service_resource_updated_event_new(void)
{
    return B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT(g_object_new(B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_TYPE, NULL));
}
