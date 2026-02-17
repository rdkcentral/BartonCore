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

#include "lib/core/CHIPError.h"
#define LOG_TAG     "DeviceDataCache"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "DeviceDataCache.h"
#include "MatterCommon.h"
#include <app-common/zap-generated/cluster-objects.h>
#include <app/data-model/Decode.h>
#include <ctime>
#include <lib/support/jsontlv/TlvJson.h>

extern "C" {
#include <icLog/logging.h>
}

using namespace barton;
using namespace barton::Subsystem;

DeviceDataCache::~DeviceDataCache()
{
    icDebug();

    // readClient and clusterStateCache need cleanup to run under the Matter stack context.
    // We must wait for destruction to complete synchronously to avoid use-after-free,
    // since clusterStateCache holds a reference to *this as its callback.

    // Move the unique_ptrs to local variables so they can be safely destroyed on the Matter thread
    auto *cacheToDestroy = clusterStateCache.release();
    auto *clientToDestroy = readClient.release();

    // Only perform cleanup if there's something to destroy
    if (cacheToDestroy != nullptr || clientToDestroy != nullptr)
    {
        // Check if we're already on the Matter thread to avoid deadlock
        if (chip::DeviceLayer::PlatformMgr().IsChipStackLockedByCurrentThread())
        {
            // Already on Matter thread - destroy directly
            // Reset in proper order: client depends on cache, so destroy client first
            delete clientToDestroy;
            delete cacheToDestroy;
        }
        else
        {
            // Not on Matter thread - schedule destruction and wait
            std::promise<void> destructionComplete;
            std::future<void> destructionFuture = destructionComplete.get_future();

            chip::DeviceLayer::PlatformMgr().ScheduleWork(
                [](intptr_t arg) {
                    auto *context = reinterpret_cast<std::tuple<void *, void *, std::promise<void> *> *>(arg);
                    auto *cache = static_cast<chip::app::ClusterStateCache *>(std::get<0>(*context));
                    auto *client = static_cast<chip::app::ReadClient *>(std::get<1>(*context));
                    auto *promise = std::get<2>(*context);

                    // Reset in proper order: client depends on cache, so destroy client first
                    delete client;
                    delete cache;

                    // Signal that destruction is complete
                    promise->set_value();
                    delete context;
                },
                reinterpret_cast<intptr_t>(
                    new std::tuple<void *, void *, std::promise<void> *>(cacheToDestroy, clientToDestroy, &destructionComplete)));

            // Wait for the Matter thread to complete destruction before returning
            destructionFuture.wait();
        }
    }
}

std::future<bool> DeviceDataCache::Start()
{
    icDebug();
    std::future<bool> future;

    // Check if a startup is already in progress and allocate the startup promise
    {
        std::lock_guard<std::mutex> lock(startupPromiseMutex);
        if (startupPromise)
        {
            icError("Start() called while startup already in progress");
            // Create a promise on the heap, set it, get the future, then let it be destroyed
            auto failedPromise = std::make_shared<std::promise<bool>>();
            auto failedFuture = failedPromise->get_future();
            failedPromise->set_value(false);
            return failedFuture;
        }
        startupPromise = std::make_unique<std::promise<bool>>();
        future = startupPromise->get_future();
    }

    chip::DeviceLayer::PlatformMgr().ScheduleWork(
        [](intptr_t arg) {
            auto *self = reinterpret_cast<DeviceDataCache *>(arg);

            if (self->controller->GetConnectedDevice(barton::Subsystem::Matter::UuidToNodeId(self->deviceUuid),
                                                     &self->mOnDeviceConnectedCallback,
                                                     &self->mOnDeviceConnectionFailureCallback) != CHIP_NO_ERROR)
            {
                icError("Failed to start device connection");
                std::lock_guard<std::mutex> lock(self->startupPromiseMutex);
                if (self->startupPromise)
                {
                    self->startupPromise->set_value(false);
                    self->startupPromise.reset();
                }
            }
        },
        reinterpret_cast<intptr_t>(this));

    return future;
}

CHIP_ERROR DeviceDataCache::GetStringAttribute(chip::EndpointId endpointId,
                                               chip::ClusterId clusterId,
                                               chip::AttributeId attributeId,
                                               std::string &outValue) const
{
    if (!clusterStateCache)
    {
        return CHIP_ERROR_INCORRECT_STATE;
    }

    chip::app::ConcreteAttributePath attributePath(endpointId, clusterId, attributeId);

    chip::TLV::TLVReader reader;
    CHIP_ERROR err = clusterStateCache->Get(attributePath, reader);
    if (err != CHIP_NO_ERROR)
    {
        return err;
    }

    chip::CharSpan value;
    err = reader.Get(value);
    if (err != CHIP_NO_ERROR)
    {
        return err;
    }

    outValue = std::string(value.data(), value.size());
    return CHIP_NO_ERROR;
}

CHIP_ERROR DeviceDataCache::GetVendorName(std::string &outValue) const
{
    return GetStringAttribute(0,
                              chip::app::Clusters::BasicInformation::Id,
                              chip::app::Clusters::BasicInformation::Attributes::VendorName::Id,
                              outValue);
}

CHIP_ERROR DeviceDataCache::GetProductName(std::string &outValue) const
{
    return GetStringAttribute(0,
                              chip::app::Clusters::BasicInformation::Id,
                              chip::app::Clusters::BasicInformation::Attributes::ProductName::Id,
                              outValue);
}

CHIP_ERROR DeviceDataCache::GetHardwareVersionString(std::string &outValue) const
{
    return GetStringAttribute(0,
                              chip::app::Clusters::BasicInformation::Id,
                              chip::app::Clusters::BasicInformation::Attributes::HardwareVersionString::Id,
                              outValue);
}

CHIP_ERROR DeviceDataCache::GetSoftwareVersionString(std::string &outValue) const
{
    return GetStringAttribute(0,
                              chip::app::Clusters::BasicInformation::Id,
                              chip::app::Clusters::BasicInformation::Attributes::SoftwareVersionString::Id,
                              outValue);
}

CHIP_ERROR DeviceDataCache::GetSerialNumber(std::string &outValue) const
{
    return GetStringAttribute(0,
                              chip::app::Clusters::BasicInformation::Id,
                              chip::app::Clusters::BasicInformation::Attributes::SerialNumber::Id,
                              outValue);
}

CHIP_ERROR DeviceDataCache::GetMacAddress(std::string &outValue) const
{
    auto interfaceInfo = GetInterfaceInfo();
    if (!interfaceInfo.HasValue())
    {
        icWarn("No MAC address for the operational network is found for the device %s", deviceUuid.c_str());
        return CHIP_ERROR_NOT_FOUND;
    }

    outValue = interfaceInfo.Value().macAddress;
    return CHIP_NO_ERROR;
}

CHIP_ERROR DeviceDataCache::GetNetworkType(std::string &outValue) const
{
    auto interfaceInfo = GetInterfaceInfo();
    if (!interfaceInfo.HasValue())
    {
        icWarn("No type for the operational network is found for the device %s", deviceUuid.c_str());
        return CHIP_ERROR_NOT_FOUND;
    }

    outValue = interfaceInfo.Value().networkType;
    return CHIP_NO_ERROR;
}

CHIP_ERROR DeviceDataCache::GetDeviceTypes(chip::EndpointId endpointId, std::vector<uint16_t> &deviceTypes) const
{
    // Check if the endpoint has the Descriptor cluster
    if (!EndpointHasServerCluster(endpointId, chip::app::Clusters::Descriptor::Id))
    {
        return CHIP_ERROR_NOT_FOUND;
    }

    // Read the DeviceTypeList attribute from the Descriptor cluster
    chip::app::ConcreteAttributePath deviceTypeListPath(
        endpointId,
        chip::app::Clusters::Descriptor::Id,
        chip::app::Clusters::Descriptor::Attributes::DeviceTypeList::Id);

    chip::TLV::TLVReader reader;
    CHIP_ERROR err = GetAttributeData(deviceTypeListPath, reader);

    if (err != CHIP_NO_ERROR)
    {
        icWarn("Failed to read DeviceTypeList from endpoint %d: %s", endpointId, err.AsString());
        return err;
    }

    // Decode the DeviceTypeList
    chip::app::DataModel::DecodableList<chip::app::Clusters::Descriptor::Structs::DeviceTypeStruct::DecodableType>
        deviceTypeList;
    err = chip::app::DataModel::Decode(reader, deviceTypeList);

    if (err != CHIP_NO_ERROR)
    {
        icWarn("Failed to decode DeviceTypeList from endpoint %d: %s", endpointId, err.AsString());
        return err;
    }

    auto iter = deviceTypeList.begin();
    while (iter.Next())
    {
        auto deviceTypeStruct = iter.GetValue();
        deviceTypes.emplace_back(static_cast<uint16_t>(deviceTypeStruct.deviceType));
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR DeviceDataCache::GetAttributeData(const chip::app::ConcreteDataAttributePath &aPath,
                                             chip::TLV::TLVReader &aReader) const
{
    if (!clusterStateCache)
    {
        return CHIP_ERROR_INCORRECT_STATE;
    }
    return clusterStateCache->Get(aPath, aReader);
}

std::vector<chip::EndpointId> DeviceDataCache::GetEndpointIds() const
{
    std::vector<chip::EndpointId> endpointIds;

    if (!clusterStateCache)
    {
        return endpointIds; // Return empty vector
    }

    // Read the PartsList attribute on the Descriptor cluster to get all endpoints
    chip::app::ConcreteAttributePath partsListPath(
        0, chip::app::Clusters::Descriptor::Id, chip::app::Clusters::Descriptor::Attributes::PartsList::Id);

    chip::TLV::TLVReader partsListReader;
    CHIP_ERROR err = clusterStateCache->Get(partsListPath, partsListReader);

    if (err == CHIP_NO_ERROR)
    {
        // Parse the PartsList array to get all endpoints
        chip::app::DataModel::DecodableList<chip::EndpointId> partsList;
        err = chip::app::DataModel::Decode(partsListReader, partsList);

        if (err == CHIP_NO_ERROR)
        {
            // Add endpoint 0 first
            endpointIds.push_back(0);

            // Get each endpoint from PartsList
            auto partsIter = partsList.begin();
            while (partsIter.Next())
            {
                chip::EndpointId endpointId = partsIter.GetValue();
                endpointIds.push_back(endpointId);
            }
        }
        else
        {
            icWarn("Failed to decode PartsList: %s", err.AsString());
        }
    }
    else
    {
        icWarn("Failed to get PartsList from cache: %s", err.AsString());
    }

    return endpointIds;
}

chip::Optional<NetworkUtils::NetworkInterfaceInfo> DeviceDataCache::GetInterfaceInfo() const
{
    using namespace chip::app::Clusters::GeneralDiagnostics;
    chip::TLV::TLVReader reader;
    CHIP_ERROR error = GetAttributeData({chip::kRootEndpointId, Id, Attributes::NetworkInterfaces::Id}, reader);
    if (error != CHIP_NO_ERROR)
    {
        icWarn("Failed to read network interfaces report from device %s: %s", deviceUuid.c_str(), error.AsString());
    }

    using TypeInfo = Attributes::NetworkInterfaces::TypeInfo;
    TypeInfo::DecodableType value;
    error = chip::app::DataModel::Decode(reader, value);
    if (error != CHIP_NO_ERROR)
    {
        icError(
            "Failed to decode NetworkInterfaces attribute from device %s: %s", deviceUuid.c_str(), error.AsString());
        return chip::NullOptional;
    }

    auto sessionMgr = chip::app::InteractionModelEngine::GetInstance()->GetExchangeManager()->GetSessionManager();
    chip::NodeId nodeId = barton::Subsystem::Matter::UuidToNodeId(deviceUuid);
    chip::Transport::Session *foundSession = nullptr;

    sessionMgr->GetSecureSessions().ForEachSession([nodeId, &foundSession](auto *session) {
        if (session && session->GetPeer().GetNodeId() == nodeId)
        {
            foundSession = session;
            return chip::Loop::Break;
        }
        return chip::Loop::Continue;
    });

    if (foundSession)
    {
        char nodeIpv6Addr[INET6_ADDRSTRLEN] = {};
        foundSession->AsSecureSession()->GetPeerAddress().GetIPAddress().ToString(nodeIpv6Addr);
        return NetworkUtils::ExtractOperationalInterfaceInfo(value, nodeIpv6Addr);
    }
    else
    {
        icError("No session found for device %s", deviceUuid.c_str());
    }

    return chip::NullOptional;
}

CHIP_ERROR DeviceDataCache::IsBatteryPowered(bool &isBattery) const
{
    uint32_t featureMap = 0;
    CHIP_ERROR error = GetPowerSourceFeatureMap(featureMap);
    if (error != CHIP_NO_ERROR)
    {
        return error;
    }
    isBattery = featureMap & static_cast<uint32_t>(chip::app::Clusters::PowerSource::Feature::kBattery);
    return CHIP_NO_ERROR;
}

CHIP_ERROR DeviceDataCache::IsWiredPowered(bool &isWired) const
{
    uint32_t featureMap = 0;
    CHIP_ERROR error = GetPowerSourceFeatureMap(featureMap);
    if (error != CHIP_NO_ERROR)
    {
        return error;
    }
    isWired = featureMap & static_cast<uint32_t>(chip::app::Clusters::PowerSource::Feature::kWired);
    return CHIP_NO_ERROR;
}

CHIP_ERROR DeviceDataCache::GetPowerSourceFeatureMap(uint32_t &featureMap) const
{
    using namespace chip::app::Clusters::PowerSource;

    chip::TLV::TLVReader reader;
    CHIP_ERROR error = GetAttributeData({chip::kRootEndpointId, Id, Attributes::FeatureMap::Id}, reader);
    if (error != CHIP_NO_ERROR)
    {
        icDebug(
            "Failed to read PowerSource FeatureMap attribute for device %s: %s", deviceUuid.c_str(), error.AsString());
        return error;
    }

    using TypeInfo = Attributes::FeatureMap::TypeInfo;
    TypeInfo::DecodableType value;
    error = chip::app::DataModel::Decode(reader, value);
    if (error != CHIP_NO_ERROR)
    {
        icError("Failed to decode PowerSource FeatureMap attribute for device %s: %s",
                deviceUuid.c_str(),
                error.AsString());
        return error;
    }

    featureMap = value;
    return CHIP_NO_ERROR;
}

bool DeviceDataCache::EndpointHasServerCluster(chip::EndpointId endpointId, chip::ClusterId clusterId) const
{
    if (!clusterStateCache)
    {
        return false;
    }

    bool hasCluster = false;

    // Use ForEachCluster to check if the cluster exists on the endpoint
    clusterStateCache->ForEachCluster(endpointId, [clusterId, &hasCluster](chip::ClusterId cachedClusterId) {
        if (cachedClusterId == clusterId)
        {
            hasCluster = true;
        }
        return CHIP_NO_ERROR;
    });

    return hasCluster;
}

SubscriptionIntervalSecs DeviceDataCache::CalculateFinalSubscriptionIntervalSecs()
{
    icDebug();

    SubscriptionIntervalSecs intervalSecs = {1, kSubscriptionMaxIntervalPublisherLimit};

    // The report interval should be "significantly less" than the comm fail timeout to avoid
    // requiring phase-locked clocks and realtime processing. A factor of 2 was chosen here to
    // allow for weathering transient conditions (e.g., delivery delays/retries), and provide
    // some fudge factor for activities like reducing the commFailOverrideSeconds metadata.
    // This allows a reduction by up to 40% (10% safety margin) to "just work" without complex schemes
    // like a queued-for-next-contact reconfiguration for a value that is unlikely to change
    // in practice. Extending this timeout doesn't require any special consideration.

    // TODO get this per driver? GetCommFailTimeoutSecs(deviceId.c_str()) / 2;
    uint32_t desiredCommfailCeilSecs = 3360;

    if (intervalSecs.maxIntervalCeilingSecs > desiredCommfailCeilSecs)
    {
        icInfo("Reducing max interval ceiling from %d secs to %d secs to line up with commfail timeout",
               intervalSecs.maxIntervalCeilingSecs,
               desiredCommfailCeilSecs);

        intervalSecs.maxIntervalCeilingSecs = desiredCommfailCeilSecs;
    }

    if (intervalSecs.minIntervalFloorSecs > intervalSecs.maxIntervalCeilingSecs)
    {
        uint16_t adjustedMinIntervalFloor =
            intervalSecs.maxIntervalCeilingSecs - 1 > 0 ? intervalSecs.maxIntervalCeilingSecs - 1 : 1;
        icWarn("The requested min interval floor of %d secs is greater than the max interval ceiling of %d secs; "
               "adjusting the floor to %d secs",
               intervalSecs.minIntervalFloorSecs,
               intervalSecs.maxIntervalCeilingSecs,
               adjustedMinIntervalFloor);
        intervalSecs.minIntervalFloorSecs = adjustedMinIntervalFloor;
    }

    icDebug("Will request min interval floor of %d seconds for the subscription", intervalSecs.minIntervalFloorSecs);
    icDebug("Will request max interval ceiling of %d seconds for the subscription",
            intervalSecs.maxIntervalCeilingSecs);

    intervalSecs.minIntervalFloorSecs = intervalSecs.minIntervalFloorSecs;
    intervalSecs.maxIntervalCeilingSecs = intervalSecs.maxIntervalCeilingSecs;
    return intervalSecs;
}

void DeviceDataCache::OnReportBegin()
{
    icDebug();
}

Json::Value DeviceDataCache::GetEndpointAsJson(chip::EndpointId endpointId)
{
    Json::Value endpointJson(Json::objectValue);

    if (!clusterStateCache)
    {
        return endpointJson; // Return empty object
    }

    // Iterate over all clusters for this endpoint
    clusterStateCache->ForEachCluster(endpointId, [this, endpointId, &endpointJson](chip::ClusterId clusterId) {
        Json::Value clusterJson(Json::objectValue);

        // Iterate over all attributes for this cluster
        clusterStateCache->ForEachAttribute(
            endpointId, clusterId, [this, &clusterJson](const chip::app::ConcreteAttributePath &path) {
                // Get the TLV data for this attribute
                chip::TLV::TLVReader reader;
                CHIP_ERROR err = clusterStateCache->Get(path, reader);
                if (err != CHIP_NO_ERROR)
                {
                    return CHIP_NO_ERROR; // Continue iterating
                }

                // Convert TLV to JSON using the library
                Json::Value attributeValue;
                err = chip::TlvToJson(reader, attributeValue);
                if (err == CHIP_NO_ERROR)
                {
                    clusterJson[std::to_string(path.mAttributeId)] = attributeValue;
                }

                return CHIP_NO_ERROR;
            });

        endpointJson[std::to_string(clusterId)] = clusterJson;

        return CHIP_NO_ERROR;
    });

    return endpointJson;
}

Json::Value DeviceDataCache::GetCacheAsJson()
{
    Json::Value rootJson(Json::objectValue);
    Json::Value endpointsJson(Json::objectValue);

    // Add the last report timestamp at the top level
    rootJson["lastReportCompletedTimestamp"] = Json::Value::UInt64(lastReportCompletedTimestamp);

    // Step 1: Get endpoint 0 first (required endpoint with Descriptor cluster)
    endpointsJson["0"] = GetEndpointAsJson(0);

    // Step 2: Get all other endpoints from the PartsList
    std::vector<chip::EndpointId> endpointIds = GetEndpointIds();
    for (chip::EndpointId endpointId : endpointIds)
    {
        endpointsJson[std::to_string(endpointId)] = GetEndpointAsJson(endpointId);
    }

    rootJson["endpoints"] = endpointsJson;

    return rootJson;
}

CHIP_ERROR DeviceDataCache::RegenerateAttributeReport()
{
    icDebug();

    chip::DeviceLayer::PlatformMgr().ScheduleWork(
        [](intptr_t context) {
            auto *self = reinterpret_cast<DeviceDataCache *>(context);

            if (!self->clusterStateCache)
            {
                icError("ClusterStateCache is null");
                return;
            }

            if (!self->callback)
            {
                icWarn("No registered callback to notify for attribute report regeneration");
                return;
            }

            // Get all endpoint IDs
            std::vector<chip::EndpointId> endpointIds = self->GetEndpointIds();

            self->callback->OnReportBegin();

            // Iterate through all endpoints, clusters, and attributes
            for (chip::EndpointId endpointId : endpointIds)
            {
                self->clusterStateCache->ForEachCluster(endpointId, [self, endpointId](chip::ClusterId clusterId) {

                    // Iterate through all attributes in this cluster
                    self->clusterStateCache->ForEachAttribute(
                        endpointId, clusterId, [self](const chip::app::ConcreteAttributePath &path) {
                            chip::app::ConcreteDataAttributePath dataPath(
                                path.mEndpointId, path.mClusterId, path.mAttributeId);
                            chip::TLV::TLVReader reader;

                            CHIP_ERROR err = self->clusterStateCache->Get(dataPath, reader);
                            if (err != CHIP_NO_ERROR)
                            {
                                icWarn("Failed to get cached attribute data for endpoint 0x%02x, cluster 0x%04" PRIx32 ", attribute 0x%04" PRIx32 ": %" CHIP_ERROR_FORMAT,
                                       static_cast<unsigned int>(dataPath.mEndpointId),
                                       static_cast<uint32_t>(dataPath.mClusterId),
                                       static_cast<uint32_t>(dataPath.mAttributeId),
                                       err.Format());

                                // Skip notifying the callback for this attribute; continue with others.
                                return CHIP_NO_ERROR;
                            }

                            chip::app::StatusIB aStatus(chip::Protocols::InteractionModel::Status::Success);
                            self->OnAttributeData(dataPath, &reader, aStatus);

                            return CHIP_NO_ERROR;
                        });

                    return CHIP_NO_ERROR;
                });
            }

            self->callback->OnReportEnd();

            icDebug("Done regenerating attribute report");
        },
        reinterpret_cast<intptr_t>(this));

    return CHIP_NO_ERROR;
}

void DeviceDataCache::OnReportEnd()
{
    icDebug();

    Json::Value cacheJson = GetCacheAsJson();
    std::string jsonString = chip::JsonToString(cacheJson);
    icDebug("Cluster state cache JSON:\n%s", jsonString.c_str());

    // Set the startup promise if it exists (initial startup case)
    std::lock_guard<std::mutex> lock(startupPromiseMutex);
    if (startupPromise)
    {
        startupPromise->set_value(true);
        startupPromise.reset();
    }
}

void DeviceDataCache::OnSubscriptionEstablished(chip::SubscriptionId aSubscriptionId)
{
    icDebug("Subscription established with ID: %" PRIu32, aSubscriptionId);
}

void DeviceDataCache::OnError(CHIP_ERROR aError)
{
    icError("ReadClient encountered an error: %s", aError.AsString());
}

void DeviceDataCache::OnDeallocatePaths(chip::app::ReadPrepareParams &&aReadPrepareParams)
{
    icDebug();

    if (aReadPrepareParams.mpAttributePathParamsList != nullptr)
    {
        delete aReadPrepareParams.mpAttributePathParamsList;
    }

    if (aReadPrepareParams.mpEventPathParamsList != nullptr)
    {
        delete aReadPrepareParams.mpEventPathParamsList;
    }

    if (aReadPrepareParams.mpDataVersionFilterList != nullptr)
    {
        delete[] aReadPrepareParams.mpDataVersionFilterList;
    }
}

void DeviceDataCache::OnDone(chip::app::ReadClient *apReadClient)
{
    icDebug();
    lastReportCompletedTimestamp = static_cast<uint64_t>(time(nullptr));
    icDebug("Report completed at timestamp: %" PRIu64, lastReportCompletedTimestamp);
}

void DeviceDataCache::OnDeviceConnectedCallback(void *context,
                                                chip::Messaging::ExchangeManager &exchangeMgr,
                                                const chip::SessionHandle &sessionHandle)
{
    auto *self = static_cast<DeviceDataCache *>(context);
    self->OnDeviceConnected(exchangeMgr, sessionHandle);
}

void DeviceDataCache::OnDeviceConnectionFailureCallback(void *context,
                                                        const chip::ScopedNodeId &peerId,
                                                        CHIP_ERROR error)
{
    auto *self = static_cast<DeviceDataCache *>(context);
    self->OnDeviceConnectionFailure(peerId, error);
}

void DeviceDataCache::OnDeviceConnected(chip::Messaging::ExchangeManager &exchangeMgr,
                                        const chip::SessionHandle &sessionHandle)
{
    icDebug();

    clusterStateCache = std::make_unique<chip::app::ClusterStateCache>(*this);
    readClient =
        std::make_unique<chip::app::ReadClient>(chip::app::InteractionModelEngine::GetInstance(),
                                                chip::app::InteractionModelEngine::GetInstance()->GetExchangeManager(),
                                                clusterStateCache->GetBufferedCallback(),
                                                chip::app::ReadClient::InteractionType::Subscribe);

    auto subscriptionIntervalSecs = CalculateFinalSubscriptionIntervalSecs();
    if (subscriptionIntervalSecs.minIntervalFloorSecs == 0 || subscriptionIntervalSecs.maxIntervalCeilingSecs == 0)
    {
        icError("Failed to calculate valid subscription interval params");
        std::lock_guard<std::mutex> lock(startupPromiseMutex);
        if (startupPromise)
        {
            startupPromise->set_value(false);
            startupPromise.reset();
        }
        return;
    }

    chip::app::ReadPrepareParams params;

    // We want reporting on everything, so we just wildcard everything.
    auto eventPathParams = new chip::app::EventPathParams;
    eventPathParams->SetWildcardEndpointId();
    eventPathParams->SetWildcardClusterId();
    eventPathParams->SetWildcardEventId();
    auto attributePathParams = new chip::app::AttributePathParams;
    attributePathParams->SetWildcardEndpointId();
    attributePathParams->SetWildcardClusterId();
    attributePathParams->SetWildcardAttributeId();

    params.mMinIntervalFloorSeconds = subscriptionIntervalSecs.minIntervalFloorSecs;
    params.mMaxIntervalCeilingSeconds = subscriptionIntervalSecs.maxIntervalCeilingSecs;
    params.mpEventPathParamsList = eventPathParams;
    params.mEventPathParamsListSize = 1;
    params.mpAttributePathParamsList = attributePathParams;
    params.mAttributePathParamsListSize = 1;
    params.mKeepSubscriptions = true;
    params.mSessionHolder = chip::SessionHolder(sessionHandle);

    CHIP_ERROR err = readClient->SendAutoResubscribeRequest(std::move(params));

    if (CHIP_NO_ERROR != err)
    {
        icError("Failed to send subscribe request (%s)", err.AsString());
        std::lock_guard<std::mutex> lock(startupPromiseMutex);
        if (startupPromise)
        {
            startupPromise->set_value(false);
            startupPromise.reset();
        }
    }
}

void DeviceDataCache::OnDeviceConnectionFailure(const chip::ScopedNodeId &peerId, CHIP_ERROR error)
{
    icDebug();
    icError("Device connection failed: %s", error.AsString());
    std::lock_guard<std::mutex> lock(startupPromiseMutex);
    if (startupPromise)
    {
        startupPromise->set_value(false);
        startupPromise.reset();
    }
}
