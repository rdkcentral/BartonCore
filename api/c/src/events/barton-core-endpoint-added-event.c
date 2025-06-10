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

#include "events/barton-core-endpoint-added-event.h"
#include "barton-core-endpoint.h"

struct _BCoreEndpointAddedEvent
{
    BCoreEvent parent;

    BCoreEndpoint *endpoint;
    gchar *deviceClass;
};

G_DEFINE_TYPE(BCoreEndpointAddedEvent, b_core_endpoint_added_event, B_CORE_EVENT_TYPE)

static GParamSpec *properties[N_B_CORE_ENDPOINT_ADDED_EVENT_PROPERTIES];

static void b_core_endpoint_added_event_init(BCoreEndpointAddedEvent *self)
{
    self->endpoint = NULL;
    self->deviceClass = NULL;
}

static void b_core_endpoint_added_event_finalize(GObject *object)
{
    BCoreEndpointAddedEvent *self = B_CORE_ENDPOINT_ADDED_EVENT(object);

    g_clear_object(&self->endpoint);
    g_free(self->deviceClass);

    G_OBJECT_CLASS(b_core_endpoint_added_event_parent_class)->finalize(object);
}

static void
b_core_endpoint_added_event_get_property(GObject *object, guint propertyId, GValue *value, GParamSpec *pspec)
{
    BCoreEndpointAddedEvent *self = B_CORE_ENDPOINT_ADDED_EVENT(object);

    switch (propertyId)
    {
        case B_CORE_ENDPOINT_ADDED_EVENT_PROP_ENDPOINT:
            g_value_set_object(value, self->endpoint);
            break;
        case B_CORE_ENDPOINT_ADDED_EVENT_PROP_DEVICE_CLASS:
            g_value_set_string(value, self->deviceClass);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
            break;
    }
}

static void b_core_endpoint_added_event_set_property(GObject *object,
                                                               guint propertyId,
                                                               const GValue *value,
                                                               GParamSpec *pspec)
{
    BCoreEndpointAddedEvent *self = B_CORE_ENDPOINT_ADDED_EVENT(object);

    switch (propertyId)
    {
        case B_CORE_ENDPOINT_ADDED_EVENT_PROP_ENDPOINT:
            g_clear_object(&self->endpoint);
            self->endpoint = g_value_dup_object(value);
            break;
        case B_CORE_ENDPOINT_ADDED_EVENT_PROP_DEVICE_CLASS:
            g_free(self->deviceClass);
            self->deviceClass = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
            break;
    }
}

static void b_core_endpoint_added_event_class_init(BCoreEndpointAddedEventClass *klass)
{
    GObjectClass *objectClass = G_OBJECT_CLASS(klass);

    objectClass->finalize = b_core_endpoint_added_event_finalize;
    objectClass->get_property = b_core_endpoint_added_event_get_property;
    objectClass->set_property = b_core_endpoint_added_event_set_property;

    /**
     * BCoreEndpointAddedEvent:endpoint: (type BCoreEndpoint)
     *
     * The endpoint that was added
     */
    properties[B_CORE_ENDPOINT_ADDED_EVENT_PROP_ENDPOINT] = g_param_spec_object(
        B_CORE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_CORE_ENDPOINT_ADDED_EVENT_PROP_ENDPOINT],
        "Endpoint",
        "The endpoint that was added",
        B_CORE_ENDPOINT_TYPE,
        G_PARAM_READWRITE);

    properties[B_CORE_ENDPOINT_ADDED_EVENT_PROP_DEVICE_CLASS] = g_param_spec_string(
        B_CORE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_CORE_ENDPOINT_ADDED_EVENT_PROP_DEVICE_CLASS],
        "Device class",
        "The device class of the device the endpoint belongs to",
        NULL,
        G_PARAM_READWRITE);

    g_object_class_install_properties(objectClass, N_B_CORE_ENDPOINT_ADDED_EVENT_PROPERTIES, properties);
}

BCoreEndpointAddedEvent *b_core_endpoint_added_event_new(void)
{
    return g_object_new(B_CORE_ENDPOINT_ADDED_EVENT_TYPE, NULL);
}
