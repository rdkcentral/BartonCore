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
 * Created by Christian Leithner on 7/30/2024.
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_TOKEN_PROVIDER_TYPE (b_device_service_token_provider_get_type())
G_DECLARE_INTERFACE(BDeviceServiceTokenProvider,
                    b_device_service_token_provider,
                    B_DEVICE_SERVICE,
                    TOKEN_PROVIDER,
                    GObject);

typedef enum
{
    B_DEVICE_SERVICE_TOKEN_TYPE_XPKI_MATTER = 1,
} BDeviceServiceTokenType;

// This the old school way of creating GEnum types. We should be using G_DEFINE_ENUM_TYPE but that requires glib >=2.74
GType b_device_service_token_type_get_type(void);
#define B_DEVICE_SERVICE_TOKEN_TYPE_TYPE      (b_device_service_token_type_get_type())

#define B_DEVICE_SERVICE_TOKEN_PROVIDER_ERROR (b_device_service_token_provider_error_quark())

typedef enum
{
    B_DEVICE_SERVICE_TOKEN_PROVIDER_ERROR_GENERIC, // A catch-all error for clients to use.
    B_DEVICE_SERVICE_TOKEN_PROVIDER_ERROR_ARG,     // An argument was invalid.
} BDeviceServiceTokenProviderError;

GQuark b_device_service_token_provider_error_quark(void);

struct _BDeviceServiceTokenProviderInterface
{
    GTypeInterface parent_iface;

    gchar *(*get_token)(BDeviceServiceTokenProvider *self, BDeviceServiceTokenType token_type, GError **error);
};

gchar *b_device_service_token_provider_get_token(BDeviceServiceTokenProvider *self,
                                                 BDeviceServiceTokenType token_type,
                                                 GError **error);

G_END_DECLS
