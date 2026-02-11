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
#include "subsystems/matter/MatterCommon.h"
#include <cassert>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <forward_list>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <unistd.h>
#include <unordered_set>
#include <utility>
#include <vector>

#include "MatterDeviceDriver.h"
#include "app/OperationalSessionSetup.h"
#include "clusters/BasicInformation.hpp"
#include "clusters/GeneralDiagnostics.h"
#include "clusters/OTARequestor.h"
#include "clusters/PowerSource.h"
#include "controller/CHIPCluster.h"
#include "lib/core/CHIPError.h"
#include "lib/core/DataModelTypes.h"
#include "messaging/ExchangeMgr.h"
#include "platform/CHIPDeviceLayer.h"
#include "subsystems/matter/Matter.h"
#include "subsystems/matter/matterSubsystem.h"
#include "transport/Session.h"

extern "C" {
#include "device-driver/device-driver.h"
#include "device/deviceModelHelper.h"
#include "device/icDevice.h"
#include "device/icDeviceResource.h"
#include "device/icInitialResourceValues.h"
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
static bool registerResources(void *ctx, icDevice *device, icInitialResourceValues *initialResourceValues);
static bool devicePersisted(void *ctx, icDevice *device);
static bool readResource(void *ctx, icDeviceResource *resource, char **value);
static bool writeResource(void *ctx, icDeviceResource *resource, const char *previousValue, const char *newValue);
static bool executeResource(void *ctx, icDeviceResource *resource, const char *arg, char **response);
static void communicationFailed(void *ctx, icDevice *device);
static void communicationRestored(void *ctx, icDevice *device);
static bool getDeviceClassVersion(void *ctx, const char *deviceClass, uint8_t *version);

MatterDeviceDriver::MatterDeviceDriver(const char *driverName, const char *deviceClass, uint8_t dcVersion) :
    driver(), deviceClassVersion(dcVersion + deviceModelVersion), otaRequestorEventHandler(*this),
    generalDiagnosticsEventHandler(*this), wifiDiagnosticsClusterEventHandler(*this)
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

bool MatterDeviceDriver::ClaimDevice(const DeviceDataCache *deviceDataCache)
{
    std::vector<uint16_t> deviceTypes = GetSupportedDeviceTypes();

    if (!deviceDataCache)
    {
        icError("deviceDataCache is null");
        return false;
    }

    // Get all endpoints from the device (excluding root endpoint 0)
    std::vector<chip::EndpointId> endpointIds = deviceDataCache->GetEndpointIds();

    for (chip::EndpointId endpointId : endpointIds)
    {
        // Skip root endpoint 0
        if (endpointId == 0)
        {
            continue;
        }

        std::vector<uint16_t> deviceTypes;
        if (deviceDataCache->GetDeviceTypes(endpointId, deviceTypes) != CHIP_NO_ERROR)
        {
            continue;
        }

        auto supportedDeviceTypes = GetSupportedDeviceTypes();

        // Check if any device type on this endpoint matches our supported types
        for (uint16_t deviceTypeId : deviceTypes)
        {
            if (std::find(supportedDeviceTypes.begin(), supportedDeviceTypes.end(), deviceTypeId) !=
                supportedDeviceTypes.end())
            {
                icDebug("Device claimed: endpoint %d has matching device type 0x%04x", endpointId, deviceTypeId);
                return true;
            }
        }
    }

    icDebug("Device not claimed: no matching device types found");
    return false;
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
    // via deviceDataCaches (which ultimately contains chip::app::ReadClient) must
    // occur only when the stack lock is held, or chipDie will abort the program
    // to avoid undefined behavior. Drivers are shut down before subsystems.

    devices.clear();
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

        {
            std::lock_guard<std::mutex> lock(devicesMutex);
            devices.erase(device->uuid);
        }
    });

    return sentRemoveFabricRequest;
}

bool MatterDeviceDriver::ConfigureDevice(icDevice *device, DeviceDescriptor *descriptor)
{
    icDebug();

    if (!AddDeviceIfRequired(device->uuid))
    {
        icError("Failed to configure device %s: could not add device", device->uuid);
        return false;
    }

    return ConnectAndExecute(
        device->uuid,
        [this, descriptor](std::forward_list<std::promise<bool>> &promises,
                           const std::string &deviceId,
                           chip::Messaging::ExchangeManager &exchangeMgr,
                           const chip::SessionHandle &sessionHandle) {
            icDebug();

            using namespace chip::app::Clusters;

#ifdef BARTON_CONFIG_MATTER_ENABLE_OTA_PROVIDER
            if (!ConfigureOTARequestorCluster(promises, deviceId, descriptor, exchangeMgr, sessionHandle))
            {
                icError("Failed to configure OTA Requestor for device %s", deviceId.c_str());
                FailOperation(promises);
                return;
            }
#endif

            DoConfigureDevice(promises, deviceId, descriptor, exchangeMgr, sessionHandle);
        },
        MATTER_ASYNC_DEVICE_TIMEOUT_SECS);
}

void MatterDeviceDriver::SynchronizeDevice(icDevice *device)
{
    icDebug();

    if (!AddDeviceIfRequired(device->uuid))
    {
        icError("Failed to synchronize device %s: could not add device", device->uuid);
        return;
    }

    if (!ConnectAndExecute(
            device->uuid,
            [this](std::forward_list<std::promise<bool>> &promises,
                   const std::string &deviceId,
                   chip::Messaging::ExchangeManager &exchangeMgr,
                   const chip::SessionHandle &sessionHandle) {
                icDebug();

                using namespace chip::app::Clusters;

#ifdef BARTON_CONFIG_MATTER_ENABLE_OTA_PROVIDER
                if (!ConfigureOTARequestorCluster(promises, deviceId, nullptr, exchangeMgr, sessionHandle))
                {
                    icError("Failed to configure OTA Requestor for device %s", deviceId.c_str());
                    FailOperation(promises);
                    return;
                }
#endif

                // reprocessing the attributes in the cache will trigger the callbacks from registered clusters which
                //  can update resources
                auto deviceCache = GetDeviceDataCache(deviceId);
                if (deviceCache != nullptr)
                {
                    deviceCache->RegenerateAttributeReport();
                }

                DoSynchronizeDevice(promises, deviceId, exchangeMgr, sessionHandle);
            },
            MATTER_ASYNC_SYNCHRONIZE_DEVICE_TIMEOUT_SECS))
    {
        icError("Failed to sync device %s", device->uuid);
    }
}

void MatterDeviceDriver::DoSynchronizeDevice(std::forward_list<std::promise<bool>> &promises,
                                             const std::string &deviceId,
                                             chip::Messaging::ExchangeManager &exchangeMgr,
                                             const chip::SessionHandle &sessionHandle)
{
    icDebug("Unimplemented");
}

void MatterDeviceDriver::SetTampered(const std::string &deviceId, bool tampered)
{
    icDebug("Unimplemented");
}

void MatterDeviceDriver::ReadResource(std::forward_list<std::promise<bool>> &promises,
                                      const std::string &deviceId,
                                      icDeviceResource *resource,
                                      char **value,
                                      chip::Messaging::ExchangeManager &exchangeMgr,
                                      const chip::SessionHandle &sessionHandle)
{
    icDebug("%s", resource->id);

    if (g_strcmp0(resource->id, COMMON_DEVICE_RESOURCE_FERSSI) == 0 ||
        g_strcmp0(resource->id, COMMON_DEVICE_RESOURCE_LINK_SCORE) == 0 ||
        g_strcmp0(resource->id, COMMON_DEVICE_RESOURCE_LINK_QUALITY) == 0)
    {
        auto *wifiDiagServer = static_cast<WifiNetworkDiagnostics *>(
            GetAnyServerById(deviceId, chip::app::Clusters::WiFiNetworkDiagnostics::Id));

        if (wifiDiagServer == nullptr)
        {
            icError("WiFiNetworkDiagnostics cluster not present on device %s!", deviceId.c_str());
            FailOperation(promises);
            return;
        }

        promises.emplace_front();
        auto &readPromise = promises.front();
        auto readContext = new ClusterReadContext {};
        readContext->driverContext = &readPromise;
        readContext->value = value;
        readContext->resourceId = resource->id;

        // Paired with WifiNetworkDiagnosticsEventHandler::RssiReadComplete()
        AssociateStoredContext(readContext->driverContext);

        if (!wifiDiagServer->GetRssi(readContext, exchangeMgr, sessionHandle))
        {
            AbandonDeviceWork(readPromise);
            delete readContext;
        }
    }
    else
    {
        // If the resource read was not handled above, then let the subclass try to handle it
        DoReadResource(promises, deviceId, resource, value, exchangeMgr, sessionHandle);
    }
}

void MatterDeviceDriver::DoReadResource(std::forward_list<std::promise<bool>> &promises,
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

bool MatterDeviceDriver::RegisterResources(icDevice *device)
{
    icDebug();

    auto deviceCache = GetDeviceDataCache(device->uuid);

    if (!deviceCache)
    {
        icError("No device cache for %s", device->uuid);
        return false;
    }

    // these are required and will fail the operation if not found/populated
    std::string versionStr;
    if (deviceCache->GetSoftwareVersionString(versionStr) != CHIP_NO_ERROR || versionStr.empty())
    {
        icError("No SoftwareVersionString found for device %s!", device->uuid);
        return false;
    }

    std::string macAddressStr;
    if (deviceCache->GetMacAddress(macAddressStr) != CHIP_NO_ERROR || macAddressStr.empty())
    {
        icError("No MAC address found for device %s!", device->uuid);
        return false;
    }

    createDeviceResource(device,
                         COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION_STRING,
                         versionStr.c_str(),
                         RESOURCE_TYPE_STRING,
                         RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                         CACHING_POLICY_ALWAYS);

    createDeviceResource(device,
                         COMMON_DEVICE_RESOURCE_MAC_ADDRESS,
                         macAddressStr.c_str(),
                         RESOURCE_TYPE_MAC_ADDRESS,
                         RESOURCE_MODE_READABLE,
                         CACHING_POLICY_ALWAYS);

    const char *networkTypeStr = nullptr;
    std::string networkType;
    if (deviceCache->GetNetworkType(networkType) == CHIP_NO_ERROR && !networkType.empty())
    {
        networkTypeStr = networkType.c_str();
    }

    createDeviceResource(device,
                         COMMON_DEVICE_RESOURCE_NETWORK_TYPE,
                         networkTypeStr,
                         RESOURCE_TYPE_NETWORK_TYPE,
                         RESOURCE_MODE_READABLE,
                         CACHING_POLICY_ALWAYS);

    g_autofree char *serialNumber = nullptr;

    // Matter 1.0 11.1.6.3: serialNumber must be displayable and human readable.
    // Displaying an empty string is unreasonable and is a defined 'not present'
    // value in the details model. The deviceService resource model defines null
    // as unknown, rather than the empty string.

    std::string serialNumberStr;
    if (deviceCache->GetSerialNumber(serialNumberStr) == CHIP_NO_ERROR && !serialNumberStr.empty())
    {
        serialNumber = strdup(serialNumberStr.c_str());
    }

    createDeviceResource(device,
                         COMMON_DEVICE_RESOURCE_SERIAL_NUMBER,
                         serialNumber,
                         RESOURCE_TYPE_SERIAL_NUMBER,
                         RESOURCE_MODE_READABLE,
                         CACHING_POLICY_ALWAYS);

    if (g_strcmp0(networkTypeStr, NETWORK_TYPE_WIFI) == 0)
    {
        auto *feRssiResource = static_cast<icDeviceResource *>(calloc(1, sizeof(icDeviceResource)));
        feRssiResource->id = strdup(COMMON_DEVICE_RESOURCE_FERSSI);
        feRssiResource->endpointId = nullptr;
        feRssiResource->deviceUuid = strdup(device->uuid);
        feRssiResource->type = strdup(RESOURCE_TYPE_RSSI);
        feRssiResource->mode = RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS;
        feRssiResource->cachingPolicy = CACHING_POLICY_ALWAYS;
        linkedListAppend(device->resources, feRssiResource);

        auto *linkScoreResource = static_cast<icDeviceResource *>(calloc(1, sizeof(icDeviceResource)));
        linkScoreResource->id = strdup(COMMON_DEVICE_RESOURCE_LINK_SCORE);
        linkScoreResource->endpointId = nullptr;
        linkScoreResource->deviceUuid = strdup(device->uuid);
        linkScoreResource->type = strdup(RESOURCE_TYPE_PERCENTAGE);
        linkScoreResource->mode = RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS;
        linkScoreResource->cachingPolicy = CACHING_POLICY_ALWAYS;
        linkedListAppend(device->resources, linkScoreResource);

        auto *linkQualityResource = static_cast<icDeviceResource *>(calloc(1, sizeof(icDeviceResource)));
        linkQualityResource->id = strdup(COMMON_DEVICE_RESOURCE_LINK_QUALITY);
        linkQualityResource->endpointId = nullptr;
        linkQualityResource->deviceUuid = strdup(device->uuid);
        linkQualityResource->type = strdup(RESOURCE_TYPE_SIGNAL_STRENGTH);
        linkQualityResource->mode = RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS;
        linkQualityResource->cachingPolicy = CACHING_POLICY_ALWAYS;
        linkedListAppend(device->resources, linkQualityResource);
    }

    bool isBatteryPowered = false;
    CHIP_ERROR error = deviceCache->IsBatteryPowered(isBatteryPowered);
    if (error != CHIP_NO_ERROR)
    {
        icWarn("Failed to determine power source for device %s: %s", device->uuid, error.AsString());
    }
    else if (isBatteryPowered)
    {
        auto *batteryLowResource = static_cast<icDeviceResource *>(calloc(1, sizeof(icDeviceResource)));
        batteryLowResource->id = strdup(COMMON_DEVICE_RESOURCE_BATTERY_LOW);
        batteryLowResource->endpointId = nullptr;
        batteryLowResource->deviceUuid = strdup(device->uuid);
        batteryLowResource->type = strdup(RESOURCE_TYPE_BOOLEAN);
        batteryLowResource->mode = RESOURCE_MODE_READABLE | RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_DYNAMIC;
        batteryLowResource->cachingPolicy = CACHING_POLICY_ALWAYS;
        linkedListAppend(device->resources, batteryLowResource);

        // TODO: This resource is optional, per Matter 1.4 section 11.7.7, so maybe we should conditionally create it?
        auto *batteryPercentageResource = static_cast<icDeviceResource *>(calloc(1, sizeof(icDeviceResource)));
        batteryPercentageResource->id = strdup(COMMON_DEVICE_RESOURCE_BATTERY_PERCENTAGE_REMAINING);
        batteryPercentageResource->endpointId = nullptr;
        batteryPercentageResource->deviceUuid = strdup(device->uuid);
        batteryPercentageResource->type = strdup(RESOURCE_TYPE_PERCENTAGE);
        batteryPercentageResource->mode = RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS;
        batteryPercentageResource->cachingPolicy = CACHING_POLICY_ALWAYS;
        linkedListAppend(device->resources, batteryPercentageResource);
    }

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
    } wrapper = {this, device};

    bool registerDone = false;

    RunOnMatterSync([&wrapper, &registerDone] { registerDone = wrapper.driver->DoRegisterResources(wrapper.device); });

    return registerDone;
}

bool MatterDeviceDriver::DoRegisterResources(icDevice *device)
{
    icDebug();
    return false;
}

bool MatterDeviceDriver::DevicePersisted(icDevice *device)
{
    icDebug();
    return true;
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
            Matter::GetInstance().GetCommissioner()->GetConnectedDevice(nodeId, &successCb, &failCb);
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

bool MatterDeviceDriver::AddDeviceIfRequired(const std::string &deviceUuid)
{
    //if a device isnt available yet (there would have been if it had just been paired),
    // create and start the device data cache and add the device.
    if (GetDevice(deviceUuid) == nullptr)
    {
        auto newCache = std::make_shared<DeviceDataCache>(deviceUuid, Matter::GetInstance().GetCommissioner());
        std::future<bool> success = newCache->Start();

        if (success.wait_for(std::chrono::seconds(MATTER_ASYNC_SYNCHRONIZE_DEVICE_TIMEOUT_SECS)) != std::future_status::ready ||
            !success.get())
        {
            icError("Failed to start DeviceDataCache for device %s", deviceUuid.c_str());
            return false;
        }

        AddDevice(std::make_unique<MatterDevice>(deviceUuid, std::move(newCache)));
    }

    return true;
}

static bool configureDevice(void *self, icDevice *device, DeviceDescriptor *descriptor)
{
    return static_cast<MatterDeviceDriver *>(self)->ConfigureDevice(device, descriptor);
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
    static_cast<MatterDeviceDriver *>(self)->SynchronizeDevice(device);
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
    return static_cast<MatterDeviceDriver *>(ctx)->RegisterResources(device);
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
    std::vector<chip::EndpointId> endpoints;
    auto deviceCache = GetDeviceDataCache(deviceUuid);
    if (deviceCache == nullptr)
    {
        icError("No device cache for %s", deviceUuid.c_str());
        return endpoints;
    }

    for (auto &entry : deviceCache->GetEndpointIds())
    {
        if (deviceCache->EndpointHasServerCluster(entry, clusterId))
        {
            endpoints.push_back(entry);
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

    auto deviceCache = GetDeviceDataCache(deviceUuid);
    for (auto &entry : deviceCache->GetEndpointIds())
    {
        if (entry == endpointId && deviceCache->EndpointHasServerCluster(entry, clusterId))
        {
            return true;
        }
    }

    return false;
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
    auto deviceDataCache = GetDeviceDataCache(deviceUuid);

    if (serverIt == clusterServers.end())
    {
        std::unique_ptr<MatterCluster> serverRef;

        switch (clusterId)
        {
            case OtaSoftwareUpdateRequestor::Id:
                serverRef = std::make_unique<OTARequestor>(
                    (OTARequestor::EventHandler *) &otaRequestorEventHandler, deviceUuid, endpointId, deviceDataCache);
                break;

            case chip::app::Clusters::BasicInformation::Id:
                serverRef = std::make_unique<BasicInformation>(&basicInfoEventHandler, deviceUuid, endpointId, deviceDataCache);
                break;

            case chip::app::Clusters::GeneralDiagnostics::Id:
                serverRef =
                    std::make_unique<GeneralDiagnostics>(&generalDiagnosticsEventHandler, deviceUuid, endpointId, deviceDataCache);
                break;

            case chip::app::Clusters::PowerSource::Id:
                serverRef = std::make_unique<PowerSource>(&powerSourceEventHandler, deviceUuid, endpointId, deviceDataCache);
                break;

            case chip::app::Clusters::WiFiNetworkDiagnostics::Id:
                serverRef = std::make_unique<WifiNetworkDiagnostics>(
                    &wifiDiagnosticsClusterEventHandler, deviceUuid, endpointId, deviceDataCache);
                break;

            default:
                serverRef = MakeCluster(deviceUuid, endpointId, clusterId, deviceDataCache);
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
                                                    uint32_t softwareVersion)
{
    std::string deviceUuid = source.GetDeviceId();

    icWarn("Device %s failed to upgrade to version %#" PRIx32, deviceUuid.c_str(), softwareVersion);

    updateResource(
        deviceUuid.c_str(), NULL, COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS, FIRMWARE_UPDATE_STATUS_FAILED, NULL);
}

void MatterDeviceDriver::OtaRequestorEventHandler::OnStateTransition(
    OTARequestor &source,
    OtaSoftwareUpdateRequestor::OTAUpdateStateEnum oldState,
    OtaSoftwareUpdateRequestor::OTAUpdateStateEnum newState,
    OtaSoftwareUpdateRequestor::OTAChangeReasonEnum reason,
    Nullable<uint32_t> targetSoftwareVersion)
{
    icDebug();

    using OTAUpdateStateEnum = chip::app::Clusters::OtaSoftwareUpdateRequestor::OTAUpdateStateEnum;
    using OTAChangeReasonEnum = chip::app::Clusters::OtaSoftwareUpdateRequestor::OTAChangeReasonEnum;

    if (oldState == OTAUpdateStateEnum::kDownloading && newState == OTAUpdateStateEnum::kIdle &&
        ((reason == OTAChangeReasonEnum::kFailure) || (reason == OTAChangeReasonEnum::kTimeOut)))
    {
        driver.DeviceFirmwareUpdateFailed(
            source, targetSoftwareVersion.IsNull() ? 0 : targetSoftwareVersion.Value());
        icWarn("Update faiure reason: %s", reason == OTAChangeReasonEnum::kFailure ? "Failed" : "Timed out");
    }
    else if (newState == OTAUpdateStateEnum::kDownloading)
    {
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
                                                                   Nullable<int64_t> platformCode)
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

    driver.DeviceFirmwareUpdateFailed(source, softwareVersion);
}

void MatterDeviceDriver::OtaRequestorEventHandler::OnVersionApplied(OTARequestor &source,
                                                                    uint32_t softwareVersion,
                                                                    uint16_t productId)
{
    std::string deviceUuid = source.GetDeviceId();
    std::string versionStr = Matter::VersionToString(softwareVersion);

    icInfo("Software version %s installed successfully on device %s", versionStr.c_str(), deviceUuid.c_str());

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

void MatterDeviceDriver::WifiNetworkDiagnosticsEventHandler::RssiReadComplete(const std::string &deviceUuid,
                                                                              const int8_t *rssi,
                                                                              bool success,
                                                                              void *asyncContext)
{
    auto readContext = static_cast<ClusterReadContext *>(asyncContext);

    // Per Matter 1.4 11.15.6, the RSSI attribute is not included in subscription reports, so changes to the RSSI
    // and related resources must be updated here.

    g_autofree char *rssiStr = nullptr;
    g_autofree char *linkScoreStr = nullptr;
    uint8_t linkScore = 0;
    g_autofree char *linkQuality = nullptr;
    if (rssi)
    {
        rssiStr = g_strdup_printf("%" PRId8, *rssi);
        linkScore = Subsystem::Matter::calculateLinkScore(*rssi);
        linkScoreStr = g_strdup_printf("%" PRIu8, linkScore);
        linkQuality = Subsystem::Matter::determineLinkQuality(linkScore);
    }
    else
    {
        linkQuality = g_strdup(LINK_QUALITY_UNKNOWN);
    }

    if (success)
    {
        updateResource(deviceUuid.c_str(), nullptr, COMMON_DEVICE_RESOURCE_FERSSI, rssiStr, nullptr);
        updateResource(deviceUuid.c_str(), nullptr, COMMON_DEVICE_RESOURCE_LINK_SCORE, linkScoreStr, nullptr);
        updateResource(deviceUuid.c_str(), nullptr, COMMON_DEVICE_RESOURCE_LINK_QUALITY, linkQuality, nullptr);

        // All three of these resources rely on the value of the RSSI attribute, but the readResource() call was only
        // invoked for one of them, so the value must be set to that of the requested resource.
        if (readContext->resourceId != nullptr)
        {
            if (g_strcmp0(readContext->resourceId, COMMON_DEVICE_RESOURCE_FERSSI) == 0)
            {
                *readContext->value = g_strdup(rssiStr);
            }
            else if (g_strcmp0(readContext->resourceId, COMMON_DEVICE_RESOURCE_LINK_SCORE) == 0)
            {
                *readContext->value = g_strdup(linkScoreStr);
            }
            else if (g_strcmp0(readContext->resourceId, COMMON_DEVICE_RESOURCE_LINK_QUALITY) == 0)
            {
                *readContext->value = g_strdup(linkQuality);
            }
            else
            {
                icError("Unexpected resource ID %s in RSSI read context", readContext->resourceId);
            }
        }
    }

    deviceDriver.OnDeviceWorkCompleted(readContext->driverContext, success);

    delete readContext;
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
