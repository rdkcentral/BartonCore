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
 * Created by Thomas Lea on 11/15/21.
 */

#define LOG_TAG      "MatterBaseDD"
#define logFmt(fmt)  "(%s): " fmt, __func__
#define G_LOG_DOMAIN LOG_TAG
#include "app-common/zap-generated/ids/Clusters.h"
#include "subsystems/matter/MatterCommon.h"
#include <cassert>
#include <chrono>
#include <cinttypes>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <forward_list>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unistd.h>
#include <unordered_set>
#include <utility>
#include <vector>

#include "MatterDeviceDriver.h"
#include "app-common/zap-generated/cluster-objects.h"
#include "app/AttributePathParams.h"
#include "app/ClusterStateCache.h"
#include "app/EventPathParams.h"
#include "app/OperationalSessionSetup.h"
#include "app/ReadPrepareParams.h"
#include "clusters/BasicInformation.hpp"
#include "clusters/GeneralDiagnostics.h"
#include "clusters/OTARequestor.h"
#include "controller/CHIPCluster.h"
#include "lib/core/CHIPError.h"
#include "lib/core/DataModelTypes.h"
#include "matter/subscriptions/SubscribeInteraction.h"
#include "messaging/ExchangeMgr.h"
#include "platform/CHIPDeviceLayer.h"
#include "platform/PlatformManager.h"
#include "subsystems/matter/DiscoveredDeviceDetailsStore.h"
#include "subsystems/matter/Matter.h"
#include "subsystems/matter/matterSubsystem.h"
#include "transport/Session.h"

extern "C" {
#include "device-driver/device-driver.h"
#include "device/deviceModelHelper.h"
#include "device/icDevice.h"
#include "device/icDeviceMetadata.h"
#include "device/icDeviceResource.h"
#include "device/icInitialResourceValues.h"
#include "deviceCommunicationWatchdog.h"
#include "deviceDescriptor.h"
#include "deviceService.h"
#include "deviceService/resourceModes.h"
#include "glib.h"
#include "icTypes/icStringHashMap.h"
#include "icUtil/stringUtils.h"
#include "resourceTypes.h"
#include <commonDeviceDefs.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <icTypes/icLinkedList.h>
#include <versionUtils.h>
}

using namespace barton;

// the DeviceDriver interface callbacks
static void startup(void *ctx);
static void shutdown(void *ctx);
static void deleteDriver(void *ctx);
static void deviceRemoved(void *ctx, icDevice *device);
static bool configureDevice(void *ctx, icDevice *device, DeviceDescriptor *descriptor);
static void synchronizeDevice(void *ctx, icDevice *device);
static bool processDeviceDescriptor(void *ctx, icDevice *device, DeviceDescriptor *dd);
static bool fetchInitialResourceValues(void *ctx, icDevice *device, icInitialResourceValues *initialResourceValues);
static bool registerResources(void *ctx, icDevice *device, icInitialResourceValues *initialResourceValues);
static bool devicePersisted(void *ctx, icDevice *device);
static bool readResource(void *ctx, icDeviceResource *resource, char **value);
static bool writeResource(void *ctx, icDeviceResource *resource, const char *previousValue, const char *newValue);
static bool executeResource(void *ctx, icDeviceResource *resource, const char *arg, char **response);
static void communicationFailed(void *ctx, icDevice *device);
static void communicationRestored(void *ctx, icDevice *device);
static bool getDeviceClassVersion(void *ctx, const char *deviceClass, uint8_t *version);

MatterDeviceDriver::MatterDeviceDriver(const char *driverName, const char *deviceClass, uint8_t dcVersion) :
    driver(), deviceClassVersion(dcVersion + deviceModelVersion), otaRequestorEventHandler(*this)
{
    driver.driverName = strdup(driverName);
    driver.callbackContext = this;
    driver.neverReject = true; // Matter devices dont participate in the DDL logic
    driver.subsystemName = strdup(MATTER_SUBSYSTEM_NAME);
    driver.supportedDeviceClasses = linkedListCreate();
    linkedListAppend(driver.supportedDeviceClasses, strdup(deviceClass));
    driver.startup = startup;
    driver.shutdown = shutdown;
    driver.destroy = deleteDriver;
    driver.deviceRemoved = deviceRemoved;
    driver.configureDevice = configureDevice;
    driver.fetchInitialResourceValues = fetchInitialResourceValues;
    driver.registerResources = registerResources;
    driver.devicePersisted = devicePersisted;
    driver.readResource = readResource;
    driver.writeResource = writeResource;
    driver.executeResource = executeResource;
    driver.synchronizeDevice = synchronizeDevice;
    driver.processDeviceDescriptor = processDeviceDescriptor;
    driver.communicationFailed = communicationFailed;
    driver.communicationRestored = communicationRestored;
    driver.getDeviceClassVersion = getDeviceClassVersion;

    driver.commFailTimeoutSecsChanged = [](DeviceDriver *driver, const icDevice *device, uint32_t timeoutSecs) {
        static_cast<MatterDeviceDriver *>(driver->callbackContext)->commFailTimeoutSecs = timeoutSecs;
    };
}

MatterDeviceDriver::~MatterDeviceDriver()
{
    free(driver.driverName);
    free(driver.subsystemName);
    linkedListDestroy(driver.supportedDeviceClasses, nullptr);
}

icStringHashMap *MatterDeviceDriver::GetEndpointToProfileMap(const DiscoveredDeviceDetails *details)
{
    return nullptr;
}

const char *MatterDeviceDriver::GetDeviceClass() const
{
    return (const char *) linkedListGetElementAt(driver.supportedDeviceClasses, 0);
}

void MatterDeviceDriver::Startup()
{
    icDebug();
}

void MatterDeviceDriver::Shutdown()
{
    icDebug();

    // Don't rely on ~MatterDeviceDriver() to cleanly deal with objects that
    // interact with the Matter stack: interaction with the Matter stack
    // via subscriptionMap (which ultimately contains chip::app::ReadClient) must
    // occur only when the stack lock is held, or chipDie will abort the program
    // to avoid undefined behavior. Drivers are shut down before subsystems.

    subscriptionMap.clear();
    clusterServers.clear();
}

void MatterDeviceDriver::ReadCurrentFabricIndex(std::forward_list<std::promise<bool>> &promises,
                                                const std::string &deviceId,
                                                chip::FabricIndex &fabricIndex,
                                                chip::Messaging::ExchangeManager &exchangeMgr,
                                                const chip::SessionHandle &sessionHandle)
{
    icDebug();

    promises.emplace_front();
    auto &readPromise = promises.front();
    auto readRequestContext =
        new AttributeReadRequestContext<chip::FabricIndex>(this, &readPromise, &fabricIndex, deviceId);
    AssociateStoredContext(readRequestContext->readComplete);

    chip::Controller::ClusterBase cluster(exchangeMgr, sessionHandle, 0);

    CHIP_ERROR error =
        cluster.ReadAttribute<chip::app::Clusters::OperationalCredentials::Attributes::CurrentFabricIndex::TypeInfo>(
            readRequestContext,
            [](void *context,
               chip::FabricIndex value) { /* read response success callback */
                                          bool indexValid = (value != kUndefinedFabricIndex);
                                          auto readRequestContext =
                                              static_cast<AttributeReadRequestContext<chip::FabricIndex> *>(context);

                                          indexValid ? icDebug("Received fabric index of 0x%x from device %s",
                                                               value,
                                                               readRequestContext->deviceId.c_str())
                                                     : icError("Received invalid fabric index from device %s",
                                                               readRequestContext->deviceId.c_str());

                                          *(readRequestContext->attributeData) = value;
                                          readRequestContext->driver->OnDeviceWorkCompleted(
                                              readRequestContext->readComplete, indexValid);
                                          delete readRequestContext;
            },
            [](void *context,
               CHIP_ERROR error) { /* read response failure callback */
                                   auto readRequestContext =
                                       static_cast<AttributeReadRequestContext<chip::FabricIndex> *>(context);

                                   icError("Failed to read FabricIndex attribute from device %s with error: %s",
                                           readRequestContext->deviceId.c_str(),
                                           error.AsString());

                                   readRequestContext->driver->AbandonDeviceWork(*(readRequestContext->readComplete));
                                   delete readRequestContext;
            });

    if (error != CHIP_NO_ERROR)
    {
        icError("CurrentFabricIndex ReadAttribute command failed with error: %s", error.AsString());
        AbandonDeviceWork(readPromise);
        delete readRequestContext;
    }
}

bool MatterDeviceDriver::SendRemoveFabric(std::forward_list<std::promise<bool>> &promises,
                                          const std::string &deviceId,
                                          const chip::FabricIndex fabricIndex,
                                          chip::Messaging::ExchangeManager &exchangeMgr,
                                          const chip::SessionHandle &sessionHandle)
{
    icDebug();

    // remove our fabric from the device. Fire and forget.
    chip::app::Clusters::OperationalCredentials::Commands::RemoveFabric::Type data;
    data.fabricIndex = fabricIndex;

    chip::app::CommandSender commandSender(nullptr, &exchangeMgr, false);
    chip::app::CommandPathParams commandPath(0, /* endpoint 0 */
                                             0, /* group not used */
                                             chip::app::Clusters::OperationalCredentials::Id,
                                             chip::app::Clusters::OperationalCredentials::Commands::RemoveFabric::Id,
                                             (chip::app::CommandPathFlags::kEndpointIdValid));
    commandSender.AddRequestData(commandPath, data);

    bool sentRemoveFabricRequest = commandSender.SendCommandRequest(sessionHandle) == CHIP_NO_ERROR;
    if (!sentRemoveFabricRequest)
    {
        icError("Failed to send RemoveFabric command request");
        FailOperation(promises);
    }
    return sentRemoveFabricRequest;
}

bool MatterDeviceDriver::DeviceRemoved(icDevice *device)
{
    icDebug();

    // First, the CurrentFabricIndex attribute, which shall contain the
    // accessing fabric index, must be read from the remote device.
    chip::FabricIndex fabricIndex = kUndefinedFabricIndex;

    bool fabricIndexReadSuccess = ConnectAndExecute(
        device->uuid,
        [this, &fabricIndex](std::forward_list<std::promise<bool>> &promises,
                             const std::string &deviceId,
                             chip::Messaging::ExchangeManager &exchangeMgr,
                             const chip::SessionHandle &sessionHandle) {
            ReadCurrentFabricIndex(promises, deviceId, fabricIndex, exchangeMgr, sessionHandle);
        },
        MATTER_ASYNC_DEVICE_TIMEOUT_SECS);

    bool sentRemoveFabricRequest = false;

    if (fabricIndexReadSuccess)
    {
        sentRemoveFabricRequest = ConnectAndExecute(
            device->uuid,
            [this, fabricIndex](std::forward_list<std::promise<bool>> &promises,
                                const std::string &deviceId,
                                chip::Messaging::ExchangeManager &exchangeMgr,
                                const chip::SessionHandle &sessionHandle) {
                SendRemoveFabric(promises, deviceId, fabricIndex, exchangeMgr, sessionHandle);
            },
            MATTER_ASYNC_DEVICE_TIMEOUT_SECS);
    }
    else
    {
        icError("Failed to retrieve the device's current fabric index or received an invalid index; RemoveFabric "
                "command will not be sent");
    }

    RunOnMatterSync([this, device] {
        chip::NodeId nodeId = Subsystem::Matter::UuidToNodeId(device->uuid);

        for (auto entry = clusterServers.begin(); entry != clusterServers.end();)
        {
            if (entry->second->GetNodeId() == nodeId)
            {
                entry = clusterServers.erase(entry);
            }
            else
            {
                entry++;
            }
        }

        subscriptionMap.erase(device->uuid);
    });

    return sentRemoveFabricRequest;
}

void MatterDeviceDriver::ConfigureDevice(std::forward_list<std::promise<bool>> &promises,
                                         const std::string &deviceId,
                                         DeviceDescriptor *deviceDescriptor,
                                         chip::Messaging::ExchangeManager &exchangeMgr,
                                         const chip::SessionHandle &sessionHandle)
{
    icDebug();

    using namespace chip::app::Clusters;

    if (!ConfigureOTARequestorCluster(promises, deviceId, deviceDescriptor, exchangeMgr, sessionHandle))
    {
        icError("Failed to configure OTA Requestor for device %s", deviceId.c_str());
        FailOperation(promises);
        return;
    }

    DoConfigureDevice(promises, deviceId, deviceDescriptor, exchangeMgr, sessionHandle);

    ConfigureSubscription(promises, deviceId, deviceDescriptor, exchangeMgr, sessionHandle);
}

void MatterDeviceDriver::ConfigureSubscription(std::forward_list<std::promise<bool>> &promises,
                                               const std::string &deviceId,
                                               DeviceDescriptor *deviceDescriptor,
                                               chip::Messaging::ExchangeManager &exchangeMgr,
                                               const chip::SessionHandle &sessionHandle)
{
    icDebug();

    auto subscriptionIntervalSecs = CalculateFinalSubscriptionIntervalSecs(deviceId);
    if (subscriptionIntervalSecs.minIntervalFloorSecs == 0 || subscriptionIntervalSecs.maxIntervalCeilingSecs == 0)
    {
        icError("Failed to calculate valid subscription interval params");
        FailOperation(promises);
        return;
    }

    chip::app::ReadPrepareParams params(sessionHandle);

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

    auto subEventHandler = std::make_unique<DriverSubscribeEventHandler>(*this, deviceId);

    promises.emplace_front();
    auto &configureSubscriptionPromise = promises.front();
    AssociateStoredContext(&configureSubscriptionPromise);

    auto subscribeInteraction = std::make_unique<SubscribeInteraction>(
        std::move(subEventHandler), deviceId, exchangeMgr, configureSubscriptionPromise);

    CHIP_ERROR err = subscribeInteraction->Send(std::move(params));

    if (CHIP_NO_ERROR == err)
    {
        subscriptionMap[deviceId] = std::move(subscribeInteraction);
    }
    else
    {
        icError("Failed to send subscribe request (%s)", err.AsString());
        subscribeInteraction->AbandonSubscription();
    }
}

SubscriptionIntervalSecs MatterDeviceDriver::CalculateFinalSubscriptionIntervalSecs(const std::string &deviceId)
{
    icDebug();

    SubscriptionIntervalSecs intervalSecs(0, 0);
    uint16_t minIntervalFloorSeconds = 1;
    uint16_t maxIntervalCeilingSeconds = UINT16_MAX;
    auto commonClusters = GetCommonClustersToSubscribeTo(deviceId);
    auto clusters = GetClustersToSubscribeTo(deviceId);
    clusters.insert(clusters.end(), commonClusters.begin(), commonClusters.end());

    for (auto clusterServer : clusters)
    {
        auto intervals = clusterServer->GetDesiredSubscriptionIntervalSecs();
        if (intervals.minIntervalFloorSecs == 0 || intervals.maxIntervalCeilingSecs == 0)
        {
            icError("Failed to retrieve valid subscription interval parameters for our clusters!");
            return intervalSecs;
        }
        minIntervalFloorSeconds = minIntervalFloorSeconds > intervals.minIntervalFloorSecs
                                      ? minIntervalFloorSeconds
                                      : intervals.minIntervalFloorSecs;
        maxIntervalCeilingSeconds = maxIntervalCeilingSeconds < intervals.maxIntervalCeilingSecs
                                        ? maxIntervalCeilingSeconds
                                        : intervals.maxIntervalCeilingSecs;
    }

    // The report interval should be "significantly less" than the comm fail timeout to avoid
    // requiring phase-locked clocks and realtime processing. A factor of 2 was chosen here to
    // allow for weathering transient conditions (e.g., delivery delays/retries), and provide
    // some fudge factor for activities like reducing the commFailOverrideSeconds metadata.
    // This allows a reduction by up to 40% (10% safety margin) to "just work" without complex schemes
    // like a queued-for-next-contact reconfiguration for a value that is unlikely to change
    // in practice. Extending this timeout doesn't require any special consideration.
    uint32_t desiredCommfailCeilSecs = GetCommFailTimeoutSecs(deviceId.c_str()) / 2;

    if (maxIntervalCeilingSeconds > desiredCommfailCeilSecs)
    {
        icInfo("Reducing max interval ceiling from %d secs to %d secs to line up with commfail timeout",
               maxIntervalCeilingSeconds,
               desiredCommfailCeilSecs);

        maxIntervalCeilingSeconds = desiredCommfailCeilSecs;
    }

    if (minIntervalFloorSeconds > maxIntervalCeilingSeconds)
    {
        uint16_t adjustedMinIntervalFloor = maxIntervalCeilingSeconds - 1 > 0 ? maxIntervalCeilingSeconds - 1 : 1;
        icWarn("The requested min interval floor of %d secs is greater than the max interval ceiling of %d secs; "
               "adjusting the floor to %d secs",
               minIntervalFloorSeconds,
               maxIntervalCeilingSeconds,
               adjustedMinIntervalFloor);
        minIntervalFloorSeconds = adjustedMinIntervalFloor;
    }

    icDebug("Will request min interval floor of %d seconds for the subscription on %s",
            minIntervalFloorSeconds,
            deviceId.c_str());
    icDebug("Will request max interval ceiling of %d seconds for the subscription on %s",
            maxIntervalCeilingSeconds,
            deviceId.c_str());

    intervalSecs.minIntervalFloorSecs = minIntervalFloorSeconds;
    intervalSecs.maxIntervalCeilingSecs = maxIntervalCeilingSeconds;
    return intervalSecs;
}

std::vector<MatterCluster *> MatterDeviceDriver::GetCommonClustersToSubscribeTo(const std::string &deviceId)
{
    icDebug();

    auto basicInfoServer = (BasicInformation *) GetAnyServerById(deviceId, chip::app::Clusters::BasicInformation::Id);
    std::vector<MatterCluster *> clusters = {basicInfoServer};

    auto generalDiagnosticsServer =
        (GeneralDiagnostics *) GetAnyServerById(deviceId, chip::app::Clusters::GeneralDiagnostics::Id);
    clusters.push_back(generalDiagnosticsServer);

    auto requestorServer = (OTARequestor *) GetAnyServerById(deviceId, OtaSoftwareUpdateRequestor::Id);

    if (requestorServer != nullptr)
    {
        clusters.push_back(requestorServer);
    }
    else
    {
        icInfo("OTA cluster not supported on device %s!", deviceId.c_str());
    }

    return clusters;
}

std::vector<MatterCluster *> MatterDeviceDriver::GetClustersToSubscribeTo(const std::string &deviceId)
{
    icDebug("Unimplemented");
    return {};
}

void MatterDeviceDriver::FetchInitialResourceValues(std::forward_list<std::promise<bool>> &promises,
                                                    const std::string &deviceId,
                                                    icInitialResourceValues *initialResourceValues,
                                                    chip::Messaging::ExchangeManager &exchangeMgr,
                                                    const chip::SessionHandle &sessionHandle)
{
    icDebug("Unimplemented");
    // default implementation just fails
    FailOperation(promises);
}

void MatterDeviceDriver::PopulateInitialResourceValues(std::forward_list<std::promise<bool>> &promises,
                                                       const std::string &deviceId,
                                                       icInitialResourceValues *initialResourceValues,
                                                       chip::Messaging::ExchangeManager &exchangeMgr,
                                                       const chip::SessionHandle &sessionHandle)
{
    FetchInitialResourceValues(promises, deviceId, initialResourceValues, exchangeMgr, sessionHandle);
}

bool MatterDeviceDriver::CreateResources(icDevice *device, icInitialResourceValues *initialResourceValues)
{

    auto *basicInfoServer = static_cast<barton::BasicInformation *>(
        GetAnyServerById(device->uuid, chip::app::Clusters::BasicInformation::Id));

    if (basicInfoServer == nullptr)
    {
        icError("No basic information server on device %s!", device->uuid);
        return false;
    }

    /*
     * The device may have reported its info upon subscription.
     * In the event that it hasn't yet, (e.g., sleepy), the value will
     * be filled in when the next report arrives, so always create the
     * resource. The absence of a value "now" is not an error.
     */

    const char *versionStr = nullptr;
    auto tmp = basicInfoServer->GetFirmwareVersionString();

    if (!tmp.empty())
    {
        versionStr = tmp.c_str();
    }

    createDeviceResource(device,
                         COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION_STRING,
                         versionStr,
                         RESOURCE_TYPE_STRING,
                         RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                         CACHING_POLICY_ALWAYS);

    auto *generalDiagnosticsServer = static_cast<barton::GeneralDiagnostics *>(
        GetAnyServerById(device->uuid, chip::app::Clusters::GeneralDiagnostics::Id));

    if (generalDiagnosticsServer == nullptr)
    {
        icError("No general diagnostics server on device %s!", device->uuid);
        return false;
    }

    const char *macAddressStr = nullptr;
    tmp = generalDiagnosticsServer->GetMacAddress();

    if (!tmp.empty())
    {
        macAddressStr = tmp.c_str();
    }

    createDeviceResource(device,
                         COMMON_DEVICE_RESOURCE_MAC_ADDRESS,
                         macAddressStr,
                         RESOURCE_TYPE_MAC_ADDRESS,
                         RESOURCE_MODE_READABLE,
                         CACHING_POLICY_ALWAYS);

    const char *networkTypeStr = nullptr;
    tmp = generalDiagnosticsServer->GetNetworkType();

    if (!tmp.empty())
    {
        networkTypeStr = tmp.c_str();
    }

    createDeviceResource(device,
                         COMMON_DEVICE_RESOURCE_NETWORK_TYPE,
                         networkTypeStr,
                         RESOURCE_TYPE_NETWORK_TYPE,
                         RESOURCE_MODE_READABLE,
                         CACHING_POLICY_ALWAYS);

    return RegisterResources(device, initialResourceValues);
}

void MatterDeviceDriver::SynchronizeDevice(std::forward_list<std::promise<bool>> &promises,
                                           const std::string &deviceId,
                                           chip::Messaging::ExchangeManager &exchangeMgr,
                                           const chip::SessionHandle &sessionHandle)
{
    icDebug("Unimplemented");
    FailOperation(promises);
}

void MatterDeviceDriver::ReadResource(std::forward_list<std::promise<bool>> &promises,
                                      const std::string &deviceId,
                                      icDeviceResource *resource,
                                      char **value,
                                      chip::Messaging::ExchangeManager &exchangeMgr,
                                      const chip::SessionHandle &sessionHandle)
{
    icDebug("Unimplemented");
    FailOperation(promises);
}

bool MatterDeviceDriver::WriteResource(std::forward_list<std::promise<bool>> &promises,
                                       const std::string &deviceId,
                                       icDeviceResource *resource,
                                       const char *previousValue,
                                       const char *newValue,
                                       chip::Messaging::ExchangeManager &exchangeMgr,
                                       const chip::SessionHandle &sessionHandle)
{
    icDebug("Unimplemented");
    FailOperation(promises);
    return false;
}

void MatterDeviceDriver::ExecuteResource(std::forward_list<std::promise<bool>> &promises,
                                         const std::string &deviceId,
                                         icDeviceResource *resource,
                                         const char *arg,
                                         char **response,
                                         chip::Messaging::ExchangeManager &exchangeMgr,
                                         const chip::SessionHandle &sessionHandle)
{
    icDebug("Unimplemented");
    FailOperation(promises);
}

bool MatterDeviceDriver::RegisterResources(icDevice *device, icInitialResourceValues *initialResourceValues)
{
    icDebug();
    return false;
}

bool MatterDeviceDriver::DevicePersisted(icDevice *device)
{
    bool result = false;

    icDebug();

    auto details = DiscoveredDeviceDetailsStore::Instance().Get(device->uuid);

    if (details == nullptr)
    {
        icError("No matter details found to persist!");
        return false;
    }

    DiscoveredDeviceDetailsStore::Instance().Put(device->uuid, details);
    result = true;

    return result;
}

// DeviceDriver interface implementation.  Redirects to class methods
static void startup(void *ctx)
{
    static_cast<MatterDeviceDriver *>(ctx)->Startup();
}

static void shutdown(void *ctx)
{
    auto driver = static_cast<MatterDeviceDriver *>(ctx);

    driver->RunOnMatterSync([driver] { driver->Shutdown(); });
}

static void deleteDriver(void *ctx)
{
    auto driver = static_cast<MatterDeviceDriver *>(ctx);

    delete driver;
}

static void deviceRemoved(void *ctx, icDevice *device)
{
    static_cast<MatterDeviceDriver *>(ctx)->DeviceRemoved(device);
}

namespace
{
    constexpr uint16_t SLOW_ASYNC_MILLIS = 200;

    /**
     * @brief This is an internal type for OnMatterDeviceConnectionSuccess and ConnectAndExecute
     * @param exhcangeMgr
     * @param sessionHandle
     */
    using connect_work_wrapper =
        std::function<void(chip::Messaging::ExchangeManager &exchangeMgr, const chip::SessionHandle &sessionHandle)>;

    void OnMatterDeviceConnectionFailure(void *context, const chip::ScopedNodeId &peerId, CHIP_ERROR error)
    {
        auto connectPromise = static_cast<std::promise<bool> *>(context);

        connectPromise->set_value(false);
    }

    void OnMatterDeviceConnectionSuccess(void *context,
                                         chip::Messaging::ExchangeManager &exchangeMgr,
                                         const chip::SessionHandle &sessionHandle)
    {
        auto workWrapper = static_cast<connect_work_wrapper *>(context);

        workWrapper->operator()(exchangeMgr, sessionHandle);
    }
} // namespace

void MatterDeviceDriver::AbortDeviceConnectionAttempt(chip::Callback::Callback<OnDeviceConnected> &successCb,
                                                      chip::Callback::Callback<OnDeviceConnectionFailure> &failCb)
{
    RunOnMatterSync([&successCb, &failCb] {
        successCb.Cancel();
        failCb.Cancel();
    });
}

bool MatterDeviceDriver::ConnectAndExecute(const std::string &deviceId, connect_work_cb &&work, uint16_t timeoutSeconds)
{
    icDebug();

    if (!Matter::GetInstance().IsRunning())
    {
        icError("Matter subsystem is down!");
        return false;
    }

    std::forward_list<std::promise<bool>> promises;
    std::promise<bool> connectPromise;

    connect_work_wrapper workWrapper =
        [&deviceId, &promises, &work, &connectPromise](chip::Messaging::ExchangeManager &exchangeMgr,
                                                       const chip::SessionHandle &sessionHandle) {
            auto start = std::chrono::steady_clock::now();

            work(promises, deviceId, exchangeMgr, sessionHandle);

            auto elapsedMillis =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

            if (elapsedMillis.count() > SLOW_ASYNC_MILLIS)
            {
                icWarn("Slow work took %" PRIdLEAST64 " ms", elapsedMillis.count());
            }

            connectPromise.set_value(true);
        };

    chip::Callback::Callback<OnDeviceConnectionFailure> failCb(OnMatterDeviceConnectionFailure,
                                                               static_cast<void *>(&connectPromise));

    chip::Callback::Callback<OnDeviceConnected> successCb(OnMatterDeviceConnectionSuccess,
                                                          static_cast<void *>(&workWrapper));

    // SDK event size limitations prevent directly capturing too many objects.
    // connectPromise is directly pointed at in failCb.mContext, so it is indirectly
    // captured via failCb.

    auto err = chip::DeviceLayer::SystemLayer().ScheduleLambda([&deviceId, &successCb, &failCb]() {
        auto nodeId = Subsystem::Matter::UuidToNodeId(deviceId);

        CHIP_ERROR getConnectedErr =
            Matter::GetInstance().GetCommissioner().GetConnectedDevice(nodeId, &successCb, &failCb);
        if (getConnectedErr != CHIP_NO_ERROR)
        {
            icError("Failed to start device connection: %s", getConnectedErr.AsString());
            static_cast<std::promise<bool> *>(failCb.mContext)->set_value(false);
        }
    });

    if (err != CHIP_NO_ERROR)
    {
        icError("Failed to schedule connect task: %s", err.AsString());
        return false;
    }

    std::chrono::time_point<std::chrono::steady_clock> abortAtTime =
        std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);
    bool abort = false;

    auto connectFuture = connectPromise.get_future();

    if (connectFuture.wait_until(abortAtTime) == std::future_status::timeout)
    {
        icError("Connect to %s timed out after %" PRIu16 " s!", deviceId.c_str(), timeoutSeconds);
        AbortDeviceConnectionAttempt(successCb, failCb);
        return false;
    }

    if (!connectFuture.get())
    {
        icError("Failed to connect to %s", deviceId.c_str());
        return false;
    }

    auto start = std::chrono::steady_clock::now();
    size_t processedPromises = 0;

    for (auto &&promise : promises)
    {
        std::future<bool> workDone = promise.get_future();
        std::future_status workStatus = workDone.wait_until(abortAtTime);
        processedPromises++;

        if (workStatus == std::future_status::ready)
        {
            if (!workDone.get())
            {
                abort = true;
                break;
            }
        }
        else
        {
            icError("Timed out waiting to execute async operation after %" PRIu16 " s", timeoutSeconds);

            abort = true;
            break;
        }
    }

    auto elapsedMillis =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    icDebug("Processed %zd async task(s) in %" PRIdLEAST64 " ms", processedPromises, elapsedMillis.count());

    // Drivers may have stored promises as Matter SDK context pointers
    // with AssociateStoredContext. Prior to expiring 'promises,' invalidate
    // any pointers that the SDK stored so OnDeviceWorkCompleted doesn't try
    // to access invalid pointers.
    {
        std::lock_guard<std::mutex> lock(storedContextsMtx);

        for (auto &&promise : promises)
        {
            auto erased = storedContexts.erase(static_cast<void *>(&promise));

            // If this isn't an abort, someone forgot to call OnDeviceWorkCompleted
            // after promising to complete work that was delegated to the SDK.
            if (!abort && erased != 0)
            {
                icError("A stored promise was not disassociated!");
                assert(false);
            }
        }
    }

    if (abort)
    {
        AbortDeviceConnectionAttempt(successCb, failCb);
    }

    return !abort;
}

static bool configureDevice(void *self, icDevice *device, DeviceDescriptor *descriptor)
{
    icDebug();

    MatterDeviceDriver *myDriver = static_cast<MatterDeviceDriver *>(self);

    return myDriver->ConnectAndExecute(
        device->uuid,
        [myDriver, descriptor](std::forward_list<std::promise<bool>> &promises,
                               const std::string &deviceId,
                               chip::Messaging::ExchangeManager &exchangeMgr,
                               const chip::SessionHandle &sessionHandle) {
            myDriver->ConfigureDevice(promises, deviceId, descriptor, exchangeMgr, sessionHandle);
        },
        MATTER_ASYNC_DEVICE_TIMEOUT_SECS);
}

static bool fetchInitialResourceValues(void *self, icDevice *device, icInitialResourceValues *initialResourceValues)
{
    icDebug();

    MatterDeviceDriver *myDriver = static_cast<MatterDeviceDriver *>(self);

    return myDriver->ConnectAndExecute(
        device->uuid,
        [myDriver, initialResourceValues](std::forward_list<std::promise<bool>> &promises,
                                          const std::string &deviceId,
                                          chip::Messaging::ExchangeManager &exchangeMgr,
                                          const chip::SessionHandle &sessionHandle) {
            myDriver->PopulateInitialResourceValues(
                promises, deviceId, initialResourceValues, exchangeMgr, sessionHandle);
        },
        MATTER_ASYNC_DEVICE_TIMEOUT_SECS);
}

static bool readResource(void *self, icDeviceResource *resource, char **value)
{
    icDebug();

    MatterDeviceDriver *myDriver = static_cast<MatterDeviceDriver *>(self);

    return myDriver->ConnectAndExecute(
        resource->deviceUuid,
        [myDriver, resource, value](std::forward_list<std::promise<bool>> &promises,
                                    const std::string &deviceId,
                                    chip::Messaging::ExchangeManager &exchangeMgr,
                                    const chip::SessionHandle &sessionHandle) {
            myDriver->ReadResource(promises, deviceId, resource, value, exchangeMgr, sessionHandle);
        },
        MATTER_ASYNC_DEVICE_TIMEOUT_SECS);
}

static bool writeResource(void *self, icDeviceResource *resource, const char *previousValue, const char *newValue)
{
    icDebug();

    bool result = true;
    MatterDeviceDriver *myDriver = static_cast<MatterDeviceDriver *>(self);

    bool changed = stringCompare(previousValue, newValue, false) != 0;
    bool shouldUpdateResource = true;

    if (stringCompare(resource->id, COMMON_ENDPOINT_RESOURCE_LABEL, true) != 0)
    {
        result = myDriver->ConnectAndExecute(
            resource->deviceUuid,
            [myDriver, resource, previousValue, newValue, &shouldUpdateResource](
                std::forward_list<std::promise<bool>> &promises,
                const std::string &deviceId,
                chip::Messaging::ExchangeManager &exchangeMgr,
                const chip::SessionHandle &sessionHandle) {
                shouldUpdateResource = myDriver->WriteResource(
                    promises, deviceId, resource, previousValue, newValue, exchangeMgr, sessionHandle);
            },
            MATTER_ASYNC_DEVICE_TIMEOUT_SECS);
    }

    if (result && changed && shouldUpdateResource)
    {
        updateResource(resource->deviceUuid, resource->endpointId, resource->id, newValue, NULL);
    }

    return result;
}

static bool executeResource(void *self, icDeviceResource *resource, const char *arg, char **response)
{
    icDebug();

    MatterDeviceDriver *myDriver = static_cast<MatterDeviceDriver *>(self);

    return myDriver->ConnectAndExecute(
        resource->deviceUuid,
        [myDriver, resource, arg, response](std::forward_list<std::promise<bool>> &promises,
                                            const std::string &deviceId,
                                            chip::Messaging::ExchangeManager &exchangeMgr,
                                            const chip::SessionHandle &sessionHandle) {
            myDriver->ExecuteResource(promises, deviceId, resource, arg, response, exchangeMgr, sessionHandle);
        },
        MATTER_ASYNC_DEVICE_TIMEOUT_SECS);
}

static void synchronizeDevice(void *self, icDevice *device)
{
    icDebug();

    MatterDeviceDriver *myDriver = static_cast<MatterDeviceDriver *>(self);
    if (!myDriver->ConnectAndExecute(
            device->uuid,
            [myDriver](std::forward_list<std::promise<bool>> &promises,
                       const std::string &deviceId,
                       chip::Messaging::ExchangeManager &exchangeMgr,
                       const chip::SessionHandle &sessionHandle) {
                myDriver->SynchronizeDevice(promises, deviceId, exchangeMgr, sessionHandle);
            },

            MATTER_ASYNC_SYNCHRONIZE_DEVICE_TIMEOUT_SECS))
    {
        icError("Failed to sync device %s", device->uuid);
    }
}


void MatterDeviceDriver::ProcessDeviceDescriptorMetadata(const icDevice *device, const icStringHashMap *metadata)
{
    // If this device is not yet in our database, then it is a newly pairing device which already processed the metadata
    // in createDevice
    // FIXME: DeviceService should just do this for all DD processing, not just when pairing a new one.
    if (device->uri == NULL)
    {
        icLogDebug(LOG_TAG, "%s: skipping metadata processing for newly paired device", __FUNCTION__);
        return;
    }

    scoped_icStringHashMapIterator *it = stringHashMapIteratorCreate(const_cast<icStringHashMap *>(metadata));

    while (stringHashMapIteratorHasNext(it))
    {
        char *key;
        char *value;

        if (stringHashMapIteratorGetNext(it, &key, &value))
        {
            icLogInfo(LOG_TAG, "%s: setting metadata (%s=%s) on device %s", __FUNCTION__, key, value, device->uuid);

            setMetadata(device->uuid, NULL, key, value);
        }
    }
}

bool MatterDeviceDriver::ProcessDeviceDescriptor(const icDevice *device, const DeviceDescriptor *dd)
{
    if (dd->category != CATEGORY_TYPE_MATTER)
    {
        return true;
    }

    if (dd->latestFirmware == NULL || dd->latestFirmware->version == NULL)
    {
        icDebug("No latest firmware for dd uuid: %s; ignoring", dd->uuid);
        return true;
    }

    icDebug("%s", device->uuid);

    scoped_icDeviceResource *upgradeStatus =
        deviceServiceGetResourceById(device->uuid, NULL, COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS);

    /*
     * Only reset status to pending/upToDate if uninitialized or to reset from a failed/completed state.
     * This avoids clobbering any other state, which may generate noise or overwrite important information.
     */

    if (upgradeStatus == NULL || upgradeStatus->value == NULL ||
        g_strcmp0(upgradeStatus->value, FIRMWARE_UPDATE_STATUS_FAILED) == 0 ||
        g_strcmp0(upgradeStatus->value, FIRMWARE_UPDATE_STATUS_COMPLETED) == 0)
    {
        scoped_generic char *currentVersion = deviceServiceGetDeviceFirmwareVersion(device->uuid);

        if (currentVersion == NULL)
        {
            return true;
        }

        int versionDiff = compareVersionStrings(currentVersion, dd->latestFirmware->version);

        if (versionDiff < 0)
        {
            icWarn("Device %s is newer than its descriptor's latestFirmare %s",
                   currentVersion,
                   dd->latestFirmware->version);
        }

        const char *status = versionDiff > 0 ? FIRMWARE_UPDATE_STATUS_PENDING : FIRMWARE_UPDATE_STATUS_UP_TO_DATE;

        updateResource(device->uuid, NULL, COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS, status, NULL);
    }

    // TODO: announce ota provider if a firmware upgrade is available. For now, the device will
    // discover any updates on its own when it polls the provider. A poll is typically scheduled just
    // after commissioning.

    if (dd->metadata != nullptr)
    {
        ProcessDeviceDescriptorMetadata(device, dd->metadata);
    }

    return true;
}

static bool processDeviceDescriptor(void *ctx, icDevice *device, DeviceDescriptor *dd)
{
    return static_cast<MatterDeviceDriver *>(ctx)->ProcessDeviceDescriptor(device, dd);
}

static bool registerResources(void *ctx, icDevice *device, icInitialResourceValues *initialResourceValues)
{
    auto matterDetails = DiscoveredDeviceDetailsStore::Instance().Get(device->uuid);

    if (!matterDetails)
    {
        return false;
    }

    const char *serialNumber = nullptr;

    // Matter 1.0 11.1.6.3: serialNumber must be displayable and human readable.
    // Displaying an empty string is unreasonable and is a defined 'not present'
    // value in the details model. The deviceService resource model defines null
    // as unknown, rather than the empty string.

    if (matterDetails->serialNumber.HasValue() && !matterDetails->serialNumber.Value().empty())
    {
        serialNumber = matterDetails->serialNumber.Value().c_str();
    }

    createDeviceResource(device,
                         COMMON_DEVICE_RESOURCE_SERIAL_NUMBER,
                         serialNumber,
                         RESOURCE_TYPE_SERIAL_NUMBER,
                         RESOURCE_MODE_READABLE,
                         CACHING_POLICY_ALWAYS);

    auto *driver = static_cast<MatterDeviceDriver *>(ctx);

    /*
     * RunOnMatterSync can only handle a few captures before
     * the message gets too big. A reference to a bag of
     * objects is safe to use here, as the lambda will be
     * retired via std::future before this method returns.
     */

    struct
    {
        MatterDeviceDriver *driver;
        icDevice *device;
        icInitialResourceValues *initialResourceValues;
    } wrapper = {driver, device, initialResourceValues};

    bool registerDone = false;

    driver->RunOnMatterSync([&wrapper, &registerDone] {
        registerDone = wrapper.driver->CreateResources(wrapper.device, wrapper.initialResourceValues);
    });

    return registerDone;
}

static bool devicePersisted(void *ctx, icDevice *device)
{
    return static_cast<MatterDeviceDriver *>(ctx)->DevicePersisted(device);
}

MatterCluster *MatterDeviceDriver::GetAnyServerById(std::string const &deviceUuid, chip::ClusterId clusterId)
{
    auto serverEndpoints = FindServerEndpoints(deviceUuid, clusterId);
    MatterCluster *server = nullptr;

    if (!serverEndpoints.empty())
    {
        server = GetServerById(deviceUuid, serverEndpoints.front(), clusterId);
    }
    else
    {
        icError("Failed to locate cluster %#" PRIx32 " endpoint ID for device %s", clusterId, deviceUuid.c_str());
    }

    return server;
}

std::vector<chip::EndpointId> MatterDeviceDriver::FindServerEndpoints(std::string const &deviceUuid,
                                                                      chip::ClusterId clusterId)
{
    return FindServerEndpoints(deviceUuid, kInvalidEndpointId, clusterId);
}

std::vector<chip::EndpointId> MatterDeviceDriver::FindServerEndpoints(std::string const &deviceUuid,
                                                                      chip::EndpointId endpointId,
                                                                      chip::ClusterId clusterId)
{
    auto detailsRef = DiscoveredDeviceDetailsStore::Instance().Get(deviceUuid);
    auto details = detailsRef.get();
    std::vector<chip::EndpointId> endpoints;

    if (details == nullptr)
    {
        icError("No discovery details for %s", deviceUuid.c_str());
        return endpoints;
    }

    for (auto &entry : details->endpointDescriptorData)
    {
        for (auto &&cluster : *entry.second->serverList)
        {
            if (cluster == clusterId && (endpointId == kInvalidEndpointId || entry.first == endpointId))
            {
                endpoints.push_back(entry.first);

                if (endpointId != kInvalidEndpointId)
                {
                    break;
                }
            }
        }
    }

    return endpoints;
}

bool MatterDeviceDriver::IsServerPresent(std::string const &deviceUuid,
                                         chip::EndpointId endpointId,
                                         chip::ClusterId clusterId)
{
    if (endpointId == kInvalidEndpointId)
    {
        return false;
    }

    return !FindServerEndpoints(deviceUuid, endpointId, clusterId).empty();
}

void MatterDeviceDriver::RunOnMatterSync(std::function<void(void)> work)
{
    // FIXME: Drivers can be interacted with even when their subsystem is
    // down or even nonexistent. This is a kludge and should be removed
    // after preventing illegal driver interactions. Singletons are evil anyway.

    if (!Matter::GetInstance().IsRunning())
    {
        icError("Matter subsystem is down!");
        return;
    }

    std::promise<void> donePromise;
    auto done = donePromise.get_future();

    struct
    {
        std::function<void(void)> &work;
        std::promise<void> &donePromise;
    } wrapper = {work, donePromise};

    /*
     * This will fail to schedule if the stack is not running, so abandon the
     * task when it can't be scheduled
     */

    auto err = chip::DeviceLayer::SystemLayer().ScheduleLambda([&wrapper]() {
        wrapper.work();
        wrapper.donePromise.set_value();
    });

    if (err != CHIP_NO_ERROR)
    {
        icWarn("Failed to schedule work: %s", err.AsString());
        donePromise.set_value();
    }

    done.wait();
}

MatterCluster *
MatterDeviceDriver::GetServerById(std::string const &deviceUuid, chip::EndpointId endpointId, chip::ClusterId clusterId)
{
    MatterCluster *server = nullptr;

    if (!IsServerPresent(deviceUuid, endpointId, clusterId))
    {
        icError("Cluster %#" PRIx32 " not supported on %s/%#" PRIx32, clusterId, deviceUuid.c_str(), endpointId);

        return nullptr;
    }

    auto locator = std::make_tuple(deviceUuid, endpointId, clusterId);
    auto serverIt = clusterServers.find(locator);

    if (serverIt == clusterServers.end())
    {
        std::unique_ptr<MatterCluster> serverRef;

        switch (clusterId)
        {
            case OtaSoftwareUpdateRequestor::Id:
                serverRef = std::make_unique<OTARequestor>(
                    (OTARequestor::EventHandler *) &otaRequestorEventHandler, deviceUuid, endpointId);
                break;

            case chip::app::Clusters::BasicInformation::Id:
                serverRef = std::make_unique<BasicInformation>(&basicInfoEventHandler, deviceUuid, endpointId);
                break;

            case chip::app::Clusters::GeneralDiagnostics::Id:
                serverRef =
                    std::make_unique<GeneralDiagnostics>(&generalDiagnosticsEventHandler, deviceUuid, endpointId);
                break;

            default:
                serverRef = MakeCluster(deviceUuid, endpointId, clusterId);
        }

        server = serverRef.get();

        if (server == nullptr)
        {
            icTrace("Cluster %#" PRIx32 " not implemented!", clusterId);
            return server;
        }

        clusterServers[locator] = std::move(serverRef);
    }
    else
    {
        server = serverIt->second.get();
    }

    return server;
}

bool MatterDeviceDriver::ConfigureOTARequestorCluster(std::forward_list<std::promise<bool>> &promises,
                                                      const std::string &deviceId,
                                                      DeviceDescriptor *deviceDescriptor,
                                                      const chip::Messaging::ExchangeManager &exchangeMgr,
                                                      const chip::SessionHandle &sessionHandle)
{
    icDebug();

    using namespace chip::app::Clusters;

    auto requestorServer = (OTARequestor *) GetAnyServerById(deviceId, OtaSoftwareUpdateRequestor::Id);

    if (requestorServer == nullptr)
    {
        icInfo("OTA cluster not supported on device %s, not attempting to configure", deviceId.c_str());
        return true;
    }

    OtaSoftwareUpdateRequestor::Structs::ProviderLocation::Type providerLocation;
    providerLocation.providerNodeID = Matter::GetInstance().GetNodeId();
    providerLocation.endpoint = Matter::GetInstance().GetLocalEndpointId();
    providerLocation.fabricIndex = Matter::GetInstance().GetFabricIndex();

    std::vector<OtaSoftwareUpdateRequestor::Structs::ProviderLocation::Type> otaProviderList = {providerLocation};

    promises.emplace_front();
    auto &setProviderPromise = promises.front();
    AssociateStoredContext(&setProviderPromise);

    if (!requestorServer->SetDefaultOTAProviders(&setProviderPromise, otaProviderList, exchangeMgr, sessionHandle))
    {
        AbandonDeviceWork(setProviderPromise);
        return false;
    }

    return true;
}

void MatterDeviceDriver::DeviceFirmwareUpdateFailed(OTARequestor &source,
                                                    uint32_t softwareVersion,
                                                    SubscribeInteraction &subscriber)
{
    std::string deviceUuid = source.GetDeviceId();

    icWarn("Device %s failed to upgrade to version %#" PRIx32, deviceUuid.c_str(), softwareVersion);

    subscriber.SetLivenessCheckMillis(0);

    updateResource(
        deviceUuid.c_str(), NULL, COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS, FIRMWARE_UPDATE_STATUS_FAILED, NULL);
}

void MatterDeviceDriver::OtaRequestorEventHandler::OnStateTransition(
    OTARequestor &source,
    OtaSoftwareUpdateRequestor::OTAUpdateStateEnum oldState,
    OtaSoftwareUpdateRequestor::OTAUpdateStateEnum newState,
    OtaSoftwareUpdateRequestor::OTAChangeReasonEnum reason,
    Nullable<uint32_t> targetSoftwareVersion,
    SubscribeInteraction &subscriber)
{
    icDebug();

    using OTAUpdateStateEnum = chip::app::Clusters::OtaSoftwareUpdateRequestor::OTAUpdateStateEnum;
    using OTAChangeReasonEnum = chip::app::Clusters::OtaSoftwareUpdateRequestor::OTAChangeReasonEnum;

    if (oldState == OTAUpdateStateEnum::kDownloading && newState == OTAUpdateStateEnum::kIdle &&
        ((reason == OTAChangeReasonEnum::kFailure) || (reason == OTAChangeReasonEnum::kTimeOut)))
    {
        driver.DeviceFirmwareUpdateFailed(
            source, targetSoftwareVersion.IsNull() ? 0 : targetSoftwareVersion.Value(), subscriber);
        icWarn("Update faiure reason: %s", reason == OTAChangeReasonEnum::kFailure ? "Failed" : "Timed out");
    }
    else if (newState == OTAUpdateStateEnum::kDownloading)
    {
        subscriber.SetLivenessCheckMillis(driver.fastLivenessCheckMillis);

        std::string deviceUuid = source.GetDeviceId();

        updateResource(deviceUuid.c_str(),
                       NULL,
                       COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS,
                       FIRMWARE_UPDATE_STATUS_STARTED,
                       NULL);
    }
    else if (newState == OTAUpdateStateEnum::kApplying)
    {
        // TODO: some kind of failsafe callback scheduled here should raise an alert if
        //  the device fails to back to report a version applied event after this point
        icInfo("Applying firmware update to %s", source.GetDeviceId().c_str());
    }

    // kIdle->kQuerying may be used to record a resource/metadata with the latest query time
}

void MatterDeviceDriver::OtaRequestorEventHandler::OnDownloadError(OTARequestor &source,
                                                                   uint32_t softwareVersion,
                                                                   uint64_t bytesDownloaded,
                                                                   Nullable<uint8_t> percentDownloaded,
                                                                   Nullable<int64_t> platformCode,
                                                                   SubscribeInteraction &subscriber)
{
    icWarn(
        "Failed to download software version %#" PRIx32 " for device %#" PRIx64, softwareVersion, source.GetNodeId());

    if (!percentDownloaded.IsNull())
    {
        icWarn("Failed download was %" PRIu8 "%% complete", percentDownloaded.Value());
    }

    if (!platformCode.IsNull())
    {
        icWarn("Failed download indicated platform code %#" PRIx64, platformCode.Value());
    }

    driver.DeviceFirmwareUpdateFailed(source, softwareVersion, subscriber);
}

void MatterDeviceDriver::OtaRequestorEventHandler::OnVersionApplied(OTARequestor &source,
                                                                    uint32_t softwareVersion,
                                                                    uint16_t productId,
                                                                    SubscribeInteraction &subscriber)
{
    std::string deviceUuid = source.GetDeviceId();
    std::string versionStr = Matter::VersionToString(softwareVersion);

    icInfo("Software version %s installed successfully on device %s", versionStr.c_str(), deviceUuid.c_str());

    subscriber.SetLivenessCheckMillis(0);

    updateResource(deviceUuid.c_str(), NULL, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION, versionStr.c_str(), NULL);

    updateResource(deviceUuid.c_str(),
                   NULL,
                   COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS,
                   FIRMWARE_UPDATE_STATUS_COMPLETED,
                   NULL);

    if (deviceServiceGetPostUpgradeAction(deviceUuid.c_str()) == POST_UPGRADE_ACTION_RECONFIGURE)
    {
        deviceServiceReconfigureDevice(deviceUuid.c_str(), MATTER_RECONFIGURATION_DELAY_SECS, NULL, false);
    }
}

static void communicationFailed(void *ctx, icDevice *device)
{
    updateResource(device->uuid, NULL, COMMON_DEVICE_RESOURCE_COMM_FAIL, "true", NULL);
}

static void communicationRestored(void *ctx, icDevice *device)
{
    updateResource(device->uuid, NULL, COMMON_DEVICE_RESOURCE_COMM_FAIL, "false", NULL);

    synchronizeDevice(ctx, device);
}

static bool getDeviceClassVersion(void *ctx, const char *deviceClass, uint8_t *version)
{
    MatterDeviceDriver *self = (MatterDeviceDriver *) ctx;

    *version = self->GetDeviceClassVersion();
    return true;
}
