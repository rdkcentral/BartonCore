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

#include "app/CommandSender.h"
#include "lib/core/DataModelTypes.h"
#include "lib/core/TLVReader.h"
#include "matter/sbmd/SbmdSpec.h"
#include "matter/sbmd/script/SbmdScript.h"
#include "subsystems/matter/DeviceDataCache.h"
#include <forward_list>
#include <future>
#include <map>
#include <string>
#include <unordered_map>
#include <optional>

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

        void SetScript(std::unique_ptr<SbmdScript> newScript)
        {
            script = std::move(newScript);
        }

        /**
         * Set the list of cluster IDs to get feature maps from.
         * These are specified in the SBMD spec's matterMeta.featureClusters.
         * If the device cache is already available, also updates the cached feature maps.
         */
        void SetFeatureClusters(std::vector<uint32_t> clusters)
        {
            featureClusters = std::move(clusters);
            // If we already have a script and cache, update feature maps now
            if (script && deviceDataCache)
            {
                UpdateCachedFeatureMaps();
            }
        }

        std::shared_ptr<DeviceDataCache> GetDeviceDataCache() const { return deviceDataCache; }

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
         * @param clusterId The cluster ID to find.
         * @param[out] outEndpointId The endpoint ID that hosts the cluster.
         * @return True if found, false otherwise.
         */
        bool GetEndpointForCluster(chip::ClusterId clusterId, chip::EndpointId &outEndpointId);

        /**
         * Get the Nth endpoint that hosts a given cluster (0-based index).
         * Useful for devices with multiple endpoints hosting the same cluster.
         *
         * @param clusterId The cluster ID to find.
         * @param index The 0-based index (0 = first, 1 = second, etc.).
         * @param[out] outEndpointId The endpoint ID that hosts the cluster.
         * @return True if found, false otherwise.
         */
        bool GetNthEndpointForCluster(chip::ClusterId clusterId, uint8_t index, chip::EndpointId &outEndpointId);

        /**
         * Bind a resource URI for read operations.
         * Can bind either an attribute or command based on what's in the mapper.
         *
         * @param uri The resource URI
         * @param mapper The mapper containing read configuration
         * @param matterEndpointHint If set, use this Matter endpoint instead of auto-resolving
         * @return True if binding was successful, false otherwise.
         */
        bool BindResourceReadInfo(const char *uri,
                                  const SbmdMapper &mapper,
                                  std::optional<chip::EndpointId> matterEndpointHint = std::nullopt);

        /**
         * Bind a resource URI for write operations.
         * The script returns full operation details (invoke/write) including cluster/command/attribute IDs.
         *
         * @param uri The resource URI
         * @param resourceKey The resource key for script lookup (endpointId:resourceId)
         * @param endpointId The endpoint ID (may be empty for device-level resources)
         * @param resourceId The resource identifier
         * @return True if binding was successful, false otherwise.
         */
        bool BindWriteInfo(const char *uri,
                           const std::string &resourceKey,
                           const std::string &endpointId,
                           const std::string &resourceId);

        /**
         * Bind a resource URI for execute operations.
         * The script returns full operation details (invoke) including cluster/command IDs.
         *
         * @param uri The resource URI
         * @param resourceKey The resource key for script lookup (endpointId:resourceId)
         * @param endpointId The endpoint ID (may be empty for device-level resources)
         * @param resourceId The resource identifier
         * @return True if binding was successful, false otherwise.
         */
        bool BindExecuteInfo(const char *uri,
                             const std::string &resourceKey,
                             const std::string &endpointId,
                             const std::string &resourceId);

        /**
         * Bind a resource URI for event-driven updates.
         * When the specified event is received, the event mapper script will convert
         * the event data to a resource value and update the resource.
         *
         * @param uri The resource URI
         * @param event The event information
         * @param matterEndpointHint If set, use this Matter endpoint instead of auto-resolving
         * @return True if binding was successful, false otherwise.
         */
        bool BindResourceEventInfo(const char *uri,
                                   const SbmdEvent &event,
                                   std::optional<chip::EndpointId> matterEndpointHint = std::nullopt);

        /**
         * Handle a resource read request by looking up the binding and executing the script.
         * If the related attribute data is in the cache, this is a synchronous operation.
         * Otherwise, it may involve an asynchronous read from the device [NOT YET IMPLEMENTED].
         *
         * @param promises Forward list of promises to fulfill on completion
         * @param resource The device resource to read
         * @param[out] value The output string value after script execution
         * @param exchangeMgr The exchange manager for Matter communication
         * @param sessionHandle The session handle for the device
         */
        void HandleResourceRead(std::forward_list<std::promise<bool>> &promises,
                                icDeviceResource *resource,
                                char **value,
                                chip::Messaging::ExchangeManager &exchangeMgr,
                                const chip::SessionHandle &sessionHandle);

        /**
         * Handle a resource write request by looking up the binding and executing the script.
         *
         * @param promises Forward list of promises to fulfill on completion
         * @param resource The device resource to read
         * @param previousValue The previous string value before the write
         * @param newValue The new string value to write
         * @param exchangeMgr The exchange manager for Matter communication
         * @param sessionHandle The session handle for the device
         */
        void HandleResourceWrite(std::forward_list<std::promise<bool>> &promises,
                                 icDeviceResource *resource,
                                 const char *previousValue,
                                 const char *newValue,
                                 chip::Messaging::ExchangeManager &exchangeMgr,
                                 const chip::SessionHandle &sessionHandle);

        /**
         * Handle a resource execute request by looking up the binding and executing the script.
         *
         * @param promises Forward list of promises to fulfill on completion
         * @param resource The device resource to execute
         * @param arg The input argument string
         * @param[out] response The output response string
         * @param exchangeMgr The exchange manager for Matter communication
         * @param sessionHandle The session handle for the device
         */
        void HandleResourceExecute(std::forward_list<std::promise<bool>> &promises,
                                   icDeviceResource *resource,
                                   const char *arg,
                                   char **response,
                                   chip::Messaging::ExchangeManager &exchangeMgr,
                                   const chip::SessionHandle &sessionHandle);

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
         * Compute feature maps for all configured featureClusters and pass them to the script.
         * Called when subscription is established and cluster data is available.
         */
        void UpdateCachedFeatureMaps();

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

        struct ResourceBinding
        {
            enum class Type
            {
                Attribute,
                Command,
                ScriptOnly // For write/execute mappers - script returns full operation details
            };
            Type type;

            // For Attribute type
            chip::app::ConcreteAttributePath attributePath;
            std::optional<SbmdAttribute> attribute;

            // For Command type
            std::optional<SbmdCommand> command;

            // For ScriptOnly type - resource identity for script lookup
            std::string resourceKey;
            std::string endpointId;
            std::string resourceId;
        };

        // Hash function for ConcreteAttributePath to enable fast lookup
        struct AttributePathHash
        {
            std::size_t operator()(const chip::app::ConcreteAttributePath &path) const
            {
                std::size_t result = std::hash<chip::EndpointId> {}(path.mEndpointId);
                result ^= std::hash<chip::ClusterId> {}(path.mClusterId) + 0x9e3779b9 + (result << 6) + (result >> 2);
                result ^=
                    std::hash<chip::AttributeId> {}(path.mAttributeId) + 0x9e3779b9 + (result << 6) + (result >> 2);
                return result;
            }
        };

        // Equality function for ConcreteAttributePath
        struct AttributePathEqual
        {
            bool operator()(const chip::app::ConcreteAttributePath &lhs,
                            const chip::app::ConcreteAttributePath &rhs) const
            {
                return lhs.mEndpointId == rhs.mEndpointId && lhs.mClusterId == rhs.mClusterId &&
                       lhs.mAttributeId == rhs.mAttributeId;
            }
        };

        // Structure to hold URI and binding info for fast attribute lookup
        struct AttributeReadBinding
        {
            std::string uri;
            ResourceBinding binding;
        };

        // EventPath structure for event lookup
        struct EventPath
        {
            chip::EndpointId endpointId;
            chip::ClusterId clusterId;
            chip::EventId eventId;

            bool operator==(const EventPath &other) const
            {
                return endpointId == other.endpointId && clusterId == other.clusterId && eventId == other.eventId;
            }
        };

        // Hash function for EventPath to enable fast lookup
        struct EventPathHash
        {
            std::size_t operator()(const EventPath &path) const
            {
                std::size_t result = std::hash<chip::EndpointId> {}(path.endpointId);
                result ^= std::hash<chip::ClusterId> {}(path.clusterId) + 0x9e3779b9 + (result << 6) + (result >> 2);
                result ^= std::hash<chip::EventId> {}(path.eventId) + 0x9e3779b9 + (result << 6) + (result >> 2);
                return result;
            }
        };

        // Structure to hold URI and binding info for fast event lookup
        struct EventBinding
        {
            std::string uri;
            SbmdEvent event;
        };

        /**
         * Send a command to the device using pre-encoded TLV data.
         * Common helper used by both write-command and execute-command paths.
         *
         * @param promises Forward list of promises to fulfill on completion
         * @param command The command definition with cluster info and optional timed invoke timeout
         * @param endpointId The endpoint to send the command to
         * @param tlvBuffer Buffer containing the TLV-encoded command arguments
         * @param encodedLength Length of the encoded TLV data
         * @param exchangeMgr The exchange manager for Matter communication
         * @param sessionHandle The session handle for the device
         * @param uri The resource URI (for logging)
         * @param response Optional pointer to store command response (nullptr for write operations)
         * @return True if command was successfully initiated, false otherwise
         */
        bool SendCommandFromTlv(std::forward_list<std::promise<bool>> &promises,
                                const SbmdCommand &command,
                                chip::EndpointId endpointId,
                                const uint8_t *tlvBuffer,
                                size_t encodedLength,
                                chip::Messaging::ExchangeManager &exchangeMgr,
                                const chip::SessionHandle &sessionHandle,
                                const char *uri,
                                char **response);

        std::string deviceId;
        std::shared_ptr<DeviceDataCache> deviceDataCache;
        std::unique_ptr<SbmdScript> script; //add this in a SbmdDevice subclass or move all drivers completely to SBMD
        std::unique_ptr<CacheCallback> cacheCallback;
        std::vector<uint32_t> featureClusters; // Cluster IDs to get feature maps from (from SBMD spec)
        std::map<std::string, ResourceBinding> resourceReadBindings;
        std::map<std::string, ResourceBinding> resourceWriteBindings;
        std::map<std::string, ResourceBinding> resourceExecuteBindings;
        // Fast O(1) lookup for readable attributes in OnAttributeData callback
        std::unordered_map<chip::app::ConcreteAttributePath,
                           AttributeReadBinding,
                           AttributePathHash,
                           AttributePathEqual> readableAttributeLookup;
        // Fast O(1) lookup for events in OnEventData callback
        std::unordered_map<EventPath, EventBinding, EventPathHash> eventLookup;

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
            std::promise<bool> *commandPromise;
            std::unique_ptr<chip::app::CommandSender> commandSender;
            SbmdCommand commandInfo; // For response mapping
            char **response;         // Pointer to store response string
        };
        std::map<chip::app::CommandSender *, CommandContext> activeCommandContexts;
    };
} // namespace barton
