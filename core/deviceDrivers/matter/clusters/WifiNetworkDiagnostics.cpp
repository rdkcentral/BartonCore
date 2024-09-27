//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2024 Comcast Corporation
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

//
// Created by tlea on 8/9/2023
//

#define LOG_TAG     "WifiNetworkDiagnosticsCluster"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "glib-2.0/glib.h"

extern "C" {
#include "deviceServiceConfiguration.h"
#include "deviceServiceProps.h"
#include "provider/device-service-property-provider.h"
#include <icLog/logging.h>
}

#include "WifiNetworkDiagnostics.h"
#include "app-common/zap-generated/ids/Attributes.h"
#include "app-common/zap-generated/ids/Clusters.h"
#include "app/BufferedReadCallback.h"
#include "app/InteractionModelEngine.h"

namespace zilker
{
    SubscriptionIntervalSecs WifiNetworkDiagnostics::GetDesiredSubscriptionIntervalSecs()
    {
        g_autoptr(BDeviceServicePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
        uint16_t minIntervalFloorSeconds = b_device_service_property_provider_get_property_as_uint16(
            propertyProvider, DEVICE_PROP_MATTER_WIFI_DIAGNOSTICS_RSSI_MIN_INTERVAL_SECS, 60);
        uint16_t maxIntervalCeilingSeconds = b_device_service_property_provider_get_property_as_uint16(
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
                static_cast<WifiNetworkDiagnostics::EventHandler *>(eventHandler)->RssiChanged(deviceId, nullableValue, nullptr);
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

        chip::app::ConcreteAttributePath path(
            endpointId,
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
} // namespace zilker
