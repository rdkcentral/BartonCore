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
#include "matter/sbmd/SafeJSValue.h"
#include "matter/sbmd/mquickjs/MQuickJsRuntime.h"
#include "matter/sbmd/mquickjs/SbmdHandlerInvoker.h"
#endif

#include <algorithm>
#include <app/ConcreteAttributePath.h>
#include <cinttypes>
#include <lib/core/TLVReader.h>
#include <lib/core/TLVWriter.h>
#include <memory>
#include <vector>

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

namespace
{
    /*
     * Base64-decode a TLV payload into raw bytes.
     *
     * tlvBase64 - the base64-encoded TLV string (may be empty).
     *
     * Returns std::nullopt on a decode error. On success returns the decoded bytes; an empty input
     * yields an empty vector (i.e. no payload).
     */
    std::optional<std::vector<uint8_t>> DecodeBase64Tlv(const std::string &tlvBase64)
    {
        if (tlvBase64.empty())
        {
            return std::vector<uint8_t> {};
        }

        std::vector<uint8_t> decodedTlv(BASE64_MAX_DECODED_LEN(tlvBase64.size()));
        uint16_t decoded =
            chip::Base64Decode(tlvBase64.c_str(), static_cast<uint16_t>(tlvBase64.size()), decodedTlv.data());

        if (decoded == UINT16_MAX)
        {
            return std::nullopt;
        }

        decodedTlv.resize(decoded);

        return decodedTlv;
    }

    /*
     * Copy the TLV element at the reader into a scratch buffer and base64-encode it.
     *
     * reader         - positioned at the TLV element to encode; the element is copied out via
     *                  TLVWriter::CopyElement. A private copy of the reader is used for each
     *                  attempt so a retry after a too-small buffer restarts cleanly.
     * initialBufSize - initial size in bytes of the scratch buffer. Callers pass a small value
     *                  sized for the common case; the buffer grows on demand from here.
     * maxBufSize     - upper bound for the scratch buffer. Whenever the copy reports the buffer
     *                  is too small, the buffer doubles (capped at maxBufSize) and the copy is
     *                  retried, so large payloads are handled without over-allocating for the
     *                  common small case.
     *
     * Returns std::nullopt if the element could not be copied within maxBufSize bytes or the copy
     * fails for any other reason. A successfully copied zero-length element yields an empty string;
     * otherwise the base64-encoded element.
     */
    std::optional<std::string>
    EncodeTlvElementToBase64(chip::TLV::TLVReader &reader, size_t initialBufSize = 2048, size_t maxBufSize = 32768)
    {
        size_t bufSize = initialBufSize;
        bool retryable = true;

        while (retryable)
        {
            std::vector<uint8_t> tlvBuf(bufSize);
            chip::TLV::TLVWriter writer;
            writer.Init(tlvBuf.data(), static_cast<uint32_t>(tlvBuf.size()));

            // Copy the reader so a retry after a too-small buffer restarts cleanly
            chip::TLV::TLVReader readerCopy(reader);
            CHIP_ERROR err = writer.CopyElement(chip::TLV::AnonymousTag(), readerCopy);

            if (err == CHIP_NO_ERROR)
            {
                uint32_t tlvLen = writer.GetLengthWritten();

                if (tlvLen == 0)
                {
                    return std::string {};
                }

                std::string tlvBase64(BASE64_ENCODED_LEN(tlvLen) + 1, '\0');
                uint16_t encoded = chip::Base64Encode(tlvBuf.data(), static_cast<uint16_t>(tlvLen), tlvBase64.data());
                tlvBase64.resize(encoded);

                return tlvBase64;
            }

            // Retry with a larger buffer only when the copy ran out of room and we can still grow.
            retryable = (err == CHIP_ERROR_BUFFER_TOO_SMALL || err == CHIP_ERROR_NO_MEMORY) && bufSize < maxBufSize;

            if (retryable)
            {
                bufSize *= 2;

                if (bufSize > maxBufSize)
                {
                    bufSize = maxBufSize;
                }
            }
        }

        return std::nullopt;
    }

    /*
     * RAII guard that destroys a parsed handler result while holding the JS runtime mutex.
     *
     * A deferred terminal (requestCommand/readAttribute) carries its onResponse/onError/context
     * callbacks as SafeJSValues, whose destructor releases a held reference and therefore must run under
     * MQuickJsRuntime's mutex. The executors normally consume those callbacks (moving them into the
     * pending operation, or invoking and releasing them), but declaring one of these guards next to
     * the result reference guarantees any callbacks left over on any exit path — including an early
     * return inside an executor — are released under the lock. Declare it after the result so it is
     * destroyed first, emptying the result before the result's own destructor runs.
     */
    class ScopedResultRelease
    {
    public:
        explicit ScopedResultRelease(std::optional<ParsedResult> &result) : result(result) {}

        ~ScopedResultRelease()
        {
            if (!result.has_value())
            {
                return;
            }

            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            result.reset();
        }

        ScopedResultRelease(const ScopedResultRelease &) = delete;
        ScopedResultRelease &operator=(const ScopedResultRelease &) = delete;

    private:
        std::optional<ParsedResult> &result;
    };
} // namespace

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

    // The cache subscription's priming report may have completed (and OnSubscriptionEstablished
    // fired) before this device registered its CacheCallback, in which case the feature maps were
    // never cached. Populate them now that featureClusters are set and the cache is primed.
    device->UpdateCachedFeatureMaps();

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
        // Encode the incoming command's TLV payload as base64 for HandleCommand. The helper grows
        // its scratch buffer on demand, so multi-KB command payloads are handled without dropping.
        auto tlvBase64 = EncodeTlvElementToBase64(reader);

        if (!tlvBase64.has_value())
        {
            // Drop the command rather than delivering it with an empty payload, which JS handlers
            // would mis-read as a command with no arguments.
            icError("Failed to encode command TLV for cluster 0x%x command 0x%x on device %s; dropping command",
                    clusterId,
                    commandId,
                    deviceId.c_str());

            return;
        }

        HandleCommand(deviceId, endpointId, clusterId, commandId, *tlvBase64);
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
    auto device = GetDevice(deviceId);

    if (device != nullptr)
    {
        // Re-register incoming (server-side) command handlers. On app restart a previously
        // commissioned device is restored via this synchronize path rather than AddDevice,
        // so the CommandHandlerInterface for server clusters (e.g. WebRTCTransportRequestor)
        // must be re-established here or inbound commands from the device will go unhandled.
        device->UnregisterIncomingCommandHandlers();

        if (driver != nullptr)
        {
            for (uint32_t clusterId : driver->GetCommandDispatch().GetRegisteredClusterIds())
            {
                device->RegisterIncomingCommandHandler(static_cast<chip::ClusterId>(clusterId));
            }
        }
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
        else if (mode == "dynamic")
        {
            // Enabled by default in the initial bitmask. Setting the bits explicitly (rather than
            // treating this as a no-op) lets a spec re-enable dynamic behavior by listing "dynamic"
            // after "static", since modes are applied in order.
            bitmask |= RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_DYNAMIC_CAPABLE;
        }
        else if (mode == "emitEvents")
        {
            // Enabled by default in the initial bitmask. Setting the bit explicitly lets a spec
            // re-enable events by listing "emitEvents" after "noEvents", since modes are applied in
            // order.
            bitmask |= RESOURCE_MODE_EMIT_EVENTS;
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
        else if (mode == "volatile")
        {
            // Caching policy (CACHING_POLICY_NEVER) is applied separately in
            // DoRegisterDriverResources; this mode contributes no access bit.
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

    // Resolve the MatterDevice so seed handlers can prefetch declared supplements from the
    // attribute cache. Without it InvokeSeedHandler skips the prefetch and a seed that reads
    // a declared supplement receives null (see the supplement contract in AddSupplements).
    auto matterDevice = GetDevice(device->uuid);

    if (matterDevice == nullptr)
    {
        icWarn("Could not resolve MatterDevice for %s during resource registration; seed "
               "handlers that declare attribute supplements will receive null values",
               device->uuid);
    }

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
            // The 'volatile' mode also forces CACHING_POLICY_NEVER so that, for a resource that emits
            // events, updateResource delivers an event on every call (no value-change suppression) even
            // when the value is unchanged. CACHING_POLICY_NEVER also means updateResource does not
            // persist the value (or dateOfLastSyncMillis) to the device DB, and readable resources are
            // served from the driver rather than the DB cache. 'volatile' is therefore intended for
            // event-only signaling resources and should not be applied to subscription-backed state
            // resources whose reads rely on the DB-cached value.
            bool isVolatile =
                std::find(resource.modes.begin(), resource.modes.end(), "volatile") != resource.modes.end();
            ResourceCachingPolicy cachingPolicy =
                (resource.read.has_value() || isVolatile) ? CACHING_POLICY_NEVER : CACHING_POLICY_ALWAYS;

            // Seed initial value if there's a seed handler
            const char *initialValue = nullptr;
            std::string seedValue;

            if (resource.seed.has_value())
            {
                seedValue = InvokeSeedHandler(device->uuid, endpoint.id, resource, matterDevice.get());

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

    HandlerContext hctx;
    hctx.deviceUuid = deviceId;
    hctx.endpointId = endpointId;

    // Resolve declared supplements up front (device-service I/O) so the JS mutex is held
    // only around the JS work below.
    FetchedSupplements supplements;

    if (device != nullptr)
    {
        supplements = SbmdHandlerInvoker::PrefetchSupplements(resource.seed->supplements,
                                                              MakeAttrFetcher(*device),
                                                              MakeResFetcher(deviceId),
                                                              MakePersistFetcher(deviceId),
                                                              MakeTransientFetcher(deviceId));
    }

    std::optional<ParsedResult> result;
    ScopedResultRelease resultRelease {result};

    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto *ctx = MQuickJsRuntime::GetSharedContext();

        // args keeps its value alive for its whole lifetime, so it survives the allocations in
        // AddSupplements and the handler call without a separate guard.
        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(ctx, hctx, resource.id, std::nullopt);
        SbmdHandlerInvoker::AddSupplements(ctx, args, resource.seed->supplements, supplements);
        result = SbmdHandlerInvoker::InvokeHandler(ctx, resource.seed->Fn(), args);
    }

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
    const SbmdHandler *handler = nullptr;
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
    hctx.clusterFeatureMaps = device.GetCachedClusterFeatureMaps();

    // Resolve declared supplements up front (device-service I/O) so the JS mutex is held
    // only around the JS work below.
    FetchedSupplements supplements =
        SbmdHandlerInvoker::PrefetchSupplements(handler->supplements,
                                                MakeAttrFetcher(device),
                                                MakeResFetcher(device.GetDeviceId()),
                                                MakePersistFetcher(device.GetDeviceId()),
                                                MakeTransientFetcher(device.GetDeviceId()));

    // Invoke the handler under the JS mutex
    std::optional<ParsedResult> result;
    ScopedResultRelease resultRelease {result};

    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto *ctx = MQuickJsRuntime::GetSharedContext();

        // args keeps its value alive for its whole lifetime, so it survives the allocations in
        // AddSupplements and the handler call without a separate guard.
        SafeJSValue args = SbmdHandlerInvoker::BuildResourceArgs(ctx, hctx, resourceId, inputValue);
        SbmdHandlerInvoker::AddSupplements(ctx, args, handler->supplements, supplements);
        result = SbmdHandlerInvoker::InvokeHandler(ctx, handler->Fn(), args);
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
                                                  ResultTerminal &terminal,
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

        auto endpointOpt = ResolveEndpoint(device, cmd.clusterId, cmd.endpointId);

        if (!endpointOpt.has_value())
        {
            icError("Failed to find endpoint for cluster 0x%x", cmd.clusterId);
            FailOperation(promises);
            return;
        }

        chip::EndpointId endpointId = *endpointOpt;

        auto decodedTlv = DecodeBase64Tlv(cmd.tlvBase64);

        if (!decodedTlv.has_value())
        {
            icError("Failed to base64 decode TLV for sendCommand");
            FailOperation(promises);
            return;
        }

        const uint8_t *tlvBuffer = decodedTlv->empty() ? nullptr : decodedTlv->data();
        size_t tlvLength = decodedTlv->size();

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

        auto endpointOpt = ResolveEndpoint(device, wa.clusterId, wa.endpointId);

        if (!endpointOpt.has_value())
        {
            icError("Failed to find endpoint for cluster 0x%x", wa.clusterId);
            FailOperation(promises);
            return;
        }

        chip::EndpointId endpointId = *endpointOpt;

        // writeAttribute requires a payload
        if (wa.tlvBase64.empty())
        {
            icError("Empty TLV for writeAttribute");
            FailOperation(promises);
            return;
        }

        auto decodedTlv = DecodeBase64Tlv(wa.tlvBase64);

        if (!decodedTlv.has_value())
        {
            icError("Failed to base64 decode TLV for writeAttribute");
            FailOperation(promises);
            return;
        }

        if (!device.WriteAttributeFromTlv(promises,
                                          endpointId,
                                          wa.clusterId,
                                          wa.attributeId,
                                          decodedTlv->data(),
                                          static_cast<uint16_t>(decodedTlv->size()),
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
        auto &cmd = std::get<ResultTerminal::RequestCommand>(terminal.data);
        ExecuteRequestCommand(promises, device, cmd, hctx, readValue, executeResponse, exchangeMgr, sessionHandle);
        return;
    }

    if (std::holds_alternative<ResultTerminal::ReadAttribute>(terminal.data))
    {
        auto &ra = std::get<ResultTerminal::ReadAttribute>(terminal.data);
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
                                                        ResultTerminal::RequestCommand &cmd,
                                                        const HandlerContext &hctx,
                                                        char **readValue,
                                                        char **executeResponse,
                                                        chip::Messaging::ExchangeManager &exchangeMgr,
                                                        const chip::SessionHandle &sessionHandle)
{
    auto endpointOpt = ResolveEndpoint(device, cmd.clusterId, cmd.endpointId);

    if (!endpointOpt.has_value())
    {
        icError("Failed to find endpoint for cluster 0x%x (requestCommand)", cmd.clusterId);
        FailOperation(promises);
        return;
    }

    chip::EndpointId endpointId = *endpointOpt;

    auto decodedTlv = DecodeBase64Tlv(cmd.tlvBase64);

    if (!decodedTlv.has_value())
    {
        icError("Failed to base64 decode TLV for requestCommand");
        FailOperation(promises);
        return;
    }

    const uint8_t *tlvBuffer = decodedTlv->empty() ? nullptr : decodedTlv->data();
    size_t tlvLength = decodedTlv->size();

    // Create parking promise
    promises.emplace_front();
    auto &parkingPromise = promises.front();

    // Register pending operation
    uint64_t pendingId = nextPendingId++;
    PendingOperation pending;
    pending.id = pendingId;
    pending.parkingPromise = &parkingPromise;
    pending.handlerContext = hctx;
    pending.device = GetDevice(device.GetDeviceId());
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

    // The PendingOperation's SafeJSValue callbacks are empty at this point, so moving it into
    // the map performs no engine work and needs no mutex. The callbacks are transferred afterward,
    // below, using the stable map-node address.
    auto [it, inserted] = pendingOperations.emplace(pendingId, std::move(pending));
    PendingOperation &stored = it->second;

    // Move the deferred callbacks (held alive by Parse) into the stable map node under the mutex.
    // Move-assigning a SafeJSValue re-registers the held reference at the destination address and
    // clears the source, so the originating terminal ends up empty and destructs safely.
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        stored.onResponse = std::move(cmd.onResponse);
        stored.onError = std::move(cmd.onError);
        stored.context = std::move(cmd.context);
    }

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
                                                       ResultTerminal::ReadAttribute &ra,
                                                       const HandlerContext &hctx,
                                                       char **readValue,
                                                       char **executeResponse,
                                                       chip::Messaging::ExchangeManager &exchangeMgr,
                                                       const chip::SessionHandle &sessionHandle)
{
    auto endpointOpt = ResolveEndpoint(device, ra.clusterId, ra.endpointId);

    if (!endpointOpt.has_value())
    {
        icError("Failed to find endpoint for cluster 0x%x (readAttribute)", ra.clusterId);
        FailOperation(promises);
        return;
    }

    chip::EndpointId endpointId = *endpointOpt;

    // Read from cache
    chip::TLV::TLVReader reader;
    CHIP_ERROR err = device.GetCachedAttributeData(endpointId, ra.clusterId, ra.attributeId, reader);

    std::optional<ParsedResult> result;
    ScopedResultRelease resultRelease {result};

    if (err != CHIP_NO_ERROR)
    {
        icWarn(
            "Cache miss for cluster 0x%x attr 0x%x (readAttribute): %s", ra.clusterId, ra.attributeId, err.AsString());

        // Call onError handler
        if (ra.onError.HasValue())
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto *ctx = MQuickJsRuntime::GetSharedContext();

            SafeJSValue args = SbmdHandlerInvoker::BuildDeferredErrorArgs(
                ctx, hctx, "readFailed", "Attribute not in cache", -1, ra.context.Get());
            result = SbmdHandlerInvoker::InvokeHandler(ctx, ra.onError.Get(), args);

            // Release the callbacks now that they have been consumed.
            ra.onResponse = SafeJSValue {};
            ra.onError = SafeJSValue {};
            ra.context = SafeJSValue {};
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
        auto tlvBase64Opt = EncodeTlvElementToBase64(reader, 256);

        if (!tlvBase64Opt.has_value())
        {
            icError("Failed to copy cached attribute TLV for readAttribute");
            FailOperation(promises);
            return;
        }

        const std::string &tlvBase64 = *tlvBase64Opt;

        // Call onResponse handler
        if (ra.onResponse.HasValue())
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto *ctx = MQuickJsRuntime::GetSharedContext();

            SafeJSValue args = SbmdHandlerInvoker::BuildAttributeReadResponseArgs(
                ctx, hctx, ra.clusterId, ra.attributeId, tlvBase64, ra.context.Get());
            result = SbmdHandlerInvoker::InvokeHandler(ctx, ra.onResponse.Get(), args);

            // Release the callbacks now that they have been consumed.
            ra.onResponse = SafeJSValue {};
            ra.onError = SafeJSValue {};
            ra.context = SafeJSValue {};
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
        ScopedResultRelease errorResultRelease {errorResult};
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto *ctx = MQuickJsRuntime::GetSharedContext();

            if (pending.onError.HasValue())
            {
                SafeJSValue args = SbmdHandlerInvoker::BuildDeferredErrorArgs(ctx,
                                                                              pending.handlerContext,
                                                                              "timeout",
                                                                              "Overall operation deadline exceeded",
                                                                              -1,
                                                                              pending.context.Get());
                errorResult = SbmdHandlerInvoker::InvokeHandler(ctx, pending.onError.Get(), args);
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

    // Encode response data as base64 (a copy failure or empty element yields an empty payload)
    std::string tlvBase64;

    if (data != nullptr)
    {
        tlvBase64 = EncodeTlvElementToBase64(*data, 1024).value_or(std::string {});
    }

    // Invoke the onResponse handler
    std::optional<ParsedResult> result;
    ScopedResultRelease resultRelease {result};
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto *ctx = MQuickJsRuntime::GetSharedContext();

        if (pending.onResponse.HasValue())
        {
            SafeJSValue args = SbmdHandlerInvoker::BuildCommandResponseArgs(
                ctx, pending.handlerContext, path.mClusterId, path.mCommandId, tlvBase64, pending.context.Get());
            result = SbmdHandlerInvoker::InvokeHandler(ctx, pending.onResponse.Get(), args);
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
    ScopedResultRelease errorResultRelease {errorResult};
    {
        std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
        auto *ctx = MQuickJsRuntime::GetSharedContext();

        if (pending.onError.HasValue())
        {
            SafeJSValue args = SbmdHandlerInvoker::BuildDeferredErrorArgs(ctx,
                                                                          pending.handlerContext,
                                                                          "commandFailed",
                                                                          error.AsString(),
                                                                          static_cast<int32_t>(error.AsInteger()),
                                                                          pending.context.Get());
            errorResult = SbmdHandlerInvoker::InvokeHandler(ctx, pending.onError.Get(), args);
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

void SpecBasedMatterDeviceDriver::ContinueDeferredChain(PendingOperation &pending, ParsedResult &result)
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

    // All remaining terminals need the device -- verify it is still alive.
    auto device = pending.device.lock();

    if (!device)
    {
        icError("Device removed while deferred operation %" PRIu64 " was in flight", pendingId);
        CompletePendingOperation(pendingId, false);
        return;
    }

    if (std::holds_alternative<ResultTerminal::SendCommand>(result.terminal.data))
    {
        const auto &cmd = std::get<ResultTerminal::SendCommand>(result.terminal.data);

        auto endpointOpt = ResolveEndpoint(*device, cmd.clusterId, cmd.endpointId);

        if (!endpointOpt.has_value())
        {
            icError("Failed to find endpoint for cluster 0x%x in deferred chain", cmd.clusterId);
            CompletePendingOperation(pendingId, false);
            return;
        }

        chip::EndpointId endpointId = *endpointOpt;

        auto decodedTlv = DecodeBase64Tlv(cmd.tlvBase64);

        if (!decodedTlv.has_value())
        {
            CompletePendingOperation(pendingId, false);
            return;
        }

        const uint8_t *tlvBuffer = decodedTlv->empty() ? nullptr : decodedTlv->data();
        size_t tlvLength = decodedTlv->size();

        // Send the command — this resolves immediately via OnDone
        std::forward_list<std::promise<bool>> tempPromises;

        auto session = pending.sessionHandle.Get();

        if (!session.HasValue())
        {
            icError("Session expired before deferred sendCommand for operation %" PRIu64, pendingId);
            CompletePendingOperation(pendingId, false);
            return;
        }

        if (!device->SendCommandFromTlv(tempPromises,
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

        // The command was accepted for delivery. Its own promise in tempPromises resolves later
        // when the exchange completes, but a chained sendCommand does not wait on that result: the
        // parking promise is completed as success here to signal that the command was dispatched.
        CompletePendingOperation(pendingId, true);
        return;
    }

    if (std::holds_alternative<ResultTerminal::WriteAttribute>(result.terminal.data))
    {
        const auto &wa = std::get<ResultTerminal::WriteAttribute>(result.terminal.data);

        auto endpointOpt = ResolveEndpoint(*device, wa.clusterId, wa.endpointId);

        if (!endpointOpt.has_value())
        {
            CompletePendingOperation(pendingId, false);
            return;
        }

        chip::EndpointId endpointId = *endpointOpt;

        if (wa.tlvBase64.empty())
        {
            CompletePendingOperation(pendingId, false);
            return;
        }

        auto decodedTlv = DecodeBase64Tlv(wa.tlvBase64);

        if (!decodedTlv.has_value())
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

        if (!device->WriteAttributeFromTlv(tempPromises,
                                           endpointId,
                                           wa.clusterId,
                                           wa.attributeId,
                                           decodedTlv->data(),
                                           static_cast<uint16_t>(decodedTlv->size()),
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
        auto &cmd = std::get<ResultTerminal::RequestCommand>(result.terminal.data);

        // Re-arm: move the new response/error callbacks into the pending operation. The opaque handler
        // context is established once when the chain first parks and stays constant for the entire
        // deferred chain, so pending.context is deliberately preserved here; the re-arm terminal's own
        // context (if any) is left in the result and released with it. Move-assignment releases the
        // previous callbacks and clears the source; an empty source leaves the corresponding pending
        // callback empty, matching the prior "delete old, skip new" logic.
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            pending.onResponse = std::move(cmd.onResponse);
            pending.onError = std::move(cmd.onError);
        }

        pending.deferralDepth++;

        auto endpointOpt = ResolveEndpoint(*device, cmd.clusterId, cmd.endpointId);

        if (!endpointOpt.has_value())
        {
            icError("Failed to find endpoint for cluster 0x%x in deferred re-arm", cmd.clusterId);
            CompletePendingOperation(pendingId, false);
            return;
        }

        chip::EndpointId endpointId = *endpointOpt;

        auto decodedTlv = DecodeBase64Tlv(cmd.tlvBase64);

        if (!decodedTlv.has_value())
        {
            CompletePendingOperation(pendingId, false);
            return;
        }

        const uint8_t *tlvBuffer = decodedTlv->empty() ? nullptr : decodedTlv->data();
        size_t tlvLength = decodedTlv->size();

        // Send next command with deferred callbacks
        auto session = pending.sessionHandle.Get();

        if (!session.HasValue())
        {
            icError("Session expired before deferred requestCommand re-arm for operation %" PRIu64, pendingId);
            CompletePendingOperation(pendingId, false);
            return;
        }

        bool sent = device->SendCommandWithCallbacks(
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
        auto &ra = std::get<ResultTerminal::ReadAttribute>(result.terminal.data);

        // Re-arm: move the new response/error callbacks into the pending operation. The opaque handler
        // context is established once when the chain first parks and stays constant for the entire
        // deferred chain, so pending.context is deliberately preserved here; the re-arm terminal's own
        // context (if any) is left in the result and released with it. Move-assignment releases the
        // previous callbacks and clears the source; an empty source leaves the corresponding pending
        // callback empty, matching the prior "delete old, skip new" logic.
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            pending.onResponse = std::move(ra.onResponse);
            pending.onError = std::move(ra.onError);
        }

        pending.deferralDepth++;

        auto endpointOpt = ResolveEndpoint(*device, ra.clusterId, ra.endpointId);

        if (!endpointOpt.has_value())
        {
            CompletePendingOperation(pendingId, false);
            return;
        }

        chip::EndpointId endpointId = *endpointOpt;

        // Read from cache
        chip::TLV::TLVReader reader;
        CHIP_ERROR err = device->GetCachedAttributeData(endpointId, ra.clusterId, ra.attributeId, reader);

        std::optional<ParsedResult> nextResult;
        ScopedResultRelease nextResultRelease {nextResult};

        if (err != CHIP_NO_ERROR)
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto *ctx = MQuickJsRuntime::GetSharedContext();

            if (pending.onError.HasValue())
            {
                SafeJSValue args = SbmdHandlerInvoker::BuildDeferredErrorArgs(
                    ctx, pending.handlerContext, "readFailed", "Attribute not in cache", -1, pending.context.Get());
                nextResult = SbmdHandlerInvoker::InvokeHandler(ctx, pending.onError.Get(), args);
            }
        }
        else
        {
            auto tlvBase64Opt = EncodeTlvElementToBase64(reader, 256);

            if (!tlvBase64Opt.has_value())
            {
                CompletePendingOperation(pendingId, false);
                return;
            }

            const std::string &tlvBase64 = *tlvBase64Opt;

            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto *ctx = MQuickJsRuntime::GetSharedContext();

            if (pending.onResponse.HasValue())
            {
                SafeJSValue args = SbmdHandlerInvoker::BuildAttributeReadResponseArgs(
                    ctx, pending.handlerContext, ra.clusterId, ra.attributeId, tlvBase64, pending.context.Get());
                nextResult = SbmdHandlerInvoker::InvokeHandler(ctx, pending.onResponse.Get(), args);
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

    // Release held handler references
    ReleasePendingHandlers(pending);

    pendingOperations.erase(it);
}

void SpecBasedMatterDeviceDriver::ReleasePendingHandlers(PendingOperation &pending)
{
    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());

    // Move-assigning an empty wrapper releases each held reference under the runtime mutex.
    pending.onResponse = SafeJSValue {};
    pending.onError = SafeJSValue {};
    pending.context = SafeJSValue {};
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

        auto endpointOpt = ResolveEndpoint(device, alias.clusterId, std::nullopt);

        if (!endpointOpt.has_value())
        {
            icWarn("no endpoint for cluster 0x%x (supplement '%s')", alias.clusterId, aliasName.c_str());
            return std::nullopt;
        }

        chip::TLV::TLVReader reader;
        CHIP_ERROR err =
            device.GetCachedAttributeData(*endpointOpt, alias.clusterId, alias.attributeId.value(), reader);

        if (err != CHIP_NO_ERROR)
        {
            icDebug("cache miss for supplement '%s' (cluster 0x%x attr 0x%x)",
                    aliasName.c_str(),
                    alias.clusterId,
                    alias.attributeId.value());
            return std::nullopt;
        }

        auto tlvBase64 = EncodeTlvElementToBase64(reader, 256);

        if (!tlvBase64.has_value())
        {
            icWarn("failed to copy TLV for supplement '%s'", aliasName.c_str());
            return std::nullopt;
        }

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

std::optional<chip::EndpointId>
SpecBasedMatterDeviceDriver::ResolveEndpoint(MatterDevice &device,
                                             uint32_t clusterId,
                                             const std::optional<uint32_t> &explicitEndpoint)
{
    if (explicitEndpoint.has_value())
    {
        return static_cast<chip::EndpointId>(explicitEndpoint.value());
    }

    chip::EndpointId endpointId = 0;

    if (!device.GetEndpointForCluster(clusterId, endpointId))
    {
        return std::nullopt;
    }

    return endpointId;
}

void SpecBasedMatterDeviceDriver::DispatchToHandlers(const std::string &deviceId,
                                                     const std::vector<const DispatchEntry *> &matches,
                                                     MatterDevice *matterDevice,
                                                     const HandlerContext &hctx,
                                                     uint32_t clusterId,
                                                     uint32_t elementId,
                                                     const char *kindLabel,
                                                     const char *elementNoun,
                                                     const std::function<SafeJSValue(JSContext *)> &buildArgs)
{
    for (const auto *entry : matches)
    {
        if (entry->handler == nullptr || JS_IsUndefined(entry->handler->Fn()))
        {
            continue;
        }

        // Resolve declared supplements up front (device-service I/O) so the JS mutex is held
        // only around the JS work below.
        FetchedSupplements supplements;

        if (matterDevice)
        {
            supplements = SbmdHandlerInvoker::PrefetchSupplements(entry->handler->supplements,
                                                                  MakeAttrFetcher(*matterDevice),
                                                                  MakeResFetcher(deviceId),
                                                                  MakePersistFetcher(deviceId),
                                                                  MakeTransientFetcher(deviceId));
        }

        std::optional<ParsedResult> result;
        ScopedResultRelease resultRelease {result};

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto *ctx = MQuickJsRuntime::GetSharedContext();

            // args keeps its value alive for its whole lifetime, so it survives the allocations in
            // AddSupplements and the handler call without a separate guard.
            SafeJSValue args = buildArgs(ctx);
            SbmdHandlerInvoker::AddSupplements(ctx, args, entry->handler->supplements, supplements);
            result = SbmdHandlerInvoker::InvokeHandler(ctx, entry->handler->Fn(), args);
        }

        if (!result.has_value())
        {
            icWarn("%s handler '%s' returned no result for cluster 0x%x %s 0x%x",
                   kindLabel,
                   entry->handler->name.c_str(),
                   clusterId,
                   elementNoun,
                   elementId);
            continue;
        }

        // Execute ops (updateResource, setMetadata, etc.)
        SbmdHandlerInvoker::ExecuteOps(hctx, result->ops, MakeTransientSetter(deviceId));

        // Device-initiated dispatch consumes only the handler's ops: any outbound terminal
        // (sendCommand/requestCommand/readAttribute/writeAttribute) is intentionally unsupported
        // from a device-initiated report path and is ignored here with a warning — no outbound action.
        if (std::holds_alternative<ResultTerminal::Error>(result->terminal.data))
        {
            const auto &err = std::get<ResultTerminal::Error>(result->terminal.data);
            icWarn("%s handler '%s' returned error: %s", kindLabel, entry->handler->name.c_str(), err.message.c_str());
        }
        else if (!std::holds_alternative<ResultTerminal::Success>(result->terminal.data))
        {
            icWarn("%s handler '%s' returned an outbound terminal that is unsupported from a "
                   "device-initiated report; ignoring (no outbound action taken)",
                   kindLabel,
                   entry->handler->name.c_str());
        }
    }
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
    auto tlvBase64Opt = EncodeTlvElementToBase64(reader, 256); // Attributes rarely exceed this

    if (!tlvBase64Opt.has_value())
    {
        icWarn("Failed to copy TLV element for cluster 0x%x attribute 0x%x", clusterId, attributeId);
        return;
    }

    if (tlvBase64Opt->empty())
    {
        icDebug("Empty TLV data for attribute 0x%x", attributeId);
        return;
    }

    const std::string &tlvBase64 = *tlvBase64Opt;

    // Build handler context
    HandlerContext hctx;
    hctx.deviceUuid = deviceId;
    hctx.endpointId = std::to_string(endpointId);

    auto matterDevice = GetDevice(deviceId);

    if (matterDevice)
    {
        hctx.clusterFeatureMaps = matterDevice->GetCachedClusterFeatureMaps();
    }

    DispatchToHandlers(
        deviceId, matches, matterDevice.get(), hctx, clusterId, attributeId, "Attribute", "attr", [&](JSContext *ctx) {
            return SbmdHandlerInvoker::BuildAttributeArgs(ctx, hctx, clusterId, attributeId, tlvBase64);
        });
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
    auto tlvBase64Opt = EncodeTlvElementToBase64(reader, 1024); // Events may carry structured data

    if (!tlvBase64Opt.has_value())
    {
        icWarn("Failed to copy TLV element for cluster 0x%x event 0x%x", clusterId, eventId);
        return;
    }

    if (tlvBase64Opt->empty())
    {
        icDebug("Empty TLV data for event 0x%x", eventId);
        return;
    }

    const std::string &tlvBase64 = *tlvBase64Opt;

    // Build handler context
    HandlerContext hctx;
    hctx.deviceUuid = deviceId;
    hctx.endpointId = std::to_string(endpointId);

    auto matterDevice = GetDevice(deviceId);

    if (matterDevice)
    {
        hctx.clusterFeatureMaps = matterDevice->GetCachedClusterFeatureMaps();
    }

    DispatchToHandlers(
        deviceId, matches, matterDevice.get(), hctx, clusterId, eventId, "Event", "event", [&](JSContext *ctx) {
            return SbmdHandlerInvoker::BuildEventArgs(ctx, hctx, clusterId, eventId, tlvBase64);
        });
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

    if (matterDevice)
    {
        hctx.clusterFeatureMaps = matterDevice->GetCachedClusterFeatureMaps();
    }

    DispatchToHandlers(
        deviceId, matches, matterDevice.get(), hctx, clusterId, commandId, "Command", "command", [&](JSContext *ctx) {
            return SbmdHandlerInvoker::BuildCommandArgs(ctx, hctx, clusterId, commandId, tlvBase64);
        });
}
