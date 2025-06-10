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

#include "barton-core-device.h"
#include "barton-core-utils.h"
#include "glib-object.h"
#include "glib.h"
#include <inttypes.h>

struct _BCoreDevice
{
    GObject parent_instance;

    gchar *uuid;
    gchar *deviceClass;
    guint deviceClassVersion;
    gchar *uri;
    gchar *managingDeviceDriver;
    GList *endpoints;
    GList *resources;
    GList *metadata;
};

G_DEFINE_TYPE(BCoreDevice, b_core_device, G_TYPE_OBJECT);

static GParamSpec *properties[N_B_CORE_DEVICE_PROPERTIES];

static void
b_core_device_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    BCoreDevice *self = B_CORE_DEVICE(object);

    switch (property_id)
    {
        case B_CORE_DEVICE_PROP_UUID:
            g_free(self->uuid);
            self->uuid = g_value_dup_string(value);
            break;
        case B_CORE_DEVICE_PROP_DEVICE_CLASS:
            g_free(self->deviceClass);
            self->deviceClass = g_value_dup_string(value);
            break;
        case B_CORE_DEVICE_PROP_DEVICE_CLASS_VERSION:
            self->deviceClassVersion = g_value_get_uint(value);
            break;
        case B_CORE_DEVICE_PROP_URI:
            g_free(self->uri);
            self->uri = g_value_dup_string(value);
            break;
        case B_CORE_DEVICE_PROP_MANAGING_DEVICE_DRIVER:
            g_free(self->managingDeviceDriver);
            self->managingDeviceDriver = g_value_dup_string(value);
            break;
        case B_CORE_DEVICE_PROP_ENDPOINTS:
            g_list_free_full(self->endpoints, g_object_unref);
            self->endpoints = g_list_copy_deep(g_value_get_pointer(value), glistGobjectDataDeepCopy, NULL);
            break;
        case B_CORE_DEVICE_PROP_RESOURCES:
            g_list_free_full(self->resources, g_object_unref);
            self->resources = g_list_copy_deep(g_value_get_pointer(value), glistGobjectDataDeepCopy, NULL);
            break;
        case B_CORE_DEVICE_PROP_METADATA:
            g_list_free_full(self->metadata, g_object_unref);
            self->metadata = g_list_copy_deep(g_value_get_pointer(value), glistGobjectDataDeepCopy, NULL);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_device_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    BCoreDevice *self = B_CORE_DEVICE(object);

    switch (property_id)
    {
        case B_CORE_DEVICE_PROP_UUID:
            g_value_set_string(value, self->uuid);
            break;
        case B_CORE_DEVICE_PROP_DEVICE_CLASS:
            g_value_set_string(value, self->deviceClass);
            break;
        case B_CORE_DEVICE_PROP_DEVICE_CLASS_VERSION:
            g_value_set_uint(value, self->deviceClassVersion);
            break;
        case B_CORE_DEVICE_PROP_URI:
            g_value_set_string(value, self->uri);
            break;
        case B_CORE_DEVICE_PROP_MANAGING_DEVICE_DRIVER:
            g_value_set_string(value, self->managingDeviceDriver);
            break;
        case B_CORE_DEVICE_PROP_ENDPOINTS:
            g_value_set_pointer(value, g_list_copy_deep(self->endpoints, glistGobjectDataDeepCopy, NULL));
            break;
        case B_CORE_DEVICE_PROP_RESOURCES:
            g_value_set_pointer(value, g_list_copy_deep(self->resources, glistGobjectDataDeepCopy, NULL));
            break;
        case B_CORE_DEVICE_PROP_METADATA:
            g_value_set_pointer(value, g_list_copy_deep(self->metadata, glistGobjectDataDeepCopy, NULL));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_device_finalize(GObject *object)
{
    BCoreDevice *self = B_CORE_DEVICE(object);

    g_free(self->uuid);
    g_free(self->deviceClass);
    g_free(self->uri);
    g_free(self->managingDeviceDriver);
    g_list_free_full(self->endpoints, g_object_unref);
    g_list_free_full(self->resources, g_object_unref);
    g_list_free_full(self->metadata, g_object_unref);

    G_OBJECT_CLASS(b_core_device_parent_class)->finalize(object);
}

static void b_core_device_class_init(BCoreDeviceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = b_core_device_set_property;
    object_class->get_property = b_core_device_get_property;
    object_class->finalize = b_core_device_finalize;

    properties[B_CORE_DEVICE_PROP_UUID] =
        g_param_spec_string(B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_UUID],
                            "Device UUID",
                            "The UUID of the device",
                            NULL,
                            G_PARAM_READWRITE);

    properties[B_CORE_DEVICE_PROP_DEVICE_CLASS] =
        g_param_spec_string(B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_DEVICE_CLASS],
                            "Device Class",
                            "The class of the device",
                            NULL,
                            G_PARAM_READWRITE);

    properties[B_CORE_DEVICE_PROP_DEVICE_CLASS_VERSION] =
        g_param_spec_uint(B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_DEVICE_CLASS_VERSION],
                          "Device Class Version",
                          "The version of the device class",
                          0,
                          G_MAXUINT8,
                          0,
                          G_PARAM_READWRITE);

    properties[B_CORE_DEVICE_PROP_URI] =
        g_param_spec_string(B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_URI],
                            "URI",
                            "The URI of the device",
                            NULL,
                            G_PARAM_READWRITE);

    properties[B_CORE_DEVICE_PROP_MANAGING_DEVICE_DRIVER] =
        g_param_spec_string(B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_MANAGING_DEVICE_DRIVER],
                            "Managing Device Driver",
                            "The managing device driver of the device",
                            NULL,
                            G_PARAM_READWRITE);

    /**
     * BCoreDevice:endpoints: (type GLib.List(BCoreEndpoint))
     */
    properties[B_CORE_DEVICE_PROP_ENDPOINTS] =
        g_param_spec_pointer(B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_ENDPOINTS],
                             "Endpoints",
                             "The endpoints of the device",
                             G_PARAM_READWRITE);

    /**
     * BCoreDevice:resources: (type GLib.List(BCoreResource))
     */
    properties[B_CORE_DEVICE_PROP_RESOURCES] =
        g_param_spec_pointer(B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_RESOURCES],
                             "Resources",
                             "The resources of the device",
                             G_PARAM_READWRITE);

    /**
     * BCoreDevice:metadata: (type GLib.List(BCoreMetadata))
     */
    properties[B_CORE_DEVICE_PROP_METADATA] =
        g_param_spec_pointer(B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_METADATA],
                             "Metadata",
                             "The metadata of the device",
                             G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_B_CORE_DEVICE_PROPERTIES, properties);
}

static void b_core_device_init(BCoreDevice *self)
{
    self->uuid = NULL;
    self->deviceClass = NULL;
    self->deviceClassVersion = 0;
    self->uri = NULL;
    self->managingDeviceDriver = NULL;
    self->endpoints = NULL;
    self->resources = NULL;
    self->metadata = NULL;
}

BCoreDevice *b_core_device_new(void)
{
    return B_CORE_DEVICE(g_object_new(B_CORE_DEVICE_TYPE, NULL));
}
