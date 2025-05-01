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
 * Created by Thomas Lea on 5/10/2024.
 */

#include "barton-core-endpoint.h"
#include "barton-core-utils.h"
#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include <inttypes.h>

struct _BCoreEndpoint
{
    GObject parent_instance;

    gchar *id;
    gchar *uri;
    gchar *profile;
    guint profileVersion;
    gchar *deviceUuid;
    gboolean enabled;
    GList *resources;
    GList *metadata;
};

G_DEFINE_TYPE(BCoreEndpoint, b_core_endpoint, G_TYPE_OBJECT);

static GParamSpec *properties[N_B_CORE_ENDPOINT_PROPERTIES];

static void
b_core_endpoint_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    BCoreEndpoint *self = B_CORE_ENDPOINT(object);

    switch (property_id)
    {
        case B_CORE_ENDPOINT_PROP_ID:
            g_free(self->id);
            self->id = g_value_dup_string(value);
            break;
        case B_CORE_ENDPOINT_PROP_URI:
            g_free(self->uri);
            self->uri = g_value_dup_string(value);
            break;
        case B_CORE_ENDPOINT_PROP_PROFILE:
            g_free(self->profile);
            self->profile = g_value_dup_string(value);
            break;
        case B_CORE_ENDPOINT_PROP_PROFILE_VERSION:
            self->profileVersion = g_value_get_uint(value);
            break;
        case B_CORE_ENDPOINT_PROP_DEVICE_UUID:
            g_free(self->deviceUuid);
            self->deviceUuid = g_value_dup_string(value);
            break;
        case B_CORE_ENDPOINT_PROP_ENABLED:
            self->enabled = g_value_get_boolean(value);
            break;
        case B_CORE_ENDPOINT_PROP_RESOURCES:
            g_list_free_full(self->resources, g_object_unref);
            self->resources = g_list_copy_deep(g_value_get_pointer(value), glistGobjectDataDeepCopy, NULL);
            break;
        case B_CORE_ENDPOINT_PROP_METADATA:
            g_list_free_full(self->metadata, g_object_unref);
            self->metadata = g_list_copy_deep(g_value_get_pointer(value), glistGobjectDataDeepCopy, NULL);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_endpoint_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    BCoreEndpoint *self = B_CORE_ENDPOINT(object);

    switch (property_id)
    {
        case B_CORE_ENDPOINT_PROP_ID:
            g_value_set_string(value, self->id);
            break;
        case B_CORE_ENDPOINT_PROP_URI:
            g_value_set_string(value, self->uri);
            break;
        case B_CORE_ENDPOINT_PROP_PROFILE:
            g_value_set_string(value, self->profile);
            break;
        case B_CORE_ENDPOINT_PROP_PROFILE_VERSION:
            g_value_set_uint(value, self->profileVersion);
            break;
        case B_CORE_ENDPOINT_PROP_DEVICE_UUID:
            g_value_set_string(value, self->deviceUuid);
            break;
        case B_CORE_ENDPOINT_PROP_ENABLED:
            g_value_set_boolean(value, self->enabled);
            break;
        case B_CORE_ENDPOINT_PROP_RESOURCES:
            g_value_set_pointer(value, g_list_copy_deep(self->resources, glistGobjectDataDeepCopy, NULL));
            break;
        case B_CORE_ENDPOINT_PROP_METADATA:
            g_value_set_pointer(value, g_list_copy_deep(self->metadata, glistGobjectDataDeepCopy, NULL));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_endpoint_finalize(GObject *object)
{
    BCoreEndpoint *self = B_CORE_ENDPOINT(object);

    g_free(self->id);
    g_free(self->uri);
    g_free(self->profile);
    g_free(self->deviceUuid);
    g_list_free_full(self->resources, g_object_unref);
    g_list_free_full(self->metadata, g_object_unref);

    G_OBJECT_CLASS(b_core_endpoint_parent_class)->finalize(object);
}

static void b_core_endpoint_class_init(BCoreEndpointClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = b_core_endpoint_set_property;
    object_class->get_property = b_core_endpoint_get_property;
    object_class->finalize = b_core_endpoint_finalize;

    properties[B_CORE_ENDPOINT_PROP_ID] =
        g_param_spec_string(B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_ID],
                            "ID",
                            "The ID of the endpoint",
                            NULL,
                            G_PARAM_READWRITE);

    properties[B_CORE_ENDPOINT_PROP_URI] =
        g_param_spec_string(B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_URI],
                            "URI",
                            "The URI of the endpoint",
                            NULL,
                            G_PARAM_READWRITE);

    properties[B_CORE_ENDPOINT_PROP_PROFILE] =
        g_param_spec_string(B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_PROFILE],
                            "Profile",
                            "The profile of the endpoint",
                            NULL,
                            G_PARAM_READWRITE);

    properties[B_CORE_ENDPOINT_PROP_PROFILE_VERSION] =
        g_param_spec_uint(B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_PROFILE_VERSION],
                          "Profile Version",
                          "The profile version of the endpoint",
                          0,
                          G_MAXUINT8,
                          0,
                          G_PARAM_READWRITE);

    properties[B_CORE_ENDPOINT_PROP_DEVICE_UUID] =
        g_param_spec_string(B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_DEVICE_UUID],
                            "Device UUID",
                            "The device UUID of the endpoint",
                            NULL,
                            G_PARAM_READWRITE);

    properties[B_CORE_ENDPOINT_PROP_ENABLED] =
        g_param_spec_boolean(B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_ENABLED],
                             "Enabled",
                             "The enabled status of the endpoint",
                             FALSE,
                             G_PARAM_READWRITE);

    /**
     * BCoreEndpoint:resources: (type GLib.List(BCoreResource))
     */
    properties[B_CORE_ENDPOINT_PROP_RESOURCES] =
        g_param_spec_pointer(B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_RESOURCES],
                             "Resources",
                             "The resources of the endpoint",
                             G_PARAM_READWRITE);

    /**
     * BCoreEndpoint:metadata: (type GLib.List(BCoreMetadata))
     */
    properties[B_CORE_ENDPOINT_PROP_METADATA] =
        g_param_spec_pointer(B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_METADATA],
                             "Metadata",
                             "The metadata of the endpoint",
                             G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_B_CORE_ENDPOINT_PROPERTIES, properties);
}

static void b_core_endpoint_init(BCoreEndpoint *self)
{
    self->id = NULL;
    self->uri = NULL;
    self->profile = NULL;
    self->profileVersion = 0;
    self->deviceUuid = NULL;
    self->enabled = FALSE;
    self->resources = NULL;
    self->metadata = NULL;
}

BCoreEndpoint *b_core_endpoint_new(void)
{
    return B_CORE_ENDPOINT(g_object_new(B_CORE_ENDPOINT_TYPE, NULL));
}
