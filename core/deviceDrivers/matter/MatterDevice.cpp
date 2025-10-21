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

#include "app/ConcreteCommandPath.h"
#include "app/WriteClient.h"
#include <platform/CHIPDeviceLayer.h>

#include "MatterDevice.h"

extern "C" {
#include <device/icDeviceResource.h>
#include <icLog/logging.h>
}

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

    // The callback is automatically unregistered when SetCallback is called with nullptr
    // or when the DeviceDataCache is destroyed
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

void MatterDevice::CacheCallback::OnAttributeData(const chip::app::ConcreteDataAttributePath &aPath,
                                                   chip::TLV::TLVReader *apData,
                                                   const chip::app::StatusIB &aStatus)
{
    icDebug("OnAttributeData for device %s, endpoint %u, cluster 0x%x, attribute 0x%x",
               device->deviceId.c_str(), aPath.mEndpointId, aPath.mClusterId, aPath.mAttributeId);
}

bool MatterDevice::GetEndpointForCluster(chip::ClusterId clusterId, chip::EndpointId &outEndpointId)
{
    auto deviceCache = GetDeviceDataCache();
    if (deviceCache == nullptr)
    {
        icError("No device cache for %s", deviceId.c_str());
        return false;
    }

    for (auto &entry : deviceCache->GetEndpointIds())
    {
        if (deviceCache->EndpointHasServerCluster(entry, clusterId))
        {
            outEndpointId = entry;
            return true;
        }
    }

    return false;
}

bool MatterDevice::BindResourceInfo(const char *uri,
                                    const std::optional<SbmdAttribute> &attribute,
                                    const std::optional<SbmdCommand> &command,
                                    const char *operationType,
                                    std::map<std::string, ResourceBinding> &bindings)
{
    if (uri == nullptr)
    {
        icError("URI is null");
        return false;
    }

    // Validate: must have exactly one of attribute or command
    if ((!attribute.has_value() && !command.has_value()) || (attribute.has_value() && command.has_value()))
    {
        icError("Must have either attribute or command, but not both");
        return false;
    }

    ResourceBinding binding;
    chip::EndpointId endpointId;

    if (attribute.has_value())
    {
        // Bind attribute
        if (!GetEndpointForCluster(attribute->clusterId, endpointId))
        {
            icError("Failed to find endpoint for cluster 0x%x", attribute->clusterId);
            return false;
        }

        binding.isAttribute = true;
        binding.attributePath.mEndpointId = endpointId;
        binding.attributePath.mClusterId = attribute->clusterId;
        binding.attributePath.mAttributeId = attribute->attributeId;
        binding.attribute = attribute;

        icDebug("Bound resource %s for URI: %s (endpoint: %u, cluster: 0x%x, attribute: 0x%x)",
                operationType,
                uri,
                endpointId,
                attribute->clusterId,
                attribute->attributeId);
    }
    else
    {
        // Bind command
        if (!GetEndpointForCluster(command->clusterId, endpointId))
        {
            icError("Failed to find endpoint for cluster 0x%x", command->clusterId);
            return false;
        }

        binding.isAttribute = false;
        binding.commandPath.mEndpointId = endpointId;
        binding.commandPath.mClusterId = command->clusterId;
        binding.commandPath.mCommandId = command->commandId;
        binding.command = command;

        icDebug("Bound resource %s for URI: %s (endpoint: %u, cluster: 0x%x, command: 0x%x)",
                operationType,
                uri,
                endpointId,
                command->clusterId,
                command->commandId);
    }

    bindings[uri] = binding;
    return true;
}

bool MatterDevice::BindResourceReadInfo(const char *uri, const SbmdMapper &mapper)
{
    return BindResourceInfo(uri, mapper.readAttribute, mapper.readCommand, "read", resourceReadBindings);
}

bool MatterDevice::BindResourceWriteInfo(const char *uri, const SbmdMapper &mapper)
{
    return BindResourceInfo(uri, mapper.writeAttribute, mapper.writeCommand, "write", resourceWriteBindings);
}

bool MatterDevice::BindResourceExecuteInfo(const char *uri, const SbmdMapper &mapper)
{
    return BindResourceInfo(uri, mapper.executeAttribute, mapper.executeCommand, "execute", resourceExecuteBindings);
}

void MatterDevice::HandleResourceRead(std::forward_list<std::promise<bool>> &promises,
                                      icDeviceResource *resource,
                                      char **value,
                                      chip::Messaging::ExchangeManager &exchangeMgr,
                                      const chip::SessionHandle &sessionHandle)
{
    if (resource == nullptr || resource->uri == nullptr)
    {
        icError("Resource or URI is null");
        FailOperation(promises);
        return;
    }

    // Look up the binding
    auto it = resourceReadBindings.find(resource->uri);
    if (it == resourceReadBindings.end())
    {
        icError("No read binding found for URI: %s", resource->uri);
        FailOperation(promises);
        return;
    }

    const ResourceBinding &binding = it->second;

    std::string outValue;

    if (binding.isAttribute)
    {
        // Get the attribute data from the cache
        chip::TLV::TLVReader reader;
        CHIP_ERROR err = GetCachedAttributeData(binding.attributePath.mEndpointId,
                                                binding.attributePath.mClusterId,
                                                binding.attributePath.mAttributeId,
                                                reader);

        if (err != CHIP_NO_ERROR)
        {
            icError("Failed to get cached attribute data for URI: %s, error: %s", resource->uri, err.AsString());
            FailOperation(promises);
            return;
        }

        // Check if we have a script engine
        if (!script)
        {
            icError("No script engine available for device %s", deviceId.c_str());
            FailOperation(promises);
            return;
        }

        // Execute the script to map the TLV data to a string value using the stored mapper
        if (!script->MapAttributeRead(binding.attribute.value(), reader, outValue))
        {
            icError("Failed to execute read mapping script for URI: %s", resource->uri);
            FailOperation(promises);
            return;
        }
    }
    else
    {
        // Reading from a command is not yet implemented
        icError("Reading from command for URI: %s is not yet implemented", resource->uri);
        FailOperation(promises);
        return;
    }

    icDebug("Successfully read resource %s = %s", resource->uri, outValue.c_str());
    *value = strdup(outValue.c_str());
}

void MatterDevice::HandleResourceWrite(std::forward_list<std::promise<bool>> &promises,
                                       icDeviceResource *resource,
                                       const char *previousValue,
                                       const char *newValue,
                                       chip::Messaging::ExchangeManager &exchangeMgr,
                                       const chip::SessionHandle &sessionHandle)
{
    if (resource == nullptr || resource->uri == nullptr)
    {
        icError("Resource or URI is null");
        FailOperation(promises);
        return;
    }

    // Look up the binding
    auto it = resourceWriteBindings.find(resource->uri);
    if (it == resourceWriteBindings.end())
    {
        icError("No write binding found for URI: %s", resource->uri);
        FailOperation(promises);
        return;
    }

    const ResourceBinding &binding = it->second;

    if (binding.isAttribute)
    {
        // Check if we have a script engine
        if (!script)
        {
            icError("No script engine available for device %s", deviceId.c_str());
            FailOperation(promises);
            return;
        }

        // Execute the script to map the string value to TLV data
        chip::Platform::ScopedMemoryBuffer<uint8_t> tlvBuffer;
        size_t encodedLength = 0;

        if (!script->MapAttributeWrite(binding.attribute.value(), newValue, tlvBuffer, encodedLength))
        {
            icError("Failed to execute write mapping script for URI: %s", resource->uri);
            FailOperation(promises);
            return;
        }

        // Create a TLV reader from the encoded data
        chip::TLV::TLVReader reader;
        reader.Init(tlvBuffer.Get(), encodedLength);

        // Create WriteClient to send the attribute write
        auto writeClient = std::make_unique<chip::app::WriteClient>(
            const_cast<chip::Messaging::ExchangeManager *>(&exchangeMgr), this, chip::Optional<uint16_t>::Missing());

        if (!writeClient)
        {
            icError("Failed to create WriteClient for URI: %s", resource->uri);
            FailOperation(promises);
            return;
        }

        // Use PutPreencodedAttribute to send the pre-encoded TLV data
        CHIP_ERROR err = writeClient->PutPreencodedAttribute(binding.attributePath, reader);
        if (err != CHIP_NO_ERROR)
        {
            icError("Failed to encode preencoded attribute for URI: %s, error: %s", resource->uri, err.AsString());
            FailOperation(promises);
            return;
        }

        // Create a promise for this write operation
        promises.emplace_front();
        auto &writePromise = promises.front();

        // Send the write request
        err = writeClient->SendWriteRequest(sessionHandle);
        if (err != CHIP_NO_ERROR)
        {
            icError("Failed to send write request for URI: %s, error: %s", resource->uri, err.AsString());
            writePromise.set_value(false);
            return;
        }

        icDebug("Successfully initiated write for resource %s", resource->uri);

        // Store the context to track this write operation
        WriteContext context;
        context.writePromise = &writePromise;
        context.writeClient = std::move(writeClient);
        activeWriteContexts[context.writeClient.get()] = std::move(context);
    }
    else
    {
        // Writing to a command is not yet implemented
        icError("Writing to command for URI: %s is not yet implemented", resource->uri);
        FailOperation(promises);
        return;
    }
}

void MatterDevice::CacheCallback::OnSubscriptionEstablished(chip::SubscriptionId aSubscriptionId)
{
    icDebug("OnSubscriptionEstablished for device %s, subscription ID: %u",
               device->deviceId.c_str(), aSubscriptionId);
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
        bool success = attributeStatus.IsSuccess();
        if (!success)
        {
            icError("Write failed with status 0x%x", static_cast<unsigned int>(attributeStatus.mStatus));
        }
        // Note: We don't set the promise here; we wait for OnDone to do that
        // because there may be multiple attribute responses
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
        // If we haven't already signaled the promise (via OnError), signal success now
        // Check if the promise is still valid by trying to get its future state
        try
        {
            // If set_value hasn't been called, this will succeed
            it->second.writePromise->set_value(true);
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

} // namespace barton
