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
 * Created by Christian Leithner on 6/16/2024.
 */

#include "events/device-service-endpoint-removed-event.h"
#include "device-service-endpoint.h"

struct _BDeviceServiceEndpointRemovedEvent
{
    BDeviceServiceEvent parent_instance;

    BDeviceServiceEndpoint *endpoint;
    gchar *device_class;
    gboolean deviceRemoved;
};

G_DEFINE_TYPE(BDeviceServiceEndpointRemovedEvent, b_device_service_endpoint_removed_event, B_DEVICE_SERVICE_EVENT_TYPE)

static GParamSpec *properties[N_B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTIES];

static void b_device_service_endpoint_removed_event_init(BDeviceServiceEndpointRemovedEvent *self)
{
    self->endpoint = NULL;
    self->device_class = NULL;
    self->deviceRemoved = FALSE;
}

static void b_device_service_endpoint_removed_event_finalize(GObject *object)
{
    BDeviceServiceEndpointRemovedEvent *self = B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT(object);

    g_clear_object(&self->endpoint);
    g_free(self->device_class);

    G_OBJECT_CLASS(b_device_service_endpoint_removed_event_parent_class)->finalize(object);
}

static void b_device_service_endpoint_removed_event_get_property(GObject *object,
                                                                 guint property_id,
                                                                 GValue *value,
                                                                 GParamSpec *pspec)
{
    BDeviceServiceEndpointRemovedEvent *self = B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_ENDPOINT:
            g_value_set_object(value, self->endpoint);
            break;
        case B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_CLASS:
            g_value_set_string(value, self->device_class);
            break;
        case B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_REMOVED:
            g_value_set_boolean(value, self->deviceRemoved);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_endpoint_removed_event_set_property(GObject *object,
                                                                 guint property_id,
                                                                 const GValue *value,
                                                                 GParamSpec *pspec)
{
    BDeviceServiceEndpointRemovedEvent *self = B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_ENDPOINT:
            if (self->endpoint)
            {
                g_object_unref(self->endpoint);
            }
            self->endpoint = g_value_dup_object(value);
            break;
        case B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_CLASS:
            g_free(self->device_class);
            self->device_class = g_value_dup_string(value);
            break;
        case B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_REMOVED:
            self->deviceRemoved = g_value_get_boolean(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_endpoint_removed_event_class_init(BDeviceServiceEndpointRemovedEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_device_service_endpoint_removed_event_finalize;
    object_class->get_property = b_device_service_endpoint_removed_event_get_property;
    object_class->set_property = b_device_service_endpoint_removed_event_set_property;

    /**
     * BDeviceServiceEndpointRemovedEvent:endpoint: (type BDeviceServiceEndpoint)
     *
     * The endpoint that was removed
     */
    properties[B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_ENDPOINT] = g_param_spec_object(
        B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_ENDPOINT],
        "Endpoint",
        "The endpoint that was removed",
        B_DEVICE_SERVICE_ENDPOINT_TYPE,
        G_PARAM_READWRITE);

    properties[B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_CLASS] =
        g_param_spec_string(B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES
                                [B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_CLASS],
                            "Device class",
                            "The device class of the device the endpoint belonged to",
                            NULL,
                            G_PARAM_READWRITE);

    properties[B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_REMOVED] =
        g_param_spec_boolean(B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTY_NAMES
                                 [B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROP_DEVICE_REMOVED],
                             "Device removed",
                             "A boolean describing is the device owning the endpoint was also removed",
                             FALSE,
                             G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_PROPERTIES, properties);
}

BDeviceServiceEndpointRemovedEvent *b_device_service_endpoint_removed_event_new(void)
{
    return B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT(g_object_new(B_DEVICE_SERVICE_ENDPOINT_REMOVED_EVENT_TYPE, NULL));
}
