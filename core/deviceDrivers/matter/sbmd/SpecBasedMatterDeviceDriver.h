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

#pragma once

#include "../MatterDevice.h"
#include "../MatterDeviceDriver.h"
#include "SbmdDriver.h"
#include "mquickjs/SbmdHandlerInvoker.h"
#include "mquickjs/SbmdResultExecutor.h"
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <transport/SessionHolder.h>
#include <unordered_map>

namespace barton
{
    /**
     * Tracks a parked resource operation waiting for a deferred response.
     *
     * When a handler returns a requestCommand or readAttribute terminal,
     * the resource operation is parked and the promise is held until
     * the deferred chain completes.
     */
    struct PendingOperation
    {
        uint64_t id = 0;
        std::promise<bool> *parkingPromise = nullptr;

        // Deferred handlers held alive for the operation's lifetime. An empty SafeJSValue (HasValue() == false)
        // means the callback was not supplied.
        SafeJSValue onResponse;
        SafeJSValue onError;
        SafeJSValue context;

        // Context for handler invocation
        HandlerContext handlerContext;

        // Context for continuing the chain
        std::weak_ptr<MatterDevice> device;
        chip::Messaging::ExchangeManager *exchangeMgr = nullptr;
        // SessionHolder (not a raw SessionHandle pointer): the deferred chain re-sends
        // commands across later event-loop turns, long after the originating connection
        // callback has returned. The holder keeps a stable, lifetime-tracked reference to
        // the session and reports release via its bool operator.
        chip::SessionHolder sessionHandle;
        char **readValue = nullptr;
        char **executeResponse = nullptr;

        // Timeout and depth tracking
        std::chrono::steady_clock::time_point overallDeadline;
        uint32_t deferralDepth = 0;
        static constexpr uint32_t MAX_DEFERRAL_DEPTH = 10;
        static constexpr uint32_t DEFAULT_OVERALL_TIMEOUT_MS = 30000;
    };

    class SpecBasedMatterDeviceDriver : public MatterDeviceDriver
    {
    public:
        SpecBasedMatterDeviceDriver(SbmdDriver *driver);

        std::vector<uint16_t> GetSupportedDeviceTypes() override;

        uint16_t GetSupportedVendorId() const override;
        uint16_t GetSupportedProductId() const override;
        bool IsVendorSpecificDriver() const override;

        bool AddDevice(std::unique_ptr<MatterDevice> device) override;

    protected:
        SubscriptionIntervalSecs GetDesiredSubscriptionIntervalSecs() override;

        void DoConfigureDevice(std::forward_list<std::promise<bool>> &promises,
                               const std::string &deviceId,
                               const DeviceDescriptor *deviceDescriptor,
                               chip::Messaging::ExchangeManager &exchangeMgr,
                               const chip::SessionHandle &sessionHandle) override;

        bool DoRegisterResources(icDevice *device) override;

        void DoSynchronizeDevice(std::forward_list<std::promise<bool>> &promises,
                                 const std::string &deviceId,
                                 chip::Messaging::ExchangeManager &exchangeMgr,
                                 const chip::SessionHandle &sessionHandle) override;

        void DoReadResource(std::forward_list<std::promise<bool>> &promises,
                            const std::string &deviceId,
                            icDeviceResource *resource,
                            char **value,
                            chip::Messaging::ExchangeManager &exchangeMgr,
                            const chip::SessionHandle &sessionHandle) override;

        bool DoWriteResource(std::forward_list<std::promise<bool>> &promises,
                             const std::string &deviceId,
                             icDeviceResource *resource,
                             const char *previousValue,
                             const char *newValue,
                             chip::Messaging::ExchangeManager &exchangeMgr,
                             const chip::SessionHandle &sessionHandle) override;

        void ExecuteResource(std::forward_list<std::promise<bool>> &promises,
                             const std::string &deviceId,
                             icDeviceResource *resource,
                             const char *arg,
                             char **response,
                             chip::Messaging::ExchangeManager &exchangeMgr,
                             const chip::SessionHandle &sessionHandle) override;

    private:
        SbmdDriver *driver = nullptr; // Non-owning. Owned by SbmdFactory.

        // Driver-based internal methods
        bool DoRegisterDriverResources(icDevice *device);
        void SeedInitialResourceValues(const std::string &deviceId);

        /**
         * Prerequisite check — evaluates prerequisites from registration data
         * against the device's data cache.
         */
        static bool CheckPrerequisites(const SbmdResource &resource, const MatterDevice &device);

        /**
         * Invoke a seed handler for a resource. Returns the seed value or empty string.
         */
        std::string InvokeSeedHandler(const std::string &deviceId,
                                      const std::string &endpointId,
                                      const SbmdResource &resource,
                                      MatterDevice *device = nullptr);

        /**
         * Find a resource by endpoint ID and resource ID.
         */
        const SbmdResource *FindDriverResource(const char *endpointId, const char *resourceId) const;

        /**
         * Handle a read/write/execute resource operation through the handler system.
         */
        void HandleResourceOp(std::forward_list<std::promise<bool>> &promises,
                              MatterDevice &device,
                              icDeviceResource *resource,
                              const char *input,
                              char **readValue,
                              char **executeResponse,
                              chip::Messaging::ExchangeManager &exchangeMgr,
                              const chip::SessionHandle &sessionHandle,
                              const char *opType);

        /**
         * Execute a result chain terminal — success, error, sendCommand, writeAttribute,
         * requestCommand, or readAttribute.
         */
        void ExecuteTerminal(std::forward_list<std::promise<bool>> &promises,
                             MatterDevice &device,
                             ResultTerminal &terminal,
                             const HandlerContext &hctx,
                             const char *uri,
                             char **readValue,
                             char **executeResponse,
                             chip::Messaging::ExchangeManager &exchangeMgr,
                             const chip::SessionHandle &sessionHandle);

        /**
         * Execute a requestCommand deferred terminal.
         * Sends the command and parks the resource operation.
         */
        void ExecuteRequestCommand(std::forward_list<std::promise<bool>> &promises,
                                   MatterDevice &device,
                                   ResultTerminal::RequestCommand &cmd,
                                   const HandlerContext &hctx,
                                   char **readValue,
                                   char **executeResponse,
                                   chip::Messaging::ExchangeManager &exchangeMgr,
                                   const chip::SessionHandle &sessionHandle);

        /**
         * Execute a readAttribute deferred terminal.
         * Reads from cache and calls onResponse handler.
         */
        void ExecuteReadAttribute(std::forward_list<std::promise<bool>> &promises,
                                  MatterDevice &device,
                                  ResultTerminal::ReadAttribute &ra,
                                  const HandlerContext &hctx,
                                  char **readValue,
                                  char **executeResponse,
                                  chip::Messaging::ExchangeManager &exchangeMgr,
                                  const chip::SessionHandle &sessionHandle);

        /**
         * Handle a deferred command response. Called from MatterDevice::OnResponse
         * via the deferred callback.
         */
        void HandleDeferredCommandResponse(uint64_t pendingId,
                                           const chip::app::ConcreteCommandPath &path,
                                           chip::TLV::TLVReader *data);

        /**
         * Handle a deferred command error. Called from MatterDevice::OnError
         * or MatterDevice::OnResponse (status failure) via the deferred callback.
         */
        void HandleDeferredCommandError(uint64_t pendingId, CHIP_ERROR error);

        /**
         * Continue a deferred chain with a new result. Handles the terminal
         * and may re-arm the pending operation or complete it.
         */
        void ContinueDeferredChain(PendingOperation &pending, ParsedResult &result);

        /**
         * Complete a pending operation — resolve the parking promise and clean up.
         */
        void CompletePendingOperation(uint64_t pendingId, bool success);

        /**
         * Release the held JS handler references for a pending operation.
         */
        void ReleasePendingHandlers(PendingOperation &pending);

        /**
         * Create an AttributeSupplementFetcher for the given device.
         * Resolves alias names via the driver's alias map, reads cached TLV,
         * and returns base64-encoded values.
         */
        AttributeSupplementFetcher MakeAttrFetcher(MatterDevice &device) const;

        /**
         * Create a ResourceSupplementFetcher for the given device UUID.
         * Reads resource values via deviceServiceGetResourceById.
         */
        ResourceSupplementFetcher MakeResFetcher(const std::string &deviceUuid) const;

        /**
         * Create a PersistentDataFetcher for the given device UUID.
         * Reads values via deviceServiceGetMetadata with sbmd. prefix.
         */
        PersistentDataFetcher MakePersistFetcher(const std::string &deviceUuid) const;

        /**
         * Create a TransientDataFetcher for the given device UUID.
         * Reads values from the in-memory transient store, checking TTL.
         */
        TransientDataFetcher MakeTransientFetcher(const std::string &deviceUuid);

        /**
         * Store a transient data value with TTL for a device.
         */
        void SetTransientData(const std::string &deviceUuid,
                              const std::string &key,
                              const std::string &value,
                              uint32_t ttlSecs);

        /**
         * Retrieve a transient data value for a device, returning nullopt if missing or expired.
         */
        std::optional<std::string> GetTransientData(const std::string &deviceUuid, const std::string &key);

        /**
         * Create a TransientDataSetter for the given device UUID.
         */
        TransientDataSetter MakeTransientSetter(const std::string &deviceUuid);

        struct TransientEntry
        {
            std::string value;
            std::chrono::steady_clock::time_point expiry;
        };

        /**
         * Per-device transient storage: deviceUuid → (key → entry).
         *
         * Thread-confinement invariant: this map (along with pendingOperations, nextPendingId, and
         * skippedOptionalResources) is only accessed from the Matter/CHIP event-loop thread. All
         * mutation and lookup happens inside handler dispatch and deferred-operation callbacks, which
         * are serialized onto that single thread, so no additional locking is required. Do not touch
         * these members from other threads.
         */
        std::unordered_map<std::string, std::unordered_map<std::string, TransientEntry>> transientStore;

        /**
         * Handle an attribute report via the dispatch tables.
         * Called from MatterDevice::CacheCallback via the AttributeCallback.
         */
        void HandleAttributeReport(const std::string &deviceId,
                                   chip::EndpointId endpointId,
                                   chip::ClusterId clusterId,
                                   chip::AttributeId attributeId,
                                   chip::TLV::TLVReader &reader);
        /**
         * Handle an event report via the dispatch tables.
         * Called from MatterDevice::CacheCallback via the EventCallback.
         */
        void HandleEvent(const std::string &deviceId,
                         chip::EndpointId endpointId,
                         chip::ClusterId clusterId,
                         chip::EventId eventId,
                         chip::TLV::TLVReader &reader);

        /**
         * Handle an incoming (server-side) command via the dispatch tables.
         * Called from MatterDevice::IncomingCommandHandler via the CommandCallback.
         */
        void HandleCommand(const std::string &deviceId,
                           chip::EndpointId endpointId,
                           chip::ClusterId clusterId,
                           uint32_t commandId,
                           const std::string &tlvBase64);

        /*
         * Shared device-initiated dispatch loop for attribute/event/command reports. Iterates the
         * matched handlers under the JS mutex, holds the args alive, adds supplements, invokes each handler,
         * executes its non-terminal ops, and logs (but takes no outbound action on) error or other
         * non-success terminals.
         *
         * deviceId     - UUID of the reporting device (used for supplement fetchers and op execution).
         * matches      - the dispatch entries whose handlers should run for this report.
         * matterDevice - device used to fetch attribute supplements, or nullptr to skip supplements.
         * hctx         - handler context passed to op execution.
         * clusterId    - the cluster id of the report, used in log messages.
         * elementId    - the attribute/event/command id of the report, used in log messages.
         * kindLabel    - human-readable kind for log messages (e.g. "Attribute"/"Event"/"Command").
         * elementNoun  - short noun for log messages (e.g. "attr"/"event"/"command").
         * buildArgs    - builds the per-handler args object for the given JSContext.
         */
        void DispatchToHandlers(const std::string &deviceId,
                                const std::vector<const DispatchEntry *> &matches,
                                MatterDevice *matterDevice,
                                const HandlerContext &hctx,
                                uint32_t clusterId,
                                uint32_t elementId,
                                const char *kindLabel,
                                const char *elementNoun,
                                const std::function<SafeJSValue(JSContext *)> &buildArgs);

        /*
         * Resolve the target endpoint for a cluster operation: use the explicit endpoint if the
         * terminal specified one, otherwise ask the device to map the cluster to an endpoint. Static
         * because it needs friend access to MatterDevice::GetEndpointForCluster.
         *
         * device           - the device whose endpoint mapping is queried when no explicit endpoint is
         *                     given.
         * clusterId        - the cluster whose endpoint is being resolved.
         * explicitEndpoint - the endpoint explicitly requested by the terminal, if any.
         *
         * Returns the resolved endpoint id, or std::nullopt if no endpoint could be resolved (the
         * caller decides how to log/fail).
         */
        static std::optional<chip::EndpointId>
        ResolveEndpoint(MatterDevice &device, uint32_t clusterId, const std::optional<uint32_t> &explicitEndpoint);

        std::optional<uint8_t> ConvertModesToBitmask(const std::vector<std::string> &modes);

        /** Map of device ID to set of resource keys (endpointId:resourceId) for optional resources that failed
         * prerequisite checks. Matter-thread-confined (see transientStore). */
        std::map<std::string, std::set<std::string>> skippedOptionalResources;

        /** Active deferred operations indexed by unique ID. Matter-thread-confined (see transientStore). */
        std::map<uint64_t, PendingOperation> pendingOperations;

        /** Monotonic ID source for pendingOperations. Matter-thread-confined (see transientStore). */
        uint64_t nextPendingId = 1;

        friend class TestableSpecBasedMatterDeviceDriver;
    };
} // namespace barton
