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
// Created by nkhan599 on 3/27/24.
//

#pragma once

#include "MatterCluster.h"
#include "subsystems/matter/MatterCommon.h"
#include "app-common/zap-generated/ids/Attributes.h"
#include "app-common/zap-generated/ids/Clusters.h"
#include "app/ClusterStateCache.h"
#include "app/ConcreteAttributePath.h"
#include "app/ReadHandler.h"
#include "lib/core/CHIPError.h"
#include "lib/core/DataModelTypes.h"

namespace zilker
{
    using namespace chip::app::Clusters::GeneralDiagnostics;

    class GeneralDiagnostics : public MatterCluster
    {
    public:
        GeneralDiagnostics(EventHandler *handler, const std::string deviceId, chip::EndpointId endpointId) :
        MatterCluster(handler, deviceId, endpointId)
        {
        }

        class EventHandler : public MatterCluster::EventHandler
        {
        public:
            virtual void OnNetworkInterfacesChanged(GeneralDiagnostics &source, NetworkUtils::NetworkInterfaceInfo info) {};
        };

        SubscriptionIntervalSecs GetDesiredSubscriptionIntervalSecs() override
        {
            return {1, kSubscriptionMaxIntervalPublisherLimit};
        }

        void OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                const chip::app::ConcreteAttributePath &path) override;
        std::string GetMacAddress();
        std::string GetNetworkType();

    private:
        chip::Optional<NetworkUtils::NetworkInterfaceInfo> GetInterfaceInfo(chip::app::ClusterStateCache *cache);
    };
} // namespace zilker
