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
 * Created by Christian Leithner on 7/1/2024.
 */

#include "events/barton-core-device-configuration-event.h"
#include "events/barton-core-event.h"

typedef struct _BCoreDeviceConfigurationEventPrivate
{
    gchar *uuid;
    gchar *deviceClass;
} BCoreDeviceConfigurationEventPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(BCoreDeviceConfigurationEvent,
                                    b_core_device_configuration_event,
                                    B_CORE_DISCOVERY_SESSION_INFO_EVENT_TYPE);

static GParamSpec *properties[N_B_CORE_DEVICE_CONFIGURATION_EVENT_PROPERTIES];

static void b_core_device_configuration_event_get_property(GObject *object,
                                                                     guint property_id,
                                                                     GValue *value,
                                                                     GParamSpec *pspec)
{
    BCoreDeviceConfigurationEvent *self = B_CORE_DEVICE_CONFIGURATION_EVENT(object);
    BCoreDeviceConfigurationEventPrivate *priv =
        b_core_device_configuration_event_get_instance_private(self);

    switch (property_id)
    {
        case B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_UUID:
            g_value_set_string(value, priv->uuid);
            break;
        case B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS:
            g_value_set_string(value, priv->deviceClass);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_device_configuration_event_set_property(GObject *object,
                                                                     guint property_id,
                                                                     const GValue *value,
                                                                     GParamSpec *pspec)
{
    BCoreDeviceConfigurationEvent *self = B_CORE_DEVICE_CONFIGURATION_EVENT(object);
    BCoreDeviceConfigurationEventPrivate *priv =
        b_core_device_configuration_event_get_instance_private(self);

    switch (property_id)
    {
        case B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_UUID:
            g_free(priv->uuid);
            priv->uuid = g_value_dup_string(value);
            break;
        case B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS:
            g_free(priv->deviceClass);
            priv->deviceClass = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_device_configuration_event_finalize(GObject *object)
{
    BCoreDeviceConfigurationEvent *self = B_CORE_DEVICE_CONFIGURATION_EVENT(object);
    BCoreDeviceConfigurationEventPrivate *priv =
        b_core_device_configuration_event_get_instance_private(self);

    g_free(priv->uuid);
    g_free(priv->deviceClass);

    G_OBJECT_CLASS(b_core_device_configuration_event_parent_class)->finalize(object);
}

static void b_core_device_configuration_event_class_init(BCoreDeviceConfigurationEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = b_core_device_configuration_event_get_property;
    object_class->set_property = b_core_device_configuration_event_set_property;
    object_class->finalize = b_core_device_configuration_event_finalize;

    /**
     * BCoreDeviceConfigurationEvent:uuid:
     *
     * The UUID of the device.
     */
    properties[B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_UUID] =
        g_param_spec_string(B_CORE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                                [B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_UUID],
                            "UUID",
                            "The UUID of the device",
                            NULL,
                            G_PARAM_READWRITE);

    /**
     * BCoreDeviceConfigurationEvent:device-class:
     *
     * The device class of the device.
     */
    properties[B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS] =
        g_param_spec_string(B_CORE_DEVICE_CONFIGURATION_EVENT_PROPERTY_NAMES
                                [B_CORE_DEVICE_CONFIGURATION_EVENT_PROP_DEVICE_CLASS],
                            "Device class",
                            "The device class of the device",
                            NULL,
                            G_PARAM_READWRITE);

    g_object_class_install_properties(
        object_class, N_B_CORE_DEVICE_CONFIGURATION_EVENT_PROPERTIES, properties);
}

static void b_core_device_configuration_event_init(BCoreDeviceConfigurationEvent *self)
{
    BCoreDeviceConfigurationEventPrivate *priv =
        b_core_device_configuration_event_get_instance_private(self);

    priv->uuid = NULL;
    priv->deviceClass = NULL;
}
