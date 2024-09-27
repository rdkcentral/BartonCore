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

#include "device-service-metadata.h"
#include <inttypes.h>

struct _BDeviceServiceMetadata
{
    GObject parent_instance;

    char *id;
    char *uri;
    char *endpointId;
    char *deviceUuid;
    char *value;
};

G_DEFINE_TYPE(BDeviceServiceMetadata, b_device_service_metadata, G_TYPE_OBJECT)

static void
b_device_service_metadata_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    BDeviceServiceMetadata *metadata = B_DEVICE_SERVICE_METADATA(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_METADATA_PROP_ID:
            metadata->id = g_value_dup_string(value);
            break;

        case B_DEVICE_SERVICE_METADATA_PROP_URI:
            metadata->uri = g_value_dup_string(value);
            break;

        case B_DEVICE_SERVICE_METADATA_PROP_ENDPOINT_ID:
            metadata->endpointId = g_value_dup_string(value);
            break;

        case B_DEVICE_SERVICE_METADATA_PROP_DEVICE_UUID:
            metadata->deviceUuid = g_value_dup_string(value);
            break;

        case B_DEVICE_SERVICE_METADATA_PROP_VALUE:
            metadata->value = g_value_dup_string(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_metadata_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    BDeviceServiceMetadata *metadata = B_DEVICE_SERVICE_METADATA(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_METADATA_PROP_ID:
            g_value_set_string(value, metadata->id);
            break;

        case B_DEVICE_SERVICE_METADATA_PROP_URI:
            g_value_set_string(value, metadata->uri);
            break;

        case B_DEVICE_SERVICE_METADATA_PROP_ENDPOINT_ID:
            g_value_set_string(value, metadata->endpointId);
            break;

        case B_DEVICE_SERVICE_METADATA_PROP_DEVICE_UUID:
            g_value_set_string(value, metadata->deviceUuid);
            break;

        case B_DEVICE_SERVICE_METADATA_PROP_VALUE:
            g_value_set_string(value, metadata->value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_metadata_finalize(GObject *object)
{
    BDeviceServiceMetadata *metadata = B_DEVICE_SERVICE_METADATA(object);

    g_free(metadata->id);
    g_free(metadata->uri);
    g_free(metadata->endpointId);
    g_free(metadata->deviceUuid);
    g_free(metadata->value);

    G_OBJECT_CLASS(b_device_service_metadata_parent_class)->finalize(object);
}

static void b_device_service_metadata_init(BDeviceServiceMetadata *metadata)
{
    metadata->id = NULL;
    metadata->uri = NULL;
    metadata->endpointId = NULL;
    metadata->deviceUuid = NULL;
    metadata->value = NULL;
}

static void b_device_service_metadata_class_init(BDeviceServiceMetadataClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = b_device_service_metadata_set_property;
    object_class->get_property = b_device_service_metadata_get_property;
    object_class->finalize = b_device_service_metadata_finalize;

    g_object_class_install_property(
        object_class,
        B_DEVICE_SERVICE_METADATA_PROP_ID,
        g_param_spec_string(B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_ID],
                            "ID",
                            "The ID of the metadata",
                            NULL,
                            G_PARAM_READWRITE));

    g_object_class_install_property(
        object_class,
        B_DEVICE_SERVICE_METADATA_PROP_URI,
        g_param_spec_string(B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_URI],
                            "URI",
                            "The URI of the metadata",
                            NULL,
                            G_PARAM_READWRITE));

    g_object_class_install_property(
        object_class,
        B_DEVICE_SERVICE_METADATA_PROP_ENDPOINT_ID,
        g_param_spec_string(B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_ENDPOINT_ID],
                            "Endpoint ID",
                            "The endpoint ID of the metadata",
                            NULL,
                            G_PARAM_READWRITE));

    g_object_class_install_property(
        object_class,
        B_DEVICE_SERVICE_METADATA_PROP_DEVICE_UUID,
        g_param_spec_string(B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_DEVICE_UUID],
                            "Device UUID",
                            "The device UUID of the metadata",
                            NULL,
                            G_PARAM_READWRITE));

    g_object_class_install_property(
        object_class,
        B_DEVICE_SERVICE_METADATA_PROP_VALUE,
        g_param_spec_string(B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[B_DEVICE_SERVICE_METADATA_PROP_VALUE],
                            "Value",
                            "The value of the metadata",
                            NULL,
                            G_PARAM_READWRITE));
}

BDeviceServiceMetadata *b_device_service_metadata_new(void)
{
    return g_object_new(B_DEVICE_SERVICE_METADATA_TYPE, NULL);
}
