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
 * Created by Christian Leithner on 7/1/2024.
 */

#include "events/device-service-device-configuration-event.h"
#include "events/device-service-event.h"

typedef struct _BDeviceServiceDeviceConfigurationEventPrivate
{
    gchar *uuid;
    gchar *deviceClass;
} BDeviceServiceDeviceConfigurationEventPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(BDeviceServiceDeviceConfigurationEvent,
                                    b_device_service_device_configuration_event,
                                    B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_TYPE);

static GParamSpec *properties[N_B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTIES];

static void b_device_service_device_configuration_event_get_property(GObject *object,
                                                                     guint property_id,
                                                                     GValue *value,
                                                                     GParamSpec *pspec)
{
    BDeviceServiceDeviceConfigurationEvent *self = B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT(object);
    BDeviceServiceDeviceConfigurationEventPrivate *priv =
        b_device_service_device_configuration_event_get_instance_private(self);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID:
            g_value_set_string(value, priv->uuid);
            break;
        case B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS:
            g_value_set_string(value, priv->deviceClass);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_device_configuration_event_set_property(GObject *object,
                                                                     guint property_id,
                                                                     const GValue *value,
                                                                     GParamSpec *pspec)
{
    BDeviceServiceDeviceConfigurationEvent *self = B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT(object);
    BDeviceServiceDeviceConfigurationEventPrivate *priv =
        b_device_service_device_configuration_event_get_instance_private(self);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID:
            g_free(priv->uuid);
            priv->uuid = g_value_dup_string(value);
            break;
        case B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS:
            g_free(priv->deviceClass);
            priv->deviceClass = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_device_configuration_event_finalize(GObject *object)
{
    BDeviceServiceDeviceConfigurationEvent *self = B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT(object);
    BDeviceServiceDeviceConfigurationEventPrivate *priv =
        b_device_service_device_configuration_event_get_instance_private(self);

    g_free(priv->uuid);
    g_free(priv->deviceClass);

    G_OBJECT_CLASS(b_device_service_device_configuration_event_parent_class)->finalize(object);
}

static void b_device_service_device_configuration_event_class_init(BDeviceServiceDeviceConfigurationEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = b_device_service_device_configuration_event_get_property;
    object_class->set_property = b_device_service_device_configuration_event_set_property;
    object_class->finalize = b_device_service_device_configuration_event_finalize;

    /**
     * BDeviceServiceDeviceConfigurationEvent:uuid:
     *
     * The UUID of the device.
     */
    properties[B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID] =
        g_param_spec_string(B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                                [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                            "UUID",
                            "The UUID of the device",
                            NULL,
                            G_PARAM_READWRITE);

    /**
     * BDeviceServiceDeviceConfigurationEvent:device-class:
     *
     * The device class of the device.
     */
    properties[B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS] =
        g_param_spec_string(B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                                [B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                            "Device class",
                            "The device class of the device",
                            NULL,
                            G_PARAM_READWRITE);

    g_object_class_install_properties(
        object_class, N_B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_PROPERTIES, properties);
}

static void b_device_service_device_configuration_event_init(BDeviceServiceDeviceConfigurationEvent *self)
{
    BDeviceServiceDeviceConfigurationEventPrivate *priv =
        b_device_service_device_configuration_event_get_instance_private(self);

    priv->uuid = NULL;
    priv->deviceClass = NULL;
}
