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
//
// Created by mkoch201 on 5/16/19.
//

#include "device-driver/device-driver-manager.h"
#include "deviceDrivers/zigbeeDriverCommon.h"
#include <commonDeviceDefs.h>
#include <device/deviceModelHelper.h>
#include <icLog/logging.h>
#include <icUtil/array.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>

#define LOG_TAG                "ZigBeePresenceDD"
#define DEVICE_DRIVER_NAME     "ZigBeePresenceDD"
#define MY_DC_VERSION          2

#define PRESENCE_DEVICE_ID     0x1080
#define COMM_FAIL_TIMEOUT_SECS "95"

/* ZigbeeDriverCommonCallbacks */
static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId);
static bool registerResources(ZigbeeDriverCommon *ctx,
                              icDevice *device,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *initialResourceValues);

static const uint16_t myDeviceIds[] = {PRESENCE_DEVICE_ID};

// zigbee device driver registration order matters, so we pick constructor priority carefully
__attribute__((constructor(370))) static void driverRegister(void)
{
    static const ZigbeeDriverCommonCallbacks myHooks = {.mapDeviceIdToProfile = mapDeviceIdToProfile,
                                                        .registerResources = registerResources};

    DeviceDriver *myDriver = zigbeeDriverCommonCreateDeviceDriver(DEVICE_DRIVER_NAME,
                                                                  PRESENCE_DC,
                                                                  MY_DC_VERSION,
                                                                  myDeviceIds,
                                                                  ARRAY_LENGTH(myDeviceIds),
                                                                  RX_MODE_SLEEPY,
                                                                  &myHooks,
                                                                  false);

    // Doesn't need to be in device descriptor, these are special experimental devices
    myDriver->neverReject = true;

    deviceDriverManagerRegisterDriver(myDriver);
}

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId)
{
    const char *profile = NULL;

    switch (deviceId)
    {
        case PRESENCE_DEVICE_ID:
            profile = PRESENCE_PROFILE;
            break;
        default:
            break;
    }

    return profile;
}


static bool registerResources(ZigbeeDriverCommon *ctx,
                              icDevice *device,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *initialResourceValues)
{

    bool result = true;

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    // Need an endpoint or the device gets removed
    icDeviceEndpoint *endpoint = createEndpoint(device, "1", PRESENCE_PROFILE, true);

    zigbeeDriverCommonSetEndpointNumber(endpoint, 1);

    const char *tmp = deviceGetMetadata(device, COMMON_DEVICE_METADATA_COMMFAIL_OVERRIDE_SECS);

    if (tmp == NULL)
    {
        createDeviceMetadata(device, COMMON_DEVICE_METADATA_COMMFAIL_OVERRIDE_SECS, COMM_FAIL_TIMEOUT_SECS);
    }

    return result;
}
