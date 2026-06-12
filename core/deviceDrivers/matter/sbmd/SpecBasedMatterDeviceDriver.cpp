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
#include "matter/sbmd/SbmdV4Driver.h"

#if defined(BCORE_USE_MQUICKJS)
#include "matter/sbmd/mquickjs/MQuickJsRuntime.h"
#include "matter/sbmd/mquickjs/SbmdScriptImpl.h"
#include "matter/sbmd/mquickjs/SbmdV4HandlerInvoker.h"
#elif defined(BCORE_USE_QUICKJS)
#include "matter/sbmd/quickjs/SbmdScriptImpl.h"
#endif

#include <app/ConcreteAttributePath.h>
#include <cinttypes>
#include <lib/core/TLVReader.h>
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

#include <lib/support/Base64.h>

using namespace barton;
using namespace std::chrono_literals;

#define BASE_SBMD_DRIVER_NAME "sbmd-"

SpecBasedMatterDeviceDriver::SpecBasedMatterDeviceDriver(std::shared_ptr<SbmdSpec> spec) :
    MatterDeviceDriver((BASE_SBMD_DRIVER_NAME + spec->name).c_str(),
                       spec->bartonMeta.deviceClass.c_str(),
                       spec->bartonMeta.deviceClassVersion),
    spec(std::move(spec))
{
    icDebug("Created SBMD v3 driver for: %s", this->spec->name.c_str());
}

SpecBasedMatterDeviceDriver::SpecBasedMatterDeviceDriver(SbmdV4Driver *v4Driver) :
    MatterDeviceDriver((BASE_SBMD_DRIVER_NAME + v4Driver->GetRegistration().name).c_str(),
                       v4Driver->GetRegistration().barton.deviceClass.c_str(),
                       v4Driver->GetRegistration().barton.deviceClassVersion),
    v4Driver(v4Driver)
{
    icDebug("Created SBMD v4 driver for: %s", v4Driver->GetName().c_str());
}

uint16_t SpecBasedMatterDeviceDriver::GetSupportedVendorId() const
{
    if (IsV4())
    {
        return v4Driver->GetRegistration().matter.vendorId.value_or(0);
    }

    return spec->matterMeta.vendorId.value_or(0);
}

uint16_t SpecBasedMatterDeviceDriver::GetSupportedProductId() const
{
    if (IsV4())
    {
        return v4Driver->GetRegistration().matter.productId.value_or(0);
    }

    return spec->matterMeta.productId.value_or(0);
}

bool SpecBasedMatterDeviceDriver::IsVendorSpecificDriver() const
{
    if (IsV4())
    {
        const auto &m = v4Driver->GetRegistration().matter;

        return m.vendorId.has_value() && m.productId.has_value();
    }

    return spec->matterMeta.vendorId.has_value() && spec->matterMeta.productId.has_value();
}

std::vector<uint16_t> SpecBasedMatterDeviceDriver::GetSupportedDeviceTypes()
{
    if (IsV4())
    {
        return v4Driver->GetRegistration().matter.deviceTypes;
    }

    return spec->matterMeta.deviceTypes;
}

bool SpecBasedMatterDeviceDriver::AddDevice(std::unique_ptr<MatterDevice> device)
{
    if (IsV4())
    {
        // V4 path: no script creation, no resource binding.
        // The dispatch tables on the driver handle everything.
        device->SetFeatureClusters(v4Driver->GetRegistration().matter.featureClusters);

        if (!device->ResolveEndpointMap(v4Driver->GetRegistration().matter.deviceTypes))
        {
            icError("V4: Failed to resolve endpoint map for device %s, no matching device types found",
                    device->GetDeviceId().c_str());
            return false;
        }

        // Set the v4 attribute callback so CacheCallback delegates to our dispatch tables
        device->SetV4AttributeCallback(
            [this](const std::string &deviceId,
                   chip::EndpointId endpointId,
                   chip::ClusterId clusterId,
                   chip::AttributeId attributeId,
                   chip::TLV::TLVReader &reader) {
                HandleV4AttributeReport(deviceId, endpointId, clusterId, attributeId, reader);
            });

        // Check prerequisites for v4 resources
        const auto &reg = v4Driver->GetRegistration();

        for (const auto &endpoint : reg.endpoints)
        {
            for (const auto &resource : endpoint.resources)
            {
                if (!CheckPrerequisitesV4(resource, *device))
                {
                    if (resource.optional)
                    {
                        icDebug("V4: Optional resource '%s' prerequisites not met, skipping", resource.id.c_str());
                        std::string key = endpoint.id + ":" + resource.id;
                        skippedOptionalResources[device->GetDeviceId()].insert(key);
                        continue;
                    }

                    icError("V4: Required resource '%s' prerequisites not met, aborting commissioning",
                            resource.id.c_str());

                    return false;
                }
            }
        }

        return MatterDeviceDriver::AddDevice(std::move(device));
    }

    // V3 path
    auto script = CreateConfiguredScript(device->GetDeviceId());
    if (!script)
    {
        icLogError(LOG_TAG, "Failed to create script for device %s, cannot add device", device->GetDeviceId().c_str());
        return false;
    }
    device->SetScript(std::move(script));

    // Set feature clusters from the spec for featureMap lookup
    device->SetFeatureClusters(spec->matterMeta.featureClusters);

    // Resolve the endpoint map before resource binding
    if (!device->ResolveEndpointMap(spec->matterMeta.deviceTypes))
    {
        icError("Failed to resolve endpoint map for device %s, no matching device types found",
                device->GetDeviceId().c_str());
        return false;
    }

    // for each resource in the spec, configure mapper bindings.
    // Note: resource modes ("read", "dynamic", "emitEvents") describe client-facing capabilities
    // (e.g. "read" means the resource can be read by clients). Mappers describe how the resource
    // is populated: read mappers query the device, event mappers update the value from events.
    // A resource can be readable without a read mapper if its value is populated by events.
    auto configureResource = [&device](const SbmdResource &sbmdResource,
                                       std::optional<uint32_t> sbmdEndpointIndex) -> bool {
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
            if (!device->BindResourceReadInfo(uri, sbmdResource.mapper, sbmdEndpointIndex))
            {
                icError("  Failed to bind read script for resource %s", sbmdResource.id.c_str());
                return false;
            }
        }
        if (sbmdResource.mapper.hasWrite)
        {
            // Write mappers are script-only - script returns full operation details
            std::string resourceKey = sbmdResource.resourceEndpointId.value_or("") + ":" + sbmdResource.id;
            if (!device->BindWriteInfo(
                    uri, resourceKey, sbmdResource.resourceEndpointId.value_or(""), sbmdResource.id, sbmdEndpointIndex))
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
                    uri, resourceKey, sbmdResource.resourceEndpointId.value_or(""), sbmdResource.id, sbmdEndpointIndex))
            {
                icError("  Failed to bind execute script for resource %s", sbmdResource.id.c_str());
                return false;
            }
        }
        if (sbmdResource.mapper.event.has_value())
        {
            // Event mappers - bind event to resource for automatic updates
            if (!device->BindResourceEventInfo(uri, sbmdResource.mapper.event.value(), sbmdEndpointIndex))
            {
                icError("  Failed to bind event for resource %s", sbmdResource.id.c_str());
                return false;
            }
        }

        if (sbmdResource.mapper.seedFromAttribute.has_value())
        {
            if (!device->BindResourceSeedFromInfo(uri, sbmdResource.mapper, sbmdEndpointIndex))
            {
                icError("  Failed to bind seedFrom for resource %s", sbmdResource.id.c_str());
                return false;
            }
        }

        return true;
    };

    // Helper lambda that evaluates prerequisites and, if satisfied, attempts to configure the resource.
    // Handles optional/required branching and skip bookkeeping so that the two loops below stay symmetric.
    // Returns false if commissioning must be aborted (required resource failed), true otherwise.
    auto processResource = [&](const SbmdResource &resource, std::optional<uint32_t> endpointIndex) -> bool {
        if (!CheckPrerequisites(resource, *device))
        {
            if (resource.optional)
            {
                icDebug("Optional resource '%s' prerequisites not met, skipping", resource.id.c_str());
                skippedOptionalResources[device->GetDeviceId()].insert(MakeResourceKey(resource));

                return true;
            }

            icError("Required resource '%s' prerequisites not met, aborting commissioning", resource.id.c_str());

            return false;
        }

        if (!configureResource(resource, endpointIndex))
        {
            if (resource.optional)
            {
                icWarn("Optional resource %s failed to configure for device %s, skipping",
                       resource.id.c_str(),
                       device->GetDeviceId().c_str());
                skippedOptionalResources[device->GetDeviceId()].insert(MakeResourceKey(resource));

                return true;
            }

            icError("Required resource '%s' failed to configure, aborting commissioning", resource.id.c_str());

            return false;
        }

        return true;
    };

    // Configure device-level resources (no SBMD endpoint index — uses cluster-based lookup)
    for (const auto &resource : spec->resources)
    {
        if (!processResource(resource, std::nullopt))
        {
            return false;
        }
    }

    // Configure endpoint-level resources
    for (uint32_t epIdx = 0; epIdx < static_cast<uint32_t>(spec->endpoints.size()); ++epIdx)
    {
        const auto &endpoint = spec->endpoints[epIdx];

        for (const auto &resource : endpoint.resources)
        {
            if (!processResource(resource, epIdx))
            {
                return false;
            }
        }
    }

    return MatterDeviceDriver::AddDevice(std::move(device));
}

std::unique_ptr<SbmdScript> SpecBasedMatterDeviceDriver::CreateConfiguredScript(const std::string &deviceId)
{
    auto script = SbmdScriptImpl::Create(deviceId);
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

    if (resource.mapper.seedFromAttribute.has_value() && !resource.mapper.seedFromScript.empty())
    {
        // SeedFrom mappers reuse the attribute read mapper interface — same script shape as read mappers
        script.AddAttributeReadMapper(resource.mapper.seedFromAttribute.value(), resource.mapper.seedFromScript);
    }
}

SubscriptionIntervalSecs SpecBasedMatterDeviceDriver::GetDesiredSubscriptionIntervalSecs()
{
    icDebug();

    if (IsV4())
    {
        const auto &r = v4Driver->GetRegistration().reporting;

        return {r.minSecs, r.maxSecs};
    }

    return {spec->reporting.minSecs, spec->reporting.maxSecs};
}

void SpecBasedMatterDeviceDriver::ForEachNonSkippedResource(
    const std::string &deviceId,
    const std::function<void(const SbmdResource &, const SbmdEndpoint *)> &callback) const
{
    auto skipIt = skippedOptionalResources.find(deviceId);
    const auto *skipped = (skipIt != skippedOptionalResources.end()) ? &skipIt->second : nullptr;

    for (const auto &resource : spec->resources)
    {
        if (skipped && skipped->count(MakeResourceKey(resource)))
        {
            continue;
        }

        callback(resource, nullptr);
    }

    for (const auto &endpoint : spec->endpoints)
    {
        for (const auto &resource : endpoint.resources)
        {
            if (skipped && skipped->count(MakeResourceKey(resource)))
            {
                continue;
            }

            callback(resource, &endpoint);
        }
    }
}

bool SpecBasedMatterDeviceDriver::DoRegisterResources(icDevice *device)
{
    if (IsV4())
    {
        return DoRegisterResourcesV4(device);
    }

    bool result = true;

    icDebug();

    auto matterDevice = GetDevice(device->uuid);
    std::map<const SbmdEndpoint *, icDeviceEndpoint *> icEndpoints;

    ForEachNonSkippedResource(device->uuid, [&](const SbmdResource &sbmdResource, const SbmdEndpoint *sbmdEndpoint) {
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

        // Seed resource with value from attribute cache if specified
        const char *initialValue = nullptr;
        std::string seedValue;

        if (sbmdEndpoint == nullptr)
        {
            if (sbmdResource.mapper.seedFromAttribute.has_value() && matterDevice != nullptr)
            {
                g_autofree char *uri = createDeviceResourceUri(device->uuid, sbmdResource.id.c_str());
                auto maybeSeedValue = matterDevice->ReadSeedValueFromAttribute(uri);

                if (maybeSeedValue.has_value())
                {
                    seedValue = std::move(*maybeSeedValue);
                    initialValue = seedValue.c_str();
                }
            }

            result &= createDeviceResource(device,
                                           sbmdResource.id.c_str(),
                                           initialValue,
                                           sbmdResource.type.c_str(),
                                           resourceMode,
                                           cachingPolicy) != nullptr;

            return;
        }

        auto [epIt, inserted] = icEndpoints.emplace(sbmdEndpoint, nullptr);

        if (inserted)
        {
            auto *ep = createEndpoint(device, sbmdEndpoint->id.c_str(), sbmdEndpoint->profile.c_str(), true);

            if (ep == nullptr)
            {
                icError("Failed to create endpoint '%s' with profile '%s'",
                        sbmdEndpoint->id.c_str(),
                        sbmdEndpoint->profile.c_str());
                result = false;

                return;
            }

            ep->profileVersion = sbmdEndpoint->profileVersion;
            epIt->second = ep;
        }

        auto *ep = epIt->second;

        if (ep == nullptr)
        {
            return;
        }

        if (sbmdResource.mapper.seedFromAttribute.has_value() && matterDevice != nullptr)
        {
            g_autofree char *uri = createEndpointResourceUri(
                device->uuid, sbmdResource.resourceEndpointId.value_or("").c_str(), sbmdResource.id.c_str());
            auto maybeSeedValue = matterDevice->ReadSeedValueFromAttribute(uri);

            if (maybeSeedValue.has_value())
            {
                seedValue = std::move(*maybeSeedValue);
                initialValue = seedValue.c_str();
            }
        }

        result &=
            createEndpointResource(
                ep, sbmdResource.id.c_str(), initialValue, sbmdResource.type.c_str(), resourceMode, cachingPolicy) !=
            nullptr;
    });

    return result;
}

void SpecBasedMatterDeviceDriver::DoSynchronizeDevice(std::forward_list<std::promise<bool>> &promises,
                                                      const std::string &deviceId,
                                                      chip::Messaging::ExchangeManager &exchangeMgr,
                                                      const chip::SessionHandle &sessionHandle)
{
    if (IsV4())
    {
        SeedInitialResourceValuesV4(deviceId);
        return;
    }

    SeedInitialResourceValues(deviceId);
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

    if (IsV4())
    {
        HandleV4ResourceOp(promises, *device, resource, nullptr, value, nullptr, exchangeMgr, sessionHandle, "read");
        return;
    }

    device->HandleResourceRead(promises, resource, value, exchangeMgr, sessionHandle);
}

bool SpecBasedMatterDeviceDriver::DoWriteResource(std::forward_list<std::promise<bool>> &promises,
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

    if (IsV4())
    {
        HandleV4ResourceOp(promises, *device, resource, newValue, nullptr, nullptr, exchangeMgr, sessionHandle, "write");
        return true; // let the base driver update the resource
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

    if (IsV4())
    {
        HandleV4ResourceOp(promises, *device, resource, arg, nullptr, response, exchangeMgr, sessionHandle, "execute");
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

void SpecBasedMatterDeviceDriver::SeedInitialResourceValues(const std::string &deviceId)
{
    icDebug("Seeding initial resource values for device %s", deviceId.c_str());

    auto device = GetDevice(deviceId);

    if (device == nullptr)
    {
        icError("Device %s not found for seedFrom seeding", deviceId.c_str());
        return;
    }

    ForEachNonSkippedResource(deviceId, [&](const SbmdResource &resource, const SbmdEndpoint *sbmdEndpoint) {
        if (!resource.mapper.seedFromAttribute.has_value())
        {
            return;
        }

        g_autofree char *uri = (sbmdEndpoint == nullptr)
                                   ? createDeviceResourceUri(deviceId.c_str(), resource.id.c_str())
                                   : createEndpointResourceUri(deviceId.c_str(),
                                                               resource.resourceEndpointId.value_or("").c_str(),
                                                               resource.id.c_str());

        device->SeedResourceFromAttribute(uri);
    });
}

bool SpecBasedMatterDeviceDriver::CheckPrerequisites(const SbmdResource &resource, const MatterDevice &device)
{
    // Empty prerequisites vector means always attempt to register (declared as "none" in the spec)
    if (resource.prerequisites.empty())
    {
        return true;
    }

    auto cache = device.GetDeviceDataCache();

    if (!cache)
    {
        icWarn("No device data cache for device %s; prerequisites cannot be evaluated and will be treated as unmet",
               device.GetDeviceId().c_str());

        return false;
    }

    const auto endpointIds = cache->GetEndpointIds();

    for (const auto &prereq : resource.prerequisites)
    {
        uint32_t clusterId = prereq.clusterId;
        const std::vector<uint32_t> &attributeIds = prereq.attributeIds;

        // Check cluster presence on any endpoint
        bool clusterFound = false;

        for (auto endpointId : endpointIds)
        {
            if (cache->EndpointHasServerCluster(endpointId, clusterId))
            {
                clusterFound = true;
                break;
            }
        }

        if (!clusterFound)
        {
            icDebug("Resource '%s': prerequisite cluster 0x%08" PRIx32 " not found on device %s; prerequisite not met",
                    resource.id.c_str(),
                    clusterId,
                    device.GetDeviceId().c_str());

            return false;
        }

        // Check attribute presence for each required attribute ID
        for (uint32_t attributeId : attributeIds)
        {
            bool attributeFound = false;

            for (auto endpointId : endpointIds)
            {
                // find the endpoint with the cluster and then check for the attribute
                if (!cache->EndpointHasServerCluster(endpointId, clusterId))
                {
                    continue;
                }

                chip::app::ConcreteDataAttributePath path(endpointId, clusterId, attributeId);
                chip::TLV::TLVReader reader;

                if (cache->GetAttributeData(path, reader) == CHIP_NO_ERROR)
                {
                    attributeFound = true;
                    break;
                }
            }

            if (!attributeFound)
            {
                icDebug("Resource '%s': prerequisite attribute 0x%08" PRIx32 " on cluster 0x%08" PRIx32
                        " not found on device %s; prerequisite not met",
                        resource.id.c_str(),
                        attributeId,
                        clusterId,
                        device.GetDeviceId().c_str());

                return false;
            }
        }
    }

    return true;
}

// =============================================================================
// V4-specific implementation methods
// =============================================================================

bool SpecBasedMatterDeviceDriver::DoRegisterResourcesV4(icDevice *device)
{
    bool result = true;
    const auto &reg = v4Driver->GetRegistration();
    const auto *skipped = skippedOptionalResources.count(device->uuid)
                              ? &skippedOptionalResources[device->uuid]
                              : nullptr;

    icDebug("V4: Registering resources for device %s", device->uuid);

    std::map<std::string, icDeviceEndpoint *> icEndpoints; // endpoint id → created endpoint

    for (const auto &endpoint : reg.endpoints)
    {
        for (const auto &resource : endpoint.resources)
        {
            std::string key = endpoint.id + ":" + resource.id;

            if (skipped && skipped->count(key))
            {
                continue;
            }

            // Create endpoint on first resource that needs it
            auto [epIt, inserted] = icEndpoints.emplace(endpoint.id, nullptr);

            if (inserted)
            {
                auto *ep = createEndpoint(device, endpoint.id.c_str(), endpoint.profile.c_str(), true);

                if (ep == nullptr)
                {
                    icError("V4: Failed to create endpoint '%s' with profile '%s'",
                            endpoint.id.c_str(),
                            endpoint.profile.c_str());
                    result = false;
                    continue;
                }

                ep->profileVersion = endpoint.profileVersion;
                epIt->second = ep;
            }

            auto *ep = epIt->second;

            if (ep == nullptr)
            {
                continue;
            }

            uint8_t resourceMode = ConvertModesToBitmask(resource.modes);

            if (resource.execute.has_value())
            {
                resourceMode |= RESOURCE_MODE_EXECUTABLE;
            }

            // V4 resources with read handlers use CACHING_POLICY_ALWAYS because
            // the attribute dispatch handles live updates
            ResourceCachingPolicy cachingPolicy =
                resource.read.has_value() ? CACHING_POLICY_ALWAYS : CACHING_POLICY_NEVER;

            // Seed initial value if there's a seed handler
            const char *initialValue = nullptr;
            std::string seedValue;

            if (resource.seed.has_value())
            {
                seedValue = InvokeV4SeedHandler(device->uuid, endpoint.id, resource);

                if (!seedValue.empty())
                {
                    initialValue = seedValue.c_str();
                }
            }

            result &=
                createEndpointResource(
                    ep, resource.id.c_str(), initialValue, resource.type.c_str(), resourceMode, cachingPolicy) !=
                nullptr;
        }
    }

    return result;
}

void SpecBasedMatterDeviceDriver::SeedInitialResourceValuesV4(const std::string &deviceId)
{
    icDebug("V4: Seeding initial resource values for device %s", deviceId.c_str());

    const auto &reg = v4Driver->GetRegistration();
    const auto *skipped = skippedOptionalResources.count(deviceId) ? &skippedOptionalResources[deviceId] : nullptr;

    for (const auto &endpoint : reg.endpoints)
    {
        for (const auto &resource : endpoint.resources)
        {
            if (!resource.seed.has_value())
            {
                continue;
            }

            std::string key = endpoint.id + ":" + resource.id;

            if (skipped && skipped->count(key))
            {
                continue;
            }

            std::string seedValue = InvokeV4SeedHandler(deviceId, endpoint.id, resource);

            if (!seedValue.empty())
            {
                updateResource(deviceId.c_str(), endpoint.id.c_str(), resource.id.c_str(), seedValue.c_str(), nullptr);
            }
        }
    }
}

std::string SpecBasedMatterDeviceDriver::InvokeV4SeedHandler(const std::string &deviceId,
                                                             const std::string &endpointId,
                                                             const SbmdV4Resource &resource)
{
    if (!resource.seed.has_value() || !v4Driver->IsActivated())
    {
        return "";
    }

    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
    auto *ctx = MQuickJsRuntime::GetSharedContext();

    HandlerContext hctx;
    hctx.deviceUuid = deviceId;
    hctx.endpointId = endpointId;

    JSValue args = SbmdV4HandlerInvoker::BuildResourceArgs(ctx, hctx, resource.id, std::nullopt);
    auto result = SbmdV4HandlerInvoker::InvokeHandler(ctx, resource.seed->handler, args);

    if (!result.has_value())
    {
        icDebug("V4: Seed handler for resource '%s' returned no result", resource.id.c_str());
        return "";
    }

    SbmdV4HandlerInvoker::ExecuteOps(hctx, result->ops);

    // For seed, we expect a success terminal — check if any ops produced an updateResource
    // for this resource. If so, the seed value was set via ops. Return empty to avoid
    // double-setting.
    for (const auto &op : result->ops)
    {
        if (std::holds_alternative<ResultOp::UpdateResource>(op.data))
        {
            const auto &ur = std::get<ResultOp::UpdateResource>(op.data);

            if (ur.resource == resource.id)
            {
                return ur.value;
            }
        }
    }

    return "";
}

bool SpecBasedMatterDeviceDriver::CheckPrerequisitesV4(const SbmdV4Resource &resource, const MatterDevice &device)
{
    if (resource.prerequisites.empty())
    {
        return true;
    }

    // V4 prerequisites are alias names. We need the driver's alias map to resolve them
    // to (clusterId, attributeId) pairs. For now, prerequisites just check cluster presence.
    auto cache = device.GetDeviceDataCache();

    if (!cache)
    {
        icWarn("V4: No device data cache for device %s; prerequisites cannot be evaluated",
               device.GetDeviceId().c_str());
        return false;
    }

    const auto endpointIds = cache->GetEndpointIds();

    for (const auto &prereqAlias : resource.prerequisites)
    {
        // Prerequisites are cluster IDs specified as alias names.
        // For now, we just check that at least one endpoint has the cluster.
        // TODO: resolve aliases through registration's alias map for attribute-level prereqs

        // Try parsing as a numeric cluster ID first
        uint32_t clusterId = 0;

        try
        {
            clusterId = std::stoul(prereqAlias);
        }
        catch (...)
        {
            icDebug("V4: Prerequisite '%s' is not a numeric cluster ID, skipping", prereqAlias.c_str());
            continue;
        }

        bool clusterFound = false;

        for (auto endpointId : endpointIds)
        {
            if (cache->EndpointHasServerCluster(endpointId, clusterId))
            {
                clusterFound = true;
                break;
            }
        }

        if (!clusterFound)
        {
            icDebug("V4: Prerequisite cluster 0x%08" PRIx32 " not found on device %s",
                    clusterId,
                    device.GetDeviceId().c_str());

            return false;
        }
    }

    return true;
}

const SbmdV4Resource *SpecBasedMatterDeviceDriver::FindV4Resource(const char *endpointId, const char *resourceId) const
{
    if (!IsV4())
    {
        return nullptr;
    }

    const auto &reg = v4Driver->GetRegistration();

    for (const auto &endpoint : reg.endpoints)
    {
        // If endpointId is provided, match it
        if (endpointId != nullptr && !endpoint.id.empty() && endpoint.id != endpointId)
        {
            continue;
        }

        for (const auto &resource : endpoint.resources)
        {
            if (resource.id == resourceId)
            {
                return &resource;
            }
        }
    }

    return nullptr;
}

void SpecBasedMatterDeviceDriver::HandleV4ResourceOp(std::forward_list<std::promise<bool>> &promises,
                                                     MatterDevice &device,
                                                     icDeviceResource *resource,
                                                     const char *input,
                                                     char **readValue,
                                                     char **executeResponse,
                                                     chip::Messaging::ExchangeManager &exchangeMgr,
                                                     const chip::SessionHandle &sessionHandle,
                                                     const char *opType)
{
    // Extract endpoint ID and resource ID from the resource
    const char *endpointId = resource->endpointId;
    const char *resourceId = resource->id;

    const SbmdV4Resource *v4Resource = FindV4Resource(endpointId, resourceId);

    if (v4Resource == nullptr)
    {
        icError("V4: Resource %s not found in registration", resourceId);
        FailOperation(promises);
        return;
    }

    // Determine which handler to use
    const SbmdV4ResourceHandler *handler = nullptr;
    std::optional<std::string> inputValue;

    if (strcmp(opType, "read") == 0)
    {
        handler = v4Resource->read.has_value() ? &v4Resource->read.value() : nullptr;
    }
    else if (strcmp(opType, "write") == 0)
    {
        handler = v4Resource->write.has_value() ? &v4Resource->write.value() : nullptr;
        inputValue = input ? std::string(input) : std::string();
    }
    else if (strcmp(opType, "execute") == 0)
    {
        handler = v4Resource->execute.has_value() ? &v4Resource->execute.value() : nullptr;
        inputValue = input ? std::string(input) : std::string();
    }

    if (handler == nullptr)
    {
        icError("V4: No %s handler for resource %s", opType, resourceId);
        FailOperation(promises);
        return;
    }

    // Build handler context
    HandlerContext hctx;
    hctx.deviceUuid = device.GetDeviceId();
    hctx.endpointId = endpointId ? endpointId : "";

    // Invoke the handler under the JS mutex
    std::optional<ParsedResult> result;
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto *ctx = MQuickJsRuntime::GetSharedContext();

        JSValue args = SbmdV4HandlerInvoker::BuildResourceArgs(ctx, hctx, resourceId, inputValue);
        result = SbmdV4HandlerInvoker::InvokeHandler(ctx, handler->handler, args);
    }

    if (!result.has_value())
    {
        icError("V4: %s handler for resource %s returned no result", opType, resourceId);
        FailOperation(promises);
        return;
    }

    // Execute non-terminal ops
    SbmdV4HandlerInvoker::ExecuteOps(hctx, result->ops);

    // Handle the terminal
    ExecuteV4Terminal(promises, device, result->terminal, resource->uri, readValue, executeResponse,
                     exchangeMgr, sessionHandle);
}

void SpecBasedMatterDeviceDriver::ExecuteV4Terminal(std::forward_list<std::promise<bool>> &promises,
                                                    MatterDevice &device,
                                                    const ResultTerminal &terminal,
                                                    const char *uri,
                                                    char **readValue,
                                                    char **executeResponse,
                                                    chip::Messaging::ExchangeManager &exchangeMgr,
                                                    const chip::SessionHandle &sessionHandle)
{
    if (std::holds_alternative<ResultTerminal::Success>(terminal.data))
    {
        // Success — nothing more to do. For read ops, the value was set via ops.
        return;
    }

    if (std::holds_alternative<ResultTerminal::Error>(terminal.data))
    {
        const auto &err = std::get<ResultTerminal::Error>(terminal.data);
        icError("V4: Handler returned error: %s", err.message.c_str());
        FailOperation(promises);
        return;
    }

    if (std::holds_alternative<ResultTerminal::SendCommand>(terminal.data))
    {
        const auto &cmd = std::get<ResultTerminal::SendCommand>(terminal.data);

        // Resolve endpoint
        chip::EndpointId endpointId = 0;

        if (cmd.endpointId.has_value())
        {
            endpointId = static_cast<chip::EndpointId>(cmd.endpointId.value());
        }
        else if (!device.GetEndpointForCluster(cmd.clusterId, endpointId))
        {
            icError("V4: Failed to find endpoint for cluster 0x%x", cmd.clusterId);
            FailOperation(promises);
            return;
        }

        // Decode base64 TLV
        const uint8_t *tlvBuffer = nullptr;
        size_t tlvLength = 0;
        std::unique_ptr<uint8_t[]> decodedTlv;

        if (!cmd.tlvBase64.empty())
        {
            size_t maxLen = BASE64_MAX_DECODED_LEN(cmd.tlvBase64.size());
            decodedTlv = std::make_unique<uint8_t[]>(maxLen);
            uint16_t decoded = chip::Base64Decode(cmd.tlvBase64.c_str(),
                                                  static_cast<uint16_t>(cmd.tlvBase64.size()),
                                                  decodedTlv.get());

            if (decoded == UINT16_MAX)
            {
                icError("V4: Failed to base64 decode TLV for sendCommand");
                FailOperation(promises);
                return;
            }

            tlvBuffer = decodedTlv.get();
            tlvLength = decoded;
        }

        // Build SbmdCommand and send
        SbmdCommand sbmdCmd;
        sbmdCmd.clusterId = cmd.clusterId;
        sbmdCmd.commandId = cmd.commandId;
        sbmdCmd.name = "v4-command";

        if (cmd.timedInvokeTimeoutMs.has_value())
        {
            sbmdCmd.timedInvokeTimeoutMs = cmd.timedInvokeTimeoutMs.value();
        }

        if (!device.SendCommandFromTlv(promises, sbmdCmd, endpointId, tlvBuffer, tlvLength,
                                       exchangeMgr, sessionHandle, uri, executeResponse))
        {
            FailOperation(promises);
        }

        return;
    }

    if (std::holds_alternative<ResultTerminal::WriteAttribute>(terminal.data))
    {
        const auto &wa = std::get<ResultTerminal::WriteAttribute>(terminal.data);

        chip::EndpointId endpointId = 0;

        if (wa.endpointId.has_value())
        {
            endpointId = static_cast<chip::EndpointId>(wa.endpointId.value());
        }
        else if (!device.GetEndpointForCluster(wa.clusterId, endpointId))
        {
            icError("V4: Failed to find endpoint for cluster 0x%x", wa.clusterId);
            FailOperation(promises);
            return;
        }

        // Decode base64 TLV
        if (wa.tlvBase64.empty())
        {
            icError("V4: Empty TLV for writeAttribute");
            FailOperation(promises);
            return;
        }

        size_t maxLen = BASE64_MAX_DECODED_LEN(wa.tlvBase64.size());
        auto decodedTlv = std::make_unique<uint8_t[]>(maxLen);
        uint16_t decoded = chip::Base64Decode(wa.tlvBase64.c_str(),
                                              static_cast<uint16_t>(wa.tlvBase64.size()),
                                              decodedTlv.get());

        if (decoded == UINT16_MAX)
        {
            icError("V4: Failed to base64 decode TLV for writeAttribute");
            FailOperation(promises);
            return;
        }

        if (!device.WriteAttributeFromTlv(promises, endpointId, wa.clusterId, wa.attributeId,
                                          decodedTlv.get(), decoded, exchangeMgr, sessionHandle, uri))
        {
            FailOperation(promises);
        }

        return;
    }

    if (std::holds_alternative<ResultTerminal::RequestCommand>(terminal.data))
    {
        icWarn("V4: requestCommand terminal not yet implemented (deferred operations)");
        FailOperation(promises);
        return;
    }

    if (std::holds_alternative<ResultTerminal::ReadAttribute>(terminal.data))
    {
        icWarn("V4: readAttribute terminal not yet implemented (deferred operations)");
        FailOperation(promises);
        return;
    }

    icError("V4: Unknown terminal type");
    FailOperation(promises);
}

void SpecBasedMatterDeviceDriver::HandleV4AttributeReport(const std::string &deviceId,
                                                          chip::EndpointId endpointId,
                                                          chip::ClusterId clusterId,
                                                          chip::AttributeId attributeId,
                                                          chip::TLV::TLVReader &reader)
{
    if (!v4Driver || !v4Driver->IsActivated())
    {
        return;
    }

    // Look up matching handlers in the attribute dispatch table
    auto matches = v4Driver->GetAttributeDispatch().Lookup(clusterId, attributeId);

    if (matches.empty())
    {
        return;
    }

    // Encode TLV as base64 for passing to JS handlers
    // Read the TLV data into a buffer first
    const uint8_t *tlvStart = reader.GetReadPoint();

    // We need the raw TLV bytes. The reader is positioned at the element.
    // Get the total length including tag and value.
    // Use a simpler approach: copy the remaining buffer from the reader
    size_t remaining = reader.GetRemainingLength();

    if (remaining == 0)
    {
        icDebug("V4: Empty TLV data for attribute 0x%x", attributeId);
        return;
    }

    // Base64 encode the TLV data
    // The reader's read point is at the current element
    size_t maxBase64Len = BASE64_ENCODED_LEN(remaining) + 1;
    std::string tlvBase64(maxBase64Len, '\0');
    uint16_t encoded = chip::Base64Encode(tlvStart, static_cast<uint16_t>(remaining),
                                          tlvBase64.data());
    tlvBase64.resize(encoded);

    // Build handler context
    HandlerContext hctx;
    hctx.deviceUuid = deviceId;
    hctx.endpointId = std::to_string(endpointId);
    // TODO: populate clusterFeatureMaps from MatterDevice

    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
    auto *ctx = MQuickJsRuntime::GetSharedContext();

    for (const auto *entry : matches)
    {
        if (entry->handler == nullptr || JS_IsUndefined(entry->handler->handler))
        {
            continue;
        }

        JSValue args = SbmdV4HandlerInvoker::BuildAttributeArgs(ctx, hctx, clusterId, attributeId, tlvBase64);
        auto result = SbmdV4HandlerInvoker::InvokeHandler(ctx, entry->handler->handler, args);

        if (!result.has_value())
        {
            icWarn("V4: Attribute handler '%s' returned no result for cluster 0x%x attr 0x%x",
                   entry->handler->name.c_str(), clusterId, attributeId);
            continue;
        }

        // Execute ops (updateResource, setMetadata, etc.)
        SbmdV4HandlerInvoker::ExecuteOps(hctx, result->ops);

        // For attribute handlers, we typically expect a success terminal.
        // Error terminals are logged but don't abort other handler processing.
        if (std::holds_alternative<ResultTerminal::Error>(result->terminal.data))
        {
            const auto &err = std::get<ResultTerminal::Error>(result->terminal.data);
            icWarn("V4: Attribute handler '%s' returned error: %s",
                   entry->handler->name.c_str(), err.message.c_str());
        }
    }
}
