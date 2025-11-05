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
#define LOG_TAG     "MatterWindowCoveringDD"
#define logFmt(fmt) "(%s): " fmt, __func__
#include "subsystems/matter/MatterCommon.h"

#include "MatterDriverFactory.h"
#include "MatterWindowCoveringDeviceDriver.h"
#include "clusters/WindowCovering.h"

extern "C" {
#include <commonDeviceDefs.h>
#include <device/deviceModelHelper.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <resourceTypes.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
}

#include <chrono>
#include <subsystems/matter/Matter.h>

using namespace barton;
using chip::Callback::Callback;
using namespace std::chrono_literals;

#define MATTER_WINDOW_COVERING_DEVICE_DRIVER_NAME "matterWindowCovering"

// this is our endpoint, not the device's
#define WINDOW_COVERING_ENDPOINT                  "1"

struct ClusterReadContext
{
    void *driverContext;                            // the context provided to the driver for the operation
    icInitialResourceValues *initialResourceValues; // non-null if this read is the initial resource fetch
    char **value;                                   // non-null if this read is a regular resource read
};

// auto register with the factory
bool MatterWindowCoveringDeviceDriver::registeredWithFactory =
    MatterDriverFactory::Instance().RegisterDriver(new MatterWindowCoveringDeviceDriver());

MatterWindowCoveringDeviceDriver::MatterWindowCoveringDeviceDriver() :
    MatterDeviceDriver(MATTER_WINDOW_COVERING_DEVICE_DRIVER_NAME, WINDOW_COVERING_DC, 0)
{
}

std::vector<uint16_t> MatterWindowCoveringDeviceDriver::GetSupportedDeviceTypes()
{
    return {
        WINDOW_COVERING_CONTROLLER_DEVICE_ID,
    };
}

void MatterWindowCoveringDeviceDriver::CurrentPositionLiftPercentageChanged(std::string &deviceUuid,
                                                                            uint8_t percent,
                                                                            void *asyncContext)
{
    icDebug();

    auto server = static_cast<WindowCovering *>(GetAnyServerById(deviceUuid, WINDOW_COVERING_CLUSTER_ID));

    if (server == nullptr)
    {
        icError("WindowCovering cluster not present on device %s!", deviceUuid.c_str());
    }
    else
    {
        scoped_generic char *percentStr = stringBuilder("%" PRIu8, percent);

        updateResource(deviceUuid.c_str(),
                       WINDOW_COVERING_ENDPOINT,
                       WINDOW_COVERING_PROFILE_RESOURCE_LIFT_PERCENTAGE,
                       percentStr,
                       nullptr);
    }
}

void MatterWindowCoveringDeviceDriver::CurrentPositionLiftPercentageReadComplete(std::string &deviceUuid,
                                                                                 uint8_t percent,
                                                                                 void *asyncContext)
{
    auto readContext = static_cast<ClusterReadContext *>(asyncContext);

    scoped_generic char *percentStr = stringBuilder("%" PRIu8, percent);

    if (readContext->initialResourceValues)
    {
        initialResourceValuesPutEndpointValue(readContext->initialResourceValues,
                                              WINDOW_COVERING_ENDPOINT,
                                              WINDOW_COVERING_PROFILE_RESOURCE_LIFT_PERCENTAGE,
                                              percentStr);
    }
    else
    {
        *readContext->value = percentStr;
        percentStr = nullptr;
    }

    OnDeviceWorkCompleted(readContext->driverContext, true);

    delete readContext;
}

void MatterWindowCoveringDeviceDriver::DoSynchronizeDevice(std::forward_list<std::promise<bool>> &promises,
                                                           const std::string &deviceId,
                                                           chip::Messaging::ExchangeManager &exchangeMgr,
                                                           const chip::SessionHandle &sessionHandle)
{
    icDebug();

    // currently we dont do anything during configuration except set up reporting, which also triggers an immediate
    //  report and that takes care of synchronizing state/resources as well.
    DoConfigureDevice(promises, deviceId, nullptr, exchangeMgr, sessionHandle);
}

bool MatterWindowCoveringDeviceDriver::DoRegisterResources(icDevice *device)
{
    bool result = true;

    icDebug();

    auto deviceCache = GetDeviceDataCache(device->uuid);
    if (!deviceCache)
    {
        icError("No device cache for %s", device->uuid);
        return false;
    }

    icDeviceEndpoint *endpoint = createEndpoint(device, WINDOW_COVERING_ENDPOINT, WINDOW_COVERING_PROFILE, true);

    result &= createEndpointResource(endpoint,
                                     COMMON_ENDPOINT_RESOURCE_LABEL,
                                     "Matter WindowCovering",
                                     RESOURCE_TYPE_LABEL,
                                     RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_DYNAMIC,
                                     CACHING_POLICY_ALWAYS) != nullptr;

    auto *resource = static_cast<icDeviceResource *>(calloc(1, sizeof(icDeviceResource)));
    resource->id = strdup(WINDOW_COVERING_PROFILE_RESOURCE_LIFT_PERCENTAGE);
    resource->endpointId = strdup(WINDOW_COVERING_ENDPOINT);
    resource->deviceUuid = strdup(device->uuid);
    resource->type = strdup(RESOURCE_TYPE_PERCENTAGE);
    resource->mode = RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_DYNAMIC_CAPABLE |
                     RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_LAZY_SAVE_NEXT;
    resource->cachingPolicy = CACHING_POLICY_ALWAYS;
    linkedListAppend(endpoint->resources, resource);

    return result;
}

void MatterWindowCoveringDeviceDriver::ReadResource(std::forward_list<std::promise<bool>> &promises,
                                                    const std::string &deviceId,
                                                    icDeviceResource *resource,
                                                    char **value,
                                                    chip::Messaging::ExchangeManager &exchangeMgr,
                                                    const chip::SessionHandle &sessionHandle)
{
    bool asyncCleanup = false;

    icDebug("%s", resource->id);

    if (resource->endpointId == nullptr ||
        stringCompare(resource->id, WINDOW_COVERING_PROFILE_RESOURCE_LIFT_PERCENTAGE, false) != 0)
    {
        FailOperation(promises);
        return;
    }

    auto server = static_cast<WindowCovering *>(GetAnyServerById(deviceId, WINDOW_COVERING_CLUSTER_ID));

    if (server == nullptr)
    {
        icError("WindowCovering cluster not present on device %s", deviceId.c_str());
        FailOperation(promises);
        return;
    }

    promises.emplace_front();
    auto &readPromise = promises.front();
    auto readContext = new ClusterReadContext {};
    readContext->driverContext = &readPromise;
    readContext->value = value;
    AssociateStoredContext(&readPromise);

    if (!server->GetCurrentPositionLiftPercentage(readContext, exchangeMgr, sessionHandle))
    {
        AbandonDeviceWork(readPromise);
        delete readContext;
    }
}

bool MatterWindowCoveringDeviceDriver::WriteResource(std::forward_list<std::promise<bool>> &promises,
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

    if (resource->endpointId == nullptr ||
        stringCompare(resource->id, WINDOW_COVERING_PROFILE_RESOURCE_LIFT_PERCENTAGE, false) != 0)
    {
        FailOperation(promises);
        return false;
    }

    auto server = static_cast<WindowCovering *>(GetAnyServerById(deviceId, WINDOW_COVERING_CLUSTER_ID));

    if (server == nullptr)
    {
        icError("WindowCovering cluster not present on device %s", resource->deviceUuid);
        FailOperation(promises);
        return false;
    }

    uint8_t percent;

    if (!stringToUint8(newValue, &percent))
    {
        icError("Invalid arguments");
        FailOperation(promises);
        return false;
    }

    promises.emplace_front();
    auto &writePromise = promises.front();
    AssociateStoredContext(&writePromise);

    if (!server->GoToLiftPercentage(&writePromise, percent, exchangeMgr, sessionHandle))
    {
        AbandonDeviceWork(writePromise);
    }

    return shouldUpdateResource;
}

std::unique_ptr<MatterCluster> MatterWindowCoveringDeviceDriver::MakeCluster(std::string const &deviceUuid,
                                                                             chip::EndpointId endpointId,
                                                                             chip::ClusterId clusterId,
                                                                             std::shared_ptr<DeviceDataCache> deviceDataCache)
{
    if (clusterId == WINDOW_COVERING_CLUSTER_ID)
    {
        return std::make_unique<WindowCovering>((WindowCovering::EventHandler *) this, deviceUuid, endpointId, deviceDataCache);
    }

    return nullptr;
}
