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
 * Created by Thomas Lea on 6/6/2024.
 */

#include "barton-core-status.h"
#include "barton-core-discovery-type.h"
#include "glibconfig.h"
#include <inttypes.h>

struct _BCoreStatus
{
    GObject parent_instance;

    GList *device_classes;
    BCoreDiscoveryType discovery_type;
    GList *searching_device_classes;
    guint discovery_seconds;
    gboolean ready_for_operation;
    gboolean ready_for_pairing;
    GHashTable *subsystems;
    gchar *json;
};

G_DEFINE_TYPE(BCoreStatus, b_core_status, G_TYPE_OBJECT)

static GParamSpec *properties[N_B_CORE_STATUS_PROPERTIES];

static gpointer gcopy_func_strdup(gconstpointer src, gpointer data)
{
    return g_strdup((gpointer) src);
}

static void
b_core_status_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    BCoreStatus *status = B_CORE_STATUS(object);

    switch (property_id)
    {
        case B_CORE_STATUS_PROP_DEVICE_CLASSES:
            g_list_free_full(status->device_classes, g_free);
            status->device_classes = g_list_copy_deep(g_value_get_pointer(value), gcopy_func_strdup, NULL);
            break;
        case B_CORE_STATUS_PROP_DISCOVERY_TYPE:
            status->discovery_type = g_value_get_enum(value);
            break;
        case B_CORE_STATUS_PROP_SEARCHING_DEVICE_CLASSES:
            g_list_free_full(status->searching_device_classes, g_free);
            status->searching_device_classes = g_list_copy_deep(g_value_get_pointer(value), gcopy_func_strdup, NULL);
            break;
        case B_CORE_STATUS_PROP_DISCOVERY_SECONDS:
            status->discovery_seconds = g_value_get_uint(value);
            break;
        case B_CORE_STATUS_PROP_READY_FOR_OPERATION:
            status->ready_for_operation = g_value_get_boolean(value);
            break;
        case B_CORE_STATUS_PROP_READY_FOR_PAIRING:
            status->ready_for_pairing = g_value_get_boolean(value);
            break;
        case B_CORE_STATUS_PROP_SUBSYSTEMS:
            g_hash_table_unref(status->subsystems);
            status->subsystems = g_value_dup_boxed(value);
            break;
        case B_CORE_STATUS_PROP_JSON:
            g_free(status->json);
            status->json = g_strdup(g_value_get_string(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_status_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    BCoreStatus *status = B_CORE_STATUS(object);

    switch (property_id)
    {
        // implement get property for all properties
        case B_CORE_STATUS_PROP_DEVICE_CLASSES:
            g_value_set_pointer(value, g_list_copy_deep(status->device_classes, gcopy_func_strdup, NULL));
            break;
        case B_CORE_STATUS_PROP_DISCOVERY_TYPE:
            g_value_set_enum(value, status->discovery_type);
            break;
        case B_CORE_STATUS_PROP_SEARCHING_DEVICE_CLASSES:
            g_value_set_pointer(value, g_list_copy_deep(status->searching_device_classes, gcopy_func_strdup, NULL));
            break;
        case B_CORE_STATUS_PROP_DISCOVERY_SECONDS:
            g_value_set_uint(value, status->discovery_seconds);
            break;
        case B_CORE_STATUS_PROP_READY_FOR_OPERATION:
            g_value_set_boolean(value, status->ready_for_operation);
            break;
        case B_CORE_STATUS_PROP_READY_FOR_PAIRING:
            g_value_set_boolean(value, status->ready_for_pairing);
            break;
        case B_CORE_STATUS_PROP_SUBSYSTEMS:
            g_value_set_boxed(value, status->subsystems);
            break;
        case B_CORE_STATUS_PROP_JSON:
            g_value_set_string(value, status->json);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_status_finalize(GObject *object)
{
    BCoreStatus *status = B_CORE_STATUS(object);

    g_list_free_full(status->device_classes, g_free);
    g_list_free_full(status->searching_device_classes, g_free);
    g_hash_table_unref(status->subsystems);
    g_free(status->json);

    G_OBJECT_CLASS(b_core_status_parent_class)->finalize(object);
}

static void b_core_status_class_init(BCoreStatusClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = b_core_status_set_property;
    object_class->get_property = b_core_status_get_property;
    object_class->finalize = b_core_status_finalize;

    /**
     * BCoreStatus:device-classes: (type GLib.List(utf8))
     */
    properties[B_CORE_STATUS_PROP_DEVICE_CLASSES] =
        g_param_spec_pointer(B_CORE_STATUS_PROPERTY_NAMES[B_CORE_STATUS_PROP_DEVICE_CLASSES],
                             "Device Classes",
                             "Registered Device Classes",
                             G_PARAM_READWRITE);

    properties[B_CORE_STATUS_PROP_DISCOVERY_TYPE] =
        g_param_spec_enum(B_CORE_STATUS_PROPERTY_NAMES[B_CORE_STATUS_PROP_DISCOVERY_TYPE],
                          "Discovery Type",
                          "Discovery Type",
                          B_CORE_DISCOVERY_TYPE_TYPE,
                          B_CORE_DISCOVERY_TYPE_NONE,
                          G_PARAM_READWRITE);

    /**
     * BCoreStatus:searching-device-classes: (type GLib.List(utf8))
     */
    properties[B_CORE_STATUS_PROP_SEARCHING_DEVICE_CLASSES] = g_param_spec_pointer(
        B_CORE_STATUS_PROPERTY_NAMES[B_CORE_STATUS_PROP_SEARCHING_DEVICE_CLASSES],
        "Searching Device Classes",
        "Searching Device Classes",
        G_PARAM_READWRITE);

    properties[B_CORE_STATUS_PROP_DISCOVERY_SECONDS] =
        g_param_spec_uint(B_CORE_STATUS_PROPERTY_NAMES[B_CORE_STATUS_PROP_DISCOVERY_SECONDS],
                          "Discovery Seconds",
                          "Discovery Seconds",
                          0,
                          G_MAXUINT,
                          0,
                          G_PARAM_READWRITE);

    properties[B_CORE_STATUS_PROP_READY_FOR_OPERATION] =
        g_param_spec_boolean(B_CORE_STATUS_PROPERTY_NAMES[B_CORE_STATUS_PROP_READY_FOR_OPERATION],
                             "Ready for Operation",
                             "Ready for Operation",
                             FALSE,
                             G_PARAM_READWRITE);

    properties[B_CORE_STATUS_PROP_READY_FOR_PAIRING] =
        g_param_spec_boolean(B_CORE_STATUS_PROPERTY_NAMES[B_CORE_STATUS_PROP_READY_FOR_PAIRING],
                             "Ready for Pairing",
                             "Ready for Pairing",
                             FALSE,
                             G_PARAM_READWRITE);

    /**
     * BCoreStatus:subsystems: (type GLib.HashTable(utf8,utf8))
     */
    properties[B_CORE_STATUS_PROP_SUBSYSTEMS] =
        g_param_spec_boxed(B_CORE_STATUS_PROPERTY_NAMES[B_CORE_STATUS_PROP_SUBSYSTEMS],
                           "Subsystems",
                           "Subsystems",
                           G_TYPE_HASH_TABLE,
                           G_PARAM_READWRITE);

    properties[B_CORE_STATUS_PROP_JSON] =
        g_param_spec_string(B_CORE_STATUS_PROPERTY_NAMES[B_CORE_STATUS_PROP_JSON],
                            "JSON",
                            "JSON representation of the status object",
                            NULL,
                            G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_B_CORE_STATUS_PROPERTIES, properties);
}

static void b_core_status_init(BCoreStatus *status)
{
    status->device_classes = NULL;
    status->discovery_type = B_CORE_DISCOVERY_TYPE_NONE;
    status->searching_device_classes = NULL;
    status->discovery_seconds = 0;
    status->ready_for_operation = FALSE;
    status->ready_for_pairing = FALSE;
    status->subsystems = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
}

BCoreStatus *b_core_status_new(void)
{
    return g_object_new(B_CORE_STATUS_TYPE, NULL);
}
