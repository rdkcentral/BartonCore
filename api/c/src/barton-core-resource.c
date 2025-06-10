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

#include "barton-core-resource.h"
#include <inttypes.h>

struct _BCoreResource
{
    GObject parent_instance;

    gchar *id;
    gchar *uri;
    gchar *endpoint_id;
    gchar *device_uuid;
    gchar *value;
    gchar *type;
    guint mode;
    BCoreResourceCachingPolicy caching_policy;
    guint64 date_of_last_sync_millis;
};

G_DEFINE_TYPE(BCoreResource, b_core_resource, G_TYPE_OBJECT)

static void
b_core_resource_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    BCoreResource *resource = B_CORE_RESOURCE(object);

    switch (property_id)
    {
        case B_CORE_RESOURCE_PROP_ID:
            g_free(resource->id);
            resource->id = g_value_dup_string(value);
            break;
        case B_CORE_RESOURCE_PROP_URI:
            g_free(resource->uri);
            resource->uri = g_value_dup_string(value);
            break;
        case B_CORE_RESOURCE_PROP_ENDPOINT_ID:
            g_free(resource->endpoint_id);
            resource->endpoint_id = g_value_dup_string(value);
            break;
        case B_CORE_RESOURCE_PROP_DEVICE_UUID:
            g_free(resource->device_uuid);
            resource->device_uuid = g_value_dup_string(value);
            break;
        case B_CORE_RESOURCE_PROP_VALUE:
            g_free(resource->value);
            resource->value = g_value_dup_string(value);
            break;
        case B_CORE_RESOURCE_PROP_TYPE:
            g_free(resource->type);
            resource->type = g_value_dup_string(value);
            break;
        case B_CORE_RESOURCE_PROP_MODE:
            resource->mode = g_value_get_uint(value);
            break;
        case B_CORE_RESOURCE_PROP_CACHING_POLICY:
            resource->caching_policy = g_value_get_enum(value);
            break;
        case B_CORE_RESOURCE_PROP_DATE_OF_LAST_SYNC_MILLIS:
            resource->date_of_last_sync_millis = g_value_get_uint64(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_resource_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    BCoreResource *resource = B_CORE_RESOURCE(object);

    switch (property_id)
    {
        case B_CORE_RESOURCE_PROP_ID:
            g_value_set_string(value, resource->id);
            break;
        case B_CORE_RESOURCE_PROP_URI:
            g_value_set_string(value, resource->uri);
            break;
        case B_CORE_RESOURCE_PROP_ENDPOINT_ID:
            g_value_set_string(value, resource->endpoint_id);
            break;
        case B_CORE_RESOURCE_PROP_DEVICE_UUID:
            g_value_set_string(value, resource->device_uuid);
            break;
        case B_CORE_RESOURCE_PROP_VALUE:
            g_value_set_string(value, resource->value);
            break;
        case B_CORE_RESOURCE_PROP_TYPE:
            g_value_set_string(value, resource->type);
            break;
        case B_CORE_RESOURCE_PROP_MODE:
            g_value_set_uint(value, resource->mode);
            break;
        case B_CORE_RESOURCE_PROP_CACHING_POLICY:
            g_value_set_enum(value, resource->caching_policy);
            break;
        case B_CORE_RESOURCE_PROP_DATE_OF_LAST_SYNC_MILLIS:
            g_value_set_uint64(value, resource->date_of_last_sync_millis);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_resource_finalize(GObject *object)
{
    BCoreResource *resource = B_CORE_RESOURCE(object);

    g_free(resource->id);
    g_free(resource->uri);
    g_free(resource->endpoint_id);
    g_free(resource->device_uuid);
    g_free(resource->value);
    g_free(resource->type);

    G_OBJECT_CLASS(b_core_resource_parent_class)->finalize(object);
}

static void b_core_resource_class_init(BCoreResourceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = b_core_resource_set_property;
    object_class->get_property = b_core_resource_get_property;
    object_class->finalize = b_core_resource_finalize;

    /*
     * Define properties
     */
    g_object_class_install_property(
        object_class,
        B_CORE_RESOURCE_PROP_ID,
        g_param_spec_string(B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_ID],
                            "ID",
                            "Resource ID",
                            NULL,
                            G_PARAM_READWRITE));
    g_object_class_install_property(
        object_class,
        B_CORE_RESOURCE_PROP_URI,
        g_param_spec_string(B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_URI],
                            "URI",
                            "Resource URI",
                            NULL,
                            G_PARAM_READWRITE));
    g_object_class_install_property(
        object_class,
        B_CORE_RESOURCE_PROP_ENDPOINT_ID,
        g_param_spec_string(B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_ENDPOINT_ID],
                            "Endpoint ID",
                            "Endpoint ID",
                            NULL,
                            G_PARAM_READWRITE));
    g_object_class_install_property(
        object_class,
        B_CORE_RESOURCE_PROP_DEVICE_UUID,
        g_param_spec_string(B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_DEVICE_UUID],
                            "Device UUID",
                            "Device UUID",
                            NULL,
                            G_PARAM_READWRITE));
    g_object_class_install_property(
        object_class,
        B_CORE_RESOURCE_PROP_VALUE,
        g_param_spec_string(B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_VALUE],
                            "Value",
                            "Resource Value",
                            NULL,
                            G_PARAM_READWRITE));
    g_object_class_install_property(
        object_class,
        B_CORE_RESOURCE_PROP_TYPE,
        g_param_spec_string(B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_TYPE],
                            "Type",
                            "Resource Type",
                            NULL,
                            G_PARAM_READWRITE));
    g_object_class_install_property(
        object_class,
        B_CORE_RESOURCE_PROP_MODE,
        g_param_spec_uint(B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_MODE],
                          "Mode",
                          "Resource Mode",
                          0,
                          G_MAXUINT8,
                          0,
                          G_PARAM_READWRITE));
    g_object_class_install_property(
        object_class,
        B_CORE_RESOURCE_PROP_CACHING_POLICY,
        g_param_spec_enum(B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_CACHING_POLICY],
                          "Caching Policy",
                          "Resource Caching Policy",
                          B_CORE_RESOURCE_CACHING_POLICY_TYPE,
                          B_CORE_RESOURCE_CACHING_POLICY_NEVER,
                          G_PARAM_READWRITE));
    g_object_class_install_property(
        object_class,
        B_CORE_RESOURCE_PROP_DATE_OF_LAST_SYNC_MILLIS,
        g_param_spec_uint64(
            B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_DATE_OF_LAST_SYNC_MILLIS],
            "Date of Last Sync (Millis)",
            "Date of Last Sync in Milliseconds",
            0,
            G_MAXUINT64,
            0,
            G_PARAM_READWRITE));
}

static void b_core_resource_init(BCoreResource *resource)
{
    resource->id = NULL;
    resource->uri = NULL;
    resource->endpoint_id = NULL;
    resource->device_uuid = NULL;
    resource->value = NULL;
    resource->type = NULL;
    resource->mode = 0;
    resource->caching_policy = B_CORE_RESOURCE_CACHING_POLICY_NEVER;
    resource->date_of_last_sync_millis = 0;
}

BCoreResource *b_core_resource_new(void)
{
    return g_object_new(B_CORE_RESOURCE_TYPE, NULL);
}

GType b_core_resource_caching_policy_get_type(void)
{
    static gsize g_define_type_id__volatile = 0;

    // Initializes the BCoreResourceCachingPolicy enum GType only once.
    // Note: Some tools might match the code below with public code, but this just the standard use of this glib API.
    if (g_once_init_enter(&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { B_CORE_RESOURCE_CACHING_POLICY_NEVER,"B_CORE_RESOURCE_CACHING_POLICY_NEVER",  "never"                                                             },
            {B_CORE_RESOURCE_CACHING_POLICY_ALWAYS,
             "B_CORE_RESOURCE_CACHING_POLICY_ALWAYS", "always"                                                },
            {                                              0,                                             NULL,     NULL}
        };

        GType g_define_type_id =
            g_enum_register_static(g_intern_static_string("BCoreResourceCachingPolicy"), values);
        g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}
