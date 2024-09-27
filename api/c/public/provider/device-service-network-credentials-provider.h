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
 * Created by Christian Leithner on 8/21/2024.
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS


#define B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_TYPE (b_device_service_wifi_network_credentials_get_type())

G_DECLARE_FINAL_TYPE(BDeviceServiceWifiNetworkCredentials,
                     b_device_service_wifi_network_credentials,
                     B_DEVICE_SERVICE,
                     WIFI_NETWORK_CREDENTIALS,
                     GObject)

typedef enum
{
    B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROP_SSID = 1,
    B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROP_PSK,
    N_B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROPERTIES
} BDeviceServiceWifiNetworkCredentialsProperty;

static const char *B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROPERTY_NAMES[] = {NULL, "ssid", "psk"};

#define B_DEVICE_SERVICE_NETWORK_CREDENTIALS_PROVIDER_TYPE (b_device_service_network_credentials_provider_get_type())

G_DECLARE_INTERFACE(BDeviceServiceNetworkCredentialsProvider,
                    b_device_service_network_credentials_provider,
                    B_DEVICE_SERVICE,
                    NETWORK_CREDENTIALS_PROVIDER,
                    GObject);

#define B_DEVICE_SERVICE_NETWORK_CREDENTIALS_PROVIDER_ERROR                                                            \
    (b_device_service_network_credentials_provider_error_quark())

typedef enum
{
    B_DEVICE_SERVICE_NETWORK_CREDENTIALS_PROVIDER_ERROR_GENERIC, // A catch-all error for clients to use.
    B_DEVICE_SERVICE_NETWORK_CREDENTIALS_PROVIDER_ERROR_ARG,     // An argument was invalid.
} BDeviceServiceNetworkCredentialsProviderError;

GQuark b_device_service_network_credentials_provider_error_quark(void);

/**
 * b_device_service_wifi_network_credentials_new
 *
 * Returns: (transfer full): BDeviceServiceWifiNetworkCredentials*
 */
BDeviceServiceWifiNetworkCredentials *b_device_service_wifi_network_credentials_new(void);

struct _BDeviceServiceNetworkCredentialsProviderInterface
{
    GTypeInterface parent_iface;

    BDeviceServiceWifiNetworkCredentials *(
        *get_wifi_network_credentials)(BDeviceServiceNetworkCredentialsProvider *self, GError **error);
};

/**
 * b_device_service_network_credentials_provider_get_wifi_network_credentials
 *
 * Returns: (transfer full): BDeviceServiceWifiNetworkCredentials*
 */
BDeviceServiceWifiNetworkCredentials *b_device_service_network_credentials_provider_get_wifi_network_credentials(
    BDeviceServiceNetworkCredentialsProvider *self,
    GError **error);

G_END_DECLS