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

    // Fast O(1) lookup for readable attributes
    auto it = device->readableAttributeLookup.find(aPath);
    if (it == device->readableAttributeLookup.end())
    {
        // Not a readable attribute with a mapper - this is the common case
        return;
    }

    const auto &uri = it->second.uri;
    const auto &binding = it->second.binding;

    icDebug("Found readable attribute match for URI: %s", uri.c_str());

    // Check if we have a script engine
    if (!device->script)
    {
        icError("No script engine available for device %s", device->deviceId.c_str());
        return;
    }

    // Get the attribute data from the cache
    chip::TLV::TLVReader reader;
    if (cache == nullptr || cache->Get(aPath, reader) != CHIP_NO_ERROR)
    {
        icError("Failed to get attribute data from cache for device %s", device->deviceId.c_str());
        return;
    }

    // Execute the script to map the TLV data to a string value
    std::string outValue;
    if (!device->script->MapAttributeRead(binding.attribute.value(), reader, outValue))
    {
        icError("Failed to execute read mapping script for URI: %s", uri.c_str());
        return;
    }

    icDebug("Updating resource %s to value: %s", uri.c_str(), outValue.c_str());

    // Extract the resource ID from the URI
    // URI format is expected to be something like "/ep/deviceId/r/resourceId"
    const char *resourceId = strrchr(uri.c_str(), '/');
    if (resourceId != nullptr)
    {
        resourceId++; // Skip the '/'

        const char *resourceEndpointId = nullptr;
        if (binding.attribute->resourceEndpointId.has_value() && !binding.attribute->resourceEndpointId->empty())
        {
            resourceEndpointId = binding.attribute->resourceEndpointId->c_str();
        }

        // Call updateResource to notify DeviceService of the change
        updateResource(device->deviceId.c_str(),
                       resourceEndpointId,
                       resourceId,
                       outValue.c_str(),
                       nullptr); // No additional metadata for now
    }
    else
    {
        icError("Failed to extract resource ID from URI: %s", uri.c_str());
    }
}

bool MatterDevice::GetEndpointForCluster(chip::ClusterId clusterId, chip::EndpointId &outEndpointId)
{
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
    if (!script)
    {
        icDebug("No script set for device %s, skipping feature map update", deviceId.c_str());
        return;
    }

    std::map<uint32_t, uint32_t> clusterFeatureMaps;
    for (uint32_t clusterId : featureClusters)
    {
        // Find the Matter endpoint that hosts this cluster
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

    script->SetClusterFeatureMaps(clusterFeatureMaps);
    icDebug("Updated cached feature maps for device %s (%zu clusters)", deviceId.c_str(), clusterFeatureMaps.size());
}

bool MatterDevice::BindResourceReadInfo(const char *uri, const SbmdMapper &mapper)
{
    if (uri == nullptr)
    {
        icError("URI is null");
        return false;
    }

    // Validate: must have exactly one of attribute or command
    if ((!mapper.readAttribute.has_value() && !mapper.readCommand.has_value()) ||
        (mapper.readAttribute.has_value() && mapper.readCommand.has_value()))
    {
        icError("Must have either readAttribute or readCommand, but not both");
        return false;
    }

    ResourceBinding binding;
    chip::EndpointId endpointId;

    if (mapper.readAttribute.has_value())
    {
        const auto &attribute = mapper.readAttribute.value();

        if (!GetEndpointForCluster(attribute.clusterId, endpointId))
        {
            icError("Failed to find endpoint for cluster 0x%x", attribute.clusterId);
            return false;
        }

        binding.type = ResourceBinding::Type::Attribute;
        binding.attributePath.mEndpointId = endpointId;
        binding.attributePath.mClusterId = attribute.clusterId;
        binding.attributePath.mAttributeId = attribute.attributeId;
        binding.attribute = attribute;

        icDebug("Bound resource read for URI: %s (endpoint: %u, cluster: 0x%x, attribute: 0x%x)",
                uri,
                endpointId,
                attribute.clusterId,
                attribute.attributeId);

        // Add to fast lookup map for OnAttributeData callback
        AttributeReadBinding readBinding;
        readBinding.uri = uri;
        readBinding.binding = binding;
        readableAttributeLookup[binding.attributePath] = std::move(readBinding);
        icDebug("Added readable attribute to fast lookup (endpoint: %u, cluster: 0x%x, attribute: 0x%x)",
                endpointId,
                attribute.clusterId,
                attribute.attributeId);
    }
    else
    {
        binding.type = ResourceBinding::Type::Command;
        binding.command = mapper.readCommand.value();

        // Populate feature map for the command
        SbmdCommand &cmd = binding.command.value();
        if (!GetEndpointForCluster(cmd.clusterId, endpointId))
        {
            icError("Failed to find endpoint for command '%s' cluster 0x%x at URI: %s",
                    cmd.name.c_str(),
                    cmd.clusterId,
                    uri);
            return false;
        }

        icDebug("Bound resource read for URI: %s (command: %s)", uri, cmd.name.c_str());
    }

    resourceReadBindings[uri] = binding;
    return true;
}

bool MatterDevice::BindWriteInfo(const char *uri,
                                 const std::string &resourceKey,
                                 const std::string &endpointId,
                                 const std::string &resourceId)
{
    if (uri == nullptr)
    {
        icError("URI is null");
        return false;
    }

    ResourceBinding binding;
    binding.type = ResourceBinding::Type::ScriptOnly;
    binding.resourceKey = resourceKey;
    binding.endpointId = endpointId;
    binding.resourceId = resourceId;

    resourceWriteBindings[uri] = binding;
    icDebug("Bound write for URI %s (resourceKey=%s)", uri, resourceKey.c_str());
    return true;
}

bool MatterDevice::BindExecuteInfo(const char *uri,
                                   const std::string &resourceKey,
                                   const std::string &endpointId,
                                   const std::string &resourceId)
{
    if (uri == nullptr)
    {
        icError("URI is null");
        return false;
    }

    ResourceBinding binding;
    binding.type = ResourceBinding::Type::ScriptOnly;
    binding.resourceKey = resourceKey;
    binding.endpointId = endpointId;
    binding.resourceId = resourceId;

    resourceExecuteBindings[uri] = binding;
    icDebug("Bound execute for URI %s (resourceKey=%s)", uri, resourceKey.c_str());
    return true;
}

bool MatterDevice::SendCommandFromTlv(std::forward_list<std::promise<bool>> &promises,
                                      const SbmdCommand &command,
                                      chip::EndpointId endpointId,
                                      const uint8_t *tlvBuffer,
                                      size_t encodedLength,
                                      chip::Messaging::ExchangeManager &exchangeMgr,
                                      const chip::SessionHandle &sessionHandle,
                                      const char *uri,
                                      char **response)
{
    // Empty TLV structure (end-of-container marker: 0x15) for commands with no arguments
    static const uint8_t emptyTlvStruct[] = {0x15};
    static const size_t emptyTlvStructLen = sizeof(emptyTlvStruct);

    // Use empty TLV structure if no buffer provided (common for no-arg commands)
    if (tlvBuffer == nullptr || encodedLength == 0)
    {
        tlvBuffer = emptyTlvStruct;
        encodedLength = emptyTlvStructLen;
        icLogDebug(LOG_TAG, "No TLV buffer provided for command at URI: %s, using empty struct", uri);
    }

    // Create TLV reader from the encoded data
    // JsonToTlv wraps the value in a structure, so we need to navigate into it
    chip::TLV::TLVReader reader;
    reader.Init(tlvBuffer, encodedLength);
    if (reader.Next() != CHIP_NO_ERROR || reader.GetType() != chip::TLV::kTLVType_Structure)
    {
        icError("Invalid TLV structure for command at URI: %s", uri);
        return false;
    }

    // Create CommandSender with ExtendableCallback (this)
    // Pass the timed flag from the command definition - timed commands require a timed invoke
    bool isTimedRequest = command.timedInvokeTimeoutMs.has_value();
    auto commandSender = std::make_unique<chip::app::CommandSender>(this, &exchangeMgr, isTimedRequest);

    if (!commandSender)
    {
        icError("Failed to create CommandSender for URI: %s", uri);
        return false;
    }

    // Prepare the command
    // SetStartDataStruct(true) tells the SDK to start the CommandFields structure for us
    chip::app::CommandSender::PrepareCommandParameters prepareParams;
    prepareParams.SetStartDataStruct(true);

    chip::app::CommandPathParams commandPath(endpointId,
                                             0, /* group not used */
                                             command.clusterId,
                                             command.commandId,
                                             chip::app::CommandPathFlags::kEndpointIdValid);

    CHIP_ERROR err = commandSender->PrepareCommand(commandPath, prepareParams);
    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to prepare command for URI: %s, error: %s", uri, err.AsString());
        return false;
    }

    // Get the TLV writer and copy our preencoded command data
    chip::TLV::TLVWriter *writer = commandSender->GetCommandDataIBTLVWriter();
    if (writer == nullptr)
    {
        icError("Failed to get TLV writer for command at URI: %s", uri);
        return false;
    }

    // Enter the source container to access its elements
    // Our source TLV is a structure from JsonToTlv, we need to copy the elements inside
    chip::TLV::TLVType containerType;
    err = reader.EnterContainer(containerType);
    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to enter TLV container for URI: %s, error: %s", uri, err.AsString());
        return false;
    }

    // Copy each element from the reader to the writer
    while ((err = reader.Next()) == CHIP_NO_ERROR)
    {
        err = writer->CopyElement(reader);
        if (err != CHIP_NO_ERROR)
        {
            icError("Failed to copy command element for URI: %s, error: %s", uri, err.AsString());
            return false;
        }
    }

    // Check if we exited the loop due to end of container or error
    if (err != CHIP_END_OF_TLV)
    {
        icError("Error iterating TLV elements for URI: %s, error: %s", uri, err.AsString());
        return false;
    }

    // Finish the command
    // SetEndDataStruct(true) tells the SDK to end the CommandFields structure for us
    // For timed requests, we need to provide the timeout in FinishCommandParameters
    chip::app::CommandSender::FinishCommandParameters finishParams(
        isTimedRequest ? chip::MakeOptional(command.timedInvokeTimeoutMs.value()) : chip::NullOptional);
    finishParams.SetEndDataStruct(true);
    err = commandSender->FinishCommand(finishParams);
    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to finish command for URI: %s, error: %s", uri, err.AsString());
        return false;
    }

    // Create a promise for this command operation
    promises.emplace_front();
    auto &commandPromise = promises.front();

    // Send the command request
    err = commandSender->SendCommandRequest(sessionHandle);
    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to send command request for URI: %s, error: %s", uri, err.AsString());
        commandPromise.set_value(false);
        return false;
    }

    icDebug("Successfully initiated command %s for URI: %s", command.name.c_str(), uri);

    // Store the context to track this command operation
    CommandContext context;
    context.commandPromise = &commandPromise;
    context.commandSender = std::move(commandSender);
    context.commandInfo = command;
    context.response = response;
    auto * commandSenderPtr = context.commandSender.get();
    activeCommandContexts[commandSenderPtr] = std::move(context);

    return true;
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

    if (binding.type == ResourceBinding::Type::Attribute)
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

    // Check if we have a script engine (needed for all write paths)
    if (!script)
    {
        icError("No script engine available for device %s", deviceId.c_str());
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

    if (binding.type == ResourceBinding::Type::ScriptOnly)
    {
        // Execute the script to get the full operation details
        ScriptWriteResult result;
        if (!script->MapWrite(binding.resourceKey,
                              binding.endpointId,
                              binding.resourceId,
                              newValue != nullptr ? newValue : "",
                              result))
        {
            icError("Failed to execute write mapping script for URI: %s", resource->uri);
            FailOperation(promises);
            return;
        }

        // Determine the endpoint to use
        chip::EndpointId endpointId;
        if (result.endpointId.has_value())
        {
            endpointId = result.endpointId.value();
        }
        else if (!GetEndpointForCluster(result.clusterId, endpointId))
        {
            icError("Failed to find endpoint for cluster 0x%x", result.clusterId);
            FailOperation(promises);
            return;
        }

        if (result.type == ScriptWriteResult::OperationType::Invoke)
        {
            // Build a temporary SbmdCommand from the result for SendCommandFromTlv
            SbmdCommand cmd;
            cmd.clusterId = result.clusterId;
            cmd.commandId = result.commandId;
            cmd.name = "script-invoke"; // placeholder name
            if (result.timedInvokeTimeoutMs.has_value())
            {
                cmd.timedInvokeTimeoutMs = result.timedInvokeTimeoutMs.value();
            }

            if (!SendCommandFromTlv(promises,
                                    cmd,
                                    endpointId,
                                    result.tlvBuffer.Get(),
                                    result.tlvLength,
                                    exchangeMgr,
                                    sessionHandle,
                                    resource->uri,
                                    nullptr))
            {
                FailOperation(promises);
                return;
            }
        }
        else if (result.type == ScriptWriteResult::OperationType::Write)
        {
            // Create TLV reader from the result
            chip::TLV::TLVReader reader;
            reader.Init(result.tlvBuffer.Get(), result.tlvLength);
            if (reader.Next() != CHIP_NO_ERROR || reader.GetType() != chip::TLV::kTLVType_Structure)
            {
                icError("Invalid TLV structure from write script for URI: %s", resource->uri);
                FailOperation(promises);
                return;
            }
            chip::TLV::TLVType containerType;
            if (reader.EnterContainer(containerType) != CHIP_NO_ERROR || reader.Next() != CHIP_NO_ERROR)
            {
                icError("Failed to navigate TLV structure from write script for URI: %s", resource->uri);
                FailOperation(promises);
                return;
            }

            // Build the attribute path
            chip::app::ConcreteAttributePath attrPath(endpointId, result.clusterId, result.attributeId);

            // Create WriteClient to send the attribute write
            auto writeClient =
                std::make_unique<chip::app::WriteClient>(const_cast<chip::Messaging::ExchangeManager *>(&exchangeMgr),
                                                         this,
                                                         chip::Optional<uint16_t>::Missing());

            if (!writeClient)
            {
                icError("Failed to create WriteClient for URI: %s", resource->uri);
                FailOperation(promises);
                return;
            }

            CHIP_ERROR err = writeClient->PutPreencodedAttribute(attrPath, reader);
            if (err != CHIP_NO_ERROR)
            {
                icError("Failed to encode preencoded attribute for URI: %s, error: %s", resource->uri, err.AsString());
                FailOperation(promises);
                return;
            }

            promises.emplace_front();
            auto &writePromise = promises.front();

            err = writeClient->SendWriteRequest(sessionHandle);
            if (err != CHIP_NO_ERROR)
            {
                icError("Failed to send write request for URI: %s, error: %s", resource->uri, err.AsString());
                writePromise.set_value(false);
                return;
            }

            icDebug("Successfully initiated matter.js attribute write for resource %s", resource->uri);

            WriteContext context;
            context.writePromise = &writePromise;
            context.writeClient = std::move(writeClient);
            activeWriteContexts[context.writeClient.get()] = std::move(context);
        }
        else
        {
            icError("matter.js write script returned invalid operation type for URI: %s", resource->uri);
            FailOperation(promises);
            return;
        }
    }
    else
    {
        icError("Invalid write binding type for URI: %s", resource->uri);
        FailOperation(promises);
        return;
    }
}

void MatterDevice::HandleResourceExecute(std::forward_list<std::promise<bool>> &promises,
                                         icDeviceResource *resource,
                                         const char *arg,
                                         char **response,
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
    auto it = resourceExecuteBindings.find(resource->uri);
    if (it == resourceExecuteBindings.end())
    {
        icError("No execute binding found for URI: %s", resource->uri);
        FailOperation(promises);
        return;
    }

    const ResourceBinding &binding = it->second;

    if (binding.type == ResourceBinding::Type::ScriptOnly)
    {
        // Execute using script that returns full operation details
        ScriptWriteResult result;

        // Parse the input argument(s)
        std::string inputValue = (arg != nullptr) ? arg : "";

        if (!script->MapExecute(binding.resourceKey, binding.endpointId, binding.resourceId, inputValue, result))
        {
            icError("Failed to execute mapping script for URI: %s", resource->uri);
            FailOperation(promises);
            return;
        }

        // Determine the endpoint to use
        chip::EndpointId endpointId;
        if (result.endpointId.has_value())
        {
            endpointId = result.endpointId.value();
        }
        else if (!GetEndpointForCluster(result.clusterId, endpointId))
        {
            icError("Failed to find endpoint for cluster 0x%x", result.clusterId);
            FailOperation(promises);
            return;
        }

        if (result.type == ScriptWriteResult::OperationType::Invoke)
        {
            // Build a temporary SbmdCommand from the result for SendCommandFromTlv
            SbmdCommand cmd;
            cmd.clusterId = result.clusterId;
            cmd.commandId = result.commandId;
            cmd.name = "script-execute";
            if (result.timedInvokeTimeoutMs.has_value())
            {
                cmd.timedInvokeTimeoutMs = result.timedInvokeTimeoutMs.value();
            }

            if (!SendCommandFromTlv(promises,
                                    cmd,
                                    endpointId,
                                    result.tlvBuffer.Get(),
                                    result.tlvLength,
                                    exchangeMgr,
                                    sessionHandle,
                                    resource->uri,
                                    response))
            {
                FailOperation(promises);
                return;
            }
        }
        else
        {
            icError("Execute binding returned write operation instead of invoke for URI: %s", resource->uri);
            FailOperation(promises);
            return;
        }
    }
    else
    {
        icError("Invalid execute binding type for URI: %s", resource->uri);
        FailOperation(promises);
        return;
    }
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

    // Check if the command failed with a status
    if (!aResponseData.statusIB.IsSuccess())
    {
        icError("Command failed with status 0x%x for device %s",
                static_cast<unsigned int>(aResponseData.statusIB.mStatus),
                deviceId.c_str());
        // Mark the operation as failed
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

    // Command succeeded - check if we have response data and a response script to process it
    if (aResponseData.data != nullptr && context.response != nullptr && script)
    {
        // Get the TLV reader from the response
        chip::TLV::TLVReader responseReader;
        responseReader.Init(*aResponseData.data);

        // Use the script to map the response TLV to a Barton string
        std::string responseValue;
        if (script->MapCommandExecuteResponse(context.commandInfo, responseReader, responseValue))
        {
            icDebug("Mapped command response to value: %s", responseValue.c_str());
            *context.response = strdup(responseValue.c_str());
        }
        else
        {
            icWarn("Failed to map command response for device %s", deviceId.c_str());
        }
    }
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
