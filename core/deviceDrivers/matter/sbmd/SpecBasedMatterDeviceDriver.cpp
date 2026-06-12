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
    device->SetAttributeCallback(
        [this](const std::string &deviceId,
               chip::EndpointId endpointId,
               chip::ClusterId clusterId,
               chip::AttributeId attributeId,
               chip::TLV::TLVReader &reader) {
            HandleAttributeReport(deviceId, endpointId, clusterId, attributeId, reader);
        });

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

                icError("Required resource '%s' prerequisites not met, aborting commissioning",
                        resource.id.c_str());

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

// =============================================================================
// Driver-based implementation methods
// =============================================================================

bool SpecBasedMatterDeviceDriver::DoRegisterDriverResources(icDevice *device)
{
    bool result = true;
    const auto &reg = driver->GetRegistration();
    const auto *skipped = skippedOptionalResources.count(device->uuid)
                              ? &skippedOptionalResources[device->uuid]
                              : nullptr;

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

            uint8_t resourceMode = ConvertModesToBitmask(resource.modes);

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

            result &=
                createEndpointResource(
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

            std::string seedValue = InvokeSeedHandler(deviceId, endpoint.id, resource);

            if (!seedValue.empty())
            {
                updateResource(deviceId.c_str(), endpoint.id.c_str(), resource.id.c_str(), seedValue.c_str(), nullptr);
            }
        }
    }
}

std::string SpecBasedMatterDeviceDriver::InvokeSeedHandler(const std::string &deviceId,
                                                             const std::string &endpointId,
                                                             const SbmdResource &resource)
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
    auto result = SbmdHandlerInvoker::InvokeHandler(ctx, resource.seed->handler, args);

    if (!result.has_value())
    {
        icDebug("Seed handler for resource '%s' returned no result", resource.id.c_str());
        return "";
    }

    SbmdHandlerInvoker::ExecuteOps(hctx, result->ops);

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
        icWarn("No device data cache for device %s; prerequisites cannot be evaluated",
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
            icDebug("Prerequisite cluster 0x%08" PRIx32 " not found on device %s",
                    clusterId,
                    device.GetDeviceId().c_str());

            return false;
        }
    }

    return true;
}

const SbmdResource *SpecBasedMatterDeviceDriver::FindDriverResource(const char *endpointId, const char *resourceId) const
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
        result = SbmdHandlerInvoker::InvokeHandler(ctx, handler->handler, args);
    }

    if (!result.has_value())
    {
        icError("%s handler for resource %s returned no result", opType, resourceId);
        FailOperation(promises);
        return;
    }

    // Execute non-terminal ops
    SbmdHandlerInvoker::ExecuteOps(hctx, result->ops);

    // Handle the terminal
    ExecuteTerminal(promises, device, result->terminal, resource->uri, readValue, executeResponse,
                     exchangeMgr, sessionHandle);
}

void SpecBasedMatterDeviceDriver::ExecuteTerminal(std::forward_list<std::promise<bool>> &promises,
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
            uint16_t decoded = chip::Base64Decode(cmd.tlvBase64.c_str(),
                                                  static_cast<uint16_t>(cmd.tlvBase64.size()),
                                                  decodedTlv.get());

            if (decoded == UINT16_MAX)
            {
                icError("Failed to base64 decode TLV for sendCommand");
                FailOperation(promises);
                return;
            }

            tlvBuffer = decodedTlv.get();
            tlvLength = decoded;
        }

        if (!device.SendCommandFromTlv(promises, cmd.clusterId, cmd.commandId,
                                       cmd.timedInvokeTimeoutMs, endpointId,
                                       tlvBuffer, tlvLength,
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
        uint16_t decoded = chip::Base64Decode(wa.tlvBase64.c_str(),
                                              static_cast<uint16_t>(wa.tlvBase64.size()),
                                              decodedTlv.get());

        if (decoded == UINT16_MAX)
        {
            icError("Failed to base64 decode TLV for writeAttribute");
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
        icWarn("requestCommand terminal not yet implemented (deferred operations)");
        FailOperation(promises);
        return;
    }

    if (std::holds_alternative<ResultTerminal::ReadAttribute>(terminal.data))
    {
        icWarn("readAttribute terminal not yet implemented (deferred operations)");
        FailOperation(promises);
        return;
    }

    icError("Unknown terminal type");
    FailOperation(promises);
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
    uint16_t encoded = chip::Base64Encode(tlvBuf, static_cast<uint16_t>(tlvLen),
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

        JSValue args = SbmdHandlerInvoker::BuildAttributeArgs(ctx, hctx, clusterId, attributeId, tlvBase64);
        auto result = SbmdHandlerInvoker::InvokeHandler(ctx, entry->handler->handler, args);

        if (!result.has_value())
        {
            icWarn("Attribute handler '%s' returned no result for cluster 0x%x attr 0x%x",
                   entry->handler->name.c_str(), clusterId, attributeId);
            continue;
        }

        // Execute ops (updateResource, setMetadata, etc.)
        SbmdHandlerInvoker::ExecuteOps(hctx, result->ops);

        // For attribute handlers, we typically expect a success terminal.
        // Error terminals are logged but don't abort other handler processing.
        if (std::holds_alternative<ResultTerminal::Error>(result->terminal.data))
        {
            const auto &err = std::get<ResultTerminal::Error>(result->terminal.data);
            icWarn("Attribute handler '%s' returned error: %s",
                   entry->handler->name.c_str(), err.message.c_str());
        }
    }
}
