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
 * Created by Christian Leithner on 6/10/2024.
 */

#include "barton-core-device-found-details.h"

struct _BCoreDeviceFoundDetails
{
    GObject parent_instance;

    gchar *uuid;
    gchar *manufacturer;
    gchar *model;
    gchar *hardware_version;
    gchar *firmware_version;
    gchar *device_class;
    GHashTable *metadata;
    GHashTable *endpoint_profiles;
};

G_DEFINE_TYPE(BCoreDeviceFoundDetails, b_core_device_found_details, G_TYPE_OBJECT)

static GParamSpec *properties[N_B_CORE_DEVICE_FOUND_DETAILS_PROPERTIES];

static void b_core_device_found_details_set_property(GObject *object,
                                                               guint property_id,
                                                               const GValue *value,
                                                               GParamSpec *pspec)
{
    BCoreDeviceFoundDetails *self = B_CORE_DEVICE_FOUND_DETAILS(object);

    switch (property_id)
    {
        case B_CORE_DEVICE_FOUND_DETAILS_PROP_UUID:
            g_free(self->uuid);
            self->uuid = g_value_dup_string(value);
            break;
        case B_CORE_DEVICE_FOUND_DETAILS_PROP_MANUFACTURER:
            g_free(self->manufacturer);
            self->manufacturer = g_value_dup_string(value);
            break;
        case B_CORE_DEVICE_FOUND_DETAILS_PROP_MODEL:
            g_free(self->model);
            self->model = g_value_dup_string(value);
            break;
        case B_CORE_DEVICE_FOUND_DETAILS_PROP_HARDWARE_VERSION:
            g_free(self->hardware_version);
            self->hardware_version = g_value_dup_string(value);
            break;
        case B_CORE_DEVICE_FOUND_DETAILS_PROP_FIRMWARE_VERSION:
            g_free(self->firmware_version);
            self->firmware_version = g_value_dup_string(value);
            break;
        case B_CORE_DEVICE_FOUND_DETAILS_PROP_DEVICE_CLASS:
            g_free(self->device_class);
            self->device_class = g_value_dup_string(value);
            break;
        case B_CORE_DEVICE_FOUND_DETAILS_PROP_METADATA:
            g_hash_table_unref(self->metadata);
            self->metadata = g_value_dup_boxed(value);
            break;
        case B_CORE_DEVICE_FOUND_DETAILS_PROP_ENDPOINT_PROFILES:
            g_hash_table_unref(self->endpoint_profiles);
            self->endpoint_profiles = g_value_dup_boxed(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
b_core_device_found_details_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    BCoreDeviceFoundDetails *self = B_CORE_DEVICE_FOUND_DETAILS(object);

    switch (property_id)
    {
        case B_CORE_DEVICE_FOUND_DETAILS_PROP_UUID:
            g_value_set_string(value, self->uuid);
            break;
        case B_CORE_DEVICE_FOUND_DETAILS_PROP_MANUFACTURER:
            g_value_set_string(value, self->manufacturer);
            break;
        case B_CORE_DEVICE_FOUND_DETAILS_PROP_MODEL:
            g_value_set_string(value, self->model);
            break;
        case B_CORE_DEVICE_FOUND_DETAILS_PROP_HARDWARE_VERSION:
            g_value_set_string(value, self->hardware_version);
            break;
        case B_CORE_DEVICE_FOUND_DETAILS_PROP_FIRMWARE_VERSION:
            g_value_set_string(value, self->firmware_version);
            break;
        case B_CORE_DEVICE_FOUND_DETAILS_PROP_DEVICE_CLASS:
            g_value_set_string(value, self->device_class);
            break;
        case B_CORE_DEVICE_FOUND_DETAILS_PROP_METADATA:
            g_value_set_boxed(value, self->metadata);
            break;
        case B_CORE_DEVICE_FOUND_DETAILS_PROP_ENDPOINT_PROFILES:
            g_value_set_boxed(value, self->endpoint_profiles);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_device_found_details_finalize(GObject *object)
{
    BCoreDeviceFoundDetails *self = B_CORE_DEVICE_FOUND_DETAILS(object);

    g_free(self->uuid);
    g_free(self->manufacturer);
    g_free(self->model);
    g_free(self->hardware_version);
    g_free(self->firmware_version);
    g_free(self->device_class);
    g_hash_table_unref(self->metadata);
    g_hash_table_unref(self->endpoint_profiles);

    G_OBJECT_CLASS(b_core_device_found_details_parent_class)->finalize(object);
}

static void b_core_device_found_details_class_init(BCoreDeviceFoundDetailsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = b_core_device_found_details_set_property;
    object_class->get_property = b_core_device_found_details_get_property;
    object_class->finalize = b_core_device_found_details_finalize;

    properties[B_CORE_DEVICE_FOUND_DETAILS_PROP_UUID] = g_param_spec_string(
        B_CORE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_CORE_DEVICE_FOUND_DETAILS_PROP_UUID],
        "UUID",
        "The UUID of the device",
        NULL,
        G_PARAM_READWRITE);

    properties[B_CORE_DEVICE_FOUND_DETAILS_PROP_MANUFACTURER] = g_param_spec_string(
        B_CORE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_CORE_DEVICE_FOUND_DETAILS_PROP_MANUFACTURER],
        "Manufacturer",
        "The manufacturer of the device",
        NULL,
        G_PARAM_READWRITE);

    properties[B_CORE_DEVICE_FOUND_DETAILS_PROP_MODEL] = g_param_spec_string(
        B_CORE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_CORE_DEVICE_FOUND_DETAILS_PROP_MODEL],
        "Model",
        "The model of the device",
        NULL,
        G_PARAM_READWRITE);

    properties[B_CORE_DEVICE_FOUND_DETAILS_PROP_HARDWARE_VERSION] =
        g_param_spec_string(B_CORE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
                                [B_CORE_DEVICE_FOUND_DETAILS_PROP_HARDWARE_VERSION],
                            "Hardware Version",
                            "The hardware version of the device",
                            NULL,
                            G_PARAM_READWRITE);

    properties[B_CORE_DEVICE_FOUND_DETAILS_PROP_FIRMWARE_VERSION] =
        g_param_spec_string(B_CORE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
                                [B_CORE_DEVICE_FOUND_DETAILS_PROP_FIRMWARE_VERSION],
                            "Firmware Version",
                            "The firmware version of the device",
                            NULL,
                            G_PARAM_READWRITE);

    properties[B_CORE_DEVICE_FOUND_DETAILS_PROP_DEVICE_CLASS] = g_param_spec_string(
        B_CORE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_CORE_DEVICE_FOUND_DETAILS_PROP_DEVICE_CLASS],
        "Device Class",
        "The device class of the device",
        NULL,
        G_PARAM_READWRITE);

    /*
     * BCoreDeviceFoundDetails:metadata: (type GLib.HashTable(utf8, utf8))
     */
    properties[B_CORE_DEVICE_FOUND_DETAILS_PROP_METADATA] = g_param_spec_boxed(
        B_CORE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[B_CORE_DEVICE_FOUND_DETAILS_PROP_METADATA],
        "Metadata",
        "The metadata of the device",
        G_TYPE_HASH_TABLE,
        G_PARAM_READWRITE);

    /*
     * BCoreDeviceFoundDetails:endpoint-profiles: (type GLib.HashTable(utf8, utf8))
     */
    properties[B_CORE_DEVICE_FOUND_DETAILS_PROP_ENDPOINT_PROFILES] =
        g_param_spec_boxed(B_CORE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES
                               [B_CORE_DEVICE_FOUND_DETAILS_PROP_ENDPOINT_PROFILES],
                           "Endpoint Profiles",
                           "The endpoint profiles of the device",
                           G_TYPE_HASH_TABLE,
                           G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_B_CORE_DEVICE_FOUND_DETAILS_PROPERTIES, properties);
}

static void b_core_device_found_details_init(BCoreDeviceFoundDetails *self)
{
    self->uuid = NULL;
    self->manufacturer = NULL;
    self->model = NULL;
    self->hardware_version = NULL;
    self->firmware_version = NULL;
    self->device_class = NULL;
    self->metadata = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    self->endpoint_profiles = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
}

BCoreDeviceFoundDetails *b_core_device_found_details_new(void)
{
    return B_CORE_DEVICE_FOUND_DETAILS(g_object_new(B_CORE_DEVICE_FOUND_DETAILS_TYPE, NULL));
}
