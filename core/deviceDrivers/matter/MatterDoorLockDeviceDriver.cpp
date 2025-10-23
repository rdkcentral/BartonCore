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
 * Created by Thomas Lea on 4/4/2022.
 */
#define LOG_TAG     "MatterDoorLockDD"
#define logFmt(fmt) "(%s): " fmt, __func__
#include "subsystems/matter/MatterCommon.h"

#include "MatterDoorLockDeviceDriver.h"
#include "MatterDriverFactory.h"
#include "clusters/DoorLock.h"

extern "C" {
#include <commonDeviceDefs.h>
#include <device/deviceModelHelper.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <resourceTypes.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
}

#include <chrono>
#include <subsystems/matter/DiscoveredDeviceDetailsStore.h>
#include <subsystems/matter/Matter.h>

using namespace barton;
using chip::Callback::Callback;
using namespace std::chrono_literals;

#define MATTER_DOOR_LOCK_DEVICE_DRIVER_NAME "matterDoorLock"

// this is our endpoint, not the device's
#define DOOR_LOCK_ENDPOINT                  "1"

struct ClusterReadContext
{
    void *driverContext;                            // the context provided to the driver for the operation
    icInitialResourceValues *initialResourceValues; // non-null if this read is the initial resource fetch
    char **value;                                   // non-null if this read is a regular resource read
};

// auto register with the factory
bool MatterDoorLockDeviceDriver::registeredWithFactory =
    MatterDriverFactory::Instance().RegisterDriver(new MatterDoorLockDeviceDriver());

MatterDoorLockDeviceDriver::MatterDoorLockDeviceDriver() :
    MatterDeviceDriver(MATTER_DOOR_LOCK_DEVICE_DRIVER_NAME, DOORLOCK_DC, 0)
{
}

bool MatterDoorLockDeviceDriver::ClaimDevice(DiscoveredDeviceDetails *details)
{
    icDebug();

    // see if any endpoint (not the special 0 entry) has our device id
    for (auto &entry : details->endpointDescriptorData)
    {
        if (entry.first > 0)
        {
            for (auto &deviceTypeEntry : *entry.second->deviceTypes)
            {
                if (deviceTypeEntry == DOORLOCK_DEVICE_ID)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

void MatterDoorLockDeviceDriver::LockStateChanged(std::string &deviceUuid,
                                                  chip::app::Clusters::DoorLock::DlLockState lockState,
                                                  void *asyncContext)
{
    icDebug();

    updateResource(deviceUuid.c_str(),
                   DOOR_LOCK_ENDPOINT,
                   DOORLOCK_PROFILE_RESOURCE_LOCKED,
                   stringValueOfBool(lockState == chip::app::Clusters::DoorLock::DlLockState::kLocked),
                   nullptr);
}

void MatterDoorLockDeviceDriver::LockStateReadComplete(std::string &deviceUuid,
                                                       chip::app::Clusters::DoorLock::DlLockState lockState,
                                                       void *asyncContext)
{
    auto readContext = static_cast<ClusterReadContext *>(asyncContext);

    bool isLocked = (lockState == chip::app::Clusters::DoorLock::DlLockState::kLocked);

    if (readContext->initialResourceValues)
    {
        initialResourceValuesPutEndpointValue(readContext->initialResourceValues,
                                              DOOR_LOCK_ENDPOINT,
                                              DOORLOCK_PROFILE_RESOURCE_LOCKED,
                                              stringValueOfBool(isLocked));
    }
    else
    {
        *readContext->value = strdup(stringValueOfBool(isLocked));
    }

    OnDeviceWorkCompleted(readContext->driverContext, true);

    delete readContext;
}

void MatterDoorLockDeviceDriver::SynchronizeDevice(std::forward_list<std::promise<bool>> &promises,
                                                   const std::string &deviceId,
                                                   chip::Messaging::ExchangeManager &exchangeMgr,
                                                   const chip::SessionHandle &sessionHandle)
{
    icDebug();

    // currently we dont do anything during configuration except set up reporting, which also triggers an immediate
    //  report and that takes care of synchronizing state/resources as well.
    ConfigureDevice(promises, deviceId, nullptr, exchangeMgr, sessionHandle);
}

void MatterDoorLockDeviceDriver::FetchInitialResourceValues(std::forward_list<std::promise<bool>> &promises,
                                                            const std::string &deviceId,
                                                            icInitialResourceValues *initialResourceValues,
                                                            chip::Messaging::ExchangeManager &exchangeMgr,
                                                            const chip::SessionHandle &sessionHandle)
{
    bool result = false;

    icDebug();

    auto *server = static_cast<DoorLock *>(GetAnyServerById(deviceId, DOORLOCK_CLUSTER_ID));

    if (server == nullptr)
    {
        icError("DoorLock cluster not present on device %s!", deviceId.c_str());
        FailOperation(promises);
        return;
    }

    promises.emplace_front();
    auto &getStatePromise = promises.front();
    auto readContext = new ClusterReadContext {};
    readContext->driverContext = &getStatePromise;
    readContext->initialResourceValues = initialResourceValues;
    AssociateStoredContext(readContext->driverContext);

    if (!server->GetLockState(readContext, exchangeMgr, sessionHandle))
    {
        AbandonDeviceWork(getStatePromise);
        delete readContext;
    }
}

bool MatterDoorLockDeviceDriver::RegisterResources(icDevice *device, icInitialResourceValues *initialResourceValues)
{
    bool result = true;

    icDebug();

    icDeviceEndpoint *endpoint = createEndpoint(device, DOOR_LOCK_ENDPOINT, DOORLOCK_PROFILE, true);

    result &= createEndpointResource(endpoint,
                                     COMMON_ENDPOINT_RESOURCE_LABEL,
                                     "Matter DoorLock",
                                     RESOURCE_TYPE_LABEL,
                                     RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_DYNAMIC,
                                     CACHING_POLICY_ALWAYS) != nullptr;

    auto *resource = static_cast<icDeviceResource *>(calloc(1, sizeof(icDeviceResource)));
    resource->id = strdup(DOORLOCK_PROFILE_RESOURCE_LOCKED);
    resource->endpointId = strdup(DOOR_LOCK_ENDPOINT);
    resource->deviceUuid = strdup(device->uuid);

    const char *initialValue = initialResourceValuesGetEndpointValue(
        initialResourceValues, DOOR_LOCK_ENDPOINT, DOORLOCK_PROFILE_RESOURCE_LOCKED);
    if (initialValue)
    {
        resource->value = strdup(initialResourceValuesGetEndpointValue(
            initialResourceValues, DOOR_LOCK_ENDPOINT, DOORLOCK_PROFILE_RESOURCE_LOCKED));
    }
    else
    {
        result = false;
    }

    resource->type = strdup(RESOURCE_TYPE_BOOLEAN);
    resource->mode = RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_DYNAMIC_CAPABLE |
                     RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_LAZY_SAVE_NEXT;
    resource->cachingPolicy = CACHING_POLICY_ALWAYS;
    linkedListAppend(endpoint->resources, resource);

    return result;
}

void MatterDoorLockDeviceDriver::ReadResource(std::forward_list<std::promise<bool>> &promises,
                                              const std::string &deviceId,
                                              icDeviceResource *resource,
                                              char **value,
                                              chip::Messaging::ExchangeManager &exchangeMgr,
                                              const chip::SessionHandle &sessionHandle)
{
    bool asyncCleanup = false;

    icDebug("%s", resource->id);

    if (resource->endpointId == nullptr || stringCompare(resource->id, DOORLOCK_PROFILE_RESOURCE_LOCKED, false) != 0)
    {
        FailOperation(promises);
        return;
    }

    auto *server = static_cast<DoorLock *>(GetAnyServerById(deviceId, DOORLOCK_CLUSTER_ID));

    if (server == nullptr)
    {
        icError("DoorLock cluster not present on device %s!", resource->deviceUuid);
        FailOperation(promises);
        return;
    }

    promises.emplace_front();
    auto &readPromise = promises.front();
    auto readContext = new ClusterReadContext {};
    readContext->driverContext = &readPromise;
    readContext->value = value;
    AssociateStoredContext(readContext->driverContext);

    if (!server->GetLockState(readContext, exchangeMgr, sessionHandle))
    {
        AbandonDeviceWork(readPromise);
        delete readContext;
    }
}

bool MatterDoorLockDeviceDriver::WriteResource(std::forward_list<std::promise<bool>> &promises,
                                               const std::string &deviceId,
                                               icDeviceResource *resource,
                                               const char *previousValue,
                                               const char *newValue,
                                               chip::Messaging::ExchangeManager &exchangeMgr,
                                               const chip::SessionHandle &sessionHandle)
{
    bool asyncCleanup = false;

    icDebug("%s = %s", resource->id, newValue);

    bool shouldUpdateResource = true;

    if (resource->endpointId == nullptr || stringCompare(resource->id, DOORLOCK_PROFILE_RESOURCE_LOCKED, false) != 0)
    {
        FailOperation(promises);
        return false;
    }

    auto *server = static_cast<DoorLock *>(GetAnyServerById(deviceId, DOORLOCK_CLUSTER_ID));

    if (server == nullptr)
    {
        icError("DoorLock cluster not present on device %s!", deviceId.c_str());
        FailOperation(promises);
        return false;
    }

    bool locked;

    if (!stringToBoolStrict(newValue, &locked))
    {
        FailOperation(promises);
        icError("Invalid arguments");

        return false;
    }

    promises.emplace_front();
    auto &setPromise = promises.front();
    AssociateStoredContext(&setPromise);

    if (!server->SetLocked(&setPromise, locked, exchangeMgr, sessionHandle))
    {
        AbandonDeviceWork(setPromise);
    }

    return shouldUpdateResource;
}

std::unique_ptr<MatterCluster> MatterDoorLockDeviceDriver::MakeCluster(std::string const &deviceUuid,
                                                                       chip::EndpointId endpointId,
                                                                       chip::ClusterId clusterId)
{
    if (clusterId == DOORLOCK_CLUSTER_ID)
    {
        return std::make_unique<DoorLock>((DoorLock::EventHandler *) this, deviceUuid, endpointId);
    }

    return nullptr;
}
