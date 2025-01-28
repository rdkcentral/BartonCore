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
 * Code adapted from: https://github.com/project-chip/connectedhomeip
 * Modified by Comcast to enhance OTA Provider support on top of the baseline implementation.
 * Copyright 2022 Project CHIP Authors
 * All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 */

#pragma once

#include <app-common/zap-generated/cluster-objects.h>
#include <app/CommandHandler.h>
#include <app/clusters/ota-provider/OTAProviderUserConsentDelegate.h>
#include <app/clusters/ota-provider/ota-provider-delegate.h>
#include <lib/core/OTAImageHeader.h>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

extern "C" {
#include <deviceDescriptor.h>
}

// must be between 8 and 32 as per Matter R1.0 11.19.6.12
constexpr uint8_t kUpdateTokenLen = 32;

using namespace chip::app::Clusters;

namespace barton
{
    /**
     * Implementation of OTA Provider with Device Descriptor Support. This integrates directly with the
     * MatterDeviceDriver class callbacks.
     */
    class OTAProviderImpl : public chip::app::Clusters::OTAProviderDelegate
    {
    public:
        OTAProviderImpl() = default;

        using OTAQueryStatus = chip::app::Clusters::OtaSoftwareUpdateProvider::OTAQueryStatus;
        using OTAApplyUpdateAction = chip::app::Clusters::OtaSoftwareUpdateProvider::OTAApplyUpdateAction;


        /* Refer to the spec: (Matter R1.0 11.22.6) for why these values were chosen */
        static constexpr uint16_t SW_VER_STR_MAX_LEN = 64;
        static constexpr size_t kFilepathBufLen = 256;
        static constexpr size_t kUriMaxLen = 256;
        static constexpr size_t kFirmwareInformationMaxLen = 512;
        static constexpr size_t kChecksumMaxLen = 64;
        static constexpr size_t kReleaseNotesUrlMaxLen = 256;

        static constexpr const char kHttpsScheme[] = "https://";
        static constexpr size_t kHttpsSchemeLen = sizeof(kHttpsScheme) - 1;

        /* Refer: (Matter R1.0 11.19.6.7) */
        static constexpr size_t kMetadataForProviderMaxLen = 512;

        /* Refer: (Matter R1.0 11.19.6.8) */
        static constexpr size_t kMetadataForRequestorMaxLen = 512;

        //////////// OTAProviderDelegate Implementation ///////////////
        void HandleQueryImage(chip::app::CommandHandler *commandObj,
                              const chip::app::ConcreteCommandPath &commandPath,
                              const chip::app::Clusters::OtaSoftwareUpdateProvider::Commands::QueryImage::DecodableType
                                  &commandData) override;

        void HandleApplyUpdateRequest(
            chip::app::CommandHandler *commandObj,
            const chip::app::ConcreteCommandPath &commandPath,
            const chip::app::Clusters::OtaSoftwareUpdateProvider::Commands::ApplyUpdateRequest::DecodableType
                &commandData) override;

        void HandleNotifyUpdateApplied(
            chip::app::CommandHandler *commandObj,
            const chip::app::ConcreteCommandPath &commandPath,
            const chip::app::Clusters::OtaSoftwareUpdateProvider::Commands::NotifyUpdateApplied::DecodableType
                &commandData) override;

    private:
        /* Refer: (Matter R1.0 11.22.6) for the schema */
        typedef struct DeviceSoftwareVersionModel
        {
            chip::VendorId vendorId;
            uint16_t productId;
            uint32_t softwareVersion;
            char softwareVersionString[SW_VER_STR_MAX_LEN];
            uint16_t cDVersionNumber;
            char firmwareInformation[kFirmwareInformationMaxLen];
            bool softwareVersionValid;
            char otaURL[kUriMaxLen];
            uint64_t otaFileSize;
            char otaChecksum[kChecksumMaxLen];
            OTAImageDigestType otaChecksumType;
            uint32_t minApplicableSoftwareVersion;
            uint32_t maxApplicableSoftwareVersion;
            char releaseNotesUrl[kReleaseNotesUrlMaxLen];
        } DeviceSoftwareVersionModel;

        bool SelectOTACandidate(const chip::NodeId requestorNodeId,
                                const uint16_t requestorVendorID,
                                const uint16_t requestorProductID,
                                const uint32_t requestorSoftwareVersion,
                                OTAProviderImpl::DeviceSoftwareVersionModel &finalCandidate);

        chip::ota::UserConsentSubject GetUserConsentSubject(
            const chip::app::CommandHandler *commandObj,
            const chip::app::ConcreteCommandPath &commandPath,
            const chip::app::Clusters::OtaSoftwareUpdateProvider::Commands::QueryImage::DecodableType &commandData,
            uint32_t targetVersion);

        bool ParseOTAHeader(chip::OTAImageHeaderParser &parser, const char *otaFilePath, chip::OTAImageHeader &header);

        void SendImageAvailable(chip::app::CommandHandler *commandObj,
                                const chip::app::ConcreteCommandPath &commandPath,
                                bool requestorCanConsent,
                                const std::string &imageUri,
                                uint32_t softwareVersion);

        void SendImageNotAvailable(chip::app::CommandHandler *commandObj,
                                   const chip::app::ConcreteCommandPath &commandPath);
    };
} // namespace barton
