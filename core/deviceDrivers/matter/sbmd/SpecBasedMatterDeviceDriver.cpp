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

/*
 * Created by Thomas Lea on 10/20/25
 */
#include "lib/core/DataModelTypes.h"
#define LOG_TAG     "SBMDD"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "SpecBasedMatterDeviceDriver.h"
#include "app/ConcreteAttributePath.h"
#include "matter/sbmd/SbmdSpec.h"
#include "matter/sbmd/script/QuickJsScript.h"
#include "subsystems/matter/DeviceDataCache.h"
#include <memory>

extern "C" {
#include <commonDeviceDefs.h>
#include <device/deviceModelHelper.h>
#include <device/icDeviceResource.h>
#include <deviceHelper.h>
#include <deviceService/resourceModes.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <resourceTypes.h>
}

#include <subsystems/matter/Matter.h>

using namespace barton;
using namespace std::chrono_literals;

#define BASE_SBMD_DRIVER_NAME "sbmd-"

SpecBasedMatterDeviceDriver::SpecBasedMatterDeviceDriver(std::shared_ptr<SbmdSpec> _spec) :
    MatterDeviceDriver((BASE_SBMD_DRIVER_NAME + _spec->name).c_str(),
                       _spec->bartonMeta.deviceClass.c_str(),
                       _spec->bartonMeta.deviceClassVersion),
    spec(std::move(_spec))
{
    icDebug("Created SBMD driver for: %s", spec->name.c_str());
}
std::vector<uint16_t> SpecBasedMatterDeviceDriver::GetSupportedDeviceTypes()
{
    return spec->matterMeta.deviceTypes;
}

bool SpecBasedMatterDeviceDriver::GetAttributePath(const MatterDevice &device,
                                                   const SbmdAttribute &attributeInfo,
                                                   app::ConcreteAttributePath &outPath)
{
    auto deviceDataCache = GetDeviceDataCache(device.GetDeviceId());

    // first see if the attribute exists on the root endpoint
    if (deviceDataCache->EndpointHasServerCluster(0, attributeInfo.clusterId))
    {
        outPath.mEndpointId = 0;
        outPath.mClusterId = attributeInfo.clusterId;
        outPath.mAttributeId = attributeInfo.attributeId;
        return true;
    }

    // otherwise, search the endpoint used to host this driver's device type
    auto endpoints = deviceDataCache->GetEndpointIds();
    for (auto endpointId : endpoints)
    {
        if (endpointId == 0)
        {
            continue; // already checked root endpoint which is device type agnostic
        }

        std::vector<uint16_t> deviceTypes;
        deviceDataCache->GetDeviceTypes(endpointId, deviceTypes);
        // see if this endpoint has any of our device types
        for (auto deviceType : deviceTypes)
        {
            // Check if this device type matches any in our spec
            if (std::find(spec->matterMeta.deviceTypes.begin(), spec->matterMeta.deviceTypes.end(), deviceType) !=
                spec->matterMeta.deviceTypes.end())
            {
                // This endpoint hosts one of our device types
                if (deviceDataCache->EndpointHasServerCluster(endpointId, attributeInfo.clusterId))
                {
                    outPath.mEndpointId = endpointId;
                    outPath.mClusterId = attributeInfo.clusterId;
                    outPath.mAttributeId = attributeInfo.attributeId;
                    return true;
                }
            }
        }
    }

    return false;
}

bool SpecBasedMatterDeviceDriver::AddDevice(std::unique_ptr<MatterDevice> device)
{
    device->SetScript(CreateConfiguredScript(device->GetDeviceId()));

    // for each resource in the spec, add an entry to a concrete attribute or command path
    // combine device resources with endpoint resources so we can iterate over all of them
    std::vector<SbmdResource> allResources = spec->resources;
    for (const auto &endpoint : spec->endpoints)
    {
        for (const auto &resource : endpoint.resources)
        {
            allResources.push_back(resource);
        }
    }

    for (const auto &sbmdResource : allResources)
    {
        icDebug("Configuring resource %s for device %s", sbmdResource.id.c_str(), device->GetDeviceId().c_str());

        g_autofree char *uri = nullptr;
        if (sbmdResource.resourceEndpointId.has_value())
        {
            uri = createEndpointResourceUri(device->GetDeviceId().c_str(),
                                            sbmdResource.resourceEndpointId.value().c_str(),
                                            sbmdResource.id.c_str());
        }
        else
        {
            uri = createDeviceResourceUri(device->GetDeviceId().c_str(), sbmdResource.id.c_str());
        }

        // a resource can have different mappers for read, write, and execute
        if (sbmdResource.mapper.hasRead)
        {
            if (!device->BindResourceReadInfo(uri, sbmdResource.mapper))
            {
                icError("  Failed to bind read script for resource %s", sbmdResource.id.c_str());
                return false;
            }
        }
        if (sbmdResource.mapper.hasWrite)
        {
            if (!device->BindResourceWriteInfo(uri, sbmdResource.mapper))
            {
                icError("  Failed to bind write script for resource %s", sbmdResource.id.c_str());
                return false;
            }
        }
        if (sbmdResource.mapper.hasExecute)
        {
            if (!device->BindResourceExecuteInfo(uri, sbmdResource.mapper))
            {
                icError("  Failed to bind execute script for resource %s", sbmdResource.id.c_str());
                return false;
            }
        }
    }

    return MatterDeviceDriver::AddDevice(std::move(device));
}

std::unique_ptr<SbmdScript> SpecBasedMatterDeviceDriver::CreateConfiguredScript(const std::string &deviceId)
{
    // For now, only QuickJS is supported
    auto script = QuickJsScript::Create(deviceId);

    // Add mappers from top-level resources
    for (const auto &resource : spec->resources)
    {
        if (resource.mapper.hasRead && !resource.mapper.readScript.empty())
        {
            if (resource.mapper.readAttribute.has_value())
            {
                script->AddAttributeReadMapper(resource.mapper.readAttribute.value(), resource.mapper.readScript);
            }
            else if (resource.mapper.readCommand.has_value())
            {
                // Reading from command not yet implemented
                icError("Read mapper with command not yet supported for resource %s", resource.id.c_str());
            }
        }
        if (resource.mapper.hasWrite && !resource.mapper.writeScript.empty())
        {
            if (resource.mapper.writeAttribute.has_value())
            {
                script->AddAttributeWriteMapper(resource.mapper.writeAttribute.value(), resource.mapper.writeScript);
            }
            else if (resource.mapper.writeCommand.has_value())
            {
                // Writing to command not yet implemented
                icError("Write mapper with command not yet supported for resource %s", resource.id.c_str());
            }
        }
        if (resource.mapper.hasExecute && !resource.mapper.executeScript.empty())
        {
            if (resource.mapper.executeAttribute.has_value())
            {
                // Executing attribute not yet implemented
                icError("Execute mapper with attribute not yet supported for resource %s", resource.id.c_str());
            }
            else if (resource.mapper.executeCommand.has_value())
            {
                script->AddCommandExecuteMapper(resource.mapper.executeCommand.value(), resource.mapper.executeScript);

                // Add response script if present
                if (resource.mapper.executeResponseScript.has_value())
                {
                    script->AddCommandExecuteResponseMapper(resource.mapper.executeCommand.value(),
                                                           resource.mapper.executeResponseScript.value());
                }
            }
        }
    }

    // Add mappers from endpoint resources
    for (const auto &endpoint : spec->endpoints)
    {
        for (const auto &resource : endpoint.resources)
        {
            if (resource.mapper.hasRead && !resource.mapper.readScript.empty())
            {
                if (resource.mapper.readAttribute.has_value())
                {
                    script->AddAttributeReadMapper(resource.mapper.readAttribute.value(), resource.mapper.readScript);
                }
                else if (resource.mapper.readCommand.has_value())
                {
                    // Reading from command not yet implemented
                    icError("Read mapper with command not yet supported for resource %s", resource.id.c_str());
                }
            }
            if (resource.mapper.hasWrite && !resource.mapper.writeScript.empty())
            {
                if (resource.mapper.writeAttribute.has_value())
                {
                    script->AddAttributeWriteMapper(resource.mapper.writeAttribute.value(),
                                                    resource.mapper.writeScript);
                }
                else if (resource.mapper.writeCommand.has_value())
                {
                    // Writing to command not yet implemented
                    icError("Write mapper with command not yet supported for resource %s", resource.id.c_str());
                }
            }
            if (resource.mapper.hasExecute && !resource.mapper.executeScript.empty())
            {
                if (resource.mapper.executeAttribute.has_value())
                {
                    // Executing attribute not yet implemented
                    icError("Execute mapper with attribute not yet supported for resource %s", resource.id.c_str());
                }
                else if (resource.mapper.executeCommand.has_value())
                {
                    script->AddCommandExecuteMapper(resource.mapper.executeCommand.value(),
                                                    resource.mapper.executeScript);

                    // Add response script if present
                    if (resource.mapper.executeResponseScript.has_value())
                    {
                        script->AddCommandExecuteResponseMapper(resource.mapper.executeCommand.value(),
                                                               resource.mapper.executeResponseScript.value());
                    }
                }
            }
        }
    }

    return script;
}

SubscriptionIntervalSecs SpecBasedMatterDeviceDriver::GetDesiredSubscriptionIntervalSecs()
{
    icDebug();

    return {spec->reporting.minSecs, spec->reporting.maxSecs};
}

bool SpecBasedMatterDeviceDriver::DoRegisterResources(icDevice *device)
{
    bool result = true;

    icDebug();

    // register device resources
    for (auto &sbmdResource : spec->resources)
    {
        uint8_t resourceMode = ConvertModesToBitmask(sbmdResource.modes);

        // if an executable mapper was provided, we need to make sure the resource is executable
        if (sbmdResource.mapper.hasExecute)
        {
            resourceMode |= RESOURCE_MODE_EXECUTABLE;
        }

        // Use CACHING_POLICY_ALWAYS only if there's a read attribute mapper since we will have
        // an attribute cache kept up to date via subscriptions.
        ResourceCachingPolicy cachingPolicy =
            (sbmdResource.mapper.hasRead && sbmdResource.mapper.readAttribute.has_value()) ? CACHING_POLICY_ALWAYS
                                                                                           : CACHING_POLICY_NEVER;

        result &=
            createDeviceResource(
                device, sbmdResource.id.c_str(), nullptr, sbmdResource.type.c_str(), resourceMode, cachingPolicy) !=
            nullptr;
    }

    // register endpoint resources
    for (auto &sbmdEndpoint : spec->endpoints)
    {
        icDeviceEndpoint *endpoint =
            createEndpoint(device, sbmdEndpoint.id.c_str(), sbmdEndpoint.profile.c_str(), true);

        for (auto &sbmdResource : sbmdEndpoint.resources)
        {
            uint8_t resourceMode = ConvertModesToBitmask(sbmdResource.modes);

            // if an executable mapper was provided, we need to make sure the resource is executable
            if (sbmdResource.mapper.hasExecute)
            {
                resourceMode |= RESOURCE_MODE_EXECUTABLE;
            }

            // Use CACHING_POLICY_ALWAYS only if there's a read attribute mapper since we will have
            // an attribute cache kept up to date via subscriptions.
            ResourceCachingPolicy cachingPolicy =
                (sbmdResource.mapper.hasRead && sbmdResource.mapper.readAttribute.has_value()) ? CACHING_POLICY_ALWAYS
                                                                                               : CACHING_POLICY_NEVER;

            result &= createEndpointResource(endpoint,
                                             sbmdResource.id.c_str(),
                                             nullptr,
                                             sbmdResource.type.c_str(),
                                             resourceMode,
                                             cachingPolicy) != nullptr;
        }
    }

    return result;
}

void SpecBasedMatterDeviceDriver::DoReadResource(std::forward_list<std::promise<bool>> &promises,
                                                 const std::string &deviceId,
                                                 icDeviceResource *resource,
                                                 char **value,
                                                 chip::Messaging::ExchangeManager &exchangeMgr,
                                                 const chip::SessionHandle &sessionHandle)
{
    icDebug("%s", resource->id);

    auto device = GetDevice(deviceId);
    if (device == nullptr)
    {
        icError("Device %s not found", deviceId.c_str());
        FailOperation(promises);
        return;
    }

    device->HandleResourceRead(promises, resource, value, exchangeMgr, sessionHandle);
}

bool SpecBasedMatterDeviceDriver::WriteResource(std::forward_list<std::promise<bool>> &promises,
                                                const std::string &deviceId,
                                                icDeviceResource *resource,
                                                const char *previousValue,
                                                const char *newValue,
                                                chip::Messaging::ExchangeManager &exchangeMgr,
                                                const chip::SessionHandle &sessionHandle)
{
    icDebug("%s = %s", resource->id, newValue);

    auto device = GetDevice(deviceId);
    if (device == nullptr)
    {
        icError("Device %s not found", deviceId.c_str());
        FailOperation(promises);
        return false;
    }

    device->HandleResourceWrite(promises, resource, previousValue, newValue, exchangeMgr, sessionHandle);

    return true; // let the base driver update the resource
}

void SpecBasedMatterDeviceDriver::ExecuteResource(std::forward_list<std::promise<bool>> &promises,
                                                  const std::string &deviceId,
                                                  icDeviceResource *resource,
                                                  const char *arg,
                                                  char **response,
                                                  chip::Messaging::ExchangeManager &exchangeMgr,
                                                  const chip::SessionHandle &sessionHandle)
{
    icDebug("%s(%s)", resource->id, arg);

    auto device = GetDevice(deviceId);
    if (device == nullptr)
    {
        icError("Device %s not found", deviceId.c_str());
        FailOperation(promises);
        return;
    }

    device->HandleResourceExecute(promises, resource, arg, response, exchangeMgr, sessionHandle);
}

uint8_t SpecBasedMatterDeviceDriver::ConvertModesToBitmask(const std::vector<std::string> &modes)
{
    uint8_t bitmask = 0;

    for (const auto &mode : modes)
    {
        if (mode == "read")
        {
            bitmask |= RESOURCE_MODE_READABLE;
        }
        else if (mode == "write")
        {
            bitmask |= RESOURCE_MODE_WRITEABLE;
        }
        else if (mode == "execute")
        {
            bitmask |= RESOURCE_MODE_EXECUTABLE;
        }
        else if (mode == "dynamic")
        {
            bitmask |= RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_DYNAMIC_CAPABLE;
        }
        else if (mode == "emitEvents")
        {
            bitmask |= RESOURCE_MODE_EMIT_EVENTS;
        }
        else if (mode == "lazySaveNext")
        {
            bitmask |= RESOURCE_MODE_LAZY_SAVE_NEXT;
        }
        else if (mode == "sensitive")
        {
            bitmask |= RESOURCE_MODE_SENSITIVE;
        }
        else
        {
            icWarn("Unknown resource mode: %s", mode.c_str());
        }
    }

    return bitmask;
}
