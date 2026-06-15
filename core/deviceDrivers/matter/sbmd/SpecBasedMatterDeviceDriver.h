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

        // GC-rooted deferred handlers (stored in JSGCRef for proper GC management)
        JSGCRef onResponseRef {};
        JSGCRef onErrorRef {};
        bool onResponseRooted = false;
        bool onErrorRooted = false;

        // Match criteria (for requestCommand responses)
        uint32_t clusterId = 0;
        uint32_t responseCommandId = 0;

        // Context for handler invocation
        HandlerContext handlerContext;

        // Context for continuing the chain
        MatterDevice *device = nullptr;
        chip::Messaging::ExchangeManager *exchangeMgr = nullptr;
        const chip::SessionHandle *sessionHandle = nullptr;
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
                             const ResultTerminal &terminal,
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
                                   const ResultTerminal::RequestCommand &cmd,
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
                                  const ResultTerminal::ReadAttribute &ra,
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
        void ContinueDeferredChain(PendingOperation &pending, const ParsedResult &result);

        /**
         * Complete a pending operation — resolve the parking promise and clean up.
         */
        void CompletePendingOperation(uint64_t pendingId, bool success);

        /**
         * Release GC roots for a pending operation's JS handlers.
         */
        void ReleasePendingGcRoots(PendingOperation &pending);

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
         * Handle a attribute report via the dispatch tables.
         * Called from MatterDevice::CacheCallback via the AttributeCallback.
         */
        void HandleAttributeReport(const std::string &deviceId,
                                   chip::EndpointId endpointId,
                                   chip::ClusterId clusterId,
                                   chip::AttributeId attributeId,
                                   chip::TLV::TLVReader &reader);

        std::optional<uint8_t> ConvertModesToBitmask(const std::vector<std::string> &modes);

        /** Map of device ID to set of resource keys (endpointId:resourceId) for optional resources that failed
         * configuration */
        std::map<std::string, std::set<std::string>> skippedOptionalResources;

        /** Active deferred operations indexed by unique ID */
        std::map<uint64_t, PendingOperation> pendingOperations;
        uint64_t nextPendingId = 1;

        friend class TestableSpecBasedMatterDeviceDriver;
    };
} // namespace barton
