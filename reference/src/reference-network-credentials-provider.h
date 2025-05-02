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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define B_REFERENCE_NETWORK_CREDENTIALS_PROVIDER_TYPE (b_reference_network_credentials_provider_get_type())
G_DECLARE_FINAL_TYPE(BReferenceNetworkCredentialsProvider,
                     b_reference_network_credentials_provider,
                     B_REFERENCE,
                     NETWORK_CREDENTIALS_PROVIDER,
                     GObject);

/**
 * b_reference_network_credentials_provider_new
 *
 * @brief
 *
 * Returns: (transfer full): BReferenceNetworkCredentialsProvider*
 */
BReferenceNetworkCredentialsProvider *b_reference_network_credentials_provider_new(void);

/**
 * b_reference_network_credentials_provider_set_wifi_network_credentials
 *
 * @brief
 *
 * Returns: None
 */
void b_reference_network_credentials_provider_set_wifi_network_credentials(const gchar *ssid, const gchar *password);

G_END_DECLS
