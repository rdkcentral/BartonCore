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
 * Created by Christian Leithner on 7/30/2024.
 */

#include "provider/device-service-token-provider.h"

G_DEFINE_INTERFACE(BDeviceServiceTokenProvider, b_device_service_token_provider, G_TYPE_OBJECT)
G_DEFINE_QUARK(b-device-service-token-provider-error-quark, b_device_service_token_provider_error)

static void b_device_service_token_provider_default_init(BDeviceServiceTokenProviderInterface *iface)
{
    iface->get_token = NULL;
}

gchar *b_device_service_token_provider_get_token(BDeviceServiceTokenProvider *self,
                                                 BDeviceServiceTokenType token_type,
                                                 GError **error)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_TOKEN_PROVIDER(self), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    BDeviceServiceTokenProviderInterface *iface = B_DEVICE_SERVICE_TOKEN_PROVIDER_GET_IFACE(self);

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

GType b_device_service_token_type_get_type(void)
{
    static gsize g_define_type_id__volatile = 0;

    // Initializes the BDeviceServiceTokenType enum GType only once.
    if (g_once_init_enter(&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            {B_DEVICE_SERVICE_TOKEN_TYPE_XPKI_MATTER, "B_DEVICE_SERVICE_TOKEN_TYPE_XPKI_MATTER", "xpki-matter"},
            {                                      0,                                      NULL,          NULL}
        };

        GType g_define_type_id = g_enum_register_static(g_intern_static_string("BDeviceServiceTokenType"), values);
        g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}