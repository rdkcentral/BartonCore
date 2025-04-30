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
 * Created by Christian Leithner on 8/21/2024.
 */

#include "provider/device-service-network-credentials-provider.h"

struct _BDeviceServiceWifiNetworkCredentials
{
    GObject parent_instance;
    gchar *ssid;
    gchar *psk;
};

G_DEFINE_TYPE(BDeviceServiceWifiNetworkCredentials, b_device_service_wifi_network_credentials, G_TYPE_OBJECT)

static GParamSpec *properties[N_B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROPERTIES];

static void b_device_service_wifi_network_credentials_finalize(GObject *object)
{
    BDeviceServiceWifiNetworkCredentials *self = B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS(object);

    g_clear_pointer(&self->ssid, g_free);
    g_clear_pointer(&self->psk, g_free);

    G_OBJECT_CLASS(b_device_service_wifi_network_credentials_parent_class)->finalize(object);
}

static void b_device_service_wifi_network_credentials_set_property(GObject *object,
                                                                   guint property_id,
                                                                   const GValue *value,
                                                                   GParamSpec *pspec)
{
    BDeviceServiceWifiNetworkCredentials *self = B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROP_SSID:
            g_clear_pointer(&self->ssid, g_free);
            self->ssid = g_value_dup_string(value);
            break;
        case B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROP_PSK:
            g_clear_pointer(&self->psk, g_free);
            self->psk = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_wifi_network_credentials_get_property(GObject *object,
                                                                   guint property_id,
                                                                   GValue *value,
                                                                   GParamSpec *pspec)
{
    BDeviceServiceWifiNetworkCredentials *self = B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROP_SSID:
            g_value_set_string(value, self->ssid);
            break;
        case B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROP_PSK:
            g_value_set_string(value, self->psk);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_wifi_network_credentials_class_init(BDeviceServiceWifiNetworkCredentialsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_device_service_wifi_network_credentials_finalize;
    object_class->set_property = b_device_service_wifi_network_credentials_set_property;
    object_class->get_property = b_device_service_wifi_network_credentials_get_property;

    properties[B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROP_SSID] = g_param_spec_string(
        B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROPERTY_NAMES[B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROP_SSID],
        "SSID",
        "The SSID of the WiFi network",
        NULL,
        G_PARAM_READWRITE);

    properties[B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROP_PSK] = g_param_spec_string(
        B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROPERTY_NAMES[B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROP_PSK],
        "PSK",
        "The PSK of the WiFi network",
        NULL,
        G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROPERTIES, properties);
}

static void b_device_service_wifi_network_credentials_init(BDeviceServiceWifiNetworkCredentials *self)
{
    self->ssid = NULL;
    self->psk = NULL;
}

BDeviceServiceWifiNetworkCredentials *b_device_service_wifi_network_credentials_new(void)
{
    return B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS(
        g_object_new(B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_TYPE, NULL));
}

G_DEFINE_INTERFACE(BDeviceServiceNetworkCredentialsProvider,
                   b_device_service_network_credentials_provider,
                   G_TYPE_OBJECT)
G_DEFINE_QUARK(b - device - service - network - credentials - provider - error - quark,
               b_device_service_network_credentials_provider_error)

static void
b_device_service_network_credentials_provider_default_init(BDeviceServiceNetworkCredentialsProviderInterface *iface)
{
    iface->get_wifi_network_credentials = NULL;
}

BDeviceServiceWifiNetworkCredentials *b_device_service_network_credentials_provider_get_wifi_network_credentials(
    BDeviceServiceNetworkCredentialsProvider *self,
    GError **error)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_NETWORK_CREDENTIALS_PROVIDER(self), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    BDeviceServiceNetworkCredentialsProviderInterface *iface =
        B_DEVICE_SERVICE_NETWORK_CREDENTIALS_PROVIDER_GET_IFACE(self);

    g_return_val_if_fail(iface != NULL, NULL);
    g_return_val_if_fail(iface->get_wifi_network_credentials != NULL, NULL);

    GError *tmp_error = NULL;
    g_autoptr(BDeviceServiceWifiNetworkCredentials) wifi_network_credentials =
        iface->get_wifi_network_credentials(self, &tmp_error);

    if (tmp_error && error)
    {
        g_propagate_error(error, tmp_error);
    }

    return g_steal_pointer(&wifi_network_credentials);
}
