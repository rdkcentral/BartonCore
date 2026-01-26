//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2024 Comcast Cable Communications Management, LLC
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
 * Created by Thomas Lea on 11/4/2025
 */

#pragma once

#include "MatterCommon.h"
#include "app/ClusterStateCache.h"
#include "controller/CHIPDeviceController.h"
#include "lib/core/DataModelTypes.h"

#include <future>
#include <json/json.h>
#include <string>

using namespace barton::Subsystem::Matter;

namespace barton
{

    class DeviceDataCache : public chip::app::ClusterStateCache::Callback
    {
    public:
        DeviceDataCache(const std::string &deviceUuid,
                        std::shared_ptr<chip::Controller::DeviceController> controller) :
            deviceUuid(deviceUuid), mOnDeviceConnectedCallback(OnDeviceConnectedCallback, this),
            mOnDeviceConnectionFailureCallback(OnDeviceConnectionFailureCallback, this), controller(controller)
        {
        }

        ~DeviceDataCache();

        /**
         * Start the device data cache.
         */
        std::future<bool> Start();

        std::string GetDeviceUuid() const { return deviceUuid; }

        /**
         * Get the timestamp of the last completed report.
         *
         * @return Unix timestamp (seconds since epoch) of the last OnDone callback, or 0 if no report has completed
         * yet.
         */
        uint64_t GetLastReportCompletedTimestamp() const { return lastReportCompletedTimestamp; }

        /**
         * Get a string attribute value from the cache.
         *
         * @param endpointId The endpoint ID.
         * @param clusterId The cluster ID.
         * @param attributeId The attribute ID.
         * @param[out] outValue The string value if found.
         * @return CHIP_NO_ERROR on success, error code otherwise.
         */
        CHIP_ERROR GetStringAttribute(chip::EndpointId endpointId,
                                      chip::ClusterId clusterId,
                                      chip::AttributeId attributeId,
                                      std::string &outValue) const;

        // Some simple accessors for common Basic Information Cluster attributes
        CHIP_ERROR GetVendorName(std::string &outValue) const;
        CHIP_ERROR GetProductName(std::string &outValue) const;
        CHIP_ERROR GetHardwareVersionString(std::string &outValue) const;
        CHIP_ERROR GetSoftwareVersionString(std::string &outValue) const;
        CHIP_ERROR GetSerialNumber(std::string &outValue) const;

        // Some common accessors for General Diagnostics Cluster attributes
        CHIP_ERROR GetMacAddress(std::string &outValue) const;
        CHIP_ERROR GetNetworkType(std::string &outValue) const;

        CHIP_ERROR GetDeviceTypes(chip::EndpointId endpointId, std::vector<uint16_t> &deviceTypes) const;

        /**
         * Get a list of all endpoint IDs from the device's Descriptor cluster PartsList.
         *
         * @return std::vector<chip::EndpointId> containing endpoint 0 and all endpoints from the PartsList.
         */
        std::vector<chip::EndpointId> GetEndpointIds() const;

        /**
         * Check if a specific endpoint has a server cluster.
         *
         * @param endpointId The ID of the endpoint to check.
         * @param clusterId The ID of the cluster to check for.
         * @return True if the endpoint has the server cluster, false otherwise.
         */
        bool EndpointHasServerCluster(chip::EndpointId endpointId, chip::ClusterId clusterId) const;

        /**
         * Get a complete JSON representation of all cached device data.
         *
         * @return Json::Value object containing all endpoints, clusters, and attributes.
         */
        Json::Value GetCacheAsJson();

        /**
         * Set the callback handler for cluster events and attribute changes.
         *
         * @param callback Pointer to the callback for a particular device+endpoint+cluster.
         *                 The cache does not take ownership.
         */
        void SetClusterCallback(std::tuple<std::string, chip::EndpointId, chip::ClusterId> key,
                                chip::app::ClusterStateCache::Callback *callback)
        {
            clusterCallbacks[key] = callback;
        }

        /**
         * Get attribute data stored in the cache as a TLVReader.
         */
        CHIP_ERROR GetAttributeData(const chip::app::ConcreteDataAttributePath &aPath,
                                      chip::TLV::TLVReader &aReader) const;

        /**
         * Get the network interface info for the operational network.
         *
         * @return Optional containing the network interface info if available, or NullOptional otherwise.
         */
        chip::Optional<NetworkUtils::NetworkInterfaceInfo> GetInterfaceInfo() const;

        /**
         * Check the device's power source type.
         * TODO: Account for multiple power sources (i.e. PowerSource clusters on multiple endpoints).
         */
        CHIP_ERROR IsBatteryPowered(bool &isBattery) const;
        CHIP_ERROR IsWiredPowered(bool &isWired) const;

        /**
         * Trigger a full reprocessing of the cache's attribute data as if a full attribute report was
         * received, invoking report and attribute callbacks (events not reprocessed).
         *
         * @return CHIP_NO_ERROR on success
         */
        CHIP_ERROR RegenerateAttributeReport();

    private:
        static void OnDeviceConnectedCallback(void *context,
                                              chip::Messaging::ExchangeManager &exchangeMgr,
                                              const chip::SessionHandle &sessionHandle);

        static void
        OnDeviceConnectionFailureCallback(void *context, const chip::ScopedNodeId &peerId, CHIP_ERROR error);

        // Non-static member methods called by the static callbacks
        void OnDeviceConnected(chip::Messaging::ExchangeManager &exchangeMgr, const chip::SessionHandle &sessionHandle);

        void OnDeviceConnectionFailure(const chip::ScopedNodeId &peerId, CHIP_ERROR error);

        /**
         * Used to notify a (maybe empty) report data is received from peer and the subscription and the peer is alive.
         *
         * The ReadClient MUST NOT be destroyed during execution of this callback (i.e. before the callback returns).
         *
         */
        void NotifySubscriptionStillActive(const chip::app::ReadClient &apReadClient) override {}

        /**
         * Used to signal the commencement of processing of the first attribute or event report received in a given
         * exchange.
         *
         * The ReadClient MUST NOT be destroyed during execution of this callback (i.e. before the callback returns).
         *
         * Once OnReportBegin has been called, either OnReportEnd or OnError will be called before OnDone.
         *
         */
        void OnReportBegin() override;

        /**
         * Used to signal the completion of processing of the last attribute or event report in a given exchange.
         *
         * The ReadClient MUST NOT be destroyed during execution of this callback (i.e. before the callback returns).
         *
         */
        void OnReportEnd() override;

        /**
         * Used to deliver event data received through the Read and Subscribe interactions
         *
         * Only one of the apData and apStatus can be non-null.
         *
         * The ReadClient MUST NOT be destroyed during execution of this callback (i.e. before the callback returns).
         *
         * @param[in] aEventHeader The event header in report response.
         * @param[in] apData A TLVReader positioned right on the payload of the event.
         * @param[in] apStatus Event-specific status, containing an InteractionModel::Status code as well as an optional
         *                     cluster-specific status code.
         */
        void OnEventData(const chip::app::EventHeader &aEventHeader,
                         chip::TLV::TLVReader *apData,
                         const chip::app::StatusIB *apStatus) override
        {
            auto key = std::make_tuple(deviceUuid, aEventHeader.mPath.mEndpointId, aEventHeader.mPath.mClusterId);
            auto it = clusterCallbacks.find(key);
            chip::app::ClusterStateCache::Callback *callback = (it != clusterCallbacks.end()) ? it->second : nullptr;
            if (callback != nullptr)
            {
                callback->OnEventData(aEventHeader, apData, apStatus);
            }
        }

        /**
         * Used to deliver attribute data received through the Read and Subscribe interactions.
         *
         * This callback will be called when:
         *   - Receiving attribute data as response of Read interactions
         *   - Receiving attribute data as reports of subscriptions
         *   - Receiving attribute data as initial reports of subscriptions
         *
         * The ReadClient MUST NOT be destroyed during execution of this callback (i.e. before the callback returns).
         *
         * @param[in] aPath        The attribute path field in report response.
         * @param[in] apData       The attribute data of the given path, will be a nullptr if status is not Success.
         * @param[in] aStatus      Attribute-specific status, containing an InteractionModel::Status code as well as an
         *                         optional cluster-specific status code.
         */
        void OnAttributeData(const chip::app::ConcreteDataAttributePath &aPath,
                             chip::TLV::TLVReader *apData,
                             const chip::app::StatusIB &aStatus) override
        {
            auto key = std::make_tuple(deviceUuid, aPath.mEndpointId, aPath.mClusterId);
            auto it = clusterCallbacks.find(key);
            chip::app::ClusterStateCache::Callback *callback = (it != clusterCallbacks.end()) ? it->second : nullptr;
            if (callback != nullptr && aStatus.IsSuccess())
            {
                callback->OnAttributeChanged(clusterStateCache.get(), aPath);
            }
        }

        /**
         * OnSubscriptionEstablished will be called when a subscription is established for the given subscription
         * transaction. If using auto resubscription, OnSubscriptionEstablished will be called whenever resubscription
         * is established.
         *
         * The ReadClient MUST NOT be destroyed during execution of this callback (i.e. before the callback returns).
         *
         * @param[in] aSubscriptionId The identifier of the subscription that was established.
         */
        void OnSubscriptionEstablished(chip::SubscriptionId aSubscriptionId) override;

        /**
         * OnError will be called when an error occurs *after* a successful call to SendRequest(). The following
         * errors will be delivered through this call in the aError field:
         *
         * - CHIP_ERROR_TIMEOUT: A response was not received within the expected response timeout.
         * - CHIP_ERROR_*TLV*: A malformed, non-compliant response was received from the server.
         * - CHIP_ERROR encapsulating a StatusIB: If we got a non-path-specific
         *   status response from the server.  In that case, constructing
         *   a StatusIB from the error can be used to extract the status.
         * - CHIP_ERROR*: All other cases.
         *
         * The ReadClient MUST NOT be destroyed during execution of this callback (i.e. before the callback returns).
         *
         * @param[in] aError       A system error code that conveys the overall error code.
         */
        void OnError(CHIP_ERROR aError) override;

        /**
         * This function is invoked when using SendAutoResubscribeRequest, where the ReadClient was configured to auto
         * re-subscribe and the ReadPrepareParams was moved into this client for management. This will have to be
         * free'ed appropriately by the application. If SendAutoResubscribeRequest fails, this function will be called
         * before it returns the failure. If SendAutoResubscribeRequest succeeds, this function will be called
         * immediately before calling OnDone, or when the ReadClient is destroyed, if that happens before OnDone. If
         * SendAutoResubscribeRequest is not called, this function will not be called.
         */
        void OnDeallocatePaths(chip::app::ReadPrepareParams &&aReadPrepareParams) override;

        /**
         * OnDone will be called when ReadClient has finished all work for the ongoing Read or Subscribe interaction.
         * There will be no more callbacks after OnDone is called.
         *
         * @param[in] apReadClient A pointer to the ReadClient for the completed interaction.
         */
        void OnDone(chip::app::ReadClient *apReadClient) override;

        SubscriptionIntervalSecs CalculateFinalSubscriptionIntervalSecs();

        // TODO: Account for multiple power sources (i.e. PowerSource clusters on multiple endpoints).
        CHIP_ERROR GetPowerSourceFeatureMap(uint32_t &featureMap) const;

        /**
         * Get a JSON representation of all clusters and attributes for a given endpoint.
         *
         * @param[in] endpointId The endpoint ID to get.
         * @return Json::Value object containing the endpoint data.
         */
        Json::Value GetEndpointAsJson(chip::EndpointId endpointId);

        std::unique_ptr<chip::app::ClusterStateCache> clusterStateCache;
        std::unique_ptr<chip::app::ReadClient> readClient;
        std::string deviceUuid;
        uint32_t mOnResubscriptionsAttempted = 0;
        std::unique_ptr<std::promise<bool>> startupPromise;
        std::mutex startupPromiseMutex;
        uint64_t lastReportCompletedTimestamp = 0;
        std::shared_ptr<chip::Controller::DeviceController> controller = nullptr;

        // device id + endpoint id + cluster id to ClusterStateCache Callback
        std::map<std::tuple<std::string, chip::EndpointId, chip::ClusterId>, chip::app::ClusterStateCache::Callback*>
            clusterCallbacks;
        chip::Callback::Callback<chip::OnDeviceConnected> mOnDeviceConnectedCallback;
        chip::Callback::Callback<chip::OnDeviceConnectionFailure> mOnDeviceConnectionFailureCallback;
    };

} // namespace barton
