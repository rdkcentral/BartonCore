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
 * Created by Thomas Lea on 11/17/2025
 */
#define LOG_TAG     "MatterCastingVideoClientDD"
#define logFmt(fmt) "(%s): " fmt, __func__
#include "subsystems/matter/MatterCommon.h"

#include "MatterCastingVideoClientDeviceDriver.h"
#include "MatterDriverFactory.h"

extern "C" {
#include <commonDeviceDefs.h>
#include <device/deviceModelHelper.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <resourceTypes.h>
}

#include <subsystems/matter/DiscoveredDeviceDetailsStore.h>
#include <subsystems/matter/Matter.h>

using namespace barton;
using chip::Callback::Callback;

#define MATTER_CASTING_VIDEO_CLIENT_DEVICE_DRIVER_NAME "matterCastingVideoClient"
#define CASTING_VIDEO_CLIENT_DC "castingVideoClient"
#define CASTING_VIDEO_CLIENT_DEVICE_TYPE_ID 0x0029

// this is our endpoint, not the device's
#define CASTING_VIDEO_CLIENT_ENDPOINT                  "1"

struct ClusterReadContext
{
    void *driverContext;                            // the context provided to the driver for the operation
    icInitialResourceValues *initialResourceValues; // non-null if this read is the initial resource fetch
    char **value;                                   // non-null if this read is a regular resource read
};

// auto register with the factory
bool MatterCastingVideoClientDeviceDriver::registeredWithFactory =
    MatterDriverFactory::Instance().RegisterDriver(new MatterCastingVideoClientDeviceDriver());

MatterCastingVideoClientDeviceDriver::MatterCastingVideoClientDeviceDriver() :
    MatterDeviceDriver(MATTER_CASTING_VIDEO_CLIENT_DEVICE_DRIVER_NAME, CASTING_VIDEO_CLIENT_DC, 0)
{
}

bool MatterCastingVideoClientDeviceDriver::ClaimDevice(DiscoveredDeviceDetails *details)
{
    icDebug();

    // see if any endpoint (not the special 0 entry) has our device id
    for (auto &entry : details->endpointDescriptorData)
    {
        if (entry.first > 0)
        {
            for (auto &deviceTypeEntry : *entry.second->deviceTypes)
            {
                if (deviceTypeEntry == CASTING_VIDEO_CLIENT_DEVICE_TYPE_ID)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

std::unique_ptr<MatterCluster> MatterCastingVideoClientDeviceDriver::MakeCluster(std::string const &deviceUuid,
                                                                       chip::EndpointId endpointId,
                                                                       chip::ClusterId clusterId)
{
    return nullptr;
}
