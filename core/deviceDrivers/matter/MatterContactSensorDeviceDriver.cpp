// ------------------------------ tabstop = 4 ----------------------------------
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
// ------------------------------ tabstop = 4 ----------------------------------

//
// Created by Raiyan Chowdhury on 8/9/2024.
//

#define LOG_TAG     "MatterContactSensorDD"
#define logFmt(fmt) "(%s): " fmt, __func__
#include "subsystems/matter/MatterCommon.h"

#include "MatterContactSensorDeviceDriver.h"
#include "MatterDriverFactory.h"
#include "clusters/BooleanState.h"
#include "app-common/zap-generated/ids/Clusters.h"

extern "C" {
#include <commonDeviceDefs.h>
#include <device/deviceModelHelper.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <resourceTypes.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
}

#include <subsystems/matter/DiscoveredDeviceDetailsStore.h>
#include <subsystems/matter/Matter.h>

using namespace barton;

#define MATTER_CONTACT_SENSOR_ID                 0x0015
#define MATTER_CONTACT_SENSOR_DEVICE_DRIVER_NAME "matterContactSensor"

// this is our endpoint, not the device's
#define CONTACT_SENSOR_ENDPOINT "1"

// auto register with the factory
bool MatterContactSensorDeviceDriver::registeredWithFactory =
    MatterDriverFactory::Instance().RegisterDriver(new MatterContactSensorDeviceDriver());

MatterContactSensorDeviceDriver::MatterContactSensorDeviceDriver() :
    MatterDeviceDriver(MATTER_CONTACT_SENSOR_DEVICE_DRIVER_NAME, SENSOR_DC, 0)
{
    booleanStateClusterEventHandler.deviceDriver = this;
}

bool MatterContactSensorDeviceDriver::ClaimDevice(DiscoveredDeviceDetails *details)
{
    icDebug();

    // see if any endpoint (not the special 0 entry) has our device id
    for (auto &entry : details->endpointDescriptorData)
    {
        if (entry.first > 0)
        {
            for (auto &deviceTypeEntry : *entry.second->deviceTypes)
            {
                if (deviceTypeEntry == MATTER_CONTACT_SENSOR_ID)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

void MatterContactSensorDeviceDriver::SynchronizeDevice(std::forward_list<std::promise<bool>> &promises,
                                                        const std::string &deviceId,
                                                        chip::Messaging::ExchangeManager &exchangeMgr,
                                                        const chip::SessionHandle &sessionHandle)
{
    icDebug();

    // currently we dont do anything during configuration except set up reporting, which also triggers an immediate
    // report and that takes care of synchronizing state/resources as well.
    ConfigureDevice(promises, deviceId, nullptr, exchangeMgr, sessionHandle);
}

void MatterContactSensorDeviceDriver::FetchInitialResourceValues(std::forward_list<std::promise<bool>> &promises,
                                                                 const std::string &deviceId,
                                                                 icInitialResourceValues *initialResourceValues,
                                                                 chip::Messaging::ExchangeManager &exchangeMgr,
                                                                 const chip::SessionHandle &sessionHandle)
{
    icDebug();

    using namespace chip::app;
    auto booleanStateServer = static_cast<BooleanState *>(GetAnyServerById(deviceId, Clusters::BooleanState::Id));
    if (booleanStateServer == nullptr)
    {
        icError("BooleanState cluster not present on device %s!", deviceId.c_str());
        FailOperation(promises);
        return;
    }

    promises.emplace_front();
    auto &readPromise = promises.front();
    auto readContext = new ClusterReadContext {};
    readContext->driverContext = &readPromise;
    readContext->initialResourceValues = initialResourceValues;

    // Paired with StateValueReadComplete
    AssociateStoredContext(readContext->driverContext);

    if (!booleanStateServer->GetStateValue(readContext, exchangeMgr, sessionHandle))
    {
        AbandonDeviceWork(readPromise);
        delete readContext;
    }

    auto generalDiagnosticsServer = static_cast<barton::GeneralDiagnostics *>(
        GetAnyServerById(deviceId, chip::app::Clusters::GeneralDiagnostics::Id));

    if (generalDiagnosticsServer == nullptr)
    {
        icError("No general diagnostics server on device %s!", deviceId.c_str());
        FailOperation(promises);
        return;
    }

    std::vector<chip::app::Clusters::GeneralDiagnostics::HardwareFaultEnum> currentFaults;
    CHIP_ERROR err = generalDiagnosticsServer->GetCurrentActiveHardwareFaults(currentFaults);

    if (err == CHIP_NO_ERROR)
    {
        bool isTampered = (std::find(currentFaults.begin(), currentFaults.end(), HardwareFaultEnum::kTamperDetected) !=
                           currentFaults.end());

        initialResourceValuesPutEndpointValue(initialResourceValues,
                                              CONTACT_SENSOR_ENDPOINT,
                                              SENSOR_PROFILE_RESOURCE_TAMPERED,
                                              stringValueOfBool(isTampered));
    }
    else if (err == CHIP_ERROR_KEY_NOT_FOUND)
    {
        icInfo("Device %s does not support active hardware faults attribute", deviceId.c_str());
        // This attribute is optional to support as per Matter 1.4 spec 11.12.6, so no need to fail the operation here.
    }
    else
    {
        icError("Could not get active hardware faults attribute from device %s: %s", deviceId.c_str(), err.AsString());
        FailOperation(promises);
        return;
    }
}

bool MatterContactSensorDeviceDriver::RegisterResources(icDevice *device, icInitialResourceValues *initialResourceValues)
{
    icDebug();

    bool result = true;

    icDeviceEndpoint *sensorEndpoint = createEndpoint(device, CONTACT_SENSOR_ENDPOINT, SENSOR_PROFILE, true);

    result &= createEndpointResource(sensorEndpoint,
                                     COMMON_ENDPOINT_RESOURCE_LABEL,
                                     "Matter ContactSensor",
                                     RESOURCE_TYPE_LABEL,
                                     RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_DYNAMIC,
                                     CACHING_POLICY_ALWAYS) != nullptr;

    const char *initialIsFaultedValue = initialResourceValuesGetEndpointValue(
        initialResourceValues, CONTACT_SENSOR_ENDPOINT, SENSOR_PROFILE_RESOURCE_FAULTED);

    result &= createEndpointResource(sensorEndpoint,
                                     SENSOR_PROFILE_RESOURCE_FAULTED,
                                     initialIsFaultedValue,
                                     RESOURCE_TYPE_BOOLEAN,
                                     RESOURCE_MODE_READABLE | RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_DYNAMIC,
                                     CACHING_POLICY_ALWAYS) != nullptr;

    result &= createEndpointResource(sensorEndpoint,
                                     SENSOR_PROFILE_RESOURCE_TYPE,
                                     SENSOR_PROFILE_CONTACT_SWITCH_TYPE,
                                     RESOURCE_TYPE_SENSOR_TYPE,
                                     RESOURCE_MODE_READABLE,
                                     CACHING_POLICY_ALWAYS) != nullptr;

    const char *initialIsTamperedValue = initialResourceValuesGetEndpointValue(
        initialResourceValues, CONTACT_SENSOR_ENDPOINT, SENSOR_PROFILE_RESOURCE_TAMPERED);

    result &= createEndpointResource(sensorEndpoint,
                                     SENSOR_PROFILE_RESOURCE_TAMPERED,
                                     initialIsTamperedValue,
                                     RESOURCE_TYPE_BOOLEAN,
                                     RESOURCE_MODE_READABLE | RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_DYNAMIC,
                                     CACHING_POLICY_ALWAYS) != nullptr;

    return result;
}

void MatterContactSensorDeviceDriver::ReadResource(std::forward_list<std::promise<bool>> &promises,
                                                   const std::string &deviceId,
                                                   icDeviceResource *resource,
                                                   char **value,
                                                   chip::Messaging::ExchangeManager &exchangeMgr,
                                                   const chip::SessionHandle &sessionHandle)
{
    icDebug("%s", resource->id);

    if (resource->endpointId == nullptr || stringCompare(resource->id, SENSOR_PROFILE_RESOURCE_FAULTED, false) != 0)
    {
        FailOperation(promises);
        return;
    }

    using namespace chip::app;
    auto *server = static_cast<BooleanState *>(GetAnyServerById(deviceId, Clusters::BooleanState::Id));

    if (server == nullptr)
    {
        icError("BooleanState cluster not present on device %s!", resource->deviceUuid);
        FailOperation(promises);
        return;
    }

    promises.emplace_front();
    auto &readPromise = promises.front();
    auto readContext = new ClusterReadContext {};
    readContext->driverContext = &readPromise;
    readContext->value = value;

    // Paired with StateValueReadComplete
    AssociateStoredContext(readContext->driverContext);

    if (!server->GetStateValue(readContext, exchangeMgr, sessionHandle))
    {
        AbandonDeviceWork(readPromise);
        delete readContext;
    }
}

std::unique_ptr<MatterCluster> MatterContactSensorDeviceDriver::MakeCluster(std::string const &deviceUuid,
                                                                            chip::EndpointId endpointId,
                                                                            chip::ClusterId clusterId)
{
    using namespace chip::app;
    switch (clusterId)
    {
        case Clusters::BooleanState::Id:
            return std::make_unique<BooleanState>(
                (BooleanState::EventHandler *) &booleanStateClusterEventHandler, deviceUuid, endpointId);
            break;

        default:
            return nullptr;
    }
}

void MatterContactSensorDeviceDriver::SetTamperedEndpointResource(const std::string &deviceId, bool tampered)
{
    icDebug();

    updateResource(
        deviceId.c_str(), CONTACT_SENSOR_ENDPOINT, SENSOR_PROFILE_RESOURCE_TAMPERED, stringValueOfBool(tampered), NULL);
}

void MatterContactSensorDeviceDriver::BooleanStateClusterEventHandler::StateValueChanged(std::string &deviceUuid,
                                                                                         bool state)
{
    icDebug();

    bool faulted = !state;
    updateResource(deviceUuid.c_str(),
                   CONTACT_SENSOR_ENDPOINT,
                   SENSOR_PROFILE_RESOURCE_FAULTED,
                   stringValueOfBool(faulted),
                   nullptr);
}

void MatterContactSensorDeviceDriver::BooleanStateClusterEventHandler::StateValueReadComplete(std::string &deviceUuid,
                                                                                              bool state,
                                                                                              void *asyncContext)
{
    icDebug();

    auto readContext = static_cast<ClusterReadContext *>(asyncContext);

    // state == false means open, so faulted is the inverse of the boolean state
    bool faulted = !state;

    if (readContext->initialResourceValues)
    {
        initialResourceValuesPutEndpointValue(readContext->initialResourceValues,
                                              CONTACT_SENSOR_ENDPOINT,
                                              SENSOR_PROFILE_RESOURCE_FAULTED,
                                              stringValueOfBool(faulted));
    }
    else
    {
        *readContext->value = strdup(stringValueOfBool(faulted));
    }

    deviceDriver->OnDeviceWorkCompleted(readContext->driverContext, true);

    delete readContext;
}
