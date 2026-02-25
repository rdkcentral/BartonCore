//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2026 Comcast Cable Communications Management, LLC
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
#define LOG_TAG     "SBMDD"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "SpecBasedMatterDeviceDriver.h"
#include "matter/sbmd/SbmdSpec.h"
#include "matter/sbmd/script/QuickJsScript.h"
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

SpecBasedMatterDeviceDriver::SpecBasedMatterDeviceDriver(std::shared_ptr<SbmdSpec> spec) :
    MatterDeviceDriver((BASE_SBMD_DRIVER_NAME + spec->name).c_str(),
                       spec->bartonMeta.deviceClass.c_str(),
                       spec->bartonMeta.deviceClassVersion),
    spec(std::move(spec))
{
    icDebug("Created SBMD driver for: %s", this->spec->name.c_str());
}
std::vector<uint16_t> SpecBasedMatterDeviceDriver::GetSupportedDeviceTypes()
{
    return spec->matterMeta.deviceTypes;
}

bool SpecBasedMatterDeviceDriver::AddDevice(std::unique_ptr<MatterDevice> device)
{
    auto script = CreateConfiguredScript(device->GetDeviceId());
    if (!script)
    {
        icLogError(LOG_TAG, "Failed to create script for device %s, cannot add device", device->GetDeviceId().c_str());
        return false;
    }
    device->SetScript(std::move(script));

    // Set feature clusters from the spec for featureMap lookup
    device->SetFeatureClusters(spec->matterMeta.featureClusters);

    // for each resource in the spec, configure mapper bindings.
    // Note: resource modes ("read", "dynamic", "emitEvents") describe client-facing capabilities
    // (e.g. "read" means the resource can be read by clients). Mappers describe how the resource
    // is populated: read mappers query the device, event mappers update the value from events.
    // A resource can be readable without a read mapper if its value is populated by events.
    auto configureResource = [&device](const SbmdResource &sbmdResource) -> bool {
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
            // Write mappers are script-only - script returns full operation details
            std::string resourceKey = sbmdResource.resourceEndpointId.value_or("") + ":" + sbmdResource.id;
            if (!device->BindWriteInfo(uri, resourceKey, sbmdResource.resourceEndpointId.value_or(""), sbmdResource.id))
            {
                icError("  Failed to bind write script for resource %s", sbmdResource.id.c_str());
                return false;
            }
        }
        if (sbmdResource.mapper.hasExecute)
        {
            // Execute mappers are script-only - script returns full operation details
            std::string resourceKey = sbmdResource.resourceEndpointId.value_or("") + ":" + sbmdResource.id;
            if (!device->BindExecuteInfo(
                    uri, resourceKey, sbmdResource.resourceEndpointId.value_or(""), sbmdResource.id))
            {
                icError("  Failed to bind execute script for resource %s", sbmdResource.id.c_str());
                return false;
            }
        }
        if (sbmdResource.mapper.event.has_value())
        {
            // Event mappers - bind event to resource for automatic updates
            if (!device->BindResourceEventInfo(uri, sbmdResource.mapper.event.value()))
            {
                icError("  Failed to bind event for resource %s", sbmdResource.id.c_str());
                return false;
            }
        }
        return true;
    };

    // Configure device-level resources
    for (const auto &resource : spec->resources)
    {
        if (!configureResource(resource))
        {
            if (resource.optional)
            {
                icWarn("Optional resource %s failed to configure for device %s, skipping",
                       resource.id.c_str(),
                       device->GetDeviceId().c_str());
                skippedOptionalResources[device->GetDeviceId()].insert(MakeResourceKey(resource));
            }
            else
            {
                return false;
            }
        }
    }

    // Configure endpoint-level resources
    for (const auto &endpoint : spec->endpoints)
    {
        for (const auto &resource : endpoint.resources)
        {
            if (!configureResource(resource))
            {
                if (resource.optional)
                {
                    icWarn("Optional resource %s failed to configure for device %s, skipping",
                           resource.id.c_str(),
                           device->GetDeviceId().c_str());
                    skippedOptionalResources[device->GetDeviceId()].insert(MakeResourceKey(resource));
                }
                else
                {
                    return false;
                }
            }
        }
    }

    return MatterDeviceDriver::AddDevice(std::move(device));
}

std::unique_ptr<SbmdScript> SpecBasedMatterDeviceDriver::CreateConfiguredScript(const std::string &deviceId)
{
    // For now, only QuickJS is supported
    auto script = QuickJsScript::Create(deviceId);
    if (!script)
    {
        icLogError(LOG_TAG, "Failed to create script for device %s", deviceId.c_str());
        return nullptr;
    }

    // Add mappers from top-level resources
    for (const auto &resource : spec->resources)
    {
        AddResourceMappers(*script, resource);
    }

    // Add mappers from endpoint resources
    for (const auto &endpoint : spec->endpoints)
    {
        for (const auto &resource : endpoint.resources)
        {
            AddResourceMappers(*script, resource);
        }
    }

    return script;
}

void SpecBasedMatterDeviceDriver::AddResourceMappers(SbmdScript &script, const SbmdResource &resource)
{
    if (resource.mapper.hasRead && !resource.mapper.readScript.empty())
    {
        if (resource.mapper.readAttribute.has_value())
        {
            script.AddAttributeReadMapper(resource.mapper.readAttribute.value(), resource.mapper.readScript);
        }
        else if (resource.mapper.readCommand.has_value())
        {
            icError("Read mapper with command not yet supported for resource %s", resource.id.c_str());
        }
    }
    if (resource.mapper.hasWrite && !resource.mapper.writeScript.empty())
    {
        // Write mappers are script-only - script returns full operation details (invoke/write)
        std::string resourceKey = resource.resourceEndpointId.value_or("") + ":" + resource.id;
        script.AddWriteMapper(resourceKey, resource.mapper.writeScript);
    }
    if (resource.mapper.hasExecute && !resource.mapper.executeScript.empty())
    {
        // Execute mappers are script-only - script returns full operation details (invoke)
        std::string resourceKey = resource.resourceEndpointId.value_or("") + ":" + resource.id;
        script.AddExecuteMapper(resourceKey, resource.mapper.executeScript, resource.mapper.executeResponseScript);
    }
    if (resource.mapper.event.has_value() && !resource.mapper.eventScript.empty())
    {
        // Event mappers convert event TLV to resource values
        script.AddEventMapper(resource.mapper.event.value(), resource.mapper.eventScript);
    }
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

    auto skipIt = skippedOptionalResources.find(device->uuid);
    const auto *skipped = (skipIt != skippedOptionalResources.end()) ? &skipIt->second : nullptr;

    // register device resources
    for (auto &sbmdResource : spec->resources)
    {
        if (skipped && skipped->count(MakeResourceKey(sbmdResource)))
        {
            continue;
        }
        uint8_t resourceMode = ConvertModesToBitmask(sbmdResource.modes);

        // if an executable mapper was provided, we need to make sure the resource is executable
        if (sbmdResource.mapper.hasExecute)
        {
            resourceMode |= RESOURCE_MODE_EXECUTABLE;
        }

        // Use CACHING_POLICY_ALWAYS when the resource value is kept up to date
        // automatically — either via attribute subscription or event mapper updates.
        // With CACHING_POLICY_ALWAYS, the device service returns the stored value on read
        // without calling the driver's readResource callback.
        // Note: resource modes ("read", "dynamic", etc.) describe client-facing capabilities,
        // while mappers describe how the value is populated (attribute read, event, etc.).
        ResourceCachingPolicy cachingPolicy =
            (sbmdResource.mapper.hasRead && sbmdResource.mapper.readAttribute.has_value()) ||
            sbmdResource.mapper.event.has_value()
                ? CACHING_POLICY_ALWAYS
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

        if (endpoint == nullptr)
        {
            icError("Failed to create endpoint '%s' with profile '%s'",
                    sbmdEndpoint.id.c_str(),
                    sbmdEndpoint.profile.c_str());
            result = false;
            continue;
        }

        endpoint->profileVersion = sbmdEndpoint.profileVersion;
        for (auto &sbmdResource : sbmdEndpoint.resources)
        {
            if (skipped && skipped->count(MakeResourceKey(sbmdResource)))
            {
                continue;
            }

            uint8_t resourceMode = ConvertModesToBitmask(sbmdResource.modes);

            // if an executable mapper was provided, we need to make sure the resource is executable
            if (sbmdResource.mapper.hasExecute)
            {
                resourceMode |= RESOURCE_MODE_EXECUTABLE;
            }

            // Use CACHING_POLICY_ALWAYS when the resource value is kept up to date
            // automatically — either via attribute subscription or event mapper updates.
            // With CACHING_POLICY_ALWAYS, the device service returns the stored value on read
            // without calling the driver's readResource callback.
            ResourceCachingPolicy cachingPolicy =
                (sbmdResource.mapper.hasRead && sbmdResource.mapper.readAttribute.has_value()) ||
                sbmdResource.mapper.event.has_value()
                    ? CACHING_POLICY_ALWAYS
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

std::string SpecBasedMatterDeviceDriver::MakeResourceKey(const SbmdResource &resource)
{
    return resource.resourceEndpointId.value_or("") + ":" + resource.id;
}
