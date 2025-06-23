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

#include "provider/barton-core-token-provider.h"

G_DEFINE_INTERFACE(BCoreTokenProvider, b_core_token_provider, G_TYPE_OBJECT)
G_DEFINE_QUARK(b - device - service - token - provider - error - quark, b_core_token_provider_error)

static void b_core_token_provider_default_init(BCoreTokenProviderInterface *iface)
{
    iface->get_token = NULL;
}

gchar *b_core_token_provider_get_token(BCoreTokenProvider *self,
                                                 BCoreTokenType token_type,
                                                 GError **error)
{
    g_return_val_if_fail(B_CORE_IS_TOKEN_PROVIDER(self), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    BCoreTokenProviderInterface *iface = B_CORE_TOKEN_PROVIDER_GET_IFACE(self);

    g_return_val_if_fail(iface != NULL, NULL);
    g_return_val_if_fail(iface->get_token != NULL, NULL);

    GError *tmp_error = NULL;
    g_autofree gchar *token = iface->get_token(self, token_type, &tmp_error);

    if (tmp_error && error)
    {
        g_propagate_error(error, tmp_error);
    }

    return g_steal_pointer(&token);
}

GType b_core_token_type_get_type(void)
{
    static gsize g_define_type_id__volatile = 0;

    // Initializes the BCoreTokenType enum GType only once.
    // Note: Some tools might match the code below with public code, but this just the standard use of this glib API.
    if (g_once_init_enter(&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            {B_CORE_TOKEN_TYPE_XPKI_MATTER, "B_CORE_TOKEN_TYPE_XPKI_MATTER", "xpki-matter"},
            {                                      0,                                      NULL,          NULL}
        };

        GType g_define_type_id = g_enum_register_static(g_intern_static_string("BCoreTokenType"), values);
        g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}
