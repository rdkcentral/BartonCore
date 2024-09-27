//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2024 Comcast
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
// Comcast retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*
 * Created by Christian Leithner on 6/5/2024.
 */

#include "device-service-discovery-filter.h"

struct _BDeviceServiceDiscoveryFilter
{
    GObject parent_instance;

    gchar *uri;
    gchar *value;
};

G_DEFINE_TYPE(BDeviceServiceDiscoveryFilter, b_device_service_discovery_filter, G_TYPE_OBJECT);

static GParamSpec *properties[N_B_DEVICE_SERVICE_DISCOVERY_FILTER_PROPERTIES];

static void b_device_service_discovery_filter_set_property(GObject *object,
                                                           guint property_id,
                                                           const GValue *value,
                                                           GParamSpec *pspec)
{
    BDeviceServiceDiscoveryFilter *self = B_DEVICE_SERVICE_DISCOVERY_FILTER(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_DISCOVERY_FILTER_PROP_URI:
            g_free(self->uri);
            self->uri = g_value_dup_string(value);
            break;
        case B_DEVICE_SERVICE_DISCOVERY_FILTER_PROP_VALUE:
            g_free(self->value);
            self->value = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
b_device_service_discovery_filter_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    BDeviceServiceDiscoveryFilter *self = B_DEVICE_SERVICE_DISCOVERY_FILTER(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_DISCOVERY_FILTER_PROP_URI:
            g_value_set_string(value, self->uri);
            break;
        case B_DEVICE_SERVICE_DISCOVERY_FILTER_PROP_VALUE:
            g_value_set_string(value, self->value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_discovery_filter_finalize(GObject *object)
{
    BDeviceServiceDiscoveryFilter *self = B_DEVICE_SERVICE_DISCOVERY_FILTER(object);

    g_free(self->uri);
    g_free(self->value);

    G_OBJECT_CLASS(b_device_service_discovery_filter_parent_class)->finalize(object);
}

static void b_device_service_discovery_filter_class_init(BDeviceServiceDiscoveryFilterClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = b_device_service_discovery_filter_set_property;
    object_class->get_property = b_device_service_discovery_filter_get_property;
    object_class->finalize = b_device_service_discovery_filter_finalize;

    /**
     * BDeviceServiceDiscoveryFilter:uri: (type utf8)
     *
     * Regex string to match against the URI of the device service resource.
     */
    properties[B_DEVICE_SERVICE_DISCOVERY_FILTER_PROP_URI] = g_param_spec_string(
        B_DEVICE_SERVICE_DISCOVERY_FILTER_PROPERTY_NAMES[B_DEVICE_SERVICE_DISCOVERY_FILTER_PROP_URI],
        "URI",
        "Regex string to match against the URI of the device service resource.",
        NULL,
        G_PARAM_READWRITE);

    /**
     * BDeviceServiceDiscoveryFilter:value: (type utf8)
     *
     * Regex string to match against the value of the device service resource.
     */
    properties[B_DEVICE_SERVICE_DISCOVERY_FILTER_PROP_VALUE] = g_param_spec_string(
        B_DEVICE_SERVICE_DISCOVERY_FILTER_PROPERTY_NAMES[B_DEVICE_SERVICE_DISCOVERY_FILTER_PROP_VALUE],
        "Value",
        "Regex string to match against the value of the device service resource.",
        NULL,
        G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_B_DEVICE_SERVICE_DISCOVERY_FILTER_PROPERTIES, properties);
}

static void b_device_service_discovery_filter_init(BDeviceServiceDiscoveryFilter *self)
{
    self->uri = NULL;
    self->value = NULL;
}

BDeviceServiceDiscoveryFilter *b_device_service_discovery_filter_new(void)
{
    return B_DEVICE_SERVICE_DISCOVERY_FILTER(g_object_new(B_DEVICE_SERVICE_DISCOVERY_FILTER_TYPE, NULL));
}