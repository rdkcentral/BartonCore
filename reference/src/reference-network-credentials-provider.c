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
 * Created by Thomas Lea on 10/11/2024.
 */

#include "provider/device-service-network-credentials-provider.h"
#include "reference-network-credentials-provider.h"
#include <icConcurrent/threadUtils.h>

struct _BReferenceNetworkCredentialsProvider
{
    GObject parent_instance;
};

static pthread_mutex_t mtx = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
static gchar *network_ssid = NULL;
static gchar *network_psk = NULL;

static void
b_reference_network_credentials_provider_interface_init(BDeviceServiceNetworkCredentialsProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE(BReferenceNetworkCredentialsProvider,
                        b_reference_network_credentials_provider,
                        G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(B_DEVICE_SERVICE_NETWORK_CREDENTIALS_PROVIDER_TYPE,
                                              b_reference_network_credentials_provider_interface_init))

/*
 * Implementation of BDeviceServiceNetworkCredentialsProvider get_wifi_network_credentials
 */
static BDeviceServiceWifiNetworkCredentials *
b_reference_network_credentials_provider_get_wifi_network_credentials(
    BDeviceServiceNetworkCredentialsProvider *self,
    GError **error)
{
    g_return_val_if_fail(B_REFERENCE_IS_NETWORK_CREDENTIALS_PROVIDER(self), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    g_autoptr(BDeviceServiceWifiNetworkCredentials) wifiCredentials = NULL;

    wifiCredentials = b_device_service_wifi_network_credentials_new();

    mutexLock(&mtx);
    if (network_ssid != NULL && network_psk != NULL)
    {
        g_object_set(wifiCredentials,
                     B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROPERTY_NAMES
                         [B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROP_SSID],
                     network_ssid,
                     B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROPERTY_NAMES
                         [B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROP_PSK],
                     network_psk,
                     NULL);
    }
    mutexUnlock(&mtx);

    return g_steal_pointer(&wifiCredentials);
}

static void
b_reference_network_credentials_provider_interface_init(BDeviceServiceNetworkCredentialsProviderInterface *iface)
{
    iface->get_wifi_network_credentials =
        b_reference_network_credentials_provider_get_wifi_network_credentials;
}

static void b_reference_network_credentials_provider_init(BReferenceNetworkCredentialsProvider *self)
{
    // No instance initialization needed
}

static void
b_reference_network_credentials_provider_class_init(BReferenceNetworkCredentialsProviderClass *klass)
{
    // No class initialization needed
}

void b_reference_network_credentials_provider_set_wifi_network_credentials(const gchar *ssid, const gchar *password)
{
    g_return_if_fail(ssid != NULL);
    g_return_if_fail(password != NULL);

    mutexLock(&mtx);
    g_free(network_ssid);
    g_free(network_psk);

    network_ssid = g_strdup(ssid);
    network_psk = g_strdup(password);
    mutexUnlock(&mtx);
}

BReferenceNetworkCredentialsProvider *b_reference_network_credentials_provider_new(void)
{
    return B_REFERENCE_NETWORK_CREDENTIALS_PROVIDER(
        g_object_new(B_REFERENCE_NETWORK_CREDENTIALS_PROVIDER_TYPE, NULL));
}
