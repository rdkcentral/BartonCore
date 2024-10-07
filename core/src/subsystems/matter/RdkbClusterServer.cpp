//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2023 Comcast Corporation
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
// Comcast Corporation retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*
 * Created by Thomas Lea on 10/27/23.
 */

#include <glib-object.h>

extern "C" {
#include "deviceServiceConfiguration.h"
#include "icUtil/stringUtils.h"
#include "jsonHelper/jsonHelper.h"
#include "provider/device-service-network-credentials-provider.h"
#include <icLog/logging.h>
}

#include "RdkbClusterServer.h"
#include "app-common/zap-generated/ids/Attributes.h"
#include <cstring>

#define LOG_TAG     "RdkbClusterServer"
#define logFmt(fmt) "%s: " fmt, __func__


chip::Protocols::InteractionModel::Status RdkbClusterServer::OnAttributeRead(const EmberAfAttributeMetadata *attributeMetadata,
                                                                             uint8_t *buffer,
                                                                             uint16_t maxReadLength)
{
    chip::Protocols::InteractionModel::Status result = chip::Protocols::InteractionModel::Status::UnsupportedAttribute;

    switch (attributeMetadata->attributeId)
    {
        case chip::app::Clusters::RDKBGateway::Attributes::WifiNetworks::Id:
        {
            g_autoptr(GError) error = NULL;
            g_autoptr(BDeviceServiceNetworkCredentialsProvider) provider =
                deviceServiceConfigurationGetNetworkCredentialsProvider();
            g_autoptr(BDeviceServiceWifiNetworkCredentials) wifiCredentials =
                b_device_service_network_credentials_provider_get_wifi_network_credentials(provider, &error);

            if (error == NULL && wifiCredentials != NULL)
            {
                g_autofree gchar *ssid = NULL;
                g_autofree gchar *psk = NULL;
                g_object_get(wifiCredentials,
                             B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROPERTY_NAMES
                                 [B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROP_SSID],
                             &ssid,
                             B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROPERTY_NAMES
                                 [B_DEVICE_SERVICE_WIFI_NETWORK_CREDENTIALS_PROP_PSK],
                             &psk,
                             NULL);

                scoped_cJSON *json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "ssid", ssid);
                cJSON_AddStringToObject(json, "pass", psk);

                scoped_generic char *jsonStr = cJSON_PrintUnformatted(json);

                // first put the 2 byte string length prefix in
                uint16_t jsonStrLen = strlen(jsonStr);
                if (maxReadLength >= jsonStrLen + sizeof(jsonStrLen))
                {
                    memcpy(buffer, &jsonStrLen, sizeof(jsonStrLen));
                    memcpy(buffer + sizeof(jsonStrLen), jsonStr, jsonStrLen);
                    result = chip::Protocols::InteractionModel::Status::Success;
                }
                else
                {
                    result = chip::Protocols::InteractionModel::Status::Failure;
                }
            }
            else
            {
                icError("Failed to get WiFi credentials: [%d] %s", error->code, stringCoalesce(error->message));
                result = chip::Protocols::InteractionModel::Status::Failure;
            }
            break;
        }

        default:
            break;
    }

    return result;
}
