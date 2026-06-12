//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2025 Comcast Cable Communications Management, LLC
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
// Created by tlea on 12/9/25
//

#define LOG_TAG "MatterDevice"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "app/WriteClient.h"
#include <algorithm>
#include <clusters/shared/GlobalIds.h>
#include <platform/CHIPDeviceLayer.h>

#include "MatterDevice.h"

extern "C" {
#include <device/icDeviceResource.h>
#include <icLog/logging.h>
}

#include <private/deviceServicePrivate.h>

namespace barton
{

MatterDevice::MatterDevice(std::string deviceId, std::shared_ptr<DeviceDataCache> deviceDataCache)
    : deviceId(std::move(deviceId)), deviceDataCache(std::move(deviceDataCache))
{
    // Create and register the callback
    cacheCallback = std::make_unique<CacheCallback>(this);
    if (this->deviceDataCache)
    {
        this->deviceDataCache->SetCallback(std::move(cacheCallback));
    }
}

MatterDevice::~MatterDevice()
{
    icDebug("Destroying MatterDevice %s", deviceId.c_str());
    if (deviceDataCache)
    {
        deviceDataCache->SetCallback(nullptr);
    }
}

void MatterDevice::CacheCallback::OnReportBegin()
{
    icDebug("OnReportBegin for device %s", device->deviceId.c_str());
    // TODO: Handle report begin
}

void MatterDevice::CacheCallback::OnReportEnd()
{
    icDebug("OnReportEnd for device %s", device->deviceId.c_str());
    // TODO: Handle report end
}

void MatterDevice::CacheCallback::OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                                     const chip::app::ConcreteAttributePath &aPath)
{
    icDebug("OnAttributeChanged for device %s, endpoint %u, cluster 0x%x, attribute 0x%x",
            device->deviceId.c_str(),
            aPath.mEndpointId,
            aPath.mClusterId,
            aPath.mAttributeId);

    if (device->attributeCallback)
    {
        if (cache == nullptr)
        {
            icError("Null cache pointer for device %s", device->deviceId.c_str());
            return;
        }

        chip::TLV::TLVReader reader;

        if (cache->Get(aPath, reader) != CHIP_NO_ERROR)
        {
            icError("Failed to get attribute data from cache for dispatch, device %s",
                    device->deviceId.c_str());
            return;
        }

        device->attributeCallback(device->deviceId, aPath.mEndpointId, aPath.mClusterId, aPath.mAttributeId, reader);
    }
}

void MatterDevice::CacheCallback::OnEventData(const chip::app::EventHeader &aEventHeader,
                                              chip::TLV::TLVReader *apData,
                                              const chip::app::StatusIB *apStatus)
{
    if (apStatus != nullptr)
    {
        icDebug("OnEventData status for device %s: 0x%x",
                device->deviceId.c_str(),
                static_cast<unsigned>(apStatus->mStatus));
        return;
    }

    if (apData == nullptr)
    {
        icDebug("OnEventData with null data for device %s", device->deviceId.c_str());
        return;
    }

    icDebug("OnEventData for device %s, endpoint %u, cluster 0x%x, event 0x%x",
            device->deviceId.c_str(),
            aEventHeader.mPath.mEndpointId,
            aEventHeader.mPath.mClusterId,
            aEventHeader.mPath.mEventId);

    // Event handling is performed by the driver's dispatch system via attributeCallback
}

bool MatterDevice::GetEndpointForCluster(chip::ClusterId clusterId, chip::EndpointId &outEndpointId)
{
    if (!deviceDataCache)
    {
        icDebug("No device data cache set for device %s", deviceId.c_str());
        return false;
    }

    for (auto &entry : deviceDataCache->GetEndpointIds())
    {
        if (deviceDataCache->EndpointHasServerCluster(entry, clusterId))
        {
            outEndpointId = entry;
            return true;
        }
    }

    return false;
}

bool MatterDevice::ResolveEndpointForCluster(chip::ClusterId clusterId,
                                             std::optional<uint32_t> sbmdEndpointIndex,
                                             chip::EndpointId &outEndpointId)
{
    if (!deviceDataCache)
    {
        icError("No device data cache for device %s", deviceId.c_str());
        return false;
    }

    if (sbmdEndpointIndex.has_value())
    {
        if (!GetEndpointForSbmdIndex(sbmdEndpointIndex.value(), outEndpointId))
        {
            return false;
        }

        // The endpoint map provides a default Matter endpoint for this SBMD
        // index, but the SBMD endpoint's resources may reference clusters
        // hosted on other Matter endpoints. Check whether the mapped endpoint
        // hosts the requested cluster; if not, scan all endpoints.
        if (deviceDataCache->EndpointHasServerCluster(outEndpointId, clusterId))
        {
            return true;
        }

        // Mapped endpoint doesn't host this cluster — scan all endpoints.
        icDebug("SBMD endpoint %u (Matter EP %u) does not host cluster 0x%x, "
                "falling back to cluster-based lookup",
                sbmdEndpointIndex.value(),
                outEndpointId,
                clusterId);
    }

    return GetEndpointForCluster(clusterId, outEndpointId);
}

bool MatterDevice::ResolveEndpointMap(const std::vector<uint16_t> &driverSupportedDeviceTypes)
{
    if (!deviceDataCache)
    {
        icError("No device data cache for device %s", deviceId.c_str());
        return false;
    }

    if (driverSupportedDeviceTypes.empty())
    {
        icError("No supported device types provided for device %s", deviceId.c_str());
        return false;
    }

    sbmdEndpointMap.clear();
    uint32_t sbmdIndex = 0;

    for (auto endpointId : deviceDataCache->GetEndpointIds())
    {
        // Skip the root/aggregator endpoint
        if (endpointId == 0)
        {
            continue;
        }

        std::vector<uint16_t> endpointDeviceTypes;
        if (deviceDataCache->GetDeviceTypes(endpointId, endpointDeviceTypes) != CHIP_NO_ERROR)
        {
            icDebug("Failed to get device types for endpoint %u on device %s", endpointId, deviceId.c_str());
            continue;
        }

        // Check if any device type on this endpoint matches our supported types
        if (std::find_first_of(endpointDeviceTypes.begin(),
                               endpointDeviceTypes.end(),
                               driverSupportedDeviceTypes.begin(),
                               driverSupportedDeviceTypes.end()) != endpointDeviceTypes.end())
        {
            sbmdEndpointMap[sbmdIndex] = endpointId;
            icDebug("Mapped SBMD endpoint index %u → Matter endpoint %u for device %s",
                    sbmdIndex,
                    endpointId,
                    deviceId.c_str());
            sbmdIndex++;
        }
    }

    if (sbmdEndpointMap.empty())
    {
        icError("No matching device type endpoints found for device %s", deviceId.c_str());
        return false;
    }

    icDebug("Resolved %zu endpoint(s) for device %s", sbmdEndpointMap.size(), deviceId.c_str());
    return true;
}

bool MatterDevice::GetEndpointForSbmdIndex(uint32_t sbmdIndex, chip::EndpointId &outEndpointId) const
{
    auto it = sbmdEndpointMap.find(sbmdIndex);
    if (it == sbmdEndpointMap.end())
    {
        return false;
    }

    outEndpointId = it->second;
    return true;
}

bool MatterDevice::GetClusterFeatureMap(chip::EndpointId endpointId, chip::ClusterId clusterId, uint32_t &featureMap)
{
    chip::TLV::TLVReader reader;
    chip::app::ConcreteDataAttributePath featureMapPath(
        endpointId, clusterId, chip::app::Clusters::Globals::Attributes::FeatureMap::Id);

    if (deviceDataCache->GetAttributeData(featureMapPath, reader) != CHIP_NO_ERROR)
    {
        return false;
    }

    if (chip::app::DataModel::Decode(reader, featureMap) != CHIP_NO_ERROR)
    {
        return false;
    }

    icDebug("Got featureMap 0x%x for cluster 0x%x on endpoint %u", featureMap, clusterId, endpointId);
    return true;
}

void MatterDevice::UpdateCachedFeatureMaps()
{
    std::map<uint32_t, uint32_t> clusterFeatureMaps;

    for (uint32_t clusterId : featureClusters)
    {
        chip::EndpointId chipEndpointId;

        if (GetEndpointForCluster(clusterId, chipEndpointId))
        {
            uint32_t featureMap = 0;

            if (GetClusterFeatureMap(chipEndpointId, clusterId, featureMap))
            {
                clusterFeatureMaps[clusterId] = featureMap;
                icDebug(
                    "Cached featureMap 0x%x for cluster 0x%x on endpoint %u", featureMap, clusterId, chipEndpointId);
            }
        }
    }

    cachedClusterFeatureMaps = std::move(clusterFeatureMaps);
    icDebug("Updated cached feature maps for device %s (%zu clusters)", deviceId.c_str(), cachedClusterFeatureMaps.size());
}


bool MatterDevice::SendCommandFromTlv(std::forward_list<std::promise<bool>> &promises,
                                      chip::ClusterId clusterId,
                                      chip::CommandId commandId,
                                      std::optional<uint16_t> timedInvokeTimeoutMs,
                                      chip::EndpointId endpointId,
                                      const uint8_t *tlvBuffer,
                                      size_t encodedLength,
                                      chip::Messaging::ExchangeManager &exchangeMgr,
                                      const chip::SessionHandle &sessionHandle,
                                      const char *uri,
                                      char **response)
{
    // Empty TLV structure (STRUCT start: 0x15, END_CONTAINER: 0x18) for commands with no arguments
    static const uint8_t emptyTlvStruct[] = {0x15, 0x18};
    static const size_t emptyTlvStructLen = sizeof(emptyTlvStruct);

    // Use empty TLV structure if no buffer provided (common for no-arg commands)
    if (tlvBuffer == nullptr || encodedLength == 0)
    {
        tlvBuffer = emptyTlvStruct;
        encodedLength = emptyTlvStructLen;
        icDebug("No TLV buffer provided for command at URI: %s, using empty struct", uri);
    }

    // Create TLV reader from the encoded data
    chip::TLV::TLVReader reader;
    reader.Init(tlvBuffer, encodedLength);

    if (reader.Next() != CHIP_NO_ERROR || reader.GetType() != chip::TLV::kTLVType_Structure)
    {
        icError("Invalid TLV structure for command at URI: %s", uri);
        return false;
    }

    bool isTimedRequest = timedInvokeTimeoutMs.has_value();
    auto commandSender = std::make_unique<chip::app::CommandSender>(this, &exchangeMgr, isTimedRequest);

    if (!commandSender)
    {
        icError("Failed to create CommandSender for URI: %s", uri);
        return false;
    }

    chip::app::CommandSender::PrepareCommandParameters prepareParams;
    prepareParams.SetStartDataStruct(true);

    chip::app::CommandPathParams commandPath(endpointId,
                                             0, /* group not used */
                                             clusterId,
                                             commandId,
                                             chip::app::CommandPathFlags::kEndpointIdValid);

    CHIP_ERROR err = commandSender->PrepareCommand(commandPath, prepareParams);

    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to prepare command for URI: %s, error: %s", uri, err.AsString());
        return false;
    }

    chip::TLV::TLVWriter *writer = commandSender->GetCommandDataIBTLVWriter();

    if (writer == nullptr)
    {
        icError("Failed to get TLV writer for command at URI: %s", uri);
        return false;
    }

    chip::TLV::TLVType containerType;
    err = reader.EnterContainer(containerType);

    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to enter TLV container for URI: %s, error: %s", uri, err.AsString());
        return false;
    }

    while ((err = reader.Next()) == CHIP_NO_ERROR)
    {
        err = writer->CopyElement(reader);

        if (err != CHIP_NO_ERROR)
        {
            icError("Failed to copy command element for URI: %s, error: %s", uri, err.AsString());
            return false;
        }
    }

    if (err != CHIP_END_OF_TLV)
    {
        icError("Error iterating TLV elements for URI: %s, error: %s", uri, err.AsString());
        return false;
    }

    chip::app::CommandSender::FinishCommandParameters finishParams(
        isTimedRequest ? chip::MakeOptional(timedInvokeTimeoutMs.value()) : chip::NullOptional);
    finishParams.SetEndDataStruct(true);
    err = commandSender->FinishCommand(finishParams);

    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to finish command for URI: %s, error: %s", uri, err.AsString());
        return false;
    }

    promises.emplace_front();
    auto &commandPromise = promises.front();

    err = commandSender->SendCommandRequest(sessionHandle);

    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to send command request for URI: %s, error: %s", uri, err.AsString());
        commandPromise.set_value(false);
        return false;
    }

    icDebug("Successfully initiated command for URI: %s", uri);

    CommandContext context;
    context.commandPromise = &commandPromise;
    context.commandSender = std::move(commandSender);
    context.response = response;
    auto *commandSenderPtr = context.commandSender.get();
    activeCommandContexts[commandSenderPtr] = std::move(context);

    return true;
}

bool MatterDevice::WriteAttributeFromTlv(std::forward_list<std::promise<bool>> &promises,
                                         chip::EndpointId endpointId,
                                         chip::ClusterId clusterId,
                                         chip::AttributeId attributeId,
                                         const uint8_t *tlvBuffer,
                                         size_t encodedLength,
                                         chip::Messaging::ExchangeManager &exchangeMgr,
                                         const chip::SessionHandle &sessionHandle,
                                         const char *uri)
{
    if (tlvBuffer == nullptr || encodedLength == 0)
    {
        icError("Empty TLV buffer for attribute write at URI: %s", uri);
        return false;
    }

    chip::TLV::TLVReader reader;
    reader.Init(tlvBuffer, encodedLength);

    if (reader.Next() != CHIP_NO_ERROR)
    {
        icError("Empty or invalid TLV from write for URI: %s", uri);
        return false;
    }

    chip::app::ConcreteAttributePath attrPath(endpointId, clusterId, attributeId);

    auto writeClient =
        std::make_unique<chip::app::WriteClient>(const_cast<chip::Messaging::ExchangeManager *>(&exchangeMgr),
                                                 this,
                                                 chip::Optional<uint16_t>::Missing());

    if (!writeClient)
    {
        icError("Failed to create WriteClient for URI: %s", uri);
        return false;
    }

    CHIP_ERROR err = writeClient->PutPreencodedAttribute(attrPath, reader);

    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to encode preencoded attribute for URI: %s, error: %s", uri, err.AsString());
        return false;
    }

    promises.emplace_front();
    auto &writePromise = promises.front();

    err = writeClient->SendWriteRequest(sessionHandle);

    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to send write request for URI: %s, error: %s", uri, err.AsString());
        writePromise.set_value(false);
        return false;
    }

    icDebug("Successfully initiated attribute write for resource %s", uri);

    WriteContext context;
    context.writePromise = &writePromise;
    context.writeClient = std::move(writeClient);
    activeWriteContexts[context.writeClient.get()] = std::move(context);

    return true;
}

void MatterDevice::CacheCallback::OnSubscriptionEstablished(chip::SubscriptionId aSubscriptionId)
{
    icDebug("OnSubscriptionEstablished for device %s, subscription ID: %u",
               device->deviceId.c_str(), aSubscriptionId);

    // Now that subscription is established, compute and cache feature maps for scripts
    device->UpdateCachedFeatureMaps();
}

void MatterDevice::CacheCallback::OnError(CHIP_ERROR aError)
{
    icError("OnError for device %s: %s", device->deviceId.c_str(), aError.AsString());
}

void MatterDevice::CacheCallback::OnDeallocatePaths(chip::app::ReadPrepareParams &&aReadPrepareParams)
{
    icDebug("OnDeallocatePaths for device %s", device->deviceId.c_str());
    // Clean up paths
    if (aReadPrepareParams.mpAttributePathParamsList != nullptr)
    {
        delete aReadPrepareParams.mpAttributePathParamsList;
    }
    if (aReadPrepareParams.mpEventPathParamsList != nullptr)
    {
        delete aReadPrepareParams.mpEventPathParamsList;
    }
    if (aReadPrepareParams.mpDataVersionFilterList != nullptr)
    {
        delete[] aReadPrepareParams.mpDataVersionFilterList;
    }
}

void MatterDevice::CacheCallback::OnDone(chip::app::ReadClient *apReadClient)
{
    icDebug("OnDone for device %s", device->deviceId.c_str());
}

void MatterDevice::OnResponse(const chip::app::WriteClient * apWriteClient, const chip::app::ConcreteDataAttributePath & aPath,
                                chip::app::StatusIB attributeStatus)
{
    icDebug("OnResponse for device %s, endpoint %u, cluster 0x%x, attribute 0x%x, status 0x%x",
               deviceId.c_str(), aPath.mEndpointId, aPath.mClusterId, aPath.mAttributeId, static_cast<unsigned int>(attributeStatus.mStatus));

    // Check if this is a success or failure
    auto it = activeWriteContexts.find(const_cast<chip::app::WriteClient *>(apWriteClient));
    if (it != activeWriteContexts.end())
    {
        if (!attributeStatus.IsSuccess())
        {
            icError("Write failed with status 0x%x", static_cast<unsigned int>(attributeStatus.mStatus));
            it->second.success = false;
        }
    }
}

void MatterDevice::OnError(const chip::app::WriteClient * apWriteClient, CHIP_ERROR aError)
{
    icError("OnError for device %s: %s", deviceId.c_str(), aError.AsString());

    // Find the context for this write client
    auto it = activeWriteContexts.find(const_cast<chip::app::WriteClient *>(apWriteClient));
    if (it != activeWriteContexts.end())
    {
        // Signal failure
        it->second.writePromise->set_value(false);
        // Context will be cleaned up in OnDone
    }
}

void MatterDevice::OnDone(chip::app::WriteClient * apWriteClient)
{
    icDebug("OnDone for device %s", deviceId.c_str());

    // Find and clean up the context for this write client
    auto it = activeWriteContexts.find(apWriteClient);
    if (it != activeWriteContexts.end())
    {
        // If we haven't already signaled the promise (via OnError), signal the result now
        // Check if the promise is still valid by trying to get its future state
        try
        {
            // If set_value hasn't been called, this will succeed
            it->second.writePromise->set_value(it->second.success);
        }
        catch (const std::future_error &e)
        {
            // Promise was already satisfied (by OnError), which is fine
            icDebug("Promise already satisfied for write operation");
        }

        // Remove the context, which will destroy the WriteClient
        activeWriteContexts.erase(it);
    }
}

// CommandSender::ExtendableCallback implementations
void MatterDevice::OnResponse(chip::app::CommandSender *apCommandSender,
                              const chip::app::CommandSender::ResponseData &aResponseData)
{
    icDebug("OnResponse for command from device %s, status=0x%x, hasData=%s",
            deviceId.c_str(),
            static_cast<unsigned int>(aResponseData.statusIB.mStatus),
            aResponseData.data != nullptr ? "yes" : "no");

    auto it = activeCommandContexts.find(apCommandSender);

    if (it == activeCommandContexts.end())
    {
        icError("Received command response for unknown CommandSender");
        return;
    }

    CommandContext &context = it->second;

    if (!aResponseData.statusIB.IsSuccess())
    {
        icError("Command failed with status 0x%x for device %s",
                static_cast<unsigned int>(aResponseData.statusIB.mStatus),
                deviceId.c_str());

        try
        {
            context.commandPromise->set_value(false);
        }
        catch (const std::future_error &e)
        {
            icDebug("Promise already satisfied for command operation");
        }

        return;
    }

    // Command succeeded — response data processing is handled by the driver
}

void MatterDevice::OnError(const chip::app::CommandSender *apCommandSender,
                           const chip::app::CommandSender::ErrorData &aErrorData)
{
    icError("OnError for command from device %s: error=%s", deviceId.c_str(), aErrorData.error.AsString());

    auto it = activeCommandContexts.find(const_cast<chip::app::CommandSender *>(apCommandSender));
    if (it != activeCommandContexts.end())
    {
        // Signal failure
        try
        {
            it->second.commandPromise->set_value(false);
        }
        catch (const std::future_error &e)
        {
            icDebug("Promise already satisfied for command operation");
        }
    }
}

void MatterDevice::OnDone(chip::app::CommandSender *apCommandSender)
{
    icDebug("OnDone for command from device %s", deviceId.c_str());

    auto it = activeCommandContexts.find(apCommandSender);
    if (it != activeCommandContexts.end())
    {
        // If we haven't already signaled the promise (via OnError), signal success now
        try
        {
            it->second.commandPromise->set_value(true);
        }
        catch (const std::future_error &e)
        {
            // Promise was already satisfied (by OnError), which is fine
            icDebug("Promise already satisfied for command operation");
        }

        // Remove the context, which will destroy the CommandSender
        activeCommandContexts.erase(it);
    }
}

} // namespace barton
