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

/*
 * Created by Thomas Lea on 10/20/25
 */
#define LOG_TAG     "SBMDD"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "SpecBasedMatterDeviceDriver.h"
#include "SbmdCluster.h"
#include "SbmdScriptExecutor.h"
#include "matter/sbmd/SbmdSpec.h"

extern "C" {
#include <commonDeviceDefs.h>
#include <device/deviceModelHelper.h>
#include <device/icDeviceResource.h>
#include <deviceService/resourceModes.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <resourceTypes.h>
}

#include <subsystems/matter/Matter.h>

using namespace barton;
using namespace std::chrono_literals;

#define BASE_SBMD_DRIVER_NAME "sbmd-"

struct ClusterReadContext
{
    void *driverContext;                            // the context provided to the driver for the operation
    icInitialResourceValues *initialResourceValues; // non-null if this read is the initial resource fetch
    const SbmdMapper *mapper;                       // non-null mapper for the read request
    char **value;                                   // non-null if this read is a regular resource read
};

SpecBasedMatterDeviceDriver::SpecBasedMatterDeviceDriver(std::unique_ptr<SbmdSpec> _spec) :
    MatterDeviceDriver((BASE_SBMD_DRIVER_NAME + _spec->name).c_str(),
                       _spec->bartonMeta.deviceClass.c_str(),
                       _spec->bartonMeta.deviceClassVersion),
    spec(std::move(_spec))
{
    icDebug("Created SBMD driver for: %s", spec->name.c_str());
}
std::vector<uint16_t> SpecBasedMatterDeviceDriver::GetSupportedDeviceTypes()
{
    return {};
}

SubscriptionIntervalSecs SpecBasedMatterDeviceDriver::GetDesiredSubscriptionIntervalSecs()
{
    icDebug();

    return {spec->reporting.minSecs, spec->reporting.maxSecs};
}

bool SpecBasedMatterDeviceDriver::DoRegisterResources(icDevice *device)
{
    bool result = true;

    icDebug();

    // register device resources
    for (auto &sbmdResource : spec->resources)
    {
        uint8_t resourceMode = ConvertModesToBitmask(sbmdResource.modes);
        ResourceCachingPolicy cachingPolicy = ConvertCachingPolicy(sbmdResource.cachingPolicy);

        // if an executable mapper was provided, we need to make sure the resource is executable
        if (sbmdResource.mapper.hasExecute)
        {
            resourceMode |= RESOURCE_MODE_EXECUTABLE;
        }

        result &=
            createDeviceResource(
                device, sbmdResource.id.c_str(), nullptr, sbmdResource.type.c_str(), resourceMode, cachingPolicy) !=
            nullptr;
    }

    // register endpoint resources
    for (auto &sbmdEndpoint : spec->endpoints)
    {
        icDeviceEndpoint *endpoint =
            createEndpoint(device, sbmdEndpoint.id.c_str(), sbmdEndpoint.profile.c_str(), true);

        for (auto &sbmdResource : sbmdEndpoint.resources)
        {
            uint8_t resourceMode = ConvertModesToBitmask(sbmdResource.modes);
            ResourceCachingPolicy cachingPolicy = ConvertCachingPolicy(sbmdResource.cachingPolicy);

            // if an executable mapper was provided, we need to make sure the resource is executable
            if (sbmdResource.mapper.hasExecute)
            {
                resourceMode |= RESOURCE_MODE_EXECUTABLE;
            }

            result &= createEndpointResource(endpoint,
                                             sbmdResource.id.c_str(),
                                             nullptr,
                                             sbmdResource.type.c_str(),
                                             resourceMode,
                                             cachingPolicy) != nullptr;
        }
    }

    return result;
}

void SpecBasedMatterDeviceDriver::ReadResource(std::forward_list<std::promise<bool>> &promises,
                                               const std::string &deviceId,
                                               icDeviceResource *resource,
                                               char **value,
                                               chip::Messaging::ExchangeManager &exchangeMgr,
                                               const chip::SessionHandle &sessionHandle)
{
    icDebug("%s", resource->id);

    //get the attribute info for the matching resource in the spec
    const SbmdResource *sbmdResource = nullptr;
    for (const auto &res : spec->resources)
    {
        if (res.id == resource->id)
        {
            sbmdResource = &res;
            break;
        }
    }

    if (sbmdResource == nullptr)
    {
        // check endpoint resources
        for (const auto &sbmdEndpoint : spec->endpoints)
        {
            for (const auto &res : sbmdEndpoint.resources)
            {
                if (res.id == resource->id)
                {
                    sbmdResource = &res;
                    break;
                }
            }
            if (sbmdResource != nullptr)
            {
                break;
            }
        }
    }

    if (sbmdResource == nullptr)
    {
        icError("No matching resource found in spec for resource %s", resource->id);
        FailOperation(promises);
        return;
    }

    if (!sbmdResource->mapper.hasRead)
    {
        icError("No read mapper defined for resource %s", resource->id);
        FailOperation(promises);
        return;
    }


    auto *cluster = static_cast<SbmdCluster *>(GetAnyServerById(deviceId, sbmdResource->mapper.readAttribute.clusterId));
    if (cluster == nullptr)
    {
        icError("SbmdCluster 0x%04x not present on device %s!", sbmdResource->mapper.readAttribute.clusterId, resource->deviceUuid);
        FailOperation(promises);
        return;
    }

    promises.emplace_front();
    auto &readPromise = promises.front();
    auto readContext = new ClusterReadContext {};
    readContext->driverContext = &readPromise;
    readContext->mapper = const_cast<SbmdMapper*>(&sbmdResource->mapper); //TODO: ditch const_cast
    readContext->value = value;
    AssociateStoredContext(readContext->driverContext);

    if (cluster->ReadAttribute(readContext, sbmdResource->mapper.readAttribute.attributeId, exchangeMgr, sessionHandle) != CHIP_NO_ERROR)
    {
        icError("Failed to initiate read for resource %s", resource->id);
        AbandonDeviceWork(readPromise);
        delete readContext;
    }
}

bool SpecBasedMatterDeviceDriver::WriteResource(std::forward_list<std::promise<bool>> &promises,
                                                const std::string &deviceId,
                                                icDeviceResource *resource,
                                                const char *previousValue,
                                                const char *newValue,
                                                chip::Messaging::ExchangeManager &exchangeMgr,
                                                const chip::SessionHandle &sessionHandle)
{
    icDebug("%s = %s", resource->id, newValue);
    return false;
}

void SpecBasedMatterDeviceDriver::AttributeReadComplete(std::string &deviceUuid,
                                                        uint16_t clusterId,
                                                        uint16_t attributeId,
                                                        chip::TLV::TLVReader &reader,
                                                        void *asyncContext)
{
    icDebug("clusterId: 0x%04x, attributeId: 0x%04x", clusterId, attributeId);

    bool readResult = true;
    auto readContext = static_cast<ClusterReadContext *>(asyncContext);
    auto tlvType = reader.GetType();

    switch (tlvType)
    {
        case chip::TLV::kTLVType_Boolean:
        {
            if (readContext->value != nullptr)
            {
                // the output result value comes from running the mapper's javascript with value passed in as the
                // argument.
                if (!readContext->mapper->readScript.empty())
                {
                    if (!SbmdScriptExecutor::ExecuteMapperScript(readContext->mapper->readScript,
                                                                 readContext->mapper->readAttribute.name,
                                                                 reader,
                                                                 readContext->value))
                    {
                        icError("Failed to execute read mapper script");
                        readResult = false;
                    }
                }
                else
                {
                    // no script, so just read the value directly
                    bool value;
                    if (reader.Get(value) == CHIP_NO_ERROR)
                    {
                        const char *valueStr = value ? "true" : "false";
                        *(readContext->value) = strdup(valueStr);
                    }
                    else
                    {
                        icError("Failed to decode boolean value");
                        readResult = false;
                    }
                }
            }
            break;
        }

        default:
        {
            icError("Unsupported TLV type: %d", tlvType);
            readResult = false;
            break;
        }
    }

    OnDeviceWorkCompleted(readContext->driverContext, readResult);
    delete readContext;
}

uint8_t SpecBasedMatterDeviceDriver::ConvertModesToBitmask(const std::vector<std::string> &modes)
{
    uint8_t bitmask = 0;

    for (const auto &mode : modes)
    {
        if (mode == "read")
        {
            bitmask |= RESOURCE_MODE_READABLE;
        }
        else if (mode == "write")
        {
            bitmask |= RESOURCE_MODE_WRITEABLE;
        }
        else if (mode == "execute")
        {
            bitmask |= RESOURCE_MODE_EXECUTABLE;
        }
        else if (mode == "dynamic")
        {
            bitmask |= RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_DYNAMIC_CAPABLE;
        }
        else if (mode == "emitEvents")
        {
            bitmask |= RESOURCE_MODE_EMIT_EVENTS;
        }
        else if (mode == "lazySaveNext")
        {
            bitmask |= RESOURCE_MODE_LAZY_SAVE_NEXT;
        }
        else if (mode == "sensitive")
        {
            bitmask |= RESOURCE_MODE_SENSITIVE;
        }
        else
        {
            icLogWarn(LOG_TAG, "Unknown resource mode: %s", mode.c_str());
        }
    }

    return bitmask;
}

ResourceCachingPolicy SpecBasedMatterDeviceDriver::ConvertCachingPolicy(const std::string &policy)
{
    if (policy == "always")
    {
        return CACHING_POLICY_ALWAYS;
    }
    else if (policy == "never")
    {
        return CACHING_POLICY_NEVER;
    }
    else
    {
        icLogWarn(LOG_TAG, "Unknown caching policy: %s, defaulting to always", policy.c_str());
        return CACHING_POLICY_ALWAYS;
    }
}

std::unique_ptr<MatterCluster> SpecBasedMatterDeviceDriver::MakeCluster(std::string const &deviceUuid,
                                                                        chip::EndpointId endpointId,
                                                                        chip::ClusterId clusterId,
                                                                         std::shared_ptr<DeviceDataCache> deviceDataCache)
{
    return std::make_unique<SbmdCluster>((SbmdCluster::EventHandler *) this, deviceUuid, endpointId, clusterId, deviceDataCache);
}
