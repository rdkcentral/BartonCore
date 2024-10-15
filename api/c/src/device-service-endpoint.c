//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2024 Comcast Corporation
//
// All rights reserved.
//
// This software is protected by copyright laws of the United States
// and of foreign countries. This material may also be protected by
// patent laws of the United States and of foreign countries.
//
// This software is furnished under a license agreement and/or a
// nondisclosure agreement and may only be used or copied in accordance
// with the terms of those agreements.
//
// The mere transfer of this software does not imply any licenses of trade
// secrets, proprietary technology, copyrights, patents, trademarks, or
// any other form of intellectual property whatsoever.
//
// Comcast Corporation retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*
 * Created by Thomas Lea on 5/10/2024.
 */

#include "device-service-endpoint.h"
#include "device-service-utils.h"
#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include <inttypes.h>

struct _BDeviceServiceEndpoint
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

G_DEFINE_TYPE(BDeviceServiceEndpoint, b_device_service_endpoint, G_TYPE_OBJECT);

static GParamSpec *properties[N_B_DEVICE_SERVICE_ENDPOINT_PROPERTIES];

static void
b_device_service_endpoint_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    BDeviceServiceEndpoint *self = B_DEVICE_SERVICE_ENDPOINT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_ENDPOINT_PROP_ID:
            g_free(self->id);
            self->id = g_value_dup_string(value);
            break;
        case B_DEVICE_SERVICE_ENDPOINT_PROP_URI:
            g_free(self->uri);
            self->uri = g_value_dup_string(value);
            break;
        case B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE:
            g_free(self->profile);
            self->profile = g_value_dup_string(value);
            break;
        case B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE_VERSION:
            self->profileVersion = g_value_get_uint(value);
            break;
        case B_DEVICE_SERVICE_ENDPOINT_PROP_DEVICE_UUID:
            g_free(self->deviceUuid);
            self->deviceUuid = g_value_dup_string(value);
            break;
        case B_DEVICE_SERVICE_ENDPOINT_PROP_ENABLED:
            self->enabled = g_value_get_boolean(value);
            break;
        case B_DEVICE_SERVICE_ENDPOINT_PROP_RESOURCES:
            g_list_free_full(self->resources, g_object_unref);
            self->resources = g_list_copy_deep(g_value_get_pointer(value), glistGobjectDataDeepCopy, NULL);
            break;
        case B_DEVICE_SERVICE_ENDPOINT_PROP_METADATA:
            g_list_free_full(self->metadata, g_object_unref);
            self->metadata = g_list_copy_deep(g_value_get_pointer(value), glistGobjectDataDeepCopy, NULL);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_endpoint_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    BDeviceServiceEndpoint *self = B_DEVICE_SERVICE_ENDPOINT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_ENDPOINT_PROP_ID:
            g_value_set_string(value, self->id);
            break;
        case B_DEVICE_SERVICE_ENDPOINT_PROP_URI:
            g_value_set_string(value, self->uri);
            break;
        case B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE:
            g_value_set_string(value, self->profile);
            break;
        case B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE_VERSION:
            g_value_set_uint(value, self->profileVersion);
            break;
        case B_DEVICE_SERVICE_ENDPOINT_PROP_DEVICE_UUID:
            g_value_set_string(value, self->deviceUuid);
            break;
        case B_DEVICE_SERVICE_ENDPOINT_PROP_ENABLED:
            g_value_set_boolean(value, self->enabled);
            break;
        case B_DEVICE_SERVICE_ENDPOINT_PROP_RESOURCES:
            g_value_set_pointer(value, g_list_copy_deep(self->resources, glistGobjectDataDeepCopy, NULL));
            break;
        case B_DEVICE_SERVICE_ENDPOINT_PROP_METADATA:
            g_value_set_pointer(value, g_list_copy_deep(self->metadata, glistGobjectDataDeepCopy, NULL));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_endpoint_finalize(GObject *object)
{
    BDeviceServiceEndpoint *self = B_DEVICE_SERVICE_ENDPOINT(object);

    g_free(self->id);
    g_free(self->uri);
    g_free(self->profile);
    g_free(self->deviceUuid);
    g_list_free_full(self->resources, g_object_unref);
    g_list_free_full(self->metadata, g_object_unref);

    G_OBJECT_CLASS(b_device_service_endpoint_parent_class)->finalize(object);
}

static void b_device_service_endpoint_class_init(BDeviceServiceEndpointClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = b_device_service_endpoint_set_property;
    object_class->get_property = b_device_service_endpoint_get_property;
    object_class->finalize = b_device_service_endpoint_finalize;

    properties[B_DEVICE_SERVICE_ENDPOINT_PROP_ID] =
        g_param_spec_string(B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_ID],
                            "ID",
                            "The ID of the endpoint",
                            NULL,
                            G_PARAM_READWRITE);

    properties[B_DEVICE_SERVICE_ENDPOINT_PROP_URI] =
        g_param_spec_string(B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_URI],
                            "URI",
                            "The URI of the endpoint",
                            NULL,
                            G_PARAM_READWRITE);

    properties[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE] =
        g_param_spec_string(B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE],
                            "Profile",
                            "The profile of the endpoint",
                            NULL,
                            G_PARAM_READWRITE);

    properties[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE_VERSION] =
        g_param_spec_uint(B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE_VERSION],
                          "Profile Version",
                          "The profile version of the endpoint",
                          0,
                          G_MAXUINT8,
                          0,
                          G_PARAM_READWRITE);

    properties[B_DEVICE_SERVICE_ENDPOINT_PROP_DEVICE_UUID] =
        g_param_spec_string(B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_DEVICE_UUID],
                            "Device UUID",
                            "The device UUID of the endpoint",
                            NULL,
                            G_PARAM_READWRITE);

    properties[B_DEVICE_SERVICE_ENDPOINT_PROP_ENABLED] =
        g_param_spec_boolean(B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_ENABLED],
                             "Enabled",
                             "The enabled status of the endpoint",
                             FALSE,
                             G_PARAM_READWRITE);

    /**
     * BDeviceServiceEndpoint:resources: (type GLib.List(BDeviceServiceResource))
     */
    properties[B_DEVICE_SERVICE_ENDPOINT_PROP_RESOURCES] =
        g_param_spec_pointer(B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_RESOURCES],
                             "Resources",
                             "The resources of the endpoint",
                             G_PARAM_READWRITE);

    /**
     * BDeviceServiceEndpoint:metadata: (type GLib.List(BDeviceServiceMetadata))
     */
    properties[B_DEVICE_SERVICE_ENDPOINT_PROP_METADATA] =
        g_param_spec_pointer(B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[B_DEVICE_SERVICE_ENDPOINT_PROP_METADATA],
                             "Metadata",
                             "The metadata of the endpoint",
                             G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_B_DEVICE_SERVICE_ENDPOINT_PROPERTIES, properties);
}

static void b_device_service_endpoint_init(BDeviceServiceEndpoint *self)
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

BDeviceServiceEndpoint *b_device_service_endpoint_new(void)
{
    return B_DEVICE_SERVICE_ENDPOINT(g_object_new(B_DEVICE_SERVICE_ENDPOINT_TYPE, NULL));
}
