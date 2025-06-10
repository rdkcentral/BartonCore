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

//
// Created by tlea on 8/9/2023
//

#define LOG_TAG     "WifiNetworkDiagnosticsCluster"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "glib-2.0/glib.h"

extern "C" {
#include "deviceServiceConfiguration.h"
#include "deviceServiceProps.h"
#include "provider/barton-core-property-provider.h"
#include <icLog/logging.h>
}

#include "WifiNetworkDiagnostics.h"
#include "app-common/zap-generated/ids/Attributes.h"
#include "app-common/zap-generated/ids/Clusters.h"
#include "app/BufferedReadCallback.h"
#include "app/InteractionModelEngine.h"

namespace barton
{
    SubscriptionIntervalSecs WifiNetworkDiagnostics::GetDesiredSubscriptionIntervalSecs()
    {
        g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
        uint16_t minIntervalFloorSeconds = b_core_property_provider_get_property_as_uint16(
            propertyProvider, DEVICE_PROP_MATTER_WIFI_DIAGNOSTICS_RSSI_MIN_INTERVAL_SECS, 60);
        uint16_t maxIntervalCeilingSeconds = b_core_property_provider_get_property_as_uint16(
            propertyProvider, DEVICE_PROP_MATTER_WIFI_DIAGNOSTICS_RSSI_MAX_INTERVAL_SECS, 60 * 5);

        SubscriptionIntervalSecs intervalSecs(minIntervalFloorSeconds, maxIntervalCeilingSeconds);
        return intervalSecs;
    }

    void WifiNetworkDiagnostics::OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                                    const chip::app::ConcreteAttributePath &path)
    {
        using namespace chip::app::Clusters::WiFiNetworkDiagnostics;
        using TypeInfo = Attributes::Rssi::TypeInfo;

        if (path.mClusterId == chip::app::Clusters::WiFiNetworkDiagnostics::Id &&
            path.mAttributeId == chip::app::Clusters::WiFiNetworkDiagnostics::Attributes::Rssi::Id)
        {
            TypeInfo::DecodableType value;
            CHIP_ERROR error = cache->Get<TypeInfo>(path, value);
            if (error == CHIP_NO_ERROR)
            {
                int8_t *nullableValue = value.IsNull() ? nullptr : &value.Value();
                static_cast<WifiNetworkDiagnostics::EventHandler *>(eventHandler)
                    ->RssiChanged(deviceId, nullableValue, nullptr);
            }
            else
            {
                icError("Failed to decode rssi attribute: %s", error.AsString());
            }
        }
        else
        {
            icTrace("Unexpected cluster or attribute");
        }
    }

    bool WifiNetworkDiagnostics::GetRssi(void *context,
                                         const chip::Messaging::ExchangeManager &exchangeMgr,
                                         const chip::SessionHandle &sessionHandle)
    {
        icDebug();

        bool result = false;
        std::shared_ptr<chip::app::ClusterStateCache> cache;
        if (!(cache = clusterStateCacheRef.lock()))
        {
            icError("Failed to get rssi attribute because the cluster state cache expired or was never set");
            return result;
        }

        CHIP_ERROR error = CHIP_NO_ERROR;
        using namespace chip::app::Clusters::WiFiNetworkDiagnostics;
        using TypeInfo = Attributes::Rssi::TypeInfo;

        chip::app::ConcreteAttributePath path(endpointId,
                                              chip::app::Clusters::WiFiNetworkDiagnostics::Id,
                                              chip::app::Clusters::WiFiNetworkDiagnostics::Attributes::Rssi::Id);
        {
            TypeInfo::DecodableType value;
            error = cache->Get<TypeInfo>(path, value);
            if (error == CHIP_NO_ERROR)
            {
                int8_t *nullableValue = value.IsNull() ? nullptr : &value.Value();
                static_cast<WifiNetworkDiagnostics::EventHandler *>(eventHandler)
                    ->RssiReadComplete(deviceId, nullableValue, context);
                result = true;
            }
        }

        if (error != CHIP_NO_ERROR)
        {
            icError("Failed to decode rssi attribute: %s", error.AsString());
        }

        return result;
    };
} // namespace barton
