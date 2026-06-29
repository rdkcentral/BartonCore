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
#include "matter/sbmd/SbmdDriver.h"

#if defined(BCORE_USE_MQUICKJS)
#include "matter/sbmd/mquickjs/MQuickJsRuntime.h"
#include "matter/sbmd/mquickjs/SbmdHandlerInvoker.h"
#endif

#include <app/ConcreteAttributePath.h>
#include <cinttypes>
#include <lib/core/TLVReader.h>
#include <lib/core/TLVWriter.h>
#include <memory>

extern "C" {
#include <commonDeviceDefs.h>
#include <device/deviceModelHelper.h>
#include <device/icDeviceResource.h>
#include <deviceHelper.h>
#include <deviceService/resourceModes.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <icTypes/icHashMap.h>
#include <resourceTypes.h>
}

#include <subsystems/matter/Matter.h>

#include <lib/support/Base64.h>

using namespace barton;
using namespace std::chrono_literals;

#define BASE_SBMD_DRIVER_NAME "sbmd-"

SpecBasedMatterDeviceDriver::SpecBasedMatterDeviceDriver(SbmdDriver *driver) :
    MatterDeviceDriver((BASE_SBMD_DRIVER_NAME + driver->GetRegistration().name).c_str(),
                       driver->GetRegistration().barton.deviceClass.c_str(),
                       driver->GetRegistration().barton.deviceClassVersion),
    driver(driver)
{
    icDebug("Created SBMD driver for: %s", driver->GetName().c_str());

    // Register endpoint profile versions so deviceServiceDeviceNeedsReconfiguring()
    // can detect profile version changes and trigger reconfiguration.
    DeviceDriver *dd = GetDriver();
    const auto &endpoints = driver->GetRegistration().endpoints;

    for (const auto &endpoint : endpoints)
    {
        if (dd->endpointProfileVersions == nullptr)
        {
            dd->endpointProfileVersions = hashMapCreate();
        }

        auto *version = static_cast<uint8_t *>(malloc(sizeof(uint8_t)));
        *version = static_cast<uint8_t>(endpoint.profileVersion);
        hashMapPut(dd->endpointProfileVersions,
                   strdup(endpoint.profile.c_str()),
                   static_cast<uint16_t>(endpoint.profile.length() + 1),
                   version);
    }
}

uint16_t SpecBasedMatterDeviceDriver::GetSupportedVendorId() const
{
    return driver->GetRegistration().matter.vendorId.value_or(0);
}

uint16_t SpecBasedMatterDeviceDriver::GetSupportedProductId() const
{
    return driver->GetRegistration().matter.productId.value_or(0);
}

bool SpecBasedMatterDeviceDriver::IsVendorSpecificDriver() const
{
    const auto &m = driver->GetRegistration().matter;

    return m.vendorId.has_value() && m.productId.has_value();
}

std::vector<uint16_t> SpecBasedMatterDeviceDriver::GetSupportedDeviceTypes()
{
    return driver->GetRegistration().matter.deviceTypes;
}

bool SpecBasedMatterDeviceDriver::AddDevice(std::unique_ptr<MatterDevice> device)
{
    // The dispatch tables on the driver handle everything.
    device->SetFeatureClusters(driver->GetRegistration().matter.featureClusters);

    if (!device->ResolveEndpointMap(driver->GetRegistration().matter.deviceTypes))
    {
        icError("Failed to resolve endpoint map for device %s, no matching device types found",
                device->GetDeviceId().c_str());
        return false;
    }

    // Set the attribute callback so CacheCallback delegates to our dispatch tables
    device->SetAttributeCallback([this](const std::string &deviceId,
                                        chip::EndpointId endpointId,
                                        chip::ClusterId clusterId,
                                        chip::AttributeId attributeId,
                                        chip::TLV::TLVReader &reader) {
        HandleAttributeReport(deviceId, endpointId, clusterId, attributeId, reader);
    });

    // Set the event callback so CacheCallback delegates to our dispatch tables
    device->SetEventCallback(
        [this](const std::string &deviceId,
               chip::EndpointId endpointId,
               chip::ClusterId clusterId,
               chip::EventId eventId,
               chip::TLV::TLVReader &reader) { HandleEvent(deviceId, endpointId, clusterId, eventId, reader); });

    // Set the command callback for incoming (server-side) commands
    device->SetCommandCallback([this](const std::string &deviceId,
                                      chip::EndpointId endpointId,
                                      chip::ClusterId clusterId,
                                      chip::CommandId commandId,
                                      chip::TLV::TLVReader &reader) {
        // Encode TLV as base64 for HandleCommand
        uint8_t tlvBuf[1024];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuf, sizeof(tlvBuf));

        std::string tlvBase64;

        if (writer.CopyElement(chip::TLV::AnonymousTag(), reader) == CHIP_NO_ERROR)
        {
            uint32_t tlvLen = writer.GetLengthWritten();

            if (tlvLen > 0)
            {
                size_t maxBase64Len = BASE64_ENCODED_LEN(tlvLen) + 1;
                tlvBase64.resize(maxBase64Len, '\0');
                uint16_t encoded = chip::Base64Encode(tlvBuf, static_cast<uint16_t>(tlvLen), tlvBase64.data());
                tlvBase64.resize(encoded);
            }
        }

        HandleCommand(deviceId, endpointId, clusterId, commandId, tlvBase64);
    });

    // Register incoming command handlers for clusters in the command dispatch table
    for (uint32_t clusterId : driver->GetCommandDispatch().GetRegisteredClusterIds())
    {
        device->RegisterIncomingCommandHandler(static_cast<chip::ClusterId>(clusterId));
    }

    // Check prerequisites for resources
    const auto &reg = driver->GetRegistration();

    for (const auto &endpoint : reg.endpoints)
    {
        for (const auto &resource : endpoint.resources)
        {
            if (!CheckPrerequisites(resource, *device))
            {
                if (resource.optional)
                {
                    icDebug("Optional resource '%s' prerequisites not met, skipping", resource.id.c_str());
                    std::string key = endpoint.id + ":" + resource.id;
                    skippedOptionalResources[device->GetDeviceId()].insert(key);
                    continue;
                }

                icError("Required resource '%s' prerequisites not met, aborting commissioning", resource.id.c_str());

                return false;
            }
        }
    }

    return MatterDeviceDriver::AddDevice(std::move(device));
}

SubscriptionIntervalSecs SpecBasedMatterDeviceDriver::GetDesiredSubscriptionIntervalSecs()
{
    icDebug();

    const auto &r = driver->GetRegistration().reporting;

    return {r.minSecs, r.maxSecs};
}

void SpecBasedMatterDeviceDriver::DoConfigureDevice(std::forward_list<std::promise<bool>> &promises,
                                                    const std::string &deviceId,
                                                    const DeviceDescriptor *deviceDescriptor,
                                                    chip::Messaging::ExchangeManager &exchangeMgr,
                                                    const chip::SessionHandle &sessionHandle)
{
    icDebug("Reconfiguring SBMD device %s", deviceId.c_str());

    auto device = GetDevice(deviceId);

    if (device == nullptr)
    {
        icError("Device %s not found during reconfiguration", deviceId.c_str());
        FailOperation(promises);
        return;
    }

    // Update feature clusters in case the spec changed
    device->SetFeatureClusters(driver->GetRegistration().matter.featureClusters);

    // Re-resolve endpoint map against the (possibly updated) device type list
    if (!device->ResolveEndpointMap(driver->GetRegistration().matter.deviceTypes))
    {
        icError("Failed to resolve endpoint map for device %s during reconfiguration", deviceId.c_str());
        FailOperation(promises);
        return;
    }

    // Tear down old incoming command handlers and re-register from the current spec
    device->UnregisterIncomingCommandHandlers();

    for (uint32_t clusterId : driver->GetCommandDispatch().GetRegisteredClusterIds())
    {
        device->RegisterIncomingCommandHandler(static_cast<chip::ClusterId>(clusterId));
    }

    // Re-check prerequisites — update the set of skipped optional resources
    skippedOptionalResources.erase(deviceId);
    const auto &reg = driver->GetRegistration();

    for (const auto &endpoint : reg.endpoints)
    {
        for (const auto &resource : endpoint.resources)
        {
            if (!CheckPrerequisites(resource, *device))
            {
                if (resource.optional)
                {
                    icDebug("Optional resource '%s' prerequisites not met, skipping", resource.id.c_str());
                    std::string key = endpoint.id + ":" + resource.id;
                    skippedOptionalResources[deviceId].insert(key);
                    continue;
                }

                icError("Required resource '%s' prerequisites not met during reconfiguration", resource.id.c_str());
                FailOperation(promises);

                return;
            }
        }
    }
}

bool SpecBasedMatterDeviceDriver::DoRegisterResources(icDevice *device)
{
    return DoRegisterDriverResources(device);
}

void SpecBasedMatterDeviceDriver::DoSynchronizeDevice(std::forward_list<std::promise<bool>> &promises,
                                                      const std::string &deviceId,
                                                      chip::Messaging::ExchangeManager &exchangeMgr,
                                                      const chip::SessionHandle &sessionHandle)
{
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

    HandleResourceOp(promises, *device, resource, nullptr, value, nullptr, exchangeMgr, sessionHandle, "read");
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

    HandleResourceOp(promises, *device, resource, newValue, nullptr, nullptr, exchangeMgr, sessionHandle, "write");

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

    HandleResourceOp(promises, *device, resource, arg, nullptr, response, exchangeMgr, sessionHandle, "execute");
}

std::optional<uint8_t> SpecBasedMatterDeviceDriver::ConvertModesToBitmask(const std::vector<std::string> &modes)
{
    // dynamic and emitEvents are on by default; "static" and "noEvents" opt out.
    uint8_t bitmask = RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_DYNAMIC_CAPABLE | RESOURCE_MODE_EMIT_EVENTS;

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
        else if (mode == "static")
        {
            bitmask &= ~(RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_DYNAMIC_CAPABLE);
        }
        else if (mode == "noEvents")
        {
            bitmask &= ~RESOURCE_MODE_EMIT_EVENTS;
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
            icError("Unsupported resource mode: %s", mode.c_str());

            return std::nullopt;
        }
    }

    return bitmask;
}

// =============================================================================
// Driver-based implementation methods
// =============================================================================

bool SpecBasedMatterDeviceDriver::DoRegisterDriverResources(icDevice *device)
{
    bool result = true;
    const auto &reg = driver->GetRegistration();
    const auto *skipped =
        skippedOptionalResources.count(device->uuid) ? &skippedOptionalResources[device->uuid] : nullptr;

    icDebug("Registering resources for device %s", device->uuid);

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
                    icError("Failed to create endpoint '%s' with profile '%s'",
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

            auto modeResult = ConvertModesToBitmask(resource.modes);

            if (!modeResult.has_value())
            {
                icError("Invalid modes for resource '%s' on endpoint '%s'", resource.id.c_str(), endpoint.id.c_str());
                result = false;

                continue;
            }

            uint8_t resourceMode = *modeResult;

            if (resource.execute.has_value())
            {
                resourceMode |= RESOURCE_MODE_EXECUTABLE;
            }

            // Resources without explicit read handlers are updated via attribute subscriptions,
            // so use CACHING_POLICY_ALWAYS to return the DB-cached value on read.
            // Resources with explicit read handlers use CACHING_POLICY_NEVER so the driver is called.
            ResourceCachingPolicy cachingPolicy =
                resource.read.has_value() ? CACHING_POLICY_NEVER : CACHING_POLICY_ALWAYS;

            // Seed initial value if there's a seed handler
            const char *initialValue = nullptr;
            std::string seedValue;

            if (resource.seed.has_value())
            {
                seedValue = InvokeSeedHandler(device->uuid, endpoint.id, resource);

                if (!seedValue.empty())
                {
                    initialValue = seedValue.c_str();
                }
            }

            result &= createEndpointResource(
                          ep, resource.id.c_str(), initialValue, resource.type.c_str(), resourceMode, cachingPolicy) !=
                      nullptr;
        }
    }

    return result;
}

void SpecBasedMatterDeviceDriver::SeedInitialResourceValues(const std::string &deviceId)
{
    icDebug("Seeding initial resource values for device %s", deviceId.c_str());

    const auto &reg = driver->GetRegistration();
    const auto *skipped = skippedOptionalResources.count(deviceId) ? &skippedOptionalResources[deviceId] : nullptr;

    auto matterDevice = GetDevice(deviceId);

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

            std::string seedValue = InvokeSeedHandler(deviceId, endpoint.id, resource, matterDevice.get());

            if (!seedValue.empty())
            {
                updateResource(deviceId.c_str(), endpoint.id.c_str(), resource.id.c_str(), seedValue.c_str(), nullptr);
            }
        }
    }
}

std::string SpecBasedMatterDeviceDriver::InvokeSeedHandler(const std::string &deviceId,
                                                           const std::string &endpointId,
                                                           const SbmdResource &resource,
                                                           MatterDevice *device)
{
    if (!resource.seed.has_value() || !driver->IsActivated())
    {
        return "";
    }

    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
    auto *ctx = MQuickJsRuntime::GetSharedContext();

    HandlerContext hctx;
    hctx.deviceUuid = deviceId;
    hctx.endpointId = endpointId;

    JSValue args = SbmdHandlerInvoker::BuildResourceArgs(ctx, hctx, resource.id, std::nullopt);

    // GC-root args across AddSupplements (which allocates) and InvokeHandler
    JSGCRef argsRef {};
    JS_AddGCRef(ctx, &argsRef);
    argsRef.val = args;


    if (device != nullptr)
    {
        SbmdHandlerInvoker::AddSupplements(ctx,
                                           args,
                                           resource.seed->supplements,
                                           MakeAttrFetcher(*device),
                                           MakeResFetcher(deviceId),
                                           MakePersistFetcher(deviceId),
                                           MakeTransientFetcher(deviceId));
    }

    auto result = SbmdHandlerInvoker::InvokeHandler(ctx, resource.seed->handler, args);

    JS_DeleteGCRef(ctx, &argsRef);

    if (!result.has_value())
    {
        icDebug("Seed handler for resource '%s' returned no result", resource.id.c_str());
        return "";
    }

    SbmdHandlerInvoker::ExecuteOps(hctx, result->ops, MakeTransientSetter(deviceId));

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

bool SpecBasedMatterDeviceDriver::CheckPrerequisites(const SbmdResource &resource, const MatterDevice &device)
{
    if (resource.prerequisites.empty())
    {
        return true;
    }

    // Prerequisites are alias names. We need the driver's alias map to resolve them
    // to (clusterId, attributeId) pairs. For now, prerequisites just check cluster presence.
    auto cache = device.GetDeviceDataCache();

    if (!cache)
    {
        icWarn("No device data cache for device %s; prerequisites cannot be evaluated", device.GetDeviceId().c_str());
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
        char *endPtr = nullptr;
        unsigned long parsed = strtoul(prereqAlias.c_str(), &endPtr, 0);

        if (endPtr == prereqAlias.c_str() || *endPtr != '\0')
        {
            icDebug("Prerequisite '%s' is not a numeric cluster ID, skipping", prereqAlias.c_str());
            continue;
        }

        clusterId = static_cast<uint32_t>(parsed);

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
            icDebug(
                "Prerequisite cluster 0x%08" PRIx32 " not found on device %s", clusterId, device.GetDeviceId().c_str());

            return false;
        }
    }

    return true;
}

const SbmdResource *SpecBasedMatterDeviceDriver::FindDriverResource(const char *endpointId,
                                                                    const char *resourceId) const
{
    const auto &reg = driver->GetRegistration();

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

void SpecBasedMatterDeviceDriver::HandleResourceOp(std::forward_list<std::promise<bool>> &promises,
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

    const SbmdResource *driverResource = FindDriverResource(endpointId, resourceId);

    if (driverResource == nullptr)
    {
        icError("Resource %s not found in registration", resourceId);
        FailOperation(promises);
        return;
    }

    // Determine which handler to use
    const SbmdResourceHandler *handler = nullptr;
    std::optional<std::string> inputValue;

    if (strcmp(opType, "read") == 0)
    {
        handler = driverResource->read.has_value() ? &driverResource->read.value() : nullptr;
    }
    else if (strcmp(opType, "write") == 0)
    {
        handler = driverResource->write.has_value() ? &driverResource->write.value() : nullptr;
        inputValue = input ? std::string(input) : std::string();
    }
    else if (strcmp(opType, "execute") == 0)
    {
        handler = driverResource->execute.has_value() ? &driverResource->execute.value() : nullptr;
        inputValue = input ? std::string(input) : std::string();
    }

    if (handler == nullptr)
    {
        if (strcmp(opType, "read") == 0)
        {
            // No explicit read handler — resource value is populated by attribute subscription.
            // With CACHING_POLICY_ALWAYS this path shouldn't normally be reached,
            // but return success with the cached value as a safety net.
            icDebug("No read handler for resource %s, returning cached value", resourceId);

            if (readValue != nullptr && resource->value != nullptr)
            {
                *readValue = strdup(resource->value);
            }

            std::promise<bool> ok;
            ok.set_value(true);
            promises.push_front(std::move(ok));
            return;
        }

        icError("No %s handler for resource %s", opType, resourceId);
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

        JSValue args = SbmdHandlerInvoker::BuildResourceArgs(ctx, hctx, resourceId, inputValue);

        // GC-root args across AddSupplements (which allocates) and InvokeHandler
        JSGCRef argsRef {};
        JS_AddGCRef(ctx, &argsRef);
        argsRef.val = args;


        SbmdHandlerInvoker::AddSupplements(ctx,
                                           args,
                                           handler->supplements,
                                           MakeAttrFetcher(device),
                                           MakeResFetcher(device.GetDeviceId()),
                                           MakePersistFetcher(device.GetDeviceId()),
                                           MakeTransientFetcher(device.GetDeviceId()));

        result = SbmdHandlerInvoker::InvokeHandler(ctx, handler->handler, args);

        JS_DeleteGCRef(ctx, &argsRef);
    }

    if (!result.has_value())
    {
        icError("%s handler for resource %s returned no result", opType, resourceId);
        FailOperation(promises);
        return;
    }

    // Execute non-terminal ops
    SbmdHandlerInvoker::ExecuteOps(hctx, result->ops, MakeTransientSetter(hctx.deviceUuid));

    // Handle the terminal
    ExecuteTerminal(promises,
                    device,
                    result->terminal,
                    hctx,
                    resource->uri,
                    readValue,
                    executeResponse,
                    exchangeMgr,
                    sessionHandle);
}

void SpecBasedMatterDeviceDriver::ExecuteTerminal(std::forward_list<std::promise<bool>> &promises,
                                                  MatterDevice &device,
                                                  const ResultTerminal &terminal,
                                                  const HandlerContext &hctx,
                                                  const char *uri,
                                                  char **readValue,
                                                  char **executeResponse,
                                                  chip::Messaging::ExchangeManager &exchangeMgr,
                                                  const chip::SessionHandle &sessionHandle)
{
    if (std::holds_alternative<ResultTerminal::Success>(terminal.data))
    {
        const auto &success = std::get<ResultTerminal::Success>(terminal.data);

        if (!success.value.empty())
        {
            if (executeResponse != nullptr)
            {
                *executeResponse = strdup(success.value.c_str());
            }
            else if (readValue != nullptr)
            {
                *readValue = strdup(success.value.c_str());
            }
        }

        return;
    }

    if (std::holds_alternative<ResultTerminal::Error>(terminal.data))
    {
        const auto &err = std::get<ResultTerminal::Error>(terminal.data);
        icError("Handler returned error: %s", err.message.c_str());
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
            icError("Failed to find endpoint for cluster 0x%x", cmd.clusterId);
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
            uint16_t decoded = chip::Base64Decode(
                cmd.tlvBase64.c_str(), static_cast<uint16_t>(cmd.tlvBase64.size()), decodedTlv.get());

            if (decoded == UINT16_MAX)
            {
                icError("Failed to base64 decode TLV for sendCommand");
                FailOperation(promises);
                return;
            }

            tlvBuffer = decodedTlv.get();
            tlvLength = decoded;
        }

        if (!device.SendCommandFromTlv(promises,
                                       cmd.clusterId,
                                       cmd.commandId,
                                       cmd.timedInvokeTimeoutMs,
                                       endpointId,
                                       tlvBuffer,
                                       tlvLength,
                                       exchangeMgr,
                                       sessionHandle,
                                       uri,
                                       executeResponse))
        {
            FailOperation(promises);
            return;
        }

        // Set the response value optimistically — if the command fails,
        // the caller ignores executeResponse.
        if (!cmd.successValue.empty() && executeResponse != nullptr)
        {
            *executeResponse = strdup(cmd.successValue.c_str());
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
            icError("Failed to find endpoint for cluster 0x%x", wa.clusterId);
            FailOperation(promises);
            return;
        }

        // Decode base64 TLV
        if (wa.tlvBase64.empty())
        {
            icError("Empty TLV for writeAttribute");
            FailOperation(promises);
            return;
        }

        size_t maxLen = BASE64_MAX_DECODED_LEN(wa.tlvBase64.size());
        auto decodedTlv = std::make_unique<uint8_t[]>(maxLen);
        uint16_t decoded =
            chip::Base64Decode(wa.tlvBase64.c_str(), static_cast<uint16_t>(wa.tlvBase64.size()), decodedTlv.get());

        if (decoded == UINT16_MAX)
        {
            icError("Failed to base64 decode TLV for writeAttribute");
            FailOperation(promises);
            return;
        }

        if (!device.WriteAttributeFromTlv(promises,
                                          endpointId,
                                          wa.clusterId,
                                          wa.attributeId,
                                          decodedTlv.get(),
                                          decoded,
                                          exchangeMgr,
                                          sessionHandle,
                                          uri))
        {
            FailOperation(promises);
        }

        return;
    }

    if (std::holds_alternative<ResultTerminal::RequestCommand>(terminal.data))
    {
        const auto &cmd = std::get<ResultTerminal::RequestCommand>(terminal.data);
        ExecuteRequestCommand(promises, device, cmd, hctx, readValue, executeResponse, exchangeMgr, sessionHandle);
        return;
    }

    if (std::holds_alternative<ResultTerminal::ReadAttribute>(terminal.data))
    {
        const auto &ra = std::get<ResultTerminal::ReadAttribute>(terminal.data);
        ExecuteReadAttribute(promises, device, ra, hctx, readValue, executeResponse, exchangeMgr, sessionHandle);
        return;
    }

    icError("Unknown terminal type");
    FailOperation(promises);
}

// ================================================================
// Deferred operations — requestCommand and readAttribute terminals
// ================================================================

void SpecBasedMatterDeviceDriver::ExecuteRequestCommand(std::forward_list<std::promise<bool>> &promises,
                                                        MatterDevice &device,
                                                        const ResultTerminal::RequestCommand &cmd,
                                                        const HandlerContext &hctx,
                                                        char **readValue,
                                                        char **executeResponse,
                                                        chip::Messaging::ExchangeManager &exchangeMgr,
                                                        const chip::SessionHandle &sessionHandle)
{
    // Resolve endpoint
    chip::EndpointId endpointId = 0;

    if (cmd.endpointId.has_value())
    {
        endpointId = static_cast<chip::EndpointId>(cmd.endpointId.value());
    }
    else if (!device.GetEndpointForCluster(cmd.clusterId, endpointId))
    {
        icError("Failed to find endpoint for cluster 0x%x (requestCommand)", cmd.clusterId);
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
        uint16_t decoded =
            chip::Base64Decode(cmd.tlvBase64.c_str(), static_cast<uint16_t>(cmd.tlvBase64.size()), decodedTlv.get());

        if (decoded == UINT16_MAX)
        {
            icError("Failed to base64 decode TLV for requestCommand");
            FailOperation(promises);
            return;
        }

        tlvBuffer = decodedTlv.get();
        tlvLength = decoded;
    }

    // Create parking promise
    promises.emplace_front();
    auto &parkingPromise = promises.front();

    // Register pending operation
    uint64_t pendingId = nextPendingId++;
    PendingOperation pending;
    pending.id = pendingId;
    pending.parkingPromise = &parkingPromise;
    pending.clusterId = cmd.clusterId;
    pending.responseCommandId = cmd.responseCommandId;
    pending.handlerContext = hctx;
    pending.device = &device;
    pending.exchangeMgr = &exchangeMgr;
    pending.sessionHandle.Grab(sessionHandle);
    pending.readValue = readValue;
    pending.executeResponse = executeResponse;

    // Set overall deadline: per-operation timeoutMs > driver defaultTimeoutMs > system default
    uint32_t overallMs = PendingOperation::DEFAULT_OVERALL_TIMEOUT_MS;

    if (cmd.timeoutMs.has_value())
    {
        overallMs = cmd.timeoutMs.value();
    }
    else if (driver && driver->GetRegistration().matter.defaultTimeoutMs.has_value())
    {
        overallMs = driver->GetRegistration().matter.defaultTimeoutMs.value();
    }

    pending.overallDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(overallMs);
    pending.deferralDepth = 0;

    // GC-root the deferred handlers so they survive until we need them
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto *ctx = MQuickJsRuntime::GetSharedContext();

        if (!JS_IsUndefined(cmd.onResponse))
        {
            JS_AddGCRef(ctx, &pending.onResponseRef);
            pending.onResponseRef.val = cmd.onResponse;

            pending.onResponseRooted = true;
        }

        if (!JS_IsUndefined(cmd.onError))
        {
            JS_AddGCRef(ctx, &pending.onErrorRef);
            pending.onErrorRef.val = cmd.onError;

            pending.onErrorRooted = true;
        }

        if (!JS_IsUndefined(cmd.context))
        {
            JS_AddGCRef(ctx, &pending.contextRef);
            pending.contextRef.val = cmd.context;

            pending.contextRooted = true;
        }
    }

    pendingOperations.emplace(pendingId, std::move(pending));

    // Send the command with deferred callbacks
    bool sent = device.SendCommandWithCallbacks(
        cmd.clusterId,
        cmd.commandId,
        cmd.timedInvokeTimeoutMs,
        endpointId,
        tlvBuffer,
        tlvLength,
        exchangeMgr,
        sessionHandle,
        [this, pendingId](const chip::app::ConcreteCommandPath &path, chip::TLV::TLVReader *data) {
            HandleDeferredCommandResponse(pendingId, path, data);
        },
        [this, pendingId](CHIP_ERROR error) { HandleDeferredCommandError(pendingId, error); });

    if (!sent)
    {
        icError("Failed to send deferred command");
        CompletePendingOperation(pendingId, false);
    }
}

void SpecBasedMatterDeviceDriver::ExecuteReadAttribute(std::forward_list<std::promise<bool>> &promises,
                                                       MatterDevice &device,
                                                       const ResultTerminal::ReadAttribute &ra,
                                                       const HandlerContext &hctx,
                                                       char **readValue,
                                                       char **executeResponse,
                                                       chip::Messaging::ExchangeManager &exchangeMgr,
                                                       const chip::SessionHandle &sessionHandle)
{
    // Resolve endpoint
    chip::EndpointId endpointId = 0;

    if (ra.endpointId.has_value())
    {
        endpointId = static_cast<chip::EndpointId>(ra.endpointId.value());
    }
    else if (!device.GetEndpointForCluster(ra.clusterId, endpointId))
    {
        icError("Failed to find endpoint for cluster 0x%x (readAttribute)", ra.clusterId);
        FailOperation(promises);
        return;
    }

    // Read from cache
    chip::TLV::TLVReader reader;
    CHIP_ERROR err = device.GetCachedAttributeData(endpointId, ra.clusterId, ra.attributeId, reader);

    std::optional<ParsedResult> result;

    if (err != CHIP_NO_ERROR)
    {
        icWarn(
            "Cache miss for cluster 0x%x attr 0x%x (readAttribute): %s", ra.clusterId, ra.attributeId, err.AsString());

        // Call onError handler
        if (!JS_IsUndefined(ra.onError))
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto *ctx = MQuickJsRuntime::GetSharedContext();

            JSValue args = SbmdHandlerInvoker::BuildDeferredErrorArgs(
                ctx, hctx, "readFailed", "Attribute not in cache", -1, ra.context);
            result = SbmdHandlerInvoker::InvokeHandler(ctx, ra.onError, args);
        }

        if (!result.has_value())
        {
            FailOperation(promises);
            return;
        }
    }
    else
    {
        // Encode cached TLV as base64
        uint8_t tlvBuf[256];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuf, sizeof(tlvBuf));

        if (writer.CopyElement(chip::TLV::AnonymousTag(), reader) != CHIP_NO_ERROR)
        {
            icError("Failed to copy cached attribute TLV for readAttribute");
            FailOperation(promises);
            return;
        }

        uint32_t tlvLen = writer.GetLengthWritten();
        size_t maxBase64Len = BASE64_ENCODED_LEN(tlvLen) + 1;
        std::string tlvBase64(maxBase64Len, '\0');
        uint16_t encoded = chip::Base64Encode(tlvBuf, static_cast<uint16_t>(tlvLen), tlvBase64.data());
        tlvBase64.resize(encoded);

        // Call onResponse handler
        if (!JS_IsUndefined(ra.onResponse))
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto *ctx = MQuickJsRuntime::GetSharedContext();

            JSValue args = SbmdHandlerInvoker::BuildAttributeReadResponseArgs(
                ctx, hctx, ra.clusterId, ra.attributeId, tlvBase64, ra.context);
            result = SbmdHandlerInvoker::InvokeHandler(ctx, ra.onResponse, args);
        }

        if (!result.has_value())
        {
            icError("readAttribute onResponse handler returned no result");
            FailOperation(promises);
            return;
        }
    }

    // Execute the response handler's non-terminal ops
    SbmdHandlerInvoker::ExecuteOps(hctx, result->ops, MakeTransientSetter(hctx.deviceUuid));

    // Execute the terminal — may recurse into another deferred terminal
    ExecuteTerminal(promises,
                    device,
                    result->terminal,
                    hctx,
                    "(deferred-readAttribute)",
                    readValue,
                    executeResponse,
                    exchangeMgr,
                    sessionHandle);
}

void SpecBasedMatterDeviceDriver::HandleDeferredCommandResponse(uint64_t pendingId,
                                                                const chip::app::ConcreteCommandPath &path,
                                                                chip::TLV::TLVReader *data)
{
    auto it = pendingOperations.find(pendingId);

    if (it == pendingOperations.end())
    {
        icWarn("Received deferred response for unknown pending operation %" PRIu64, pendingId);
        return;
    }

    PendingOperation &pending = it->second;

    // Check overall deadline
    if (std::chrono::steady_clock::now() > pending.overallDeadline)
    {
        icWarn("Deferred operation %" PRIu64 " exceeded overall deadline", pendingId);

        // Call onError with timeout
        std::optional<ParsedResult> errorResult;
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto *ctx = MQuickJsRuntime::GetSharedContext();

            if (pending.onErrorRooted)
            {
                JSValue args = SbmdHandlerInvoker::BuildDeferredErrorArgs(ctx,
                                                                          pending.handlerContext,
                                                                          "timeout",
                                                                          "Overall operation deadline exceeded",
                                                                          -1,
                                                                          pending.contextRooted ? pending.contextRef.val
                                                                                                : JS_UNDEFINED);
                errorResult = SbmdHandlerInvoker::InvokeHandler(ctx, pending.onErrorRef.val, args);
            }
        }

        if (errorResult.has_value())
        {
            SbmdHandlerInvoker::ExecuteOps(
                pending.handlerContext, errorResult->ops, MakeTransientSetter(pending.handlerContext.deviceUuid));
        }

        CompletePendingOperation(pendingId, false);
        return;
    }

    // Encode response data as base64
    std::string tlvBase64;

    if (data != nullptr)
    {
        uint8_t tlvBuf[1024];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuf, sizeof(tlvBuf));

        if (writer.CopyElement(chip::TLV::AnonymousTag(), *data) == CHIP_NO_ERROR)
        {
            uint32_t tlvLen = writer.GetLengthWritten();
            size_t maxBase64Len = BASE64_ENCODED_LEN(tlvLen) + 1;
            tlvBase64.resize(maxBase64Len, '\0');
            uint16_t encoded = chip::Base64Encode(tlvBuf, static_cast<uint16_t>(tlvLen), tlvBase64.data());
            tlvBase64.resize(encoded);
        }
    }

    // Invoke the onResponse handler
    std::optional<ParsedResult> result;
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto *ctx = MQuickJsRuntime::GetSharedContext();

        if (pending.onResponseRooted)
        {
            JSValue args = SbmdHandlerInvoker::BuildCommandResponseArgs(ctx,
                                                                        pending.handlerContext,
                                                                        path.mClusterId,
                                                                        path.mCommandId,
                                                                        tlvBase64,
                                                                        pending.contextRooted ? pending.contextRef.val
                                                                                              : JS_UNDEFINED);
            result = SbmdHandlerInvoker::InvokeHandler(ctx, pending.onResponseRef.val, args);
        }
    }

    if (!result.has_value())
    {
        icError("Deferred onResponse handler returned no result for operation %" PRIu64, pendingId);
        CompletePendingOperation(pendingId, false);
        return;
    }

    // Execute non-terminal ops
    SbmdHandlerInvoker::ExecuteOps(
        pending.handlerContext, result->ops, MakeTransientSetter(pending.handlerContext.deviceUuid));

    // Continue the chain
    ContinueDeferredChain(pending, *result);
}

void SpecBasedMatterDeviceDriver::HandleDeferredCommandError(uint64_t pendingId, CHIP_ERROR error)
{
    auto it = pendingOperations.find(pendingId);

    if (it == pendingOperations.end())
    {
        icWarn("Received deferred error for unknown pending operation %" PRIu64, pendingId);
        return;
    }

    PendingOperation &pending = it->second;

    icError("Deferred command failed for operation %" PRIu64 ": %s", pendingId, error.AsString());

    // Call onError handler
    std::optional<ParsedResult> errorResult;
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto *ctx = MQuickJsRuntime::GetSharedContext();

        if (pending.onErrorRooted)
        {
            JSValue args = SbmdHandlerInvoker::BuildDeferredErrorArgs(ctx,
                                                                      pending.handlerContext,
                                                                      "commandFailed",
                                                                      error.AsString(),
                                                                      static_cast<int32_t>(error.AsInteger()),
                                                                      pending.contextRooted ? pending.contextRef.val
                                                                                            : JS_UNDEFINED);
            errorResult = SbmdHandlerInvoker::InvokeHandler(ctx, pending.onErrorRef.val, args);
        }
    }

    if (errorResult.has_value())
    {
        SbmdHandlerInvoker::ExecuteOps(
            pending.handlerContext, errorResult->ops, MakeTransientSetter(pending.handlerContext.deviceUuid));

        // Check if onError returned a recovery terminal
        if (!std::holds_alternative<ResultTerminal::Error>(errorResult->terminal.data))
        {
            ContinueDeferredChain(pending, *errorResult);
            return;
        }
    }

    CompletePendingOperation(pendingId, false);
}

void SpecBasedMatterDeviceDriver::ContinueDeferredChain(PendingOperation &pending, const ParsedResult &result)
{
    uint64_t pendingId = pending.id;

    // Check deferral depth
    if (pending.deferralDepth >= PendingOperation::MAX_DEFERRAL_DEPTH)
    {
        icError("Deferred operation %" PRIu64 " exceeded max deferral depth (%u)",
                pendingId,
                PendingOperation::MAX_DEFERRAL_DEPTH);
        CompletePendingOperation(pendingId, false);
        return;
    }

    // Handle the terminal
    if (std::holds_alternative<ResultTerminal::Success>(result.terminal.data))
    {
        const auto &success = std::get<ResultTerminal::Success>(result.terminal.data);

        if (!success.value.empty())
        {
            if (pending.executeResponse != nullptr)
            {
                *pending.executeResponse = strdup(success.value.c_str());
            }
            else if (pending.readValue != nullptr)
            {
                *pending.readValue = strdup(success.value.c_str());
            }
        }

        CompletePendingOperation(pendingId, true);
        return;
    }

    if (std::holds_alternative<ResultTerminal::Error>(result.terminal.data))
    {
        const auto &err = std::get<ResultTerminal::Error>(result.terminal.data);
        icError("Deferred handler returned error: %s", err.message.c_str());
        CompletePendingOperation(pendingId, false);
        return;
    }

    if (std::holds_alternative<ResultTerminal::SendCommand>(result.terminal.data))
    {
        const auto &cmd = std::get<ResultTerminal::SendCommand>(result.terminal.data);

        // Resolve endpoint
        chip::EndpointId endpointId = 0;

        if (cmd.endpointId.has_value())
        {
            endpointId = static_cast<chip::EndpointId>(cmd.endpointId.value());
        }
        else if (!pending.device->GetEndpointForCluster(cmd.clusterId, endpointId))
        {
            icError("Failed to find endpoint for cluster 0x%x in deferred chain", cmd.clusterId);
            CompletePendingOperation(pendingId, false);
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
            uint16_t decoded = chip::Base64Decode(
                cmd.tlvBase64.c_str(), static_cast<uint16_t>(cmd.tlvBase64.size()), decodedTlv.get());

            if (decoded == UINT16_MAX)
            {
                CompletePendingOperation(pendingId, false);
                return;
            }

            tlvBuffer = decodedTlv.get();
            tlvLength = decoded;
        }

        // Send the command — this resolves immediately via OnDone
        std::forward_list<std::promise<bool>> tempPromises;

        auto session = pending.sessionHandle.Get();

        if (!session.HasValue())
        {
            icError("Session expired before deferred sendCommand for operation %" PRIu64, pendingId);
            CompletePendingOperation(pendingId, false);
            return;
        }

        if (!pending.device->SendCommandFromTlv(tempPromises,
                                                cmd.clusterId,
                                                cmd.commandId,
                                                cmd.timedInvokeTimeoutMs,
                                                endpointId,
                                                tlvBuffer,
                                                tlvLength,
                                                *pending.exchangeMgr,
                                                session.Value(),
                                                nullptr,
                                                pending.executeResponse))
        {
            CompletePendingOperation(pendingId, false);
            return;
        }

        // Set the response value optimistically
        if (!cmd.successValue.empty() && pending.executeResponse != nullptr)
        {
            *pending.executeResponse = strdup(cmd.successValue.c_str());
        }

        // The command was sent. Completion comes via the command's own promise.
        // The parking promise remains pending until that resolves.
        // For sendCommand in a chain, we complete the parking promise when the
        // command completes, which happens via OnDone → promise.set_value(true).
        // We need to wait for that promise and then complete ours.
        // Actually, the tempPromises will be resolved when the command completes.
        // We'll complete the parking promise as success since the command was accepted.
        CompletePendingOperation(pendingId, true);
        return;
    }

    if (std::holds_alternative<ResultTerminal::WriteAttribute>(result.terminal.data))
    {
        const auto &wa = std::get<ResultTerminal::WriteAttribute>(result.terminal.data);

        chip::EndpointId endpointId = 0;

        if (wa.endpointId.has_value())
        {
            endpointId = static_cast<chip::EndpointId>(wa.endpointId.value());
        }
        else if (!pending.device->GetEndpointForCluster(wa.clusterId, endpointId))
        {
            CompletePendingOperation(pendingId, false);
            return;
        }

        if (wa.tlvBase64.empty())
        {
            CompletePendingOperation(pendingId, false);
            return;
        }

        size_t maxLen = BASE64_MAX_DECODED_LEN(wa.tlvBase64.size());
        auto decodedTlv = std::make_unique<uint8_t[]>(maxLen);
        uint16_t decoded =
            chip::Base64Decode(wa.tlvBase64.c_str(), static_cast<uint16_t>(wa.tlvBase64.size()), decodedTlv.get());

        if (decoded == UINT16_MAX)
        {
            CompletePendingOperation(pendingId, false);
            return;
        }

        std::forward_list<std::promise<bool>> tempPromises;

        auto session = pending.sessionHandle.Get();

        if (!session.HasValue())
        {
            icError("Session expired before deferred writeAttribute for operation %" PRIu64, pendingId);
            CompletePendingOperation(pendingId, false);
            return;
        }

        if (!pending.device->WriteAttributeFromTlv(tempPromises,
                                                   endpointId,
                                                   wa.clusterId,
                                                   wa.attributeId,
                                                   decodedTlv.get(),
                                                   decoded,
                                                   *pending.exchangeMgr,
                                                   session.Value(),
                                                   nullptr))
        {
            CompletePendingOperation(pendingId, false);
            return;
        }

        CompletePendingOperation(pendingId, true);
        return;
    }

    if (std::holds_alternative<ResultTerminal::RequestCommand>(result.terminal.data))
    {
        const auto &cmd = std::get<ResultTerminal::RequestCommand>(result.terminal.data);

        // Re-arm: release old GC roots, root new handlers
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto *ctx = MQuickJsRuntime::GetSharedContext();

            if (pending.onResponseRooted)
            {
                JS_DeleteGCRef(ctx, &pending.onResponseRef);
                pending.onResponseRooted = false;
            }

            if (pending.onErrorRooted)
            {
                JS_DeleteGCRef(ctx, &pending.onErrorRef);
                pending.onErrorRooted = false;
            }

            if (!JS_IsUndefined(cmd.onResponse))
            {
                JS_AddGCRef(ctx, &pending.onResponseRef);
                pending.onResponseRef.val = cmd.onResponse;

                pending.onResponseRooted = true;
            }

            if (!JS_IsUndefined(cmd.onError))
            {
                JS_AddGCRef(ctx, &pending.onErrorRef);
                pending.onErrorRef.val = cmd.onError;

                pending.onErrorRooted = true;
            }
        }

        pending.clusterId = cmd.clusterId;
        pending.responseCommandId = cmd.responseCommandId;
        pending.deferralDepth++;

        // Resolve endpoint
        chip::EndpointId endpointId = 0;

        if (cmd.endpointId.has_value())
        {
            endpointId = static_cast<chip::EndpointId>(cmd.endpointId.value());
        }
        else if (!pending.device->GetEndpointForCluster(cmd.clusterId, endpointId))
        {
            icError("Failed to find endpoint for cluster 0x%x in deferred re-arm", cmd.clusterId);
            CompletePendingOperation(pendingId, false);
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
            uint16_t decoded = chip::Base64Decode(
                cmd.tlvBase64.c_str(), static_cast<uint16_t>(cmd.tlvBase64.size()), decodedTlv.get());

            if (decoded == UINT16_MAX)
            {
                CompletePendingOperation(pendingId, false);
                return;
            }

            tlvBuffer = decodedTlv.get();
            tlvLength = decoded;
        }

        // Send next command with deferred callbacks
        auto session = pending.sessionHandle.Get();

        if (!session.HasValue())
        {
            icError("Session expired before deferred requestCommand re-arm for operation %" PRIu64, pendingId);
            CompletePendingOperation(pendingId, false);
            return;
        }

        bool sent = pending.device->SendCommandWithCallbacks(
            cmd.clusterId,
            cmd.commandId,
            cmd.timedInvokeTimeoutMs,
            endpointId,
            tlvBuffer,
            tlvLength,
            *pending.exchangeMgr,
            session.Value(),
            [this, pendingId](const chip::app::ConcreteCommandPath &path, chip::TLV::TLVReader *data) {
                HandleDeferredCommandResponse(pendingId, path, data);
            },
            [this, pendingId](CHIP_ERROR error) { HandleDeferredCommandError(pendingId, error); });

        if (!sent)
        {
            icError("Failed to send re-armed deferred command");
            CompletePendingOperation(pendingId, false);
        }

        return;
    }

    if (std::holds_alternative<ResultTerminal::ReadAttribute>(result.terminal.data))
    {
        const auto &ra = std::get<ResultTerminal::ReadAttribute>(result.terminal.data);

        // Release old GC roots and root new handlers
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto *ctx = MQuickJsRuntime::GetSharedContext();

            if (pending.onResponseRooted)
            {
                JS_DeleteGCRef(ctx, &pending.onResponseRef);
                pending.onResponseRooted = false;
            }

            if (pending.onErrorRooted)
            {
                JS_DeleteGCRef(ctx, &pending.onErrorRef);
                pending.onErrorRooted = false;
            }

            if (!JS_IsUndefined(ra.onResponse))
            {
                JS_AddGCRef(ctx, &pending.onResponseRef);
                pending.onResponseRef.val = ra.onResponse;

                pending.onResponseRooted = true;
            }

            if (!JS_IsUndefined(ra.onError))
            {
                JS_AddGCRef(ctx, &pending.onErrorRef);
                pending.onErrorRef.val = ra.onError;

                pending.onErrorRooted = true;
            }
        }

        pending.deferralDepth++;

        // Resolve endpoint
        chip::EndpointId endpointId = 0;

        if (ra.endpointId.has_value())
        {
            endpointId = static_cast<chip::EndpointId>(ra.endpointId.value());
        }
        else if (!pending.device->GetEndpointForCluster(ra.clusterId, endpointId))
        {
            CompletePendingOperation(pendingId, false);
            return;
        }

        // Read from cache
        chip::TLV::TLVReader reader;
        CHIP_ERROR err = pending.device->GetCachedAttributeData(endpointId, ra.clusterId, ra.attributeId, reader);

        std::optional<ParsedResult> nextResult;

        if (err != CHIP_NO_ERROR)
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto *ctx = MQuickJsRuntime::GetSharedContext();

            if (pending.onErrorRooted)
            {
                JSValue args = SbmdHandlerInvoker::BuildDeferredErrorArgs(ctx,
                                                                          pending.handlerContext,
                                                                          "readFailed",
                                                                          "Attribute not in cache",
                                                                          -1,
                                                                          pending.contextRooted ? pending.contextRef.val
                                                                                                : JS_UNDEFINED);
                nextResult = SbmdHandlerInvoker::InvokeHandler(ctx, pending.onErrorRef.val, args);
            }
        }
        else
        {
            uint8_t tlvBuf[256];
            chip::TLV::TLVWriter writer;
            writer.Init(tlvBuf, sizeof(tlvBuf));

            if (writer.CopyElement(chip::TLV::AnonymousTag(), reader) != CHIP_NO_ERROR)
            {
                CompletePendingOperation(pendingId, false);
                return;
            }

            uint32_t tlvLen = writer.GetLengthWritten();
            size_t maxBase64Len = BASE64_ENCODED_LEN(tlvLen) + 1;
            std::string tlvBase64(maxBase64Len, '\0');
            uint16_t encoded = chip::Base64Encode(tlvBuf, static_cast<uint16_t>(tlvLen), tlvBase64.data());
            tlvBase64.resize(encoded);

            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto *ctx = MQuickJsRuntime::GetSharedContext();

            if (pending.onResponseRooted)
            {
                JSValue args = SbmdHandlerInvoker::BuildAttributeReadResponseArgs(
                    ctx,
                    pending.handlerContext,
                    ra.clusterId,
                    ra.attributeId,
                    tlvBase64,
                    pending.contextRooted ? pending.contextRef.val : JS_UNDEFINED);
                nextResult = SbmdHandlerInvoker::InvokeHandler(ctx, pending.onResponseRef.val, args);
            }
        }

        if (!nextResult.has_value())
        {
            CompletePendingOperation(pendingId, false);
            return;
        }

        SbmdHandlerInvoker::ExecuteOps(
            pending.handlerContext, nextResult->ops, MakeTransientSetter(pending.handlerContext.deviceUuid));
        ContinueDeferredChain(pending, *nextResult);
        return;
    }

    icError("Unknown terminal type in deferred chain");
    CompletePendingOperation(pendingId, false);
}

void SpecBasedMatterDeviceDriver::CompletePendingOperation(uint64_t pendingId, bool success)
{
    auto it = pendingOperations.find(pendingId);

    if (it == pendingOperations.end())
    {
        return;
    }

    PendingOperation &pending = it->second;

    // Resolve the parking promise
    if (pending.parkingPromise != nullptr)
    {
        try
        {
            pending.parkingPromise->set_value(success);
        }
        catch (const std::future_error &e)
        {
            icDebug("Parking promise already satisfied for operation %" PRIu64, pendingId);
        }
    }

    // Release GC roots
    ReleasePendingGcRoots(pending);

    pendingOperations.erase(it);
}

void SpecBasedMatterDeviceDriver::ReleasePendingGcRoots(PendingOperation &pending)
{
    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
    auto *ctx = MQuickJsRuntime::GetSharedContext();

    if (pending.onResponseRooted)
    {
        JS_DeleteGCRef(ctx, &pending.onResponseRef);
        pending.onResponseRooted = false;
    }

    if (pending.onErrorRooted)
    {
        JS_DeleteGCRef(ctx, &pending.onErrorRef);
        pending.onErrorRooted = false;
    }

    if (pending.contextRooted)
    {
        JS_DeleteGCRef(ctx, &pending.contextRef);
        pending.contextRooted = false;
    }
}

AttributeSupplementFetcher SpecBasedMatterDeviceDriver::MakeAttrFetcher(MatterDevice &device) const
{
    return [this, &device](const std::string &aliasName) -> std::optional<std::string> {
        const auto &aliases = driver->GetRegistration().aliases;
        auto it = aliases.find(aliasName);

        if (it == aliases.end() || !it->second.attributeId.has_value())
        {
            icWarn("supplement alias '%s' not found or not an attribute", aliasName.c_str());
            return std::nullopt;
        }

        const auto &alias = it->second;
        chip::EndpointId endpointId = 0;

        if (!device.GetEndpointForCluster(alias.clusterId, endpointId))
        {
            icWarn("no endpoint for cluster 0x%x (supplement '%s')", alias.clusterId, aliasName.c_str());
            return std::nullopt;
        }

        chip::TLV::TLVReader reader;
        CHIP_ERROR err = device.GetCachedAttributeData(endpointId, alias.clusterId, alias.attributeId.value(), reader);

        if (err != CHIP_NO_ERROR)
        {
            icDebug("cache miss for supplement '%s' (cluster 0x%x attr 0x%x)",
                    aliasName.c_str(),
                    alias.clusterId,
                    alias.attributeId.value());
            return std::nullopt;
        }

        uint8_t tlvBuf[256];
        chip::TLV::TLVWriter writer;
        writer.Init(tlvBuf, sizeof(tlvBuf));

        if (writer.CopyElement(chip::TLV::AnonymousTag(), reader) != CHIP_NO_ERROR)
        {
            icWarn("failed to copy TLV for supplement '%s'", aliasName.c_str());
            return std::nullopt;
        }

        uint32_t tlvLen = writer.GetLengthWritten();
        size_t maxBase64Len = BASE64_ENCODED_LEN(tlvLen) + 1;
        std::string tlvBase64(maxBase64Len, '\0');
        uint16_t encoded = chip::Base64Encode(tlvBuf, static_cast<uint16_t>(tlvLen), tlvBase64.data());
        tlvBase64.resize(encoded);

        return tlvBase64;
    };
}

ResourceSupplementFetcher SpecBasedMatterDeviceDriver::MakeResFetcher(const std::string &deviceUuid) const
{
    return [deviceUuid](const std::string &path) -> std::optional<std::string> {
        // Parse path: "endpointId/resourceId" or just "resourceId"
        const char *epId = nullptr;
        std::string resourceId;
        auto slashPos = path.find('/');

        if (slashPos != std::string::npos)
        {
            std::string endpointPart = path.substr(0, slashPos);
            resourceId = path.substr(slashPos + 1);
            // deviceServiceGetResourceById expects NULL for device-level
            icDeviceResource *res =
                deviceServiceGetResourceById(deviceUuid.c_str(), endpointPart.c_str(), resourceId.c_str());

            if (res != nullptr && res->value != nullptr)
            {
                std::string val(res->value);
                return val;
            }

            return std::nullopt;
        }
        else
        {
            resourceId = path;
            icDeviceResource *res = deviceServiceGetResourceById(deviceUuid.c_str(), nullptr, resourceId.c_str());

            if (res != nullptr && res->value != nullptr)
            {
                std::string val(res->value);
                return val;
            }

            return std::nullopt;
        }
    };
}

PersistentDataFetcher SpecBasedMatterDeviceDriver::MakePersistFetcher(const std::string &deviceUuid) const
{
    return [deviceUuid](const std::string &key) -> std::optional<std::string> {
        std::string uri = "/devices/" + deviceUuid + "/metadata/sbmd." + key;
        char *value = nullptr;

        if (deviceServiceGetMetadata(uri.c_str(), &value) && value != nullptr)
        {
            std::string result(value);
            free(value);

            return result;
        }

        return std::nullopt;
    };
}

TransientDataFetcher SpecBasedMatterDeviceDriver::MakeTransientFetcher(const std::string &deviceUuid)
{
    return [this, deviceUuid](const std::string &key) -> std::optional<std::string> {
        return GetTransientData(deviceUuid, key);
    };
}

void SpecBasedMatterDeviceDriver::SetTransientData(const std::string &deviceUuid,
                                                   const std::string &key,
                                                   const std::string &value,
                                                   uint32_t ttlSecs)
{
    auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(ttlSecs);
    transientStore[deviceUuid][key] = TransientEntry {value, expiry};
}

std::optional<std::string> SpecBasedMatterDeviceDriver::GetTransientData(const std::string &deviceUuid,
                                                                         const std::string &key)
{
    auto deviceIt = transientStore.find(deviceUuid);

    if (deviceIt == transientStore.end())
    {
        return std::nullopt;
    }

    auto &deviceMap = deviceIt->second;
    auto it = deviceMap.find(key);

    if (it == deviceMap.end())
    {
        return std::nullopt;
    }

    if (std::chrono::steady_clock::now() >= it->second.expiry)
    {
        deviceMap.erase(it);

        return std::nullopt;
    }

    return it->second.value;
}

TransientDataSetter SpecBasedMatterDeviceDriver::MakeTransientSetter(const std::string &deviceUuid)
{
    return [this, deviceUuid](const std::string &key, const std::string &value, uint32_t ttlSecs) {
        SetTransientData(deviceUuid, key, value, ttlSecs);
    };
}

void SpecBasedMatterDeviceDriver::HandleAttributeReport(const std::string &deviceId,
                                                        chip::EndpointId endpointId,
                                                        chip::ClusterId clusterId,
                                                        chip::AttributeId attributeId,
                                                        chip::TLV::TLVReader &reader)
{
    if (!driver || !driver->IsActivated())
    {
        return;
    }

    // Look up matching handlers in the attribute dispatch table
    auto matches = driver->GetAttributeDispatch().Lookup(clusterId, attributeId);

    if (matches.empty())
    {
        return;
    }

    // Encode TLV element as base64 for passing to JS handlers.
    // The reader from ClusterStateCache::Get() is positioned at the attribute value
    // element but GetRemainingLength() may be 0 for in-memory cache data.
    // Use TLVWriter::CopyElement to extract the element into a scratch buffer.
    uint8_t tlvBuf[256]; // Attributes rarely exceed this; grow if needed
    chip::TLV::TLVWriter writer;
    writer.Init(tlvBuf, sizeof(tlvBuf));

    if (writer.CopyElement(chip::TLV::AnonymousTag(), reader) != CHIP_NO_ERROR)
    {
        icWarn("Failed to copy TLV element for cluster 0x%x attribute 0x%x", clusterId, attributeId);
        return;
    }

    uint32_t tlvLen = writer.GetLengthWritten();

    if (tlvLen == 0)
    {
        icDebug("Empty TLV data for attribute 0x%x", attributeId);
        return;
    }

    // Base64 encode the TLV data
    size_t maxBase64Len = BASE64_ENCODED_LEN(tlvLen) + 1;
    std::string tlvBase64(maxBase64Len, '\0');
    uint16_t encoded = chip::Base64Encode(tlvBuf, static_cast<uint16_t>(tlvLen), tlvBase64.data());
    tlvBase64.resize(encoded);

    // Build handler context
    HandlerContext hctx;
    hctx.deviceUuid = deviceId;
    hctx.endpointId = std::to_string(endpointId);
    // TODO: populate clusterFeatureMaps from MatterDevice

    auto matterDevice = GetDevice(deviceId);

    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
    auto *ctx = MQuickJsRuntime::GetSharedContext();

    for (const auto *entry : matches)
    {
        if (entry->handler == nullptr || JS_IsUndefined(entry->handler->handler))
        {
            continue;
        }

        JSValue args = SbmdHandlerInvoker::BuildAttributeArgs(ctx, hctx, clusterId, attributeId, tlvBase64);

        // GC-root args across AddSupplements (which allocates) and InvokeHandler
        JSGCRef argsRef {};
        JS_AddGCRef(ctx, &argsRef);
        argsRef.val = args;


        if (matterDevice)
        {
            SbmdHandlerInvoker::AddSupplements(ctx,
                                               args,
                                               entry->handler->supplements,
                                               MakeAttrFetcher(*matterDevice),
                                               MakeResFetcher(deviceId),
                                               MakePersistFetcher(deviceId),
                                               MakeTransientFetcher(deviceId));
        }

        auto result = SbmdHandlerInvoker::InvokeHandler(ctx, entry->handler->handler, args);

        JS_DeleteGCRef(ctx, &argsRef);

        if (!result.has_value())
        {
            icWarn("Attribute handler '%s' returned no result for cluster 0x%x attr 0x%x",
                   entry->handler->name.c_str(),
                   clusterId,
                   attributeId);
            continue;
        }

        // Execute ops (updateResource, setMetadata, etc.)
        SbmdHandlerInvoker::ExecuteOps(hctx, result->ops, MakeTransientSetter(deviceId));

        // For attribute handlers, we typically expect a success terminal.
        // Error terminals are logged but don't abort other handler processing.
        if (std::holds_alternative<ResultTerminal::Error>(result->terminal.data))
        {
            const auto &err = std::get<ResultTerminal::Error>(result->terminal.data);
            icWarn("Attribute handler '%s' returned error: %s", entry->handler->name.c_str(), err.message.c_str());
        }
    }
}

void SpecBasedMatterDeviceDriver::HandleEvent(const std::string &deviceId,
                                              chip::EndpointId endpointId,
                                              chip::ClusterId clusterId,
                                              chip::EventId eventId,
                                              chip::TLV::TLVReader &reader)
{
    if (!driver || !driver->IsActivated())
    {
        return;
    }

    // Look up matching handlers in the event dispatch table
    auto matches = driver->GetEventDispatch().Lookup(clusterId, eventId);

    if (matches.empty())
    {
        return;
    }

    // Encode TLV element as base64 for passing to JS handlers
    uint8_t tlvBuf[1024]; // Events may contain structured data; use larger buffer
    chip::TLV::TLVWriter writer;
    writer.Init(tlvBuf, sizeof(tlvBuf));

    if (writer.CopyElement(chip::TLV::AnonymousTag(), reader) != CHIP_NO_ERROR)
    {
        icWarn("Failed to copy TLV element for cluster 0x%x event 0x%x", clusterId, eventId);
        return;
    }

    uint32_t tlvLen = writer.GetLengthWritten();

    if (tlvLen == 0)
    {
        icDebug("Empty TLV data for event 0x%x", eventId);
        return;
    }

    // Base64 encode the TLV data
    size_t maxBase64Len = BASE64_ENCODED_LEN(tlvLen) + 1;
    std::string tlvBase64(maxBase64Len, '\0');
    uint16_t encoded = chip::Base64Encode(tlvBuf, static_cast<uint16_t>(tlvLen), tlvBase64.data());
    tlvBase64.resize(encoded);

    // Build handler context
    HandlerContext hctx;
    hctx.deviceUuid = deviceId;
    hctx.endpointId = std::to_string(endpointId);

    auto matterDevice = GetDevice(deviceId);

    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
    auto *ctx = MQuickJsRuntime::GetSharedContext();

    for (const auto *entry : matches)
    {
        if (entry->handler == nullptr || JS_IsUndefined(entry->handler->handler))
        {
            continue;
        }

        JSValue args = SbmdHandlerInvoker::BuildEventArgs(ctx, hctx, clusterId, eventId, tlvBase64);

        // GC-root args across AddSupplements (which allocates) and InvokeHandler
        JSGCRef argsRef {};
        JS_AddGCRef(ctx, &argsRef);
        argsRef.val = args;


        if (matterDevice)
        {
            SbmdHandlerInvoker::AddSupplements(ctx,
                                               args,
                                               entry->handler->supplements,
                                               MakeAttrFetcher(*matterDevice),
                                               MakeResFetcher(deviceId),
                                               MakePersistFetcher(deviceId),
                                               MakeTransientFetcher(deviceId));
        }

        auto result = SbmdHandlerInvoker::InvokeHandler(ctx, entry->handler->handler, args);

        JS_DeleteGCRef(ctx, &argsRef);

        if (!result.has_value())
        {
            icWarn("Event handler '%s' returned no result for cluster 0x%x event 0x%x",
                   entry->handler->name.c_str(),
                   clusterId,
                   eventId);
            continue;
        }

        // Execute ops (updateResource, setMetadata, etc.)
        SbmdHandlerInvoker::ExecuteOps(hctx, result->ops, MakeTransientSetter(deviceId));

        if (std::holds_alternative<ResultTerminal::Error>(result->terminal.data))
        {
            const auto &err = std::get<ResultTerminal::Error>(result->terminal.data);
            icWarn("Event handler '%s' returned error: %s", entry->handler->name.c_str(), err.message.c_str());
        }
    }
}

void SpecBasedMatterDeviceDriver::HandleCommand(const std::string &deviceId,
                                                chip::EndpointId endpointId,
                                                chip::ClusterId clusterId,
                                                uint32_t commandId,
                                                const std::string &tlvBase64)
{
    if (!driver || !driver->IsActivated())
    {
        return;
    }

    // Look up matching handlers in the command dispatch table
    auto matches = driver->GetCommandDispatch().Lookup(clusterId, commandId);

    if (matches.empty())
    {
        return;
    }

    // Build handler context
    HandlerContext hctx;
    hctx.deviceUuid = deviceId;
    hctx.endpointId = std::to_string(endpointId);

    auto matterDevice = GetDevice(deviceId);

    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
    auto *ctx = MQuickJsRuntime::GetSharedContext();

    for (const auto *entry : matches)
    {
        if (entry->handler == nullptr || JS_IsUndefined(entry->handler->handler))
        {
            continue;
        }

        JSValue args = SbmdHandlerInvoker::BuildCommandArgs(ctx, hctx, clusterId, commandId, tlvBase64);

        // GC-root args across AddSupplements (which allocates) and InvokeHandler
        JSGCRef argsRef {};
        JS_AddGCRef(ctx, &argsRef);
        argsRef.val = args;


        if (matterDevice)
        {
            SbmdHandlerInvoker::AddSupplements(ctx,
                                               args,
                                               entry->handler->supplements,
                                               MakeAttrFetcher(*matterDevice),
                                               MakeResFetcher(deviceId),
                                               MakePersistFetcher(deviceId),
                                               MakeTransientFetcher(deviceId));
        }

        auto result = SbmdHandlerInvoker::InvokeHandler(ctx, entry->handler->handler, args);

        JS_DeleteGCRef(ctx, &argsRef);

        if (!result.has_value())
        {
            icWarn("Command handler '%s' returned no result for cluster 0x%x command 0x%x",
                   entry->handler->name.c_str(),
                   clusterId,
                   commandId);
            continue;
        }

        // Execute ops (updateResource, setMetadata, etc.)
        SbmdHandlerInvoker::ExecuteOps(hctx, result->ops, MakeTransientSetter(deviceId));

        if (std::holds_alternative<ResultTerminal::Error>(result->terminal.data))
        {
            const auto &err = std::get<ResultTerminal::Error>(result->terminal.data);
            icWarn("Command handler '%s' returned error: %s", entry->handler->name.c_str(), err.message.c_str());
        }
    }
}
