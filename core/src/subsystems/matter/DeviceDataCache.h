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

    class DeviceDataCache
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
         * Trigger a full reprocessing of the cache's attribute data as if a full attribute report was
         * received, invoking report and attribute callbacks (events not reprocessed).
         *
         * @return CHIP_NO_ERROR on success
         */
        CHIP_ERROR RegenerateAttributeReport();

    private:
        /**
         * Inner callback class that implements ClusterStateCache::Callback interface.
         * Delegates all callbacks back to the outer DeviceDataCache instance.
         */
        class ClusterStateCacheCallback : public chip::app::ClusterStateCache::Callback
        {
        public:
            explicit ClusterStateCacheCallback(DeviceDataCache *parent) : parent(parent) {}

            void NotifySubscriptionStillActive(const chip::app::ReadClient &apReadClient) override {}

            void OnReportBegin() override { parent->OnReportBegin(); }

            void OnReportEnd() override { parent->OnReportEnd(); }

            void OnEventData(const chip::app::EventHeader &aEventHeader,
                             chip::TLV::TLVReader *apData,
                             const chip::app::StatusIB *apStatus) override
            {
                parent->OnEventData(aEventHeader, apData, apStatus);
            }

            void OnAttributeData(const chip::app::ConcreteDataAttributePath &aPath,
                                 chip::TLV::TLVReader *apData,
                                 const chip::app::StatusIB &aStatus) override
            {
                parent->OnAttributeData(aPath, apData, aStatus);
            }

            void OnSubscriptionEstablished(chip::SubscriptionId aSubscriptionId) override
            {
                parent->OnSubscriptionEstablished(aSubscriptionId);
            }

            void OnError(CHIP_ERROR aError) override { parent->OnError(aError); }

            void OnDeallocatePaths(chip::app::ReadPrepareParams &&aReadPrepareParams) override
            {
                parent->OnDeallocatePaths(std::move(aReadPrepareParams));
            }

            void OnDone(chip::app::ReadClient *apReadClient) override { parent->OnDone(apReadClient); }

        private:
            DeviceDataCache *parent;
        };

        static void OnDeviceConnectedCallback(void *context,
                                              chip::Messaging::ExchangeManager &exchangeMgr,
                                              const chip::SessionHandle &sessionHandle);

        static void
        OnDeviceConnectionFailureCallback(void *context, const chip::ScopedNodeId &peerId, CHIP_ERROR error);

        // Non-static member methods called by the static callbacks
        void OnDeviceConnected(chip::Messaging::ExchangeManager &exchangeMgr, const chip::SessionHandle &sessionHandle);

        void OnDeviceConnectionFailure(const chip::ScopedNodeId &peerId, CHIP_ERROR error);

        // Callback methods called by the inner CacheCallback class
        void OnReportBegin();
        void OnReportEnd();
        void OnEventData(const chip::app::EventHeader &aEventHeader,
                         chip::TLV::TLVReader *apData,
                         const chip::app::StatusIB *apStatus);
        void OnAttributeData(const chip::app::ConcreteDataAttributePath &aPath,
                             chip::TLV::TLVReader *apData,
                             const chip::app::StatusIB &aStatus);
        void OnSubscriptionEstablished(chip::SubscriptionId aSubscriptionId);
        void OnError(CHIP_ERROR aError);
        void OnDeallocatePaths(chip::app::ReadPrepareParams &&aReadPrepareParams);
        void OnDone(chip::app::ReadClient *apReadClient);

        SubscriptionIntervalSecs CalculateFinalSubscriptionIntervalSecs();


        /**
         * Get a JSON representation of all clusters and attributes for a given endpoint.
         *
         * @param[in] endpointId The endpoint ID to get.
         * @return Json::Value object containing the endpoint data.
         */
        Json::Value GetEndpointAsJson(chip::EndpointId endpointId);

        // Order matters for destruction: readClient uses clusterStateCacheCallback, so it must be destroyed first
        std::unique_ptr<ClusterStateCacheCallback> clusterStateCacheCallback;
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
