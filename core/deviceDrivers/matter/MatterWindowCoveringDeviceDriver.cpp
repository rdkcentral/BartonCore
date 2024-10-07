//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2021 Comcast Corporation
//
// All rights reserved.
//
// This software is protected by copyright laws of the United States
// and of foreign countries. This material may also be protected by
// patent laws of the United States and of foreign countries.
//
// This software is furnished under a license agreement and/or a
// nondisclosure agreement and may only be used or copied in accordance
// with the terms of those agreements.
//
// The mere transfer of this software does not imply any licenses of trade
// secrets, proprietary technology, copyrights, patents, trademarks, or
// any other form of intellectual property whatsoever.
//
// Comcast Corporation retains all ownership rights.
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
#include <subsystems/matter/DiscoveredDeviceDetailsStore.h>
#include <subsystems/matter/Matter.h>

using namespace zilker;
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

bool MatterWindowCoveringDeviceDriver::ClaimDevice(DiscoveredDeviceDetails *details)
{
    icDebug();

    // see if any endpoint (not the special 0 entry) has our device id
    for (auto &entry : details->endpointDescriptorData)
    {
        if (entry.first > 0)
        {
            for (auto &deviceTypeEntry : *entry.second->deviceTypes)
            {
                if (deviceTypeEntry == WINDOW_COVERING_CONTROLLER_DEVICE_ID)
                {
                    return true;
                }
            }
        }
    }

    return false;
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

std::vector<MatterCluster *> MatterWindowCoveringDeviceDriver::GetClustersToSubscribeTo(const std::string &deviceId)
{
    icDebug();

    auto windowCoveringServer = static_cast<WindowCovering *>(GetAnyServerById(deviceId, WINDOW_COVERING_CLUSTER_ID));
    if (windowCoveringServer == nullptr)
    {
        icError("No window covering cluster on device %s!", deviceId.c_str());
        return {};
    }

    std::vector<MatterCluster *> clusters;
    clusters.push_back(windowCoveringServer);
    return clusters;
}

void MatterWindowCoveringDeviceDriver::SynchronizeDevice(std::forward_list<std::promise<bool>> &promises,
                                                         const std::string &deviceId,
                                                         chip::Messaging::ExchangeManager &exchangeMgr,
                                                         const chip::SessionHandle &sessionHandle)
{
    icDebug();

    // currently we dont do anything during configuration except set up reporting, which also triggers an immediate
    //  report and that takes care of synchronizing state/resources as well.
    ConfigureDevice(promises, deviceId, nullptr, exchangeMgr, sessionHandle);
}

void MatterWindowCoveringDeviceDriver::FetchInitialResourceValues(std::forward_list<std::promise<bool>> &promises,
                                                                  const std::string &deviceId,
                                                                  icInitialResourceValues *initialResourceValues,
                                                                  chip::Messaging::ExchangeManager &exchangeMgr,
                                                                  const chip::SessionHandle &sessionHandle)
{
    bool result = false;

    icDebug();

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
    readContext->initialResourceValues = initialResourceValues;
    AssociateStoredContext(readContext->driverContext);

    if (!server->GetCurrentPositionLiftPercentage(readContext, exchangeMgr, sessionHandle))
    {
        AbandonDeviceWork(readPromise);
        delete readContext;
    }
}

bool MatterWindowCoveringDeviceDriver::RegisterResources(icDevice *device,
                                                         icInitialResourceValues *initialResourceValues)
{
    bool result = true;

    icDebug();

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

    const char *initialValue = initialResourceValuesGetEndpointValue(
        initialResourceValues, WINDOW_COVERING_ENDPOINT, WINDOW_COVERING_PROFILE_RESOURCE_LIFT_PERCENTAGE);
    if (initialValue)
    {
        resource->value = strdup(initialResourceValuesGetEndpointValue(
            initialResourceValues, WINDOW_COVERING_ENDPOINT, WINDOW_COVERING_PROFILE_RESOURCE_LIFT_PERCENTAGE));
    }
    else
    {
        result = false;
    }
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
                                                                             chip::ClusterId clusterId)
{
    if (clusterId == WINDOW_COVERING_CLUSTER_ID)
    {
        return std::make_unique<WindowCovering>((WindowCovering::EventHandler *) this, deviceUuid, endpointId);
    }

    return nullptr;
}
