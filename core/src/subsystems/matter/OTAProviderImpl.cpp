/*
 *
 *    Copyright 2023 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 *
 *    Modified by Comcast to enhance OTA Provider support on top of the
 *    baseline implementation.
 */

#define LOG_TAG     "MatterOtaProvider"
#define logFmt(fmt) "(%s): " fmt, __func__

#include <glib-object.h>

extern "C" {
#include "devicePrivateProperties.h"
#include "deviceServiceConfiguration.h"
#include "provider/device-service-property-provider.h"
#include <commonDeviceDefs.h>
#include <device-driver/device-driver.h>
#include <device/icDevice.h>
#include <deviceDescriptor.h>
#include <deviceService.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <icLog/telemetryMarkers.h>
#include <icTypes/icLinkedList.h>
#include <icUtil/stringUtils.h>
#include <inttypes.h>
#include <jsonHelper/jsonHelper.h>
}

#include "Matter.h"
#include "MatterCommon.h"
#include "OTAProviderImpl.h"

#include <algorithm>
#include <app-common/zap-generated/cluster-objects.h>
#include <app/clusters/ota-provider/ota-provider-delegate.h>
#include <app/server/Server.h>
#include <app/util/af.h>
#include <credentials/FabricTable.h>
#include <crypto/RandUtils.h>
#include <lib/core/OTAImageHeader.h>
#include <lib/support/BufferWriter.h>
#include <lib/support/BytesToHex.h>
#include <lib/support/CodeUtils.h>

#include <fstream>
#include <string.h>
#include <sys/stat.h>

using namespace chip::app::Clusters;

namespace zilker
{
    bool OTAProviderImpl::SelectOTACandidate(const chip::NodeId requestorNodeId,
                                             const uint16_t requestorVendorID,
                                             const uint16_t requestorProductID,
                                             const uint32_t requestorSoftwareVersion,
                                             OTAProviderImpl::DeviceSoftwareVersionModel &finalCandidate)
    {
        icDebug();

        bool candidateFound = false;
        std::string deviceUuid = Subsystem::Matter::NodeIdToUuid(requestorNodeId);

        if (deviceUuid.empty())
        {
            return false;
        }

        scoped_icDevice *device = deviceServiceGetDevice(deviceUuid.c_str());
        scoped_DeviceDescriptor *dd = deviceServiceGetDeviceDescriptorForDevice(device);

        g_autoptr(BDeviceServicePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

        scoped_generic char *firmwareBaseUrl =
            b_device_service_property_provider_get_property_as_string(propertyProvider, DEVICE_FIRMWARE_URL_NODE, NULL);

        if (firmwareBaseUrl == nullptr)
        {
            icError("Device Firmware Base URL was empty, cannot select OTA Candidate");
        }

        if (dd == nullptr || dd->latestFirmware == nullptr || dd->latestFirmware->version == nullptr ||
            dd->matter == nullptr)
        {
            return false;
        }

        scoped_generic const char *firmwareVersion = stringBuilder("%s", dd->latestFirmware->version);
        icLinkedListIterator *iterator = linkedListIteratorCreate(dd->latestFirmware->fileInfos);

        while (linkedListIteratorHasNext(iterator))
        {
            DeviceFirmwareFileInfo *fileInfo = (DeviceFirmwareFileInfo *) linkedListIteratorGetNext(iterator);
            scoped_generic char *firmwareUrl = stringBuilder("%s/%s", firmwareBaseUrl, fileInfo->fileName);

            icDebug("firmwareVersion = %s: firmwareUrl = %s", firmwareVersion, firmwareUrl);

            uint64_t tmpFirmwareVersion;

            if (!stringToUnsignedNumberWithinRange(firmwareVersion, &tmpFirmwareVersion, 16, 0, UINT32_MAX))
            {
                icWarn("failed to convert firmwareVersion to uint32_t");
                continue;
            }

            if (dd->matter->vendorId != requestorVendorID || dd->matter->productId != requestorProductID)
            {
                continue;
            }

            if (tmpFirmwareVersion > finalCandidate.softwareVersion && tmpFirmwareVersion > requestorSoftwareVersion)
            {
                // We found a more recent candidate
                finalCandidate.softwareVersion = (uint32_t) tmpFirmwareVersion;
            }
            else
            {
                continue;
            }

            chip::Platform::CopyString(finalCandidate.softwareVersionString, firmwareVersion);
            chip::Platform::CopyString(finalCandidate.otaURL, firmwareUrl);

            // We found a candidate, but there could be others that could be more recent so don't break
            candidateFound = true;
        }

        return candidateFound;
    }

    void OTAProviderImpl::SendImageAvailable(chip::app::CommandHandler *commandObj,
                                             const chip::app::ConcreteCommandPath &commandPath,
                                             bool requestorCanConsent,
                                             const std::string &imageUri,
                                             uint32_t softwareVersion)
    {
        icDebug();

        OtaSoftwareUpdateProvider::Commands::QueryImageResponse::Type response;

        uint8_t updateToken[kUpdateTokenLen];
        chip::Crypto::DRBG_get_bytes(updateToken, kUpdateTokenLen);
        scoped_generic char *token = stringBin2hex(updateToken, kUpdateTokenLen);
        ChipLogDetail(SoftwareUpdate, "Generated updateToken: %s", token);

        response.status = OtaSoftwareUpdateProvider::OTAQueryStatus::kUpdateAvailable;
        response.updateToken.Emplace(chip::ByteSpan(updateToken));
        response.imageURI.Emplace(chip::CharSpan::fromCharString(imageUri.c_str()));
        response.softwareVersion.Emplace(softwareVersion);

        std::string versionStr = Matter::VersionToString(softwareVersion);
        response.softwareVersionString.Emplace(chip::CharSpan::fromCharString(versionStr.c_str()));
        response.updateToken.Emplace(chip::ByteSpan(updateToken));

        // TODO: Automatically choose what to delegate consent to. See Matter 1.0 11.19.3.4.
        response.userConsentNeeded.Emplace(requestorCanConsent);

        commandObj->AddResponse(commandPath, response);
    }

    void OTAProviderImpl::SendImageNotAvailable(chip::app::CommandHandler *commandObj,
                                                const chip::app::ConcreteCommandPath &commandPath)
    {
        OtaSoftwareUpdateProvider::Commands::QueryImageResponse::Type response;

        response.status = OtaSoftwareUpdateProvider::OTAQueryStatus::kNotAvailable;

        commandObj->AddResponse(commandPath, response);
    }

    void
    OTAProviderImpl::HandleQueryImage(chip::app::CommandHandler *commandObj,
                                      const chip::app::ConcreteCommandPath &commandPath,
                                      const OtaSoftwareUpdateProvider::Commands::QueryImage::DecodableType &commandData)
    {
        icDebug();

        chip::NodeId requestorNodeId = commandObj->GetSubjectDescriptor().subject;

        DeviceFirmwareType firmwareType = DEVICE_FIRMWARE_TYPE_UNKNOWN;

        OTAProviderImpl::DeviceSoftwareVersionModel candidate = {
            .vendorId = chip::VendorId::NotSpecified,
            .productId = 0xFFFF,
            .softwareVersion = 0,
            .softwareVersionValid = false,
        };

        // TODO: provide queryImageObj->supportedProtocols to selector. Data source(s) used in the selector
        // are assumed to offer appropriate URLs for the device querying for images.
        if (SelectOTACandidate(
                requestorNodeId, commandData.vendorID, commandData.productID, commandData.softwareVersion, candidate) &&
            candidate.softwareVersion != commandData.softwareVersion)
        {
            SendImageAvailable(commandObj,
                               commandPath,
                               commandData.requestorCanConsent.ValueOr(false),
                               candidate.otaURL,
                               candidate.softwareVersion);
        }
        else
        {
            // selector will log any specifics as to why (e.g., no matching protocol)
            SendImageNotAvailable(commandObj, commandPath);
        }
    }

    void OTAProviderImpl::HandleApplyUpdateRequest(
        chip::app::CommandHandler *commandObj,
        const chip::app::ConcreteCommandPath &commandPath,
        const OtaSoftwareUpdateProvider::Commands::ApplyUpdateRequest::DecodableType &commandData)
    {
        icDebug();

        scoped_generic char *token = stringBin2hex(commandData.updateToken.data(), commandData.updateToken.size());
        ChipLogDetail(SoftwareUpdate, "%s: token: %s, version: %#" PRIx32, __func__, token, commandData.newVersion);

        OtaSoftwareUpdateProvider::Commands::ApplyUpdateResponse::Type response;
        response.action = OtaSoftwareUpdateProvider::OTAApplyUpdateAction::kProceed;
        response.delayedActionTime = 0;

        commandObj->AddResponse(commandPath, response);
    }

    void OTAProviderImpl::HandleNotifyUpdateApplied(
        chip::app::CommandHandler *commandObj,
        const chip::app::ConcreteCommandPath &commandPath,
        const OtaSoftwareUpdateProvider::Commands::NotifyUpdateApplied::DecodableType &commandData)
    {
        icDebug();

        scoped_generic char *token = stringBin2hex(commandData.updateToken.data(), commandData.updateToken.size());
        ChipLogDetail(
            SoftwareUpdate, "%s: token: %s, version: %#" PRIx32, __func__, token, commandData.softwareVersion);

        commandObj->AddStatus(commandPath, chip::Protocols::InteractionModel::Status::Success);
    }
} // namespace zilker
