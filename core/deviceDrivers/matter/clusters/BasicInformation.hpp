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

#pragma once

#include "MatterCluster.h"
#include "app-common/zap-generated/ids/Attributes.h"
#include "app-common/zap-generated/ids/Clusters.h"
#include "app/ClusterStateCache.h"
#include "app/ConcreteAttributePath.h"
#include "app/ReadHandler.h"
#include "lib/core/CHIPError.h"
#include "lib/core/DataModelTypes.h"

namespace zilker
{
    using namespace chip::app::Clusters::BasicInformation;
    class BasicInformation : public MatterCluster
    {
    public:
        BasicInformation(EventHandler *handler, const std::string deviceId, chip::EndpointId endpointId) :
            MatterCluster(handler, deviceId, endpointId)
        {
        }

        class EventHandler : public MatterCluster::EventHandler
        {
        public:
            virtual void OnSoftwareVersionChanged(BasicInformation &source, uint32_t softwareVersion) {};
            virtual void OnSoftwareVersionStringChanged(BasicInformation &source,
                                                        const std::string &softwareVersionString) {};
            virtual void OnSerialNumberChanged(BasicInformation &source, const std::string &serialNumber) {};
        };

        SubscriptionIntervalSecs GetDesiredSubscriptionIntervalSecs() override
        {
            return {1, kSubscriptionMaxIntervalPublisherLimit};
        }

        std::string GetFirmwareVersionString();

        void OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                const chip::app::ConcreteAttributePath &path) override;
    };
} // namespace zilker