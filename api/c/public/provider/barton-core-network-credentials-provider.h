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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS


#define B_CORE_WIFI_NETWORK_CREDENTIALS_TYPE (b_core_wifi_network_credentials_get_type())

G_DECLARE_FINAL_TYPE(BCoreWifiNetworkCredentials,
                     b_core_wifi_network_credentials,
                     B_CORE,
                     WIFI_NETWORK_CREDENTIALS,
                     GObject)

typedef enum
{
    B_CORE_WIFI_NETWORK_CREDENTIALS_PROP_SSID = 1,
    B_CORE_WIFI_NETWORK_CREDENTIALS_PROP_PSK,
    N_B_CORE_WIFI_NETWORK_CREDENTIALS_PROPERTIES
} BCoreWifiNetworkCredentialsProperty;

static const char *B_CORE_WIFI_NETWORK_CREDENTIALS_PROPERTY_NAMES[] = {NULL, "ssid", "psk"};

#define B_CORE_NETWORK_CREDENTIALS_PROVIDER_TYPE (b_core_network_credentials_provider_get_type())

G_DECLARE_INTERFACE(BCoreNetworkCredentialsProvider,
                    b_core_network_credentials_provider,
                    B_CORE,
                    NETWORK_CREDENTIALS_PROVIDER,
                    GObject);

#define B_CORE_NETWORK_CREDENTIALS_PROVIDER_ERROR                                                            \
    (b_core_network_credentials_provider_error_quark())

typedef enum
{
    B_CORE_NETWORK_CREDENTIALS_PROVIDER_ERROR_GENERIC, // A catch-all error for clients to use.
    B_CORE_NETWORK_CREDENTIALS_PROVIDER_ERROR_ARG,     // An argument was invalid.
} BCoreNetworkCredentialsProviderError;

GQuark b_core_network_credentials_provider_error_quark(void);

/**
 * b_core_wifi_network_credentials_new
 *
 * Returns: (transfer full): BCoreWifiNetworkCredentials*
 */
BCoreWifiNetworkCredentials *b_core_wifi_network_credentials_new(void);

struct _BCoreNetworkCredentialsProviderInterface
{
    GTypeInterface parent_iface;

    BCoreWifiNetworkCredentials *(
        *get_wifi_network_credentials)(BCoreNetworkCredentialsProvider *self, GError **error);
};

/**
 * b_core_network_credentials_provider_get_wifi_network_credentials
 *
 * Returns: (transfer full): BCoreWifiNetworkCredentials*
 */
BCoreWifiNetworkCredentials *b_core_network_credentials_provider_get_wifi_network_credentials(
    BCoreNetworkCredentialsProvider *self,
    GError **error);

G_END_DECLS
