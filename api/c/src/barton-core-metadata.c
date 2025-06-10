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

#include "barton-core-metadata.h"
#include <inttypes.h>

struct _BCoreMetadata
{
    GObject parent_instance;

    char *id;
    char *uri;
    char *endpointId;
    char *deviceUuid;
    char *value;
};

G_DEFINE_TYPE(BCoreMetadata, b_core_metadata, G_TYPE_OBJECT)

static void
b_core_metadata_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    BCoreMetadata *metadata = B_CORE_METADATA(object);

    switch (property_id)
    {
        case B_CORE_METADATA_PROP_ID:
            metadata->id = g_value_dup_string(value);
            break;

        case B_CORE_METADATA_PROP_URI:
            metadata->uri = g_value_dup_string(value);
            break;

        case B_CORE_METADATA_PROP_ENDPOINT_ID:
            metadata->endpointId = g_value_dup_string(value);
            break;

        case B_CORE_METADATA_PROP_DEVICE_UUID:
            metadata->deviceUuid = g_value_dup_string(value);
            break;

        case B_CORE_METADATA_PROP_VALUE:
            metadata->value = g_value_dup_string(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_metadata_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    BCoreMetadata *metadata = B_CORE_METADATA(object);

    switch (property_id)
    {
        case B_CORE_METADATA_PROP_ID:
            g_value_set_string(value, metadata->id);
            break;

        case B_CORE_METADATA_PROP_URI:
            g_value_set_string(value, metadata->uri);
            break;

        case B_CORE_METADATA_PROP_ENDPOINT_ID:
            g_value_set_string(value, metadata->endpointId);
            break;

        case B_CORE_METADATA_PROP_DEVICE_UUID:
            g_value_set_string(value, metadata->deviceUuid);
            break;

        case B_CORE_METADATA_PROP_VALUE:
            g_value_set_string(value, metadata->value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_metadata_finalize(GObject *object)
{
    BCoreMetadata *metadata = B_CORE_METADATA(object);

    g_free(metadata->id);
    g_free(metadata->uri);
    g_free(metadata->endpointId);
    g_free(metadata->deviceUuid);
    g_free(metadata->value);

    G_OBJECT_CLASS(b_core_metadata_parent_class)->finalize(object);
}

static void b_core_metadata_init(BCoreMetadata *metadata)
{
    metadata->id = NULL;
    metadata->uri = NULL;
    metadata->endpointId = NULL;
    metadata->deviceUuid = NULL;
    metadata->value = NULL;
}

static void b_core_metadata_class_init(BCoreMetadataClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = b_core_metadata_set_property;
    object_class->get_property = b_core_metadata_get_property;
    object_class->finalize = b_core_metadata_finalize;

    g_object_class_install_property(
        object_class,
        B_CORE_METADATA_PROP_ID,
        g_param_spec_string(B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_ID],
                            "ID",
                            "The ID of the metadata",
                            NULL,
                            G_PARAM_READWRITE));

    g_object_class_install_property(
        object_class,
        B_CORE_METADATA_PROP_URI,
        g_param_spec_string(B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_URI],
                            "URI",
                            "The URI of the metadata",
                            NULL,
                            G_PARAM_READWRITE));

    g_object_class_install_property(
        object_class,
        B_CORE_METADATA_PROP_ENDPOINT_ID,
        g_param_spec_string(B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_ENDPOINT_ID],
                            "Endpoint ID",
                            "The endpoint ID of the metadata",
                            NULL,
                            G_PARAM_READWRITE));

    g_object_class_install_property(
        object_class,
        B_CORE_METADATA_PROP_DEVICE_UUID,
        g_param_spec_string(B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_DEVICE_UUID],
                            "Device UUID",
                            "The device UUID of the metadata",
                            NULL,
                            G_PARAM_READWRITE));

    g_object_class_install_property(
        object_class,
        B_CORE_METADATA_PROP_VALUE,
        g_param_spec_string(B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_VALUE],
                            "Value",
                            "The value of the metadata",
                            NULL,
                            G_PARAM_READWRITE));
}

BCoreMetadata *b_core_metadata_new(void)
{
    return g_object_new(B_CORE_METADATA_TYPE, NULL);
}
