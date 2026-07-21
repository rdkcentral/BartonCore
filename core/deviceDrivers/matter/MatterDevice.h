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
// Created by tlea on 12/4/25
//

#pragma once

#include "app/CommandHandlerInterface.h"
#include "app/CommandSender.h"
#include "lib/core/DataModelTypes.h"
#include "lib/core/TLVReader.h"
#include "subsystems/matter/DeviceDataCache.h"
#include <forward_list>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
#include <device/icDeviceResource.h>
}

// Forward declarations for Matter SDK
namespace chip
{
    namespace Messaging
    {
        class ExchangeManager;
    }
    class SessionHandle;
} // namespace chip

namespace barton
{
    /**
     * An instance of a Matter device, containing all of its locally managed state (with the
     * exception of icDevice, which is managed by DeviceService). It provides access to the
     * device's data cache and other per-device state, callbacks for when the device changes,
     * and other methods to interact with the device.
     */
    class MatterDevice : public chip::app::WriteClient::Callback,
                         public chip::app::CommandSender::ExtendableCallback
    {
    public:
        MatterDevice(std::string deviceId, std::shared_ptr<DeviceDataCache> deviceDataCache);

        virtual ~MatterDevice();

        const std::string &GetDeviceId() const { return deviceId; }

        /**
         * Callback type for attribute change handling.
         * Receives the endpoint, cluster, and attribute IDs along with a TLV reader positioned
         * at the attribute value. Called from CacheCallback::OnAttributeChanged when set.
         */
        using AttributeCallback = std::function<void(const std::string &deviceId,
                                                       chip::EndpointId endpointId,
                                                       chip::ClusterId clusterId,
                                                       chip::AttributeId attributeId,
                                                       chip::TLV::TLVReader &reader)>;

        /**
         * Callback type for event data handling.
         * Receives the endpoint, cluster, and event IDs along with a TLV reader positioned
         * at the event data. Called from CacheCallback::OnEventData when set.
         */
        using EventCallback = std::function<void(const std::string &deviceId,
                                                 chip::EndpointId endpointId,
                                                 chip::ClusterId clusterId,
                                                 chip::EventId eventId,
                                                 chip::TLV::TLVReader &reader)>;

        /**
         * Set an attribute callback. When set, CacheCallback::OnAttributeChanged will
         * dispatch attribute data through this callback.
         */
        void SetAttributeCallback(AttributeCallback callback)
        {
            attributeCallback = std::move(callback);
        }

        /**
         * Callback type for incoming (server-side) command handling.
         * Receives the endpoint, cluster, and command IDs along with a TLV reader positioned
         * at the command payload. Called from IncomingCommandHandler::InvokeCommand when set.
         */
        using CommandCallback = std::function<void(const std::string &deviceId,
                                                   chip::EndpointId endpointId,
                                                   chip::ClusterId clusterId,
                                                   chip::CommandId commandId,
                                                   chip::TLV::TLVReader &reader)>;

        /**
         * Set an event callback. When set, CacheCallback::OnEventData will
         * call this to dispatch event data to the driver.
         */
        void SetEventCallback(EventCallback callback) { eventCallback = std::move(callback); }

        /**
         * Set a command callback for incoming (server-side) commands.
         * When set, IncomingCommandHandler::InvokeCommand will call this.
         */
        void SetCommandCallback(CommandCallback callback) { commandCallback = std::move(callback); }

        /**
         * Register a CommandHandlerInterface for the given cluster so that incoming
         * commands on that cluster are routed to the commandCallback.
         * Uses Optional<EndpointId>::Missing() to handle all endpoints.
         *
         * @param clusterId The Matter cluster ID to handle incoming commands for.
         */
        void RegisterIncomingCommandHandler(chip::ClusterId clusterId);

        /**
         * Unregister all incoming command handlers previously registered via
         * RegisterIncomingCommandHandler. Called from the destructor.
         */
        void UnregisterIncomingCommandHandlers();

        /**
         * Set the list of cluster IDs to get feature maps from.
         * These are specified in the SBMD spec's matterMeta.featureClusters.
         */
        void SetFeatureClusters(std::vector<uint32_t> clusters)
        {
            featureClusters = std::move(clusters);
        }

        /**
         * Get the cached cluster feature maps.
         * @return Map of cluster ID to feature map value.
         */
        std::map<uint32_t, uint32_t> GetCachedClusterFeatureMaps() const
        {
            std::lock_guard<std::mutex> lock(cachedClusterFeatureMapsMutex);
            return cachedClusterFeatureMaps;
        }

        /**
         * Compute feature maps for all configured featureClusters and cache them.
         * Normally invoked when the cache subscription is established, but may be called
         * explicitly once the priming report has populated the cache (e.g. after the device's
         * CacheCallback is registered, which can happen after the subscription was established).
         */
        void UpdateCachedFeatureMaps();

        std::shared_ptr<DeviceDataCache> GetDeviceDataCache() const { return deviceDataCache; }

        /**
         * Build the SBMD-endpoint-to-Matter-endpoint mapping by matching device type lists
         * from the Descriptor cluster against the provided device types.
         *
         * @param driverSupportedDeviceTypes The Matter device type IDs to match against.
         * @return True if at least one matching endpoint was found, false otherwise.
         */
        bool ResolveEndpointMap(const std::vector<uint16_t> &driverSupportedDeviceTypes);

        //WriteClient::Callback overrides
        /**
         * OnResponse will be called when a write response has been received
         * and processed for the given path.
         *
         * The WriteClient object MUST continue to exist after this call is completed. The application shall wait until it
         * receives an OnDone call before it shuts down the object.
         *
         * @param[in] apWriteClient   The write client object that initiated the write transaction.
         * @param[in] aPath           The attribute path field in write response.
         * @param[in] attributeStatus Attribute-specific status, containing an InteractionModel::Status code as well as
         *                            an optional cluster-specific status code.
         */
        void OnResponse(const chip::app::WriteClient * apWriteClient, const chip::app::ConcreteDataAttributePath & aPath,
                                chip::app::StatusIB attributeStatus) override;

        /**
         * OnError will be called when an error occurs *after* a successful call to SendWriteRequest(). The following
         * errors will be delivered through this call in the aError field:
         *
         * - CHIP_ERROR_TIMEOUT: A response was not received within the expected response timeout.
         * - CHIP_ERROR_*TLV*: A malformed, non-compliant response was received from the server.
         * - CHIP_ERROR encapsulating a StatusIB: If we got a non-path-specific
         *   status response from the server.  In that case, constructing
         *   a StatusIB from the error can be used to extract the status.
         * - CHIP_ERROR*: All other cases.
         *
         * The WriteClient object MUST continue to exist after this call is completed. The application shall wait until it
         * receives an OnDone call before it shuts down the object.
         *
         * @param[in] apWriteClient The write client object that initiated the attribute write transaction.
         * @param[in] aError        A system error code that conveys the overall error code.
         */
        void OnError(const chip::app::WriteClient * apWriteClient, CHIP_ERROR aError) override;

        /**
         * OnDone will be called when WriteClient has finished all work and is reserved for future WriteClient ownership change.
         * (#10366) Users may use this function to release their own objects related to this write interaction.
         *
         * This function will:
         *      - Always be called exactly *once* for a given WriteClient instance.
         *      - Be called even in error circumstances.
         *      - Only be called after a successful call to SendWriteRequest has been made.
         *
         * @param[in] apWriteClient The write client object of the terminated write transaction.
         */
        void OnDone(chip::app::WriteClient * apWriteClient) override;

        // CommandSender::ExtendableCallback overrides
        /**
         * OnResponse will be called when a command response has been received.
         * If the command was successful and returned data, aResponseData.data will be non-null.
         * If the command failed with a status, aResponseData.statusIB will contain the error.
         *
         * @param[in] apCommandSender The command sender object that initiated the command transaction.
         * @param[in] aResponseData   Information pertaining to the response.
         */
        void OnResponse(chip::app::CommandSender *apCommandSender,
                        const chip::app::CommandSender::ResponseData &aResponseData) override;

        /**
         * OnError will be called when a non-path-specific error occurs.
         *
         * @param[in] apCommandSender The command sender object that initiated the command transaction.
         * @param[in] aErrorData      Error data regarding the error that occurred.
         */
        void OnError(const chip::app::CommandSender *apCommandSender,
                     const chip::app::CommandSender::ErrorData &aErrorData) override;

        /**
         * OnDone will be called when CommandSender has finished all work and is safe to destroy.
         *
         * @param[in] apCommandSender The command sender object of the terminated command transaction.
         */
        void OnDone(chip::app::CommandSender *apCommandSender) override;

    private:
        // Allow test subclass to access private members for testing
        friend class TestableMatterDevice;
        friend class SpecBasedMatterDeviceDriver;

        /**
         * Send a command to the device using pre-encoded TLV data.
         */
        bool SendCommandFromTlv(std::forward_list<std::promise<bool>> &promises,
                                chip::ClusterId clusterId,
                                chip::CommandId commandId,
                                std::optional<uint16_t> timedInvokeTimeoutMs,
                                chip::EndpointId endpointId,
                                const uint8_t *tlvBuffer,
                                size_t encodedLength,
                                chip::Messaging::ExchangeManager &exchangeMgr,
                                const chip::SessionHandle &sessionHandle,
                                const char *uri,
                                char **response);

        /**
         * Send a command with deferred callbacks instead of promise-based completion.
         * Used by requestCommand terminals where the driver manages the promise.
         *
         * @param onResponse Called on successful response with path and optional data TLV.
         * @param onError Called when the command fails (path error or transport error).
         * @return true if the command was successfully initiated.
         */
        bool SendCommandWithCallbacks(chip::ClusterId clusterId,
                                      chip::CommandId commandId,
                                      std::optional<uint16_t> timedInvokeTimeoutMs,
                                      chip::EndpointId endpointId,
                                      const uint8_t *tlvBuffer,
                                      size_t encodedLength,
                                      chip::Messaging::ExchangeManager &exchangeMgr,
                                      const chip::SessionHandle &sessionHandle,
                                      std::function<void(const chip::app::ConcreteCommandPath &,
                                                         chip::TLV::TLVReader *)> onResponse,
                                      std::function<void(CHIP_ERROR)> onError);

        /**
         * Write an attribute to the device using pre-encoded TLV data.
         */
        bool WriteAttributeFromTlv(std::forward_list<std::promise<bool>> &promises,
                                   chip::EndpointId endpointId,
                                   chip::ClusterId clusterId,
                                   chip::AttributeId attributeId,
                                   const uint8_t *tlvBuffer,
                                   size_t encodedLength,
                                   chip::Messaging::ExchangeManager &exchangeMgr,
                                   const chip::SessionHandle &sessionHandle,
                                   const char *uri);

        /**
         * Synchronously get attribute data from the cache as a TLVReader.
         * Note that this does not initiate a read from the device; it only reads
         * what is already cached.
         *
         * @param endpointId The endpoint ID to read from.
         * @param clusterId The cluster ID to read from.
         * @param attributeId The attribute ID to read.
         * @param reader The TLVReader to populate with the attribute data.
         * @return CHIP_NO_ERROR on success, or an error code on failure.
         */
        const CHIP_ERROR GetCachedAttributeData(chip::EndpointId endpointId,
                                                chip::ClusterId clusterId,
                                                chip::AttributeId attributeId,
                                                chip::TLV::TLVReader &reader) const
        {
            CHIP_ERROR result = CHIP_ERROR_INCORRECT_STATE;

            if (deviceDataCache)
            {
                chip::app::ConcreteAttributePath attributePath(endpointId, clusterId, attributeId);
                result = deviceDataCache->GetAttributeData(attributePath, reader);
            }

            return result;
        }

        /**
         * A device could have the same cluster on multiple endpoints, but we need the one that
         * is part of our device type composition.
         *
         * Used for device-level SBMD resources (which aren't tied to a specific SBMD endpoint)
         * and for non-SBMD code paths (e.g., UpdateCachedFeatureMaps).
         * For endpoint-level SBMD resources, use GetEndpointForSbmdIndex() instead.
         *
         * @param clusterId The cluster ID to find.
         * @param[out] outEndpointId The endpoint ID that hosts the cluster.
         * @return True if found, false otherwise.
         */
        bool GetEndpointForCluster(chip::ClusterId clusterId, chip::EndpointId &outEndpointId);

        /**
         * Resolve the Matter endpoint for a given cluster, using the SBMD endpoint index if
         * available, with automatic fallback to cluster-based lookup when the mapped endpoint
         * doesn't host the required cluster (e.g. devices where clusters span
         * multiple Matter endpoints but are grouped under one SBMD endpoint).
         *
         * @param clusterId The cluster ID to resolve.
         * @param sbmdEndpointIndex Optional SBMD endpoint index; if nullopt, uses cluster-based lookup.
         * @param[out] outEndpointId The resolved Matter endpoint ID.
         * @return True if found, false otherwise.
         */
        bool ResolveEndpointForCluster(chip::ClusterId clusterId,
                                       std::optional<uint32_t> sbmdEndpointIndex,
                                       chip::EndpointId &outEndpointId);

        /**
         * Look up the pre-resolved Matter endpoint for a given SBMD endpoint index.
         *
         * @param sbmdIndex The 0-based SBMD endpoint index.
         * @param[out] outEndpointId The resolved Matter endpoint ID.
         * @return True if found, false if index is out of range.
         */
        bool GetEndpointForSbmdIndex(uint32_t sbmdIndex, chip::EndpointId &outEndpointId) const;

        /**
         * Get the feature map for a cluster from the device data cache.
         *
         * @param endpointId The endpoint ID
         * @param clusterId The cluster ID
         * @param[out] featureMap The feature map value if found
         * @return True if feature map was successfully read, false otherwise
         */
        bool GetClusterFeatureMap(chip::EndpointId endpointId, chip::ClusterId clusterId, uint32_t &featureMap);

        /**
         * @brief Call to ensure a driver operation is considered failed, e.g.,
         *        when no promises have been made. This does nothing
         *        when promises have been stored in the Matter SDK for a pending
         *        async callback.
         *
         * @param promises
         */
        void FailOperation(std::forward_list<std::promise<bool>> &promises)
        {
            std::promise<bool> operationOK;
            operationOK.set_value(false);

            // ConnectAndExecute will wait for the first failure, or the entire
            // set to become ready with 'true' values, whichever comes first.
            // Putting 'failed' states in the very beginning ensures nothing
            // will be waited on for no reason.
            promises.push_front(std::move(operationOK));
        }

        /**
         * Inner class that implements ClusterStateCache::Callback to receive notifications
         * about attribute changes from the DeviceDataCache.
         */
        class CacheCallback : public chip::app::ClusterStateCache::Callback
        {
        public:
            explicit CacheCallback(MatterDevice *device) : device(device) {}

            void OnReportBegin() override;
            void OnReportEnd() override;
            void OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                    const chip::app::ConcreteAttributePath &aPath) override;
            void OnEventData(const chip::app::EventHeader &aEventHeader,
                             chip::TLV::TLVReader *apData,
                             const chip::app::StatusIB *apStatus) override;
            void OnSubscriptionEstablished(chip::SubscriptionId aSubscriptionId) override;
            void OnError(CHIP_ERROR aError) override;
            void OnDeallocatePaths(chip::app::ReadPrepareParams &&aReadPrepareParams) override;
            void OnDone(chip::app::ReadClient *apReadClient) override;

        private:
            MatterDevice *device;
        };

        /**
         * Implements CommandHandlerInterface for a single cluster, routing
         * incoming commands to the owning MatterDevice's commandCallback.
         */
        class IncomingCommandHandler : public chip::app::CommandHandlerInterface
        {
        public:
            IncomingCommandHandler(MatterDevice *device, chip::ClusterId clusterId);
            ~IncomingCommandHandler() override;

            void InvokeCommand(HandlerContext &handlerContext) override;

        private:
            MatterDevice *device;
        };

        std::string deviceId;
        std::shared_ptr<DeviceDataCache> deviceDataCache;
        AttributeCallback attributeCallback;
        EventCallback eventCallback;
        CommandCallback commandCallback;
        std::unique_ptr<CacheCallback> cacheCallback;
        std::vector<std::unique_ptr<IncomingCommandHandler>> incomingCommandHandlers;
        std::vector<uint32_t> featureClusters;
        std::map<uint32_t, uint32_t> cachedClusterFeatureMaps;
        // Guards cachedClusterFeatureMaps. UpdateCachedFeatureMaps() runs on both the Matter event
        // loop (subscription established) and the commissioning thread (AddDevice), while
        // GetCachedClusterFeatureMaps() is read from handler threads.
        mutable std::mutex cachedClusterFeatureMapsMutex;
        std::map<uint32_t, chip::EndpointId> sbmdEndpointMap; // SBMD endpoint index -> resolved Matter EndpointId

        // Context for tracking active write operations
        struct WriteContext
        {
            std::promise<bool> *writePromise;
            std::unique_ptr<chip::app::WriteClient> writeClient;
            bool success = true; // Track overall success across all attribute responses
        };
        std::map<chip::app::WriteClient *, WriteContext> activeWriteContexts;

        // Context for tracking active command operations
        struct CommandContext
        {
            std::promise<bool> *commandPromise = nullptr;
            std::unique_ptr<chip::app::CommandSender> commandSender;
            char **response = nullptr;

            // Deferred mode: when set, OnResponse/OnError call these instead of resolving the promise
            std::function<void(const chip::app::ConcreteCommandPath &,
                               chip::TLV::TLVReader *)> deferredOnResponse;
            std::function<void(CHIP_ERROR)> deferredOnError;

            bool IsDeferred() const { return deferredOnResponse != nullptr; }
        };
        std::map<chip::app::CommandSender *, CommandContext> activeCommandContexts;
    };
} // namespace barton
