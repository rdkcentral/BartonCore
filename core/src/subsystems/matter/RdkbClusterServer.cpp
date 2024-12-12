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
