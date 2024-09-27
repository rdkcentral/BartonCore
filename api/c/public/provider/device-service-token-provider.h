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
#define B_DEVICE_SERVICE_TOKEN_TYPE_TYPE (b_device_service_token_type_get_type())

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