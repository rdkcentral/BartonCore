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

#define G_LOG_DOMAIN "BasicInformation"
#define LOG_TAG      G_LOG_DOMAIN
#define logFmt(fmt)  fmt

extern "C" {
#include "icLog/logging.h"
}

#include "BasicInformation.hpp"
#include "app-common/zap-generated/ids/Attributes.h"
#include "app-common/zap-generated/ids/Clusters.h"
#include "app/ClusterStateCache.h"
#include "app/ConcreteAttributePath.h"
#include "app/ReadHandler.h"
#include "glib.h"
#include "lib/core/CHIPError.h"
#include "lib/core/DataModelTypes.h"

namespace zilker
{
    std::string BasicInformation::GetFirmwareVersionString()
    {
        auto cache = clusterStateCacheRef.lock();

        if (cache == nullptr)
        {
            icDebug("Attribute cache not available");
            return "";
        }

        using TypeInfo = Attributes::SoftwareVersionString::TypeInfo;

        TypeInfo::DecodableType value;
        CHIP_ERROR error =
            cache->Get<TypeInfo>({chip::kRootEndpointId, Id, Attributes::SoftwareVersionString::Id}, value);

        if (error != CHIP_NO_ERROR)
        {
            icWarn(
                "Failed to read SoftwareVersionString attribute for device %s: %s", deviceId.c_str(), error.AsString());

            return "";
        }

        return {value.data(), value.size()};
    }

    void BasicInformation::OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                              const chip::app::ConcreteAttributePath &path)
    {
        if (path.mEndpointId != chip::kRootEndpointId || path.mClusterId != Id)
        {
            return;
        }

        switch (path.mAttributeId)
        {
            case Attributes::SoftwareVersion::Id:
            {
                using TypeInfo = Attributes::SoftwareVersion::TypeInfo;

                TypeInfo::DecodableType value;
                CHIP_ERROR error = cache->Get<TypeInfo>(path, value);

                if (error == CHIP_NO_ERROR)
                {
                    static_cast<BasicInformation::EventHandler *>(eventHandler)->OnSoftwareVersionChanged(*this, value);
                }
                else
                {
                    icWarn(
                        "Failed to read SoftwareVersion report from device %s: %s", deviceId.c_str(), error.AsString());
                }

                break;
            }

            case Attributes::SoftwareVersionString::Id:
            {
                using TypeInfo = Attributes::SoftwareVersionString::TypeInfo;

                TypeInfo::DecodableType value;
                CHIP_ERROR error = cache->Get<TypeInfo>(path, value);

                if (error == CHIP_NO_ERROR)
                {
                    static_cast<BasicInformation::EventHandler *>(eventHandler)
                        ->OnSoftwareVersionStringChanged(*this, {value.data(), value.size()});
                }
                else
                {
                    icWarn("Failed to read SoftwareVersionString report from device %s: %s",
                           deviceId.c_str(),
                           error.AsString());
                }

                break;
            }

            case Attributes::SerialNumber::Id:
            {
                using TypeInfo = Attributes::SerialNumber::TypeInfo;

                TypeInfo::DecodableType value;
                CHIP_ERROR error = cache->Get<TypeInfo>(path, value);

                if (error == CHIP_NO_ERROR)
                {
                    static_cast<BasicInformation::EventHandler *>(eventHandler)
                        ->OnSerialNumberChanged(*this, {value.data(), value.size()});
                }
                else
                {
                    icWarn("Failed to read SerialNumber report from device %s: %s", deviceId.c_str(), error.AsString());
                }

                break;
            }

            default:
                // Not interested
                break;
        }
    }
} // namespace zilker